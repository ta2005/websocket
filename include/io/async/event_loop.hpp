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

    // this will change when i create the
    // tcp class
    std::expected<void, Error> register_socket(TcpSocket &);
    void                       unregister_socket(TcpSocket &);
    void                       register_read(TcpSocket &, Handle);
    void                       register_write(TcpSocket &, Handle);
    // void rearm(SocketCtx &, uint32_t);

  private:
    EventLoop(int fd);
    int m_epollfd;
    // this could also take an unordered map
    // of tcpsocket
    // and the hash method is get_fd
    //  it will get the same perf
    //  but with better semantics
    std::vector<SocketCtx> m_handles;
};
} // namespace ws::async

#endif
