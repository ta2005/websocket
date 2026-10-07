#ifndef TCP_SOCKET_HPP
#define TCP_SOCKET_HPP

#include <cstdint>
#include <span>
#include <memory>
#include "common/details/ring_buffer.hpp"
// Forward declare EventLoop, or include it.
// If we include it, we need to make sure there's no circular dependency.
// But SocketCtx needs to be known, so let's forward declare it instead.
namespace ws::async {
    struct SocketCtx;
}

namespace ws::async {

class EventLoop;
struct ReadAwaitable;
struct WaitWritable;
struct AcceptAwaitable;

class TcpSocket {
  private:
    int                                m_fd;
    EventLoop                         &m_loop;
    std::unique_ptr<SocketCtx>         m_ctx;
    ws::detail::RingBuffer             m_read_buffer;

  public:
    TcpSocket(int fd, EventLoop &loop);
    ~TcpSocket();

    // Disable copy
    TcpSocket(const TcpSocket &)            = delete;
    TcpSocket &operator=(const TcpSocket &) = delete;

    // Enable move
    TcpSocket(TcpSocket &&other) noexcept;
    TcpSocket &operator=(TcpSocket &&other) noexcept;

    auto get_fd() const { return m_fd; }
    SocketCtx *get_ctx() const { return m_ctx.get(); }
    ws::detail::RingBuffer &get_buffer() { return m_read_buffer; }
    EventLoop &get_loop() { return m_loop; }

    void close();

    ReadAwaitable read(size_t n_bytes);
    WaitWritable  write(std::span<const uint8_t> meta,
                        std::span<const uint8_t> payload);
    AcceptAwaitable accept();
};
} // namespace ws::async

#endif
