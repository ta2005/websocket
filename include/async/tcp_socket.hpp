#ifndef TCP_SOCKET_HPP
#define TCP_SOCKET_HPP

#include <span>
#include <cstdint>

namespace ws::async {

class EventLoop;
struct ReadAwaitable;
struct WaitWritable;

class TcpSocket {
  private:
    int m_fd;
    EventLoop& m_loop;

  public:
    TcpSocket(int fd, EventLoop& loop) : m_fd(fd), m_loop(loop) {}
    auto get_fd() { return m_fd; }

    ReadAwaitable read_some(std::span<uint8_t> buffer);
    WaitWritable wait_writable();
};
} // namespace ws::async

#endif
