#ifndef HANDSHAKE_HPP
#define HANDSHAKE_HPP

#include "tcp_socket.hpp"
#include <string_view>

namespace ws::client {
// i will first assuem no erros then will see later on
struct StatusLine {
    std::string version;
    int         status;
    std::string reason;
};
struct HandskaheResult {
    StatusLine  line;
    std::string accept_key;
    std::string upgrade;
    std::string connection;
    std::string extensions;
    std::string protocol;
};
std::expected<HandskaheResult, std::string_view>
perform_handshake(const TcpSocket &, const std::string_view host,
                  const std::string_view path);

} // namespace ws::client

#endif
