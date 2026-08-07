#ifndef TCP_SOCKET_HPP
#define TCP_SOCKET_HPP

#include <expected>
#include <string_view>
#include <string>
#include <span>
#include <cstdint>

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
    int get_fd() const { return m_fd; }
    operator bool(){
	return m_fd!=-1;
    }
    // i don't maybe i will change the interface later
    std::expected<size_t,std::string_view> send(const std::span<const uint8_t>) const;
    std::expected<size_t,std::string_view> send(const std::string_view) const;
    std::string read(int max_len) const;
    std::expected<size_t, std::string_view> read(std::span<uint8_t> buf) const;
    // I think i can return std::string as the getaddrinfo strings
    // are statically allocated but further testing is needed
    static std::expected<TcpSocket, std::string_view>
    connect(const std::string_view host, const std::string_view port);
};
} // namespace ws

#endif
