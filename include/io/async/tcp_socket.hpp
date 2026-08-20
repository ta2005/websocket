#ifndef TCP_SOCKET_HPP
#define TCP_SOCKET_HPP

#include <cstdint>
#include <span>

namespace ws::async {

class EventLoop;
struct ReadAwaitable;
struct WaitWritable;
struct AcceptAwaitable;

class TcpSocket {
  private:
    int        m_fd;
    EventLoop &m_loop;

  public:
    // this constorctor should register
    // the client ?? aka call the epoll_ctl
    // maybe i should even make tcp_socket take
    // the loop as a param to read_some
    // and write_some
    // and have the server and clinet
    // classes pass it to their tcp instance?
    // but since tcp_socket always uses
    // the loop maybe this way is better
    TcpSocket(int fd, EventLoop &loop);
    ~TcpSocket();

    // Disable copy
    TcpSocket(const TcpSocket &)            = delete;
    TcpSocket &operator=(const TcpSocket &) = delete;

    // Enable move
    TcpSocket(TcpSocket &&other) noexcept;
    TcpSocket &operator=(TcpSocket &&other) noexcept;

    auto get_fd() const { return m_fd; }

    ReadAwaitable   read_some(std::span<uint8_t> buffer);
    WaitWritable    write_some(std::span<uint8_t> meta,
                               std::span<uint8_t> payload);
    AcceptAwaitable accept();
};
} // namespace ws::async

#endif
