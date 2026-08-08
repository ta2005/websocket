#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "opcode.hpp"
#include "tcp_socket.hpp"
#include <random>
#include <string_view>

namespace ws {
class Client {
  private:
    TcpSocket    m_socket;
    std::mt19937 m_rng;

    Client(TcpSocket s)
        : m_socket(std::move(s)), m_rng(std::random_device{}()) {};
    std::expected<void, std::string_view> send_impl(std::span<const uint8_t>,
                                                    opcode) ;

  public:
    std::expected<void, std::string_view> send(const std::string_view) ;
    std::expected<void, std::string_view> send(std::span<const uint8_t>) ;
    // next major update this will need to be and enum with an explanation
    std::expected<void, std::string_view> close() const;
    // this one should be uri
    static std::expected<Client, std::string_view>
    create(const std::string_view host, const std::string_view path,
           const std::string_view port = "80");
};
} // namespace ws

#endif 
