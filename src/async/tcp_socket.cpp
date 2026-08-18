#include "async/tcp_socket.hpp"
#include "async/read_awaitable.hpp"
#include "async/write_awaitable.hpp"

namespace ws::async {

ReadAwaitable TcpSocket::read_some(std::span<uint8_t> buffer) {
    return ReadAwaitable{*this, buffer, m_loop};
}

WaitWritable TcpSocket::write_some(std::span<uint8_t> meta,
                                   std::span<uint8_t> payload) {
    return WaitWritable{*this, m_loop, meta, payload};
}

} // namespace ws::async
