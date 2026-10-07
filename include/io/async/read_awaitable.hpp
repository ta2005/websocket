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
    TcpSocket &socket;
    size_t     n_bytes;
    bool       eof = false;
    bool       error = false;

    bool try_read_socket() {
        auto &rb = socket.get_buffer();
        while (rb.writable_bytes() > 0) {
            ssize_t n = ::read(socket.get_fd(), rb.write_ptr(), rb.writable_bytes());
            if (n > 0) {
                rb.advance_tail(n);
            } else if (n == 0) {
                eof = true;
                return true;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return false; // Hit EAGAIN, need epoll
                }
                error = true;
                return true;
            }
        }
        // Buffer is full. We didn't hit EAGAIN.
        // We will stop reading for now to exert backpressure.
        return true; 
    }

    bool await_ready() {
        auto &rb = socket.get_buffer();
        if (rb.readable_bytes() > 0) {
            return true; // Fast path: data already in buffer
        }
        try_read_socket();
        return rb.readable_bytes() > 0 || eof || error;
    }

    void await_suspend(Handle hd) { 
        socket.get_ctx()->read_handle = hd; 
    }

    std::expected<std::span<uint8_t>, Error> await_resume() {
        auto &rb = socket.get_buffer();
        
        if (rb.readable_bytes() == 0 && !eof && !error) {
            // Woken up by epoll, try reading again
            try_read_socket();
        }

        if (error) return std::unexpected(Error::ReadFailed);

        size_t to_return = std::min(n_bytes, rb.readable_bytes());
        std::span<uint8_t> data(rb.read_ptr(), to_return);
        rb.advance_head(to_return); // Mark as consumed
        
        return data;
    }
};
} // namespace ws::async

#endif // INCLUDE/home/talel/Programming/Projects/websock/include/asyncReadAwaitableReadAwaitable.hpp_
