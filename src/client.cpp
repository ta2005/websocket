#include "client.hpp"
#include "details/mask_payload.hpp"
#include "details/parse_meta.hpp"
#include "details/prepare_meta.hpp"
#include "handshake.hpp"
#include "logger.hpp"
#include "simdutf.h"
#include "simdutf.cpp"

namespace ws {

std::expected<Client, std::string_view>
Client::create(const std::string_view host, const std::string_view path,
               const std::string_view port) {
    auto s = TcpSocket::connect(host, port);
    if (!s) {
        return std::unexpected(s.error());
    }
    auto hk = perform_handshake(*s, host, path);
    if (!hk) {
        ws::log::error("Handshake failed: {}", hk.error());
        return std::unexpected(hk.error());
    }
    ws::log::info("Client created and handshake complete");
    return Client(std::move(*s));
}

std::expected<void, std::string_view>
Client::send_impl(std::span<const uint8_t> msg, opcode first_op) {
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
        auto l = m_socket.send(meta.span(), tmp);
        if (!l) {
            ws::log::error("Failed to send chunk: {}", l.error());
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
    auto l = m_socket.send(meta.span(), {tmp.data(), msg.size()});
    if (!l) {
        ws::log::error("Failed to send final chunk: {}", l.error());
        return std::unexpected(l.error());
    }
    return {};
}

std::expected<void, std::string_view>
Client::send(std::span<const uint8_t> msg) {
    return send_impl(msg, opcode::binary);
}

std::expected<void, std::string_view> Client::send(const std::string_view msg) {
    if (!simdutf::validate_utf8(msg.data(), msg.size())) {
        return std::unexpected("invalid uft8");
    }
    auto buf = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t *>(msg.data()), msg.size());
    return send_impl(buf, opcode::text);
}

std::expected<void, std::string_view> Client::close() const {
    auto meta = detail::format_meta(true, true, opcode::close, 0, 0);
    auto sz   = m_socket.send(meta.span());
    if (!sz) {
        return std::unexpected(sz.error());
    }
    return {};
}

// this should be an internal function of send control
// and then the dispatching happens with each one seding its own opcode
// and payload
std::expected<void, std::string_view>
Client::send_ping(std::span<const uint8_t> payload) {
    if (payload.size() > 125) {
        return std::unexpected(
            "control frames must have a size of 125 or less");
    }
    auto meta =
        detail::format_meta(true, true, opcode::ping, payload.size(), 0);
    auto sz = m_socket.send(meta.span(), payload);
    if (!sz) {
        return std::unexpected(sz.error());
    }
    return {};
    // detail::format_meta
}

std::expected<void, std::string_view>
Client::send_pong(std::span<const uint8_t> payload) {
    if (payload.size() > 125) {
        return std::unexpected(
            "control frames must have a size of 125 or less");
    }
    auto meta =
        detail::format_meta(true, false, opcode::pong, payload.size(), 0);
    auto sz = m_socket.send(meta.span(), payload);
    if (!sz) {
        return std::unexpected(sz.error());
    }
    return {};
    // detail::format_meta
}

std::expected<ChunckView, std::string_view>
Client::read_chunck_impl(size_t first_read) {
    constexpr int chunk_size  = 8 * 1024;
    auto         &buffer      = current_read.data;
    auto          parsed_meta = detail::parse_meta({buffer.data(), first_read});
    if (!parsed_meta) {
        return std::unexpected(parsed_meta.error());
    }
    if (parsed_meta->is_masked) {
        return std::unexpected("payload from the server should not be masked");
    }
    if (is_control(parsed_meta->op) && !parsed_meta->fin) {
        return std::unexpected("control frames must be be fin");
    }
    if (is_control(parsed_meta->op) && parsed_meta->len > 125) {
        return std::unexpected(
            "control frames must have a size of 125 or less");
    }
    //    if(is_control(parsed_meta->op) && parsed_meta->is_masked){
    // return std::unexpected("Control frames can't be masked");
    //    }
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
    switch (parsed_meta->op) {
        case opcode::ping: {
            auto pong_res = send_pong({buffer.data(), parsed_meta->len});
            if (!pong_res) {
                return std::unexpected("unable to respond to ping");
            }
            return read_chunk();
        } break;
        case opcode::text:
            if (!simdutf::validate_utf8(reinterpret_cast<char *>(buffer.data()), parsed_meta->len)) {
                return std::unexpected("invalid uft8");
            }
            // TODO: add utf8 valid
            break;
        default:;
    }
    ChunckView res = {.payload = std::span{buffer.data(), parsed_meta->len},
                      .is_fin  = parsed_meta->fin,
                      .type    = parsed_meta->op};
    return res;
}

std::expected<ChunckView, std::string_view> Client::read_chunk() {
    // i need to add a function to parse the header
    // detail::parse_meta();
    constexpr int chunk_size = 8 * 1024;
    auto         &buffer     = current_read.data;
    buffer.erase(buffer.begin(), buffer.begin() + current_read.remaing_bytes);
    if (buffer.size() != 0) {
        return read_chunck_impl(buffer.size());
    } else {
        buffer.resize(chunk_size);
        auto first_read = m_socket.read({buffer.data(), buffer.size()});
        if (!first_read) {
            return std::unexpected(first_read.error());
        }
        return read_chunck_impl(*first_read);
    }
}

} // namespace ws
