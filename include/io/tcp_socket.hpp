#ifndef TCP_SOCKET_HPP
#define TCP_SOCKET_HPP

#include "common/error.hpp"
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
    TcpSocket(TcpSocket &&);
    TcpSocket &operator=(TcpSocket &&);
    TcpSocket(const TcpSocket &)            = delete;
    TcpSocket &operator=(const TcpSocket &) = delete;
    ~TcpSocket();
    void close() {
        if (m_fd != -1) {
            ::close(m_fd);
            m_fd = -1;
        }
    }
    int get_fd() const { return m_fd; }
        operator bool() { return m_fd != -1; }
    std::expected<size_t, Error>
    write(const std::span<const uint8_t> meta_data,
          const std::span<const uint8_t> payload) const;
    std::expected<size_t, Error> write(const std::span<const uint8_t> meta_data,
                                       std::string_view payload) const;
    std::expected<size_t, Error> read(std::span<uint8_t> buf) const;
    // I think i can return std::string as the getaddrinfo strings
    // are statically allocated but further testing is needed
    static std::expected<TcpSocket, Error> connect(const std::string_view host,
                                                   const std::string_view port);
};
} // namespace ws

#endif
