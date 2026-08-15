#include "async/event_loop.hpp"
#include "async/socket_ctx.hpp"
#include <stdexcept>
#include <sys/epoll.h>
#include <unistd.h>

namespace ws::async {
EventLoop::EventLoop() {
    m_epollfd = epoll_create1(0);
    if (m_epollfd == -1) {
        throw std::runtime_error("unable to create fd");
    }
}
EventLoop::~EventLoop() {
    if (m_epollfd != -1) {
        close(m_epollfd);
    }
}

EventLoop::EventLoop(EventLoop &&rhs) : m_epollfd(rhs.m_epollfd) {
    rhs.m_epollfd = -1;
}

EventLoop &EventLoop::operator=(EventLoop &&rhs) {
    if (this != &rhs) {
        if (m_epollfd != -1) {
            close(m_epollfd);
        }
        this->m_epollfd = rhs.m_epollfd;
        rhs.m_epollfd   = -1;
    }
    return *this;
}

// this one should simply return a std::expection
// but once again
// it is exception it i can do epoll_ctl
// like wtf am i supposed to do ?
void EventLoop::register_socket(SocketCtx &ctx) {
    epoll_event ev{};
    ev.events   = EPOLLONESHOT|EPOLLET; // Crucial for one-shot notifications
    ev.data.ptr = &ctx;

    if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, ctx.fd, &ev) < 0) {
        throw "3asba nr2 ";
    }
}

void EventLoop::rearm(SocketCtx &ctx, uint32_t events) {
    ctx.events |= events;
    epoll_event ev{};
    ev.events   = ctx.events | EPOLLONESHOT|EPOLLET;
    ev.data.ptr = &ctx;

    // add error handling
    epoll_ctl(m_epollfd, EPOLL_CTL_MOD, ctx.fd, &ev);
}

void EventLoop::run() {
    epoll_event events[64];
    int         nfds = epoll_wait(m_epollfd, events, 8, -1);
    for (int i = 0; i < nfds; i++) {
        auto    *ctx     = static_cast<SocketCtx *>(events[i].data.ptr);
        uint32_t revents = events[i].events;
	// this is somewhat coupled and even a bit 
	// repetitive 
	// maybe i could brush up one some template and 
	// implement a compile time switch 
	// stmt for 
	// thing 
	// but later 
        if ((revents & (EPOLLIN | EPOLLHUP | EPOLLERR)) && ctx->read_handle) {
            ctx->events &= ~EPOLLIN; // Disarmed by epoll
            auto h           = ctx->read_handle;
            ctx->read_handle = nullptr;
            h.resume();
        }

        if ((revents & EPOLLOUT) && ctx->write_handle) {
            ctx->events &= ~EPOLLOUT; // Disarmed by epoll
            auto h            = ctx->write_handle;
            ctx->write_handle = nullptr;
            h.resume();
        }
    }
}

} // namespace ws::async
