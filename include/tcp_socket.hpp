#ifndef TCP_SOCKET_HPP
#define TCP_SOCKET_HPP

#include <expected>
#include <string_view>

namespace ws {
class TcpSocket {
  private:
    int m_fd = -1;
    TcpSocket(int fd) : m_fd{fd} {}

  public:
    // these will be in the connect method
    // TcpSocket(std::string_view uri);
    TcpSocket(const std::string_view host, const std::string_view port);
    // TcpSocket();
    TcpSocket(TcpSocket &&);
    TcpSocket &operator=(TcpSocket &&);
    TcpSocket(const TcpSocket &)            = delete;
    TcpSocket &operator=(const TcpSocket &) = delete;
    ~TcpSocket();
    int get_fd() const { return m_fd; }
    // I think i can return std::string as the getaddrinfo strings
    // are statically allocated but further testing is needed
    static std::expected<TcpSocket, std::string_view>
    connect(const std::string_view host, const std::string_view port);
};
} // namespace ws

#endif
