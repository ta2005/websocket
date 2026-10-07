#ifndef EVENT_LOOP_HPP
#define EVENT_LOOP_HPP

#include "common/error.hpp"
#include "io/async/tcp_socket.hpp"
#include <coroutine>
#include <expected>
#include <sys/epoll.h>
#include <vector>
using Handle = std::coroutine_handle<>;

namespace ws::async {
struct SocketCtx {
    int fd;
    Handle read_handle;
    Handle write_handle;
};

class EventLoop {
  public:
    EventLoop(EventLoop &)            = delete;
    EventLoop &operator=(EventLoop &) = delete;
    EventLoop(EventLoop &&);
    ~EventLoop();
    EventLoop                             &operator=(EventLoop &&);
    static std::expected<EventLoop, Error> create();

    void run();
    auto get_fd() { return m_epollfd; }

    std::expected<void, Error> register_socket(int fd, SocketCtx *ctx);
    void                       unregister_socket(int fd);

  private:
    EventLoop(int fd);
    int m_epollfd;
};
} // namespace ws::async

#endif
