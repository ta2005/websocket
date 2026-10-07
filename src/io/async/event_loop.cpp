#include "io/async/event_loop.hpp"
#include <sys/epoll.h>
#include <unistd.h>

namespace ws::async {
EventLoop::EventLoop(int fd) : m_epollfd{fd} {}
EventLoop::~EventLoop() {
    if (m_epollfd != -1) {
        close(m_epollfd);
        m_epollfd = -1;
    }
}

std::expected<EventLoop, Error> EventLoop::create() {
    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        return std::unexpected(ws::Error::EventLoopInitFailed);
    }
    return EventLoop(epollfd);
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

void EventLoop::unregister_socket(int fd) {
    epoll_ctl(m_epollfd, EPOLL_CTL_DEL, fd, nullptr);
}

std::expected<void, Error> EventLoop::register_socket(int fd, SocketCtx *ctx) {
    epoll_event ev{};
    // No EPOLLONESHOT needed for single-threaded EPOLLET!
    // We register for IN, OUT, and RDHUP right from the start.
    ev.events   = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
    ev.data.ptr = ctx;

    if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        return std::unexpected(ws::Error::ReadFailed);
    }
    return {};
}

void EventLoop::run() {
    epoll_event events[64];
    int         nfds = epoll_wait(m_epollfd, events, 64, -1);
    for (int i = 0; i < nfds; i++) {
        uint32_t   revents = events[i].events;
        SocketCtx *ctx     = static_cast<SocketCtx *>(events[i].data.ptr);

        if (revents & (EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
            if (auto h = ctx->read_handle) {
                ctx->read_handle = nullptr; // consume
                h.resume();
            }
        }

        if (revents & EPOLLOUT) {
            if (auto h = ctx->write_handle) {
                ctx->write_handle = nullptr; // consume
                h.resume();
            }
        }
    }
}

} // namespace ws::async
