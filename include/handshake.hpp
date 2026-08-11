#ifndef HANDSHAKE_HPP
#define HANDSHAKE_HPP

#include "tcp_socket.hpp"
#include <string_view>
#include <vector>

namespace ws {
// i will first assuem no erros then will see later on
struct StatusLine {
    std::string version;
    int         status;
    std::string reason;
};
struct HandskaheResult {
    StatusLine           line;
    std::string          accept_key;
    std::string          upgrade;
    std::string          connection;
    std::string          extensions;
    std::string          protocol;
    std::vector<uint8_t> leftover;
};

std::expected<HandskaheResult, Error>
perform_handshake(const TcpSocket &, const std::string_view host,
                  const std::string_view path,
                  const std::string_view port = "80");

} // namespace ws

#endif
