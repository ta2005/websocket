#ifndef SERVER_HPP
#define SERVER_HPP

#include "common/concepts.hpp"
#include "handshake.hpp"
#include "session.hpp"
#include <expected>
#include <string_view>

namespace ws {

template <typename Socket> class Server {
  public:
    // TODO: implement Server::listen and accept_loop
    // This will spawn Session<Socket, Role::Server> for each connected client
};

} // namespace ws

#endif
