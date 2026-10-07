#ifndef SESSION_HPP
#define SESSION_HPP

#include "common/concepts.hpp"
#include "common/details/advance_spans.hpp"
#include "common/details/chunk_formatter.hpp"
#include "common/details/dispatch.hpp"
#include "common/details/parse_meta.hpp"
#include "common/opcode.hpp"
#include "common/roles.hpp"
#include "common/status_code.hpp"
#include "handshake.hpp"
#include "simdutf.h"
#include "task.hpp"
#include <cstdint>
#include <iostream>
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

template <typename Socket, Role R>
// requires (IsSocket<Socket>)
class Session {

    template <typename T>
    using Result =
        std::conditional_t<isAsyncSocket<Socket>, Task<std::expected<T, Error>>,
                           std::expected<T, Error>>;

  protected:
    Socket m_socket;
    // ok this varibale probably does not belong here
    // well for the blocking part I will create a ctx global variable
    // in the other i will simply create a Session data linked to the loop
    // since the creation of random device aka reading from (/dev/urandom)
    // is expensive
    std::mt19937 m_rng;
    // this will be better as some sort
    // of specialized data strucutre
    // some sort of ring buffer
    // and for me to see how to better implement the reads
    struct ReadSate {
        std::vector<uint8_t> data;
        size_t               remaing_bytes = 0; // was uninitialized before
    } current_read;

    ConnectionState m_state = ConnectionState::Open;

    // I will see what to do with this later
    // maybe I will have some sort of transparancy
    // where socket
    // takes a context
    // and the context is owned by another part of this system
    Session(Socket s)
        : m_socket(std::move(s)), m_rng(std::random_device{}()) {};

    // these will be simplified
    // also i have now just reaalized the
    // names of these function are simply shit
    // what is read_chunk?? what is chunck?
    // the rfc has payload payload length
    // and frame
    Result<ChunkView> read_chunk_impl(size_t);
    decltype(auto)    send_impl(std::span<const uint8_t>, opcode);
    decltype(auto)    send_control_frame(std::span<const uint8_t>, opcode);
    decltype(auto)    read_header(size_t);
    decltype(auto)    read_payload(detail::PayloadMetaData);
    decltype(auto)    send_close(std::span<const uint8_t>);
    void              consume_leftover_buffer();
    decltype(auto)    fail_connection(status_code reason);
    decltype(auto)    send_ping(std::span<const uint8_t>);
    decltype(auto)    send_pong(std::span<const uint8_t>);
    // I will not now use this method
    Result<ChunkView> read_chunk();

  public:
    decltype(auto) send(std::span<const uint8_t>);
    decltype(auto) send(const std::string_view);
    decltype(auto) read_message();
    decltype(auto) close();

    // this create will be connect for the client
    // and  for the server It will simply be
    // a listen function
    // static decltype(auto) create(Socket s, const std::string_view host,
    //                              const std::string_view path,
    //                              const std::string_view port = "80");
};

template <typename Socket, Role R>
decltype(auto) Session<Socket, R>::read_message() {
    return detail::dispatch<Socket>(
        [this]<typename S = Socket>() -> Task<std::expected<ChunkView, Error>> {
            while (m_state != ConnectionState::Closed) {
                auto chunk_res =
                    co_await static_cast<Session<S, R> *>(this)->read_chunk();
                if (!chunk_res)
                    co_return std::unexpected(chunk_res.error());

                if (is_control(chunk_res->type)) {
                    if (chunk_res->type == opcode::ping) {
                        auto pong_res = co_await send_pong(chunk_res->payload);
                        if (!pong_res)
                            co_return std::unexpected(pong_res.error());
                    } else if (chunk_res->type == opcode::close) {
                        if (m_state == ConnectionState::Open) {
                            auto close_res =
                                co_await send_close(chunk_res->payload);
                            if (!close_res)
                                co_return std::unexpected(close_res.error());
                        }
                        m_socket.close();
                        m_state = ConnectionState::Closed;
                    }
                    continue;
                }

                if (m_state == ConnectionState::Closing)
                    continue;

                co_return *chunk_res;
            }
            co_return std::unexpected(Error::ConnectionClosed);
        },
        [this]<typename S = Socket>() -> std::expected<ChunkView, Error> {
            while (m_state != ConnectionState::Closed) {
                auto chunk_res = read_chunk();
                if (!chunk_res)
                    return std::unexpected(chunk_res.error());

                if (is_control(chunk_res->type)) {
                    if (chunk_res->type == opcode::ping) {
                        auto pong_res = send_pong(chunk_res->payload);
                        if (!pong_res)
                            return std::unexpected(pong_res.error());
                    } else if (chunk_res->type == opcode::close) {
                        if (m_state == ConnectionState::Open) {
                            auto close_res = send_close(chunk_res->payload);
                            if (!close_res)
                                return std::unexpected(close_res.error());
                        }
                        m_socket.close();
                        m_state = ConnectionState::Closed;
                    }
                    continue;
                }

                if (m_state == ConnectionState::Closing)
                    continue;

                return *chunk_res;
            }
            return std::unexpected(Error::ConnectionClosed);
        });
}

// some of these will be removed later
// of maybe very much updated
template <typename Socket, Role R>
decltype(auto) Session<Socket, R>::send(std::span<const uint8_t> msg) {
    return send_impl(msg, opcode::binary);
}

template <typename Socket, Role R>
decltype(auto) Session<Socket, R>::send(const std::string_view msg) {
    if (!simdutf::validate_utf8(msg.data(), msg.size())) {
        if constexpr (isAsyncSocket<Socket>) {
            return [](Error err) -> Task<std::expected<void, Error>> {
                co_return std::unexpected(err);
            }(Error::InvalidUTF8);
        } else {
            return std::expected<void, Error>(
                std::unexpected(Error::InvalidUTF8));
        }
    }
    auto buf = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t *>(msg.data()), msg.size());
    return send_impl(buf, opcode::text);
}

template <typename Socket, Role R>
decltype(auto) Session<Socket, R>::send_impl(std::span<const uint8_t> msg,
                                             opcode first_op) {
    return detail::dispatch<Socket>(
        [this, msg,
         first_op]<typename S = Socket>() -> Task<std::expected<void, Error>> {
            if (m_state != ConnectionState::Open) {
                co_return std::unexpected(Error::InvalidState);
            }
            std::cout << "[Session] Formatting message (opcode: "
                      << static_cast<int>(first_op) << ", size: " << msg.size()
                      << ")\n";
            detail::ChunkFormatter<R> fmt{msg, first_op, &m_rng};
            while (fmt.has_next()) {
                auto [meta, payload] = fmt.next();
                auto metas           = meta.span();
                std::cout << "[Session] Sending frame chunk (header: "
                          << metas.size() << metas.data()
                          << " bytes, payload: " << payload.size()
                          << " bytes)\n";
                while (!(metas).empty() || !payload.empty()) {
                    auto n = co_await static_cast<S &>(m_socket).write(metas,
                                                                       payload);
                    if (!n) {
                        std::cerr << "[Session] Error writing to socket: "
                                  << static_cast<int>(n.error()) << "\n";
                        co_return std::unexpected(n.error());
                    }
                    std::cout << "[Session] Wrote " << *n
                              << " bytes to socket\n";
                    detail::advance_span(metas, payload, *n);
                }
            }
            co_return std::expected<void, Error>{};
        },
        [this, msg,
         first_op]<typename S = Socket>() -> std::expected<void, Error> {
            if (m_state != ConnectionState::Open) {
                return std::unexpected(Error::InvalidState);
            }
            std::cout << "[Session] [Sync] Formatting message (opcode: "
                      << static_cast<int>(first_op) << ", size: " << msg.size()
                      << ")\n";
            detail::ChunkFormatter<R> fmt{msg, first_op, &m_rng};
            while (fmt.has_next()) {
                auto [meta, payload] = fmt.next();
                auto metas           = meta.span();
                std::cout << "[Session] [Sync] Sending frame chunk (header: "
                          << metas.size()
                          << " bytes, payload: " << payload.size()
                          << " bytes)\n";
                while (!(metas).empty() || !payload.empty()) {
                    auto n = m_socket.write(metas, payload);
                    if (!n) {
                        std::cerr
                            << "[Session] [Sync] Error writing to socket: "
                            << static_cast<int>(n.error()) << "\n";
                        return std::unexpected(n.error());
                    }
                    std::cout << "[Session] [Sync] Wrote " << *n
                              << " bytes to socket\n";
                    detail::advance_span(metas, payload, *n);
                }
            }
            return {};
        });
}

template <typename Socket, Role R>
decltype(auto) Session<Socket, R>::fail_connection(status_code reason) {
    auto st = htons(static_cast<uint16_t>(reason));
    if (auto send_res = send_close({reinterpret_cast<const uint8_t *>(&st), 2});
        !send_res) {
    }
    m_socket.close();
    m_state = ConnectionState::Closed;
}

template <typename Socket, Role R>
decltype(auto)
Session<Socket, R>::send_control_frame(std::span<const uint8_t> payload,
                                       opcode                   op) {
    if (payload.size() > 125) {
        if constexpr (isAsyncSocket<Socket>) {
            return [](Error err) -> Task<std::expected<void, Error>> {
                co_return std::unexpected(err);
            }(Error::InvalidPayloadLength);
        } else {
            return std::expected<void, Error>(
                std::unexpected(Error::InvalidUTF8));
        }
    }
    return send_impl(payload, op);
}

template <typename Socket, Role R>
decltype(auto) Session<Socket, R>::send_pong(std::span<const uint8_t> payload) {
    return send_control_frame(payload, opcode::pong);
}

template <typename Socket, Role R>
decltype(auto) Session<Socket, R>::send_ping(std::span<const uint8_t> payload) {
    return send_control_frame(payload, opcode::ping);
}

// template <typename Socket,Role R>
// decltype(auto) Session<Socket,R>::close(){
//     if (m_state != ConnectionState::Open){
//     }
//     m_state = ConnectionState::Closing;
//     send_close(std::span<const uint8_t>(nullptr,0));
//     m_socket.close();
//     // return {};
// }

template <typename Socket, Role R>
decltype(auto)
Session<Socket, R>::send_close(std::span<const uint8_t> payload) {
    return send_control_frame(payload, opcode::close);
}

template <typename Socket, Role R>
decltype(auto) Session<Socket, R>::read_header(size_t first_read) {
    return detail::dispatch<Socket>(
        // --- ASYNC PATH ---
        [this, first_read]<typename S = Socket>()
            -> Task<std::expected<detail::PayloadMetaData, Error>> {
            auto &buffer      = current_read.data;
            auto  parsed_meta = detail::parse_meta({buffer.data(), first_read});

            while (!parsed_meta) {
                if (parsed_meta.error() != Error::PayloadTooShort) {
                    co_return std::unexpected(parsed_meta.error());
                }
                std::array<uint8_t, chunk_size> tmp;
                // yes this one will no longer be like this after
                // I add corking but it is currently fine
                auto read_size = co_await static_cast<S &>(m_socket).read(
                    {tmp.data(), tmp.size()});
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
        [this, first_read]<typename S = Socket>()
            -> std::expected<detail::PayloadMetaData, Error> {
            auto &buffer      = current_read.data;
            auto  parsed_meta = detail::parse_meta({buffer.data(), first_read});

            while (!parsed_meta) {
                if (parsed_meta.error() != Error::PayloadTooShort) {
                    return std::unexpected(parsed_meta.error());
                }
                std::array<uint8_t, chunk_size> tmp;
                auto read_size = m_socket.read({tmp.data(), tmp.size()});
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

template <typename Socket, Role R>
decltype(auto) Session<Socket, R>::read_payload(detail::PayloadMetaData meta) {
    return detail::dispatch<Socket>(
        // --- ASYNC PATH ---
        [this,
         meta]<typename S = Socket>() -> Task<std::expected<void, Error>> {
            auto &buffer = current_read.data;
            // Buffer still holds the un-erased header (meta.meta_size bytes)
            // up front, plus whatever payload bytes already arrived as
            // leftover from the initial bulk read. Subtract the header off
            // to get how much *payload* is already sitting in the buffer.
            size_t total_read = buffer.size() - meta.meta_size;

            while (total_read < meta.len) {
                std::array<uint8_t, chunk_size> tmp;
                auto read_size = co_await static_cast<S &>(m_socket).read(
                    {tmp.data(), tmp.size()});
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
        [this, meta]<typename S = Socket>() -> std::expected<void, Error> {
            auto  &buffer     = current_read.data;
            size_t total_read = buffer.size() - meta.meta_size;

            while (total_read < meta.len) {
                std::array<uint8_t, chunk_size> tmp;
                auto read_size = m_socket.read({tmp.data(), tmp.size()});
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

template <typename Socket, Role R>
void Session<Socket, R>::consume_leftover_buffer() {
    auto &buffer = current_read.data;
    if (current_read.remaing_bytes > 0 &&
        current_read.remaing_bytes <= buffer.size()) {
        buffer.erase(buffer.begin(),
                     buffer.begin() + current_read.remaing_bytes);
        current_read.remaing_bytes = 0;
    }
}

template <typename Socket, Role R>
typename Session<Socket, R>::template Result<ChunkView>
Session<Socket, R>::read_chunk_impl(size_t first_read) {
    return detail::dispatch<Socket>(
        // --- ASYNC PATH ---
        [this, first_read]<typename S = Socket>()
            -> Task<std::expected<ChunkView, Error>> {
            auto &buffer = current_read.data;
            std::cout << "[Session] Reading frame header...\n";
            auto parsed_meta_res =
                co_await static_cast<Session<S, R> *>(this)->read_header(
                    first_read);

            if (!parsed_meta_res) {
                std::cerr << "[Session] Failed to read header: "
                          << static_cast<int>(parsed_meta_res.error()) << "\n";
                // fail_connection(get_status_code(parsed_meta_res.error()));
                co_return std::unexpected(parsed_meta_res.error());
            }
            std::cout << "[Session] Parsed header (opcode: "
                      << static_cast<int>(parsed_meta_res->op)
                      << ", fin: " << parsed_meta_res->fin
                      << ", payload_len: " << parsed_meta_res->len << ")\n";

            auto payload_res =
                co_await static_cast<Session<S, R> *>(this)->read_payload(
                    *parsed_meta_res);
            if (!payload_res) {
                std::cerr << "[Session] Failed to read payload: "
                          << static_cast<int>(payload_res.error()) << "\n";
                co_return std::unexpected(payload_res.error());
            }
            std::cout << "[Session] Successfully read full payload.\n";

            co_return ChunkView{
                .payload = std::span{buffer.data() + parsed_meta_res->meta_size,
                                     parsed_meta_res->len},
                .is_fin  = parsed_meta_res->fin,
                .type    = parsed_meta_res->op};
        },

        // --- SYNC PATH ---
        [this,
         first_read]<typename S = Socket>() -> std::expected<ChunkView, Error> {
            auto &buffer = current_read.data;
            std::cout << "[Session] [Sync] Reading frame header...\n";
            auto parsed_meta_res = read_header(first_read);

            if (!parsed_meta_res) {
                std::cerr << "[Session] [Sync] Failed to read header: "
                          << static_cast<int>(parsed_meta_res.error()) << "\n";
                // fail_connection(get_status_code(parsed_meta_res.error()));
                return std::unexpected(parsed_meta_res.error());
            }
            std::cout << "[Session] [Sync] Parsed header (opcode: "
                      << static_cast<int>(parsed_meta_res->op)
                      << ", fin: " << parsed_meta_res->fin
                      << ", payload_len: " << parsed_meta_res->len << ")\n";

            auto payload_res = read_payload(*parsed_meta_res);
            if (!payload_res) {
                std::cerr << "[Session] [Sync] Failed to read payload: "
                          << static_cast<int>(payload_res.error()) << "\n";
                return std::unexpected(payload_res.error());
            }
            std::cout << "[Session] [Sync] Successfully read full payload.\n";

            return ChunkView{
                .payload = std::span{buffer.data() + parsed_meta_res->meta_size,
                                     parsed_meta_res->len},
                .is_fin  = parsed_meta_res->fin,
                .type    = parsed_meta_res->op};
        });
}

template <typename Socket, Role R>
typename Session<Socket, R>::template Result<ChunkView>
Session<Socket, R>::read_chunk() {
    return detail::dispatch<Socket>(
        [this]<typename S = Socket>() -> Task<std::expected<ChunkView, Error>> {
            auto &buffer = current_read.data;
            consume_leftover_buffer();
            if (!buffer.empty()) {
                co_return co_await static_cast<Session<S, R> *>(this)
                    ->read_chunk_impl(buffer.size());
            }
            buffer.resize(chunk_size);
            auto first_read = co_await static_cast<S &>(m_socket).read(
                {buffer.data(), buffer.size()});
            // this is either my best or worst idea
            if (!first_read) {
                co_return std::unexpected(first_read.error());
            }
            buffer.resize(*first_read);
            co_return co_await read_chunk_impl(*first_read);
        },
        [this]<typename S = Socket>() -> std::expected<ChunkView, Error> {
            auto &buffer = current_read.data;
            consume_leftover_buffer();
            if (!buffer.empty()) {
                return read_chunk_impl(buffer.size());
            }
            buffer.resize(chunk_size);
            auto first_read = m_socket.read({buffer.data(), buffer.size()});
            if (!first_read) {
                return std::unexpected(first_read.error());
            }
            buffer.resize(*first_read);
            return read_chunk_impl(*first_read);
        });
}

} // namespace ws

#endif
