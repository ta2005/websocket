#include "io/async/event_loop.hpp"
#include <sys/epoll.h>
#include <unistd.h>

namespace ws::async {
EventLoop::EventLoop(int fd) : m_epollfd{fd} {
    // I think this is the max theortical but I might be wrong
    m_handles.resize(1 << 16);
}
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
    // I don't know about this one and wheter the vector will be dangling
    if (this != &rhs) {
        if (m_epollfd != -1) {
            close(m_epollfd);
        }
        this->m_epollfd = rhs.m_epollfd;
        rhs.m_epollfd   = -1;
    }
    this->m_handles = std::move(rhs.m_handles);
    rhs.m_handles.clear();
    return *this;
}

void EventLoop::unregister_socket(TcpSocket &socket) {
    m_handles[socket.get_fd()].read_handle  = nullptr;
    m_handles[socket.get_fd()].write_handle = nullptr;
}

void EventLoop::register_read(TcpSocket &socket, Handle h) {
    m_handles[socket.get_fd()].read_handle = h;
}

void EventLoop::register_write(TcpSocket &socket, Handle h) {
    m_handles[socket.get_fd()].write_handle = h;
}

std::expected<void, Error> EventLoop::register_socket(TcpSocket &socket) {
    epoll_event ev{};
    // No EPOLLONESHOT needed for single-threaded EPOLLET!
    // We register for IN, OUT, and RDHUP right from the start.
    ev.events  = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
    auto fd    = socket.get_fd();
    ev.data.fd = fd;

    if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        return std::unexpected(ws::Error::ReadFailed);
    }
    return {};
}

void EventLoop::run() {
    epoll_event events[64];
    int         nfds = epoll_wait(m_epollfd, events, 64, -1);
    for (int i = 0; i < nfds; i++) {
        uint32_t revents = events[i].events;
        auto     fd      = events[i].data.fd;

        if (revents & (EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
            if (auto h = m_handles[fd].read_handle) {
                m_handles[fd].read_handle = nullptr; // consume
                h.resume();
            }
        }

        if (revents & EPOLLOUT) {
            if (auto h = m_handles[fd].write_handle) {
                m_handles[fd].write_handle = nullptr; // consume
                h.resume();
            }
        }
    }
}

} // namespace ws::async
