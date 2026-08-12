#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "details/parse_meta.hpp"
#include "opcode.hpp"
#include "tcp_socket.hpp"
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
class Client {
  private:
    TcpSocket    m_socket;
    std::mt19937 m_rng;
    struct ReadSate {
        std::vector<uint8_t> data;
        size_t               remaing_bytes;
    } current_read;

    ConnectionState m_state = ConnectionState::Open;

    Client(TcpSocket s)
        : m_socket(std::move(s)), m_rng(std::random_device{}()) {};
    std::expected<void, Error> send_impl(std::span<const uint8_t>, opcode);

    std::expected<void, Error>      send_control_frame(std::span<const uint8_t>,
                                                       opcode);
    std::expected<ChunkView, Error> read_chunk_impl(size_t);
    std::expected<detail::PayloadMetaData, Error> read_header(size_t);
    std::expected<detail::PayloadMetaData, Error>
                               read_payload(detail::PayloadMetaData);
    std::expected<void, Error> send_close(std::span<const uint8_t>);
    void                       fail_connection(status_code reason);

  public:
    std::expected<void, Error> send(const std::string_view);
    std::expected<void, Error> send(std::span<const uint8_t>);
    std::expected<void, Error> send_ping(std::span<const uint8_t>);
    std::expected<void, Error> send_pong(std::span<const uint8_t>);
    // next major update this will need to be and enum with an explanation
    std::expected<void, Error>      close(std::span<const uint8_t>);
    std::expected<ChunkView, Error> read_chunk();
    std::expected<Message, Error>   read_message(
        size_t max_size = 100 * 1024 * 1024); // 100MB limit to prevent OOM
    // this one should be uri
    static std::expected<Client, Error>
    create(const std::string_view host, const std::string_view path,
           const std::string_view port = "80");
};
} // namespace ws

#endif
