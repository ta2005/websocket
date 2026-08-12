#ifndef TCP_SOCKET_HPP
#define TCP_SOCKET_HPP

#include "error.hpp"
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <unistd.h>

namespace ws {
class TcpSocket {
  private:
    int m_fd = -1;
    TcpSocket(int fd) : m_fd{fd} {}

  public:
    TcpSocket(const std::string_view host, const std::string_view port);
    TcpSocket(TcpSocket &&);
    TcpSocket &operator=(TcpSocket &&);
    TcpSocket(const TcpSocket &)            = delete;
    TcpSocket &operator=(const TcpSocket &) = delete;
    ~TcpSocket();
    void close() { ::close(m_fd); }
    int  get_fd() const { return m_fd; }
         operator bool() { return m_fd != -1; }
    // i don't maybe i will change the interface later
    std::expected<size_t, Error> send(const std::span<const uint8_t>) const;
    std::expected<size_t, Error> send(const std::string_view) const;
    std::expected<size_t, Error>
    send(const std::span<const uint8_t> meta_data,
         const std::span<const uint8_t> payload) const;
    std::expected<size_t, Error> send(const std::span<const uint8_t> meta_data,
                                      std::string_view payload) const;
    std::string                  read(int max_len) const;
    std::expected<size_t, Error> read(std::span<uint8_t> buf) const;
    // I think i can return std::string as the getaddrinfo strings
    // are statically allocated but further testing is needed
    static std::expected<TcpSocket, Error> connect(const std::string_view host,
                                                   const std::string_view port);
};
} // namespace ws

#endif
