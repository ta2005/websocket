#ifndef WRITEV_AWAITABLE_HPP
#define WRITEV_AWAITABLE_HPP

#include "io/async/event_loop.hpp"
#include "io/async/tcp_socket.hpp"
#include <array>
#include <cerrno>
#include <sys/epoll.h>
#include <sys/uio.h>

namespace ws::async {

// A lightweight awaitable that simply puts the coroutine to sleep
// until Epoll says the socket is writable again.
struct WaitWritable {
    TcpSocket               &socket;
    EventLoop               &loop;
    std::span<const uint8_t> meta;
    std::span<const uint8_t> payload;

    std::array<iovec, 2> io = {{{.iov_base = const_cast<void *>(
                                     static_cast<const void *>(meta.data())),
                                 .iov_len = meta.size()},
                                {.iov_base = const_cast<void *>(
                                     static_cast<const void *>(payload.data())),
                                 .iov_len = payload.size()}}};
    ssize_t              bytes_sent = 0;

    bool await_ready() {
        bytes_sent = ::writev(socket.get_fd(), io.data(), io.size());
        if (bytes_sent > 0 || bytes_sent == 0)
            return true; // Got data or EOF, don't suspend!
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return false; // Need data, suspend!
        return true;      // Error occurred
    }

    void await_suspend(Handle hd) { loop.register_write(socket, hd); }

    ssize_t await_resume() {
        if (bytes_sent > 0 || bytes_sent == 0)
            return bytes_sent;
        // Epoll woke us up, read now!
        return bytes_sent = ::writev(socket.get_fd(), io.data(), io.size());
    }
};

} // namespace ws::async
#endif
