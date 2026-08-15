#ifndef TCP_SOCKET_HPP
#define TCP_SOCKET_HPP

#include "async/socket_ctx.hpp"

namespace ws::async {
class TcpSocket {
  private:
    int m_fd;
};
} // namespace ws::async

#endif
