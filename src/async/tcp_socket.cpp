#include "async/tcp_socket.hpp"
#include "async/read_awaitable.hpp"
#include "async/write_awaitable.hpp"

namespace ws::async {

TcpSocket::TcpSocket(int fd, EventLoop& loop) : m_fd(fd), m_loop(loop) {}

ReadAwaitable TcpSocket::read_some(std::span<uint8_t> buffer) {
    return ReadAwaitable{*this, buffer, m_loop};
}

WaitWritable TcpSocket::wait_writable() {
    return WaitWritable{*this, m_loop};
}

} // namespace ws::async
