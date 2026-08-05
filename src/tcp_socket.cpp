#include <expected>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include "tcp_socket.hpp"

namespace ws {
TcpSocket::~TcpSocket() {
    if (m_fd != -1) {
        close(m_fd);
    }
}
std::expected<TcpSocket, std::string_view>
TcpSocket::connect(const std::string_view host, const std::string_view port) {
    addrinfo *res = NULL;
    addrinfo  hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int result;
    if ((result = getaddrinfo(host.data(), port.data(), &hints, &res)) != 0) {
        return std::unexpected(gai_strerror(result));
    }
    int socketfd = -1;
    for (addrinfo *p = res; p != NULL; p = (p)->ai_next) {
        if ((socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
            -1) {
            continue;
        }
        // maybe add some loging capability through some uniform interface later
        // char h[INET6_ADDRSTRLEN];
        // inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
        //           host, sizeof host);
        // std::print("\033[32mconnection to {}\033[0m\n", host);
        if (::connect(socketfd, p->ai_addr, p->ai_addrlen) == -1) {
            // std::print("\033[31mfailed to connect to {}\033[0m\n", host);
            close(socketfd);
            socketfd = -1;
            continue;
        }
        break;
    }
    if (socketfd == -1) {
        return std::unexpected("unable to connect");
    }
    return TcpSocket(socketfd);
}
// the move opertors
TcpSocket::TcpSocket(TcpSocket &&other) : m_fd(other.m_fd) { other.m_fd = -1; }
TcpSocket &TcpSocket::operator=(TcpSocket &&other) {
    if (this != &other) {
        if (m_fd != -1)
            close(m_fd);
        m_fd       = other.m_fd;
        other.m_fd = -1;
    }
    return *this;
}
} // namespace ws
