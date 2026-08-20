#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "common/concepts.hpp"
#include "common/details/advance_span.hpp"
#include "common/details/chunk_formatter.hpp"
#include "common/details/parse_meta.hpp"
#include "common/opcode.hpp"
#include "simdutf.h"
#include "task.hpp"
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

template <typename AsyncFn, typename SyncFn>
static decltype(auto) dispatch(AsyncFn &&async_fn, SyncFn &&sync_fn) {
    if constexpr (isAsyncSocket<Socket>) {
        return async_fn();
    } else {
        return sync_fn();
    }
}

enum class ConnectionState { Open, Closing, Closed };
template <typename Socket> class Client {
  private:
    Socket       m_socket;
    std::mt19937 m_rng;
    struct ReadSate {
        std::vector<uint8_t> data;
        size_t               remaing_bytes;
    } current_read;

    ConnectionState m_state = ConnectionState::Open;

    Client(Socket s) : m_socket(std::move(s)), m_rng(std::random_device{}()) {};
    decltype(auto) send_impl(std::span<const uint8_t>, opcode);
    //
    // std::expected<void, Error>      send_control_frame(std::span<const
    // uint8_t>,
    //                                                    opcode);
    // std::expected<ChunkView, Error> read_chunk_impl(size_t);
    // std::expected<detail::PayloadMetaData, Error> read_header(size_t);
    // std::expected<detail::PayloadMetaData, Error>
    //                            read_payload(detail::PayloadMetaData);
    // std::expected<void, Error> send_close(std::span<const uint8_t>);
    // void                       fail_connection(status_code reason);

  public:
    decltype(auto) send(std::span<const uint8_t>);
    decltype(auto) send(const std::string_view);
    // std::expected<void, Error> send_ping(std::span<const uint8_t>);
    // std::expected<void, Error> send_pong(std::span<const uint8_t>);
    // // next major update this will need to be and enum with an explanation
    // std::expected<void, Error>      close(std::span<const uint8_t>);
    // std::expected<ChunkView, Error> read_chunk();
    // std::expected<Message, Error>   read_message(
    //     size_t max_size = 100 * 1024 * 1024); // 100MB limit to prevent OOM
    // this one should be uri
    // static std::expected<Client, Error>
    // create(const std::string_view host, const std::string_view path,
    //        const std::string_view port = "80");
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
    dispatch(
        [this, msg, first_op]() -> Task<std::expected<void, Error>> {
            if (m_state != ConnectionState::open) {
                co_return std::unexpected(Error::InvalidState);
            }
            detail::ChunkFormatter fmt{msg, first_op, c.m_rng};
            while (fmt.has_next()) {
                auto [meta, payload] = fmt.next();

                while (!meta.empty() || !payload.empty()) {
                    auto n = co_await m_socket.write(meta, payload);
                    if (!n) {
                        co_return std::unexpected(n.error());
                    }
                    detail::advance_span(meta, payload, *n);
                }
            }
            co_return {};
        },
        [this, msg, first_op]() -> std::expected<void, Error> {
            if (m_state != ConnectionState::open) {
                return std::unexpected(Error::InvalidState);
            }
            detail::ChunkFormatter fmt{msg, first_op, m_rng};
            while (fmt.has_next()) {
                auto [meta, payload] = fmt.next();

                while (!meta.empty() || !payload.empty()) {
                    auto n = m_socket.write(meta, payload);
                    if (!n) {
                        return std::unexpected(n.error());
                    }
                    detail::advance_span(meta, payload, *n);
                }
            }
            return {};
        })
}
} // namespace ws

#endif
