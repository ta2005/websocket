#include <cerrno>
#include <cstring>
#include <expected>
#include <fcntl.h>
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
    //
    //    if (socketfd == -1) {
    //        return std::unexpected("unable to connect");
    //    }
    //    auto flags = fcntl(socketfd,F_GETFL);
    //    if(flags==-1){
    // return std::unexpected("unable to get the flags to socket");
    //    }
    //    flags=fcntl(socketfd,F_SETFL,flags|O_NONBLOCK);
    //    if(flags==-1){
    // return std::unexpected("unable to set non blocking socket");
    //    }

    freeaddrinfo(res);
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
// both of these functions are shit
// but they do the job
std::expected<size_t, std::string_view>
TcpSocket::send(std::span<const uint8_t> data) const {
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        ssize_t sent =
            ::write(m_fd, data.data() + total_sent, data.size() - total_sent);
        if (sent < 0) {
            if (errno == EINTR)
                continue; // Interrupted by signal, try again
            return std::unexpected(std::strerror(errno));
        }
        total_sent += sent;
    }
    return total_sent;
}
std::expected<size_t, std::string_view>
TcpSocket::send(const std::string_view buf) const {
    auto bytes = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t *>(buf.data()), buf.size());
    return send(bytes);
}

std::string TcpSocket::read(int max_len) const {
    std::string s;
    s.resize(max_len);
    ::read(m_fd, s.data(), max_len);
    return s;
}

std::expected<size_t, std::string_view> TcpSocket::read(std::span<uint8_t> buf) const {
    while (true) {
        ssize_t bytes_read = ::read(m_fd, buf.data(), buf.size());
        if (bytes_read < 0) {
            if (errno == EINTR) continue; // Interrupted by signal, try again
            return std::unexpected(std::strerror(errno));
        }
        if (bytes_read == 0) {
            return std::unexpected("Connection closed by peer");
        }
        return static_cast<size_t>(bytes_read);
    }
}

} // namespace ws
