#include "io/async/tcp_socket.hpp"
#include "io/async/accept_awaitable.hpp"
#include "io/async/read_awaitable.hpp"
#include "io/async/write_awaitable.hpp"
#include <unistd.h>
#include <utility>

namespace ws::async {

TcpSocket::TcpSocket(int fd, EventLoop &loop) : m_fd(fd), m_loop(loop) {}

TcpSocket::~TcpSocket() {
    if (m_fd != -1) {
        ::close(m_fd);
        m_loop.unregister_socket(*this);
        m_fd = -1;
    }
}

TcpSocket::TcpSocket(TcpSocket &&other) noexcept
    : m_fd(std::exchange(other.m_fd, -1)), m_loop(other.m_loop) {}

TcpSocket &TcpSocket::operator=(TcpSocket &&other) noexcept {
    if (this != &other) {
        if (m_fd != -1) {
            ::close(m_fd);
        }
        m_fd = std::exchange(other.m_fd, -1);
    }
    return *this;
}

ReadAwaitable TcpSocket::read_some(std::span<uint8_t> buffer) {
    return ReadAwaitable{*this, buffer, m_loop};
}

WaitWritable TcpSocket::write_some(std::span<uint8_t> meta,
                                   std::span<uint8_t> payload) {
    return WaitWritable{*this, m_loop, meta, payload};
}

AcceptAwaitable TcpSocket::accept() {
    return AcceptAwaitable{*this, m_loop, -1};
}

} // namespace ws::async
