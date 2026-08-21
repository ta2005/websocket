#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "common/concepts.hpp"
#include "common/details/advance_spans.hpp"
#include "common/details/chunk_formatter.hpp"
#include "common/details/dispatch.hpp"
#include "common/details/parse_meta.hpp"
#include "common/opcode.hpp"
#include "handshake.hpp"
#include "simdutf.h"
#include "task.hpp"
#include <cstdint>
#include <random>
#include <string_view>
#include <vector>

namespace ws {

inline constexpr int chunk_size = 8 * 1024;

struct ChunkView {
    std::span<uint8_t> payload;
    bool               is_fin;
    opcode             type;
};

struct Message {
    std::vector<uint8_t> payload;
    opcode               type;
};

enum class ConnectionState { Open, Closing, Closed };

// Pure validation, no I/O, no buffer mutation — shared by both branches
// of read_header. Note: does NOT erase anything from the buffer anymore;
// that's now deferred (see read_header/read_payload below).
inline std::expected<detail::PayloadMetaData, Error>
finalize(detail::PayloadMetaData meta) {
    if (meta.is_masked) {
        return std::unexpected(Error::UnmaskedServerPayload);
    }
    if (is_control(meta.op) && !meta.fin) {
        return std::unexpected(Error::NonFinControlFrame);
    }
    if (is_control(meta.op) && meta.len > 125) {
        return std::unexpected(Error::InvalidPayloadLength);
    }
    return meta;
}

template <typename Socket> class Client {
  private:
    Socket       m_socket;
    std::mt19937 m_rng;
    struct ReadSate {
        std::vector<uint8_t> data;
        size_t               remaing_bytes = 0; // was uninitialized before
    } current_read;

    ConnectionState m_state = ConnectionState::Open;

    Client(Socket s) : m_socket(std::move(s)), m_rng(std::random_device{}()) {};

    decltype(auto) send_impl(std::span<const uint8_t>, opcode);
    decltype(auto) send_control_frame(std::span<const uint8_t>, opcode);
    decltype(auto) read_header(size_t);
    decltype(auto) read_payload(detail::PayloadMetaData);
    decltype(auto) send_close(std::span<const uint8_t>);
    decltype(auto) read_chunk_impl(size_t);
    void           consume_leftover_buffer();

  public:
    decltype(auto) read_chunk();
    decltype(auto) send(std::span<const uint8_t>);
    decltype(auto) send(const std::string_view);
    decltype(auto) send_ping(std::span<const uint8_t>);
    decltype(auto) send_pong(std::span<const uint8_t>);
};

template <typename Socket>
decltype(auto) Client<Socket>::send(std::span<const uint8_t> msg) {
    return send_impl(msg, opcode::binary);
}

template <typename Socket>
decltype(auto) Client<Socket>::send(const std::string_view msg) {
    if (!simdutf::validate_utf8(msg.data(), msg.size())) {
        if constexpr (isAsyncSocket<Socket>) {
            return [](Error err) -> Task<std::expected<void, Error>> {
                co_return std::unexpected(err);
            }(Error::InvalidUTF8);
        } else {
            return std::unexpected(Error::InvalidUTF8);
        }
    }
    auto buf = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t *>(msg.data()), msg.size());
    return send_impl(buf, opcode::text);
}

template <typename Socket>
decltype(auto) Client<Socket>::send_impl(std::span<const uint8_t> msg,
                                         opcode                   first_op) {
    return detail::dispatch<Socket>(
        [this, msg, first_op]() -> Task<std::expected<void, Error>> {
            if (m_state != ConnectionState::Open) {
                co_return std::unexpected(Error::InvalidState);
            }
            detail::ChunkFormatter fmt{msg, first_op, m_rng};
            while (fmt.has_next()) {
                auto [meta, payload] = fmt.next();
                auto metas           = meta.span();
                while (!(metas).empty() || !payload.empty()) {
                    auto n = co_await m_socket.write(metas, payload);
                    if (!n) {
                        co_return std::unexpected(n.error());
                    }
                    detail::advance_span(metas, payload, *n);
                }
            }
            co_return std::expected<void, Error>{};
        },
        [this, msg, first_op]() -> std::expected<void, Error> {
            if (m_state != ConnectionState::Open) {
                return std::unexpected(Error::InvalidState);
            }
            detail::ChunkFormatter fmt{msg, first_op, m_rng};
            while (fmt.has_next()) {
                auto [meta, payload] = fmt.next();
                auto metas           = meta.span();
                while (!(metas).empty() || !payload.empty()) {
                    auto n = m_socket.write(metas, payload);
                    if (!n) {
                        return std::unexpected(n.error());
                    }
                    detail::advance_span(metas, payload, *n);
                }
            }
            return {};
        });
}

template <typename Socket>
decltype(auto)
Client<Socket>::send_control_frame(std::span<const uint8_t> payload,
                                   opcode                   op) {
    if (payload.size() > 125) {
        if constexpr (isAsyncSocket<Socket>) {
            return [](Error err) -> Task<std::expected<void, Error>> {
                co_return std::unexpected(err);
            }(Error::InvalidPayloadLength);
        } else {
            return std::unexpected(Error::InvalidPayloadLength);
        }
    }
    return send_impl(payload, op);
}

template <typename Socket>
decltype(auto) Client<Socket>::send_pong(std::span<const uint8_t> payload) {
    return send_control_frame(payload, opcode::pong);
}

template <typename Socket>
decltype(auto) Client<Socket>::send_ping(std::span<const uint8_t> payload) {
    return send_control_frame(payload, opcode::ping);
}

template <typename Socket>
decltype(auto) Client<Socket>::send_close(std::span<const uint8_t> payload) {
    return send_control_frame(payload, opcode::close);
}

template <typename Socket>
decltype(auto) Client<Socket>::read_header(size_t first_read) {
    return detail::dispatch<Socket>(
        // --- ASYNC PATH ---
        [this,
         first_read]() -> Task<std::expected<detail::PayloadMetaData, Error>> {
            auto &buffer      = current_read.data;
            auto  parsed_meta = detail::parse_meta({buffer.data(), first_read});

            while (!parsed_meta) {
                if (parsed_meta.error() != Error::PayloadTooShort) {
                    co_return std::unexpected(parsed_meta.error());
                }
                std::array<uint8_t, chunk_size> tmp;
                auto                            read_size =
                    co_await m_socket.read_some({tmp.data(), tmp.size()});
                if (!read_size) {
                    co_return std::unexpected(read_size.error());
                }
                buffer.insert(buffer.end(), tmp.begin(),
                              tmp.begin() + *read_size);
                parsed_meta =
                    detail::parse_meta({buffer.data(), buffer.size()});
            }
            // Erase deferred: we no longer trim the header here. Instead we
            // stash its size so read_payload can add its own length on top,
            // and the *next* read_chunk() erases header+payload together
            // in one shot, right before it needs a clean buffer to parse
            // the following frame's header from offset 0.
            current_read.remaing_bytes = parsed_meta->meta_size;
            co_return finalize(*parsed_meta);
        },

        // --- SYNC PATH ---
        [this, first_read]() -> std::expected<detail::PayloadMetaData, Error> {
            auto &buffer      = current_read.data;
            auto  parsed_meta = detail::parse_meta({buffer.data(), first_read});

            while (!parsed_meta) {
                if (parsed_meta.error() != Error::PayloadTooShort) {
                    return std::unexpected(parsed_meta.error());
                }
                std::array<uint8_t, chunk_size> tmp;
                auto read_size = m_socket.read_some({tmp.data(), tmp.size()});
                if (!read_size) {
                    return std::unexpected(read_size.error());
                }
                buffer.insert(buffer.end(), tmp.begin(),
                              tmp.begin() + *read_size);
                parsed_meta =
                    detail::parse_meta({buffer.data(), buffer.size()});
            }
            current_read.remaing_bytes = parsed_meta->meta_size;
            return finalize(*parsed_meta);
        });
}

template <typename Socket>
decltype(auto) Client<Socket>::read_payload(detail::PayloadMetaData meta) {
    return detail::dispatch<Socket>(
        // --- ASYNC PATH ---
        [this, meta]() -> Task<std::expected<void, Error>> {
            auto &buffer = current_read.data;
            // Buffer still holds the un-erased header (meta.meta_size bytes)
            // up front, plus whatever payload bytes already arrived as
            // leftover from the initial bulk read. Subtract the header off
            // to get how much *payload* is already sitting in the buffer.
            size_t total_read = buffer.size() - meta.meta_size;

            while (total_read < meta.len) {
                std::array<uint8_t, chunk_size> tmp;
                auto                            read_size =
                    co_await m_socket.read_some({tmp.data(), tmp.size()});
                if (!read_size) {
                    co_return std::unexpected(read_size.error());
                }
                buffer.insert(buffer.end(), tmp.begin(),
                              tmp.begin() + *read_size);
                total_read += *read_size;
            }
            // remaing_bytes now covers header + full payload for this
            // frame — the single deferred erase, next read_chunk() call.
            current_read.remaing_bytes += meta.len;
            co_return std::expected<void, Error>{};
        },

        // --- SYNC PATH ---
        [this, meta]() -> std::expected<void, Error> {
            auto  &buffer     = current_read.data;
            size_t total_read = buffer.size() - meta.meta_size;

            while (total_read < meta.len) {
                std::array<uint8_t, chunk_size> tmp;
                auto read_size = m_socket.read_some({tmp.data(), tmp.size()});
                if (!read_size) {
                    return std::unexpected(read_size.error());
                }
                buffer.insert(buffer.end(), tmp.begin(),
                              tmp.begin() + *read_size);
                total_read += *read_size;
            }
            current_read.remaing_bytes += meta.len;
            return {};
        });
}

template <typename T> void Client<T>::consume_leftover_buffer() {
    auto &buffer = current_read.data;
    if (current_read.remaing_bytes > 0 &&
        current_read.remaing_bytes <= buffer.size()) {
        buffer.erase(buffer.begin(),
                     buffer.begin() + current_read.remaing_bytes);
        current_read.remaing_bytes = 0;
    }
}

template <typename Socket>
decltype(auto) Client<Socket>::read_chunk_impl(size_t first_read) {
    return detail::dispatch<Socket>(
        // --- ASYNC PATH ---
        [this, first_read]() -> Task<std::expected<ChunkView, Error>> {
            auto &buffer          = current_read.data;
            auto  parsed_meta_res = co_await read_header(first_read);

            if (!parsed_meta_res) {
                // fail_connection(get_status_code(parsed_meta_res.error()));
                co_return std::unexpected(parsed_meta_res.error());
            }

            auto payload_res = co_await read_payload(*parsed_meta_res);
            if (!payload_res) {
                co_return std::unexpected(payload_res.error());
            }

            if (m_state == ConnectionState::Closing &&
                !is_control(parsed_meta_res->op)) {
                co_return co_await read_chunk();
            }

            switch (parsed_meta_res->op) {
                case opcode::ping: {
                    auto pong_res = co_await send_pong(
                        {buffer.data(), parsed_meta_res->len});
                    if (!pong_res) {
                        co_return std::unexpected(pong_res.error());
                    }
                    co_return co_await read_chunk();
                } break;
                case opcode::pong: {
                    co_return co_await read_chunk();
                } break;
                case opcode::close: {
                    if (m_state == ConnectionState::Open) {
                        auto close_res = co_await send_close(
                            {buffer.data(), parsed_meta_res->len});
                        if (!close_res) {
                            co_return std::unexpected(close_res.error());
                        }
                    }
                    m_state = ConnectionState::Closed;
                    // m_socket.close();
                } break;
                default:;
            }
            ChunkView res = {.payload =
                                 std::span{buffer.data(), parsed_meta_res->len},
                             .is_fin = parsed_meta_res->fin,
                             .type   = parsed_meta_res->op};
            co_return res;
        },

        // --- SYNC PATH ---
        [this, first_read]() -> std::expected<ChunkView, Error> {
            auto &buffer          = current_read.data;
            auto  parsed_meta_res = read_header(first_read);

            if (!parsed_meta_res) {
                // fail_connection(get_status_code(parsed_meta_res.error()));
                return std::unexpected(parsed_meta_res.error());
            }

            auto payload_res = read_payload(*parsed_meta_res);
            if (!payload_res) {
                return std::unexpected(payload_res.error());
            }

            if (m_state == ConnectionState::Closing &&
                !is_control(parsed_meta_res->op)) {
                return read_chunk();
            }

            switch (parsed_meta_res->op) {
                case opcode::ping: {
                    auto pong_res =
                        send_pong({buffer.data(), parsed_meta_res->len});
                    if (!pong_res) {
                        return std::unexpected(pong_res.error());
                    }
                    return read_chunk();
                } break;
                case opcode::pong: {
                    return read_chunk();
                } break;
                case opcode::close: {
                    if (m_state == ConnectionState::Open) {
                        auto close_res =
                            send_close({buffer.data(), parsed_meta_res->len});
                        if (!close_res) {
                            return std::unexpected(close_res.error());
                        }
                    }
                    m_state = ConnectionState::Closed;
                    // m_socket.close();
                } break;
                default:;
            }
            ChunkView res = {.payload =
                                 std::span{buffer.data(), parsed_meta_res->len},
                             .is_fin = parsed_meta_res->fin,
                             .type   = parsed_meta_res->op};
            return res;
        });
}

template <typename Socket> decltype(auto) Client<Socket>::read_chunk() {
    return detail::dispatch<Socket>(
        [this]() -> Task<std::expected<ChunkView, Error>> {
            auto &buffer = current_read.data;
            consume_leftover_buffer();
            if (!buffer.empty()) {
                co_return co_await read_chunk_impl(buffer.size());
            }
            buffer.resize(chunk_size);
            auto first_read =
                co_await m_socket.read_some({buffer.data(), buffer.size()});
            if (!first_read) {
                co_return std::unexpected(first_read.error());
            }
            buffer.resize(*first_read);
            co_return co_await read_chunk_impl(*first_read);
        },
        [this]() -> std::expected<ChunkView, Error> {
            auto &buffer = current_read.data;
            consume_leftover_buffer();
            if (!buffer.empty()) {
                return read_chunk_impl(buffer.size());
            }
            buffer.resize(chunk_size);
            auto first_read =
                m_socket.read_some({buffer.data(), buffer.size()});
            if (!first_read) {
                return std::unexpected(first_read.error());
            }
            buffer.resize(*first_read);
            return read_chunk_impl(*first_read);
        });
}

} // namespace ws

#endif
