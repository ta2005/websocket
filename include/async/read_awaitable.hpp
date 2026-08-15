#ifndef READ_AWAITABLE_HPP
#define READ_AWAITABLE_HPP

#include "async/event_loop.hpp"
#include "async/socket_ctx.hpp"
#include <cerrno>
#include <cstdint>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <span>

namespace ws::async {
struct ReadAwaitable {
    SocketCtx          &ctx;
    std::span<uint8_t>  buffer;
    EventLoop          &loop;
    ssize_t             bytes_read = 0;

    bool await_ready() {
        bytes_read = ::read(ctx.fd, buffer.data(), buffer.size());
        if (bytes_read > 0 || bytes_read == 0) return true; // Got data or EOF, don't suspend!
        if (errno == EAGAIN || errno == EWOULDBLOCK) return false; // Need data, suspend!
        return true; // Error occurred
    }

    void await_suspend(Handle hd) {
        ctx.read_handle = hd;
        loop.rearm(ctx, EPOLLIN);
    }

    ssize_t await_resume() {
        if (bytes_read > 0 || bytes_read == 0) return bytes_read;
        // Epoll woke us up, read now!
        return ::read(ctx.fd, buffer.data(), buffer.size());
    }
};
} // namespace ws::async

#endif // INCLUDE/home/talel/Programming/Projects/websock/include/asyncReadAwaitableReadAwaitable.hpp_
