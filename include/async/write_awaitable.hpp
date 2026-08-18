#ifndef WRITEV_AWAITABLE_HPP
#define WRITEV_AWAITABLE_HPP

#include "async/event_loop.hpp"
#include "async/tcp_socket.hpp"
#include <cerrno>
#include <sys/epoll.h>
#include <sys/uio.h>

namespace ws::async {

// A lightweight awaitable that simply puts the coroutine to sleep
// until Epoll says the socket is writable again.
struct WaitWritable {
    TcpSocket &socket;
    EventLoop &loop;

    bool await_ready() { return false; }

    void await_suspend(Handle hd) {
        loop.register_write(socket, hd);
    }

    void await_resume() {}
};

} // namespace ws::async
#endif
