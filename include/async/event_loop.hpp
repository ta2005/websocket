#ifndef EVENT_LOOP_HPP
#define EVENT_LOOP_HPP

#include "async/socket_ctx.hpp"
#include <sys/epoll.h>

namespace ws::async {
class EventLoop {
  public:
    EventLoop();
    EventLoop(EventLoop &)            = delete;
    EventLoop &operator=(EventLoop &) = delete;
    EventLoop(EventLoop &&);
    ~EventLoop();
    EventLoop &operator=(EventLoop &&);

    void run();

    // this will change when i create the
    // tcp class
    void register_socket(SocketCtx &);
    void rearm(SocketCtx &, uint32_t);

  private:
    int m_epollfd;
};
} // namespace ws::async

#endif
