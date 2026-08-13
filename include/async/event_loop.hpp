#ifndef EVENT_LOOP_HPP
#define EVENT_LOOP_HPP

#include <sys/epoll.h>

namespace ws::async {
class EventLoop {
  public:
    EventLoop();
    EventLoop(EventLoop &)            = delete;
    EventLoop &operator=(EventLoop &) = delete;
    EventLoop(EventLoop &&);
    EventLoop &operator=(EventLoop &&);

    void run();

    // void register_;

  private:
    int m_epollfd;
};
} // namespace ws::async

#endif
