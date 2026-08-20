#ifndef SERVER_HPP
#define SERVER_HPP

#include "async/event_loop.hpp"
#include "async/tcp_socket.hpp"
#include "async/task.hpp"

namespace ws::async {

class Server {
  public:
    Server(uint16_t port, EventLoop& loop);

    // Starts the infinite accept loop
    task run_accept_loop();

  private:
    EventLoop& m_loop;
    TcpSocket  m_listener;
};

} // namespace ws::async

#endif
