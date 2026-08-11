#include "client.hpp"
#include "details/mask_payload.hpp"
#include "details/parse_meta.hpp"
#include "details/prepare_meta.hpp"
#include "handshake.hpp"
#include "logger.hpp"
#include "simdutf.h"

namespace ws {

std::expected<Client, Error> Client::create(const std::string_view host,
                                            const std::string_view path,
                                            const std::string_view port) {
    auto s = TcpSocket::connect(host, port);
    if (!s) {
        return std::unexpected(s.error());
    }
    auto hk = perform_handshake(*s, host, path, port);
    if (!hk) {
        // ws::log::error("Handshake failed: {}", hk.error());
        return std::unexpected(hk.error());
    }
    ws::log::info("Client created and handshake complete");
    Client c = (std::move(*s));
    if (!hk->leftover.empty()) {
        c.current_read.data = std::move(hk->leftover);
        // c.current_read.remaing_bytes = c.current_read.data.size();
    }
    return c;
}

std::expected<void, Error>
Client::send_control_frame(std::span<const uint8_t> payload, opcode op) {
    if (payload.size() > 125) {
        return std::unexpected(Error::InvalidPayloadLength);
    }
    auto meta = detail::format_meta(true, true, op, payload.size(), m_rng());
    if (auto sz = m_socket.send(meta.span(), payload); !sz) {
        return std::unexpected(sz.error());
    }
    return {};
}

std::expected<void, Error> Client::send_impl(std::span<const uint8_t> msg,
                                             opcode first_op) {
    constexpr int                   chunk_size = 8 * 1024;
    bool                            first      = true;
    std::array<uint8_t, chunk_size> tmp;

    while (msg.size() > chunk_size) {
        // First frame gets the actual opcode, the rest get continuation (0x0)
        opcode op = first ? first_op : opcode::continuation;

        uint32_t mask = m_rng();
        auto     meta = detail::format_meta(false, true, op, chunk_size, mask);
        std::copy(msg.begin(), msg.begin() + chunk_size, tmp.begin());
        mask_payload(tmp, mask);

        ws::log::debug("Sending chunk of size {} bytes with opcode {}",
                       chunk_size, static_cast<int>(op));
        if (auto l = m_socket.send(meta.span(), tmp); !l) {
            // ws::log::error("Failed to send chunk: {}", l.error());
            return std::unexpected(l.error());
        }
        msg   = msg.subspan(chunk_size);
        first = false;
    }

    opcode   op   = first ? first_op : opcode::continuation;
    uint32_t mask = m_rng();
    std::copy(msg.begin(), msg.begin() + msg.size(), tmp.begin());
    auto meta = detail::format_meta(true, true, op, msg.size(), mask);
    mask_payload(tmp, mask);

    ws::log::debug("Sending final chunk of size {} bytes with opcode {}",
                   msg.size(), static_cast<int>(op));
    if (auto l = m_socket.send(meta.span(), {tmp.data(), msg.size()}); !l) {
        // ws::log::error("Failed to send final chunk: {}", l.error());
        return std::unexpected(l.error());
    }
    return {};
}

std::expected<void, Error> Client::send(std::span<const uint8_t> msg) {
    return send_impl(msg, opcode::binary);
}

std::expected<void, Error> Client::send(const std::string_view msg) {
    if (!simdutf::validate_utf8(msg.data(), msg.size())) {
        return std::unexpected(Error::InvalidUTF8);
    }
    auto buf = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t *>(msg.data()), msg.size());
    return send_impl(buf, opcode::text);
}

std::expected<void, Error> Client::close(std::span<const uint8_t> payload) {
    return send_control_frame(payload, opcode::close);
}

// this should be an internal function of send control
// and then the dispatching happens with each one seding its own opcode
// and payload
std::expected<void, Error> Client::send_ping(std::span<const uint8_t> payload) {
    return send_control_frame(payload, opcode::ping);
}

std::expected<void, Error> Client::send_pong(std::span<const uint8_t> payload) {
    return send_control_frame(payload, opcode::pong);
}

std::expected<ChunckView, Error> Client::read_chunck_impl(size_t first_read) {
    constexpr int chunk_size  = 8 * 1024;
    auto         &buffer      = current_read.data;
    auto          parsed_meta = detail::parse_meta({buffer.data(), first_read});
    if (!parsed_meta) {
        // ws::log::error("[READ ERROR] Failed to parse frame metadata: {}",
        //                parsed_meta.error());
        return std::unexpected(parsed_meta.error());
    }
    // 1. Log Parsed Metadata
    ws::log::debug("=== [FRAME HEADER RECEIVED] ===");
    ws::log::debug("  Opcode:    0x{:01X} ({})",
                   static_cast<uint8_t>(parsed_meta->op),
                   static_cast<int>(parsed_meta->op));
    ws::log::debug("  FIN Bit:   {}", parsed_meta->fin);
    ws::log::debug("  Masked:    {}", parsed_meta->is_masked);
    ws::log::debug("  Payload L: {} bytes", parsed_meta->len);
    ws::log::debug("  Header Size: {} bytes", parsed_meta->meta_size);
    if (parsed_meta->is_masked) {
        return std::unexpected(Error::UnmaskedServerPayload);
    }
    if (is_control(parsed_meta->op) && !parsed_meta->fin) {
        return std::unexpected(Error::NonFinControlFrame);
    }
    if (is_control(parsed_meta->op) && parsed_meta->len > 125) {
        return std::unexpected(Error::InvalidPayloadLength);
    }
    current_read.fin  = parsed_meta->fin;
    current_read.type = parsed_meta->op;
    buffer.erase(buffer.begin(), buffer.begin() + parsed_meta->meta_size);
    size_t total_read = first_read - parsed_meta->meta_size;
    while (total_read < parsed_meta->len) {
        std::array<uint8_t, chunk_size> tmp;
        auto read_size = m_socket.read({tmp.data(), tmp.size()});
        if (!read_size) {
            return std::unexpected(read_size.error());
        }
        buffer.insert(buffer.end(), tmp.begin(), tmp.begin() + *read_size);
        total_read += *read_size;
    }
    current_read.remaing_bytes = total_read;
    // 2. Log Received Data Payload
    std::string_view payload_view(reinterpret_cast<const char *>(buffer.data()),
                                  parsed_meta->len);

    ws::log::debug("=== [FRAME PAYLOAD READ] ===");
    if (parsed_meta->op == opcode::text) {
        ws::log::debug("  Text Payload ({}) bytes: \"{}\"", parsed_meta->len,
                       payload_view);
    } else {
        ws::log::debug("  Binary/Control Payload ({}) bytes", parsed_meta->len);
    }
    switch (parsed_meta->op) {
        case opcode::ping: {
            if (auto pong_res = send_pong({buffer.data(), parsed_meta->len});
                !pong_res) {
                return std::unexpected(pong_res.error());
            }
            return read_chunk();
        } break;
        case opcode::text:
            if (!simdutf::validate_utf8(reinterpret_cast<char *>(buffer.data()),
                                        parsed_meta->len)) {
                ws::log::error("[READ ERROR] UTF-8 validation failed on text "
                               "frame payload");
                return std::unexpected(Error::InvalidUTF8);
            }
            break;
        case opcode::close: {
            if (auto close_res = close({buffer.data(), parsed_meta->len});
                !close_res) {
                return std::unexpected(close_res.error());
            }
        } break;
        default:;
    }
    ChunckView res = {.payload = std::span{buffer.data(), parsed_meta->len},
                      .is_fin  = parsed_meta->fin,
                      .type    = parsed_meta->op};
    return res;
}

std::expected<ChunckView, Error> Client::read_chunk() {
    // i need to add a function to parse the header
    // detail::parse_meta();
    constexpr int chunk_size = 8 * 1024;
    auto         &buffer     = current_read.data;
    if (current_read.remaing_bytes > 0 &&
        current_read.remaing_bytes <= buffer.size()) {
        buffer.erase(buffer.begin(),
                     buffer.begin() + current_read.remaing_bytes);
        current_read.remaing_bytes = 0;
    }
    if (!buffer.empty()) {
        return read_chunck_impl(buffer.size());
    }
    buffer.resize(chunk_size);
    auto first_read = m_socket.read({buffer.data(), buffer.size()});
    if (!first_read) {
        return std::unexpected(first_read.error());
    }
    return read_chunck_impl(*first_read);
}

} // namespace ws
