#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "tcp_socket.hpp"
#include <string_view>

namespace ws {
class Client {
  private:
    TcpSocket m_socket;
    Client(TcpSocket s) : m_socket(std::move(s)) {};

  public:
    // std::expected<void, std::string_view> send(const std::string_view msg);
    std::expected<void, std::string_view> send(const std::span<uint8_t>) const;
    // this one should be uri 
    static std::expected<Client, std::string_view>
    create(const std::string_view host, const std::string_view path,
           const std::string_view port = "80");
};
} // namespace ws

#endif // INCLUDE/home/talel/Programming/Projects/websock/includeclientclient.hpp_
