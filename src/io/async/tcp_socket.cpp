#include "io/async/tcp_socket.hpp"
#include "io/async/accept_awaitable.hpp"
#include "io/async/read_awaitable.hpp"
#include "io/async/write_awaitable.hpp"
#include "io/async/event_loop.hpp"
#include <unistd.h>
#include <utility>

namespace ws::async {

TcpSocket::TcpSocket(int fd, EventLoop &loop) 
    : m_fd(fd), m_loop(loop), 
      m_ctx(std::make_unique<SocketCtx>()),
      m_read_buffer(std::move(ws::detail::RingBuffer::create(getpagesize() * 4).value())) {
    
    m_ctx->fd = m_fd;
    m_ctx->read_handle = nullptr;
    m_ctx->write_handle = nullptr;
    m_loop.register_socket(m_fd, m_ctx.get());
}

TcpSocket::~TcpSocket() { close(); }

void TcpSocket::close() {
    if (m_fd != -1) {
        m_loop.unregister_socket(m_fd);
        ::close(m_fd);
        m_fd = -1;
    }
}

TcpSocket::TcpSocket(TcpSocket &&other) noexcept
    : m_fd(std::exchange(other.m_fd, -1)), 
      m_loop(other.m_loop),
      m_ctx(std::move(other.m_ctx)),
      m_read_buffer(std::move(other.m_read_buffer)) {
    // If m_ctx is still valid, we should update the fd in it just in case, though it hasn't changed.
    if (m_ctx) {
        m_ctx->fd = m_fd;
        // The pointer in the event loop remains valid because m_ctx was heap-allocated and just moved ownership.
    }
}

TcpSocket &TcpSocket::operator=(TcpSocket &&other) noexcept {
    if (this != &other) {
        if (m_fd != -1) {
            this->close();
        }
        m_fd = std::exchange(other.m_fd, -1);
        m_ctx = std::move(other.m_ctx);
        m_read_buffer = std::move(other.m_read_buffer);
        if (m_ctx) {
            m_ctx->fd = m_fd;
        }
    }
    return *this;
}

ReadAwaitable TcpSocket::read(size_t n_bytes) {
    return ReadAwaitable{*this, n_bytes};
}

WaitWritable TcpSocket::write(std::span<const uint8_t> meta,
                              std::span<const uint8_t> payload) {
    return WaitWritable{*this, m_loop, meta, payload};
}

AcceptAwaitable TcpSocket::accept() {
    return AcceptAwaitable{*this, m_loop, -1};
}

} // namespace ws::async
