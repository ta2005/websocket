#ifndef ACCEPT_AWAITABLE_HPP
#define ACCEPT_AWAITABLE_HPP

#include "async/event_loop.hpp"
#include "async/tcp_socket.hpp"
#include <cerrno>
#include <errno.h>
#include <linux/sockios.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ws::async {
struct AcceptAwaitable {
    TcpSocket& server;
    EventLoop &loop;
    int        c_fd=-1;
    bool       await_ready() {
        c_fd = ::accept4(server.get_fd(), NULL, NULL, SOCK_NONBLOCK);
        if (c_fd >= 0)
            return true;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return false;
        return true;
    }

    void await_suspend(Handle hd) { loop.register_read(server, hd); }

    TcpSocket await_resume() {
        if (c_fd >= 0)
            return TcpSocket{c_fd, loop};
        c_fd = ::accept4(server.get_fd(), NULL, NULL, SOCK_NONBLOCK);
        return TcpSocket{c_fd, loop};
    }
};
} // namespace ws::async

#endif
