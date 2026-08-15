#ifndef WRITEV_AWAITABLE_HPP
#define WRITEV_AWAITABLE_HPP

#include "async/event_loop.hpp"
#include "async/socket_ctx.hpp"
#include <sys/uio.h>
#include <cerrno>
#include <sys/epoll.h>

namespace ws::async {

// A lightweight awaitable that simply puts the coroutine to sleep 
// until Epoll says the socket is writable again.
struct WaitWritable {
    SocketCtx &ctx;
    EventLoop &loop;

    bool await_ready() { return false; }
    
    void await_suspend(Handle hd) {
        ctx.write_handle = hd;
        loop.rearm(ctx, EPOLLOUT);
    }
    
    void await_resume() {}
};

} // namespace ws::async
#endif
