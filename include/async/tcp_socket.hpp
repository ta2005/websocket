#ifndef TCP_SOCKET_HPP
#define TCP_SOCKET_HPP

#include <cstdint>
#include <span>

namespace ws::async {

class EventLoop;
struct ReadAwaitable;
struct WaitWritable;

class TcpSocket {
  private:
    int        m_fd;
    EventLoop &m_loop;

  public:
    TcpSocket(int fd, EventLoop &loop) : m_fd(fd), m_loop(loop) {}
    auto get_fd() { return m_fd; }

    ReadAwaitable read_some(std::span<uint8_t> buffer);
    WaitWritable  write_some(std::span<uint8_t> meta,
                             std::span<uint8_t> payload);
};
} // namespace ws::async

#endif
