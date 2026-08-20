#ifndef READ_AWAITABLE_HPP
#define READ_AWAITABLE_HPP

#include "common/error.hpp"
#include "io/async/event_loop.hpp"
#include "io/async/tcp_socket.hpp"
#include <cerrno>
#include <cstdint>
#include <errno.h>
#include <expected>
#include <span>
#include <sys/epoll.h>
#include <unistd.h>

namespace ws::async {
struct ReadAwaitable {
    TcpSocket         &socket;
    std::span<uint8_t> buffer;
    EventLoop         &loop;
    ssize_t            bytes_read = 0;

    bool await_ready() {
        bytes_read = ::read(socket.get_fd(), buffer.data(), buffer.size());
        if (bytes_read > 0 || bytes_read == 0)
            return true; // Got data or EOF, don't suspend!
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return false; // Need data, suspend!
        return true;      // Error occurred
    }

    void await_suspend(Handle hd) { loop.register_read(socket, hd); }

    std::expected<size_t, Error> await_resume() {
        if (bytes_read > 0 || bytes_read == 0)
            return bytes_read;
        // Epoll woke us up, read now!
        bytes_read = read(socket.get_fd(), buffer.data(), buffer.size());
        if (bytes_read < 0) {
            return std::unexpected(Error::ConnectionFailed);
        }
        return bytes_read;
    }
};
} // namespace ws::async

#endif // INCLUDE/home/talel/Programming/Projects/websock/include/asyncReadAwaitableReadAwaitable.hpp_
