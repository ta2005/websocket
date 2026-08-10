#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "opcode.hpp"
#include "tcp_socket.hpp"
#include <random>
#include <string_view>
#include <vector>

namespace ws {

struct ChunckView {
    std::span<uint8_t> payload;
    bool               is_fin;
    opcode             type;
};
class Client {
  private:
    TcpSocket    m_socket;
    std::mt19937 m_rng;
    struct ReadSate {
        std::vector<uint8_t> data;
        bool                 fin;
        opcode               type;
        size_t               remaing_bytes;
    } current_read;

    Client(TcpSocket s)
        : m_socket(std::move(s)), m_rng(std::random_device{}()) {};
    std::expected<void, std::string_view> send_impl(std::span<const uint8_t>,
                                                    opcode);
    std::expected<ChunckView, std::string_view> read_chunck_impl(size_t);

  public:
    std::expected<void, std::string_view> send(const std::string_view);
    std::expected<void, std::string_view> send(std::span<const uint8_t>);
    std::expected<void, std::string_view> send_ping(std::span<const uint8_t>);
    std::expected<void, std::string_view> send_pong(std::span<const uint8_t>);
    // next major update this will need to be and enum with an explanation
    std::expected<void, std::string_view>       close() const;
    std::expected<ChunckView, std::string_view> read_chunk();
    // this one should be uri
    static std::expected<Client, std::string_view>
    create(const std::string_view host, const std::string_view path,
           const std::string_view port = "80");
};
} // namespace ws

#endif
