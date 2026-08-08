#include <cerrno>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "logger.hpp"
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
            ws::log::error("Failed to connect to {}:{} using protocol {}", host,
                           port, p->ai_protocol);
            close(socketfd);
            socketfd = -1;
            continue;
        }
        ws::log::info("Successfully connected to {}:{}", host, port);
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
            ws::log::error("TcpSocket::send error: {}", std::strerror(errno));
            return std::unexpected("TcpSocket::send failed");
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

std::expected<size_t, std::string_view>
TcpSocket::read(std::span<uint8_t> buf) const {
    while (true) {
        ssize_t bytes_read = ::read(m_fd, buf.data(), buf.size());
        if (bytes_read < 0) {
            if (errno == EINTR)
                continue; // Interrupted by signal, try again
            ws::log::error("TcpSocket::read error: {}", std::strerror(errno));
            return std::unexpected("TcpSocket::read failed");
        }
        if (bytes_read == 0) {
            return std::unexpected("Connection closed by peer");
        }
        return static_cast<size_t>(bytes_read);
    }
}

// Question: do i really need the const uint8_t here ?
// Answer: You don't need `const` on the span itself (i.e. `const std::span`),
// because a span is just a lightweight view (pointer + size).
// Passing `std::span<const uint8_t>` by value is the standard C++ way!
std::expected<size_t, std::string_view>
TcpSocket::send(std::span<const uint8_t> meta_data,
                std::span<const uint8_t> payload) const {
    size_t       total_sent = 0;
    size_t       total_size = meta_data.size() + payload.size();
    struct iovec io[2];

    io[0].iov_base =
        const_cast<void *>(static_cast<const void *>(meta_data.data()));
    io[0].iov_len = meta_data.size();

    io[1].iov_base =
        const_cast<void *>(static_cast<const void *>(payload.data()));
    io[1].iov_len = payload.size();

    int current_index = 0;
    int iovcnt        = 2;

    while (total_sent < total_size) {
        // BUG FIX: The 3rd argument is the number of buffers (iovcnt), not the
        // byte size!
        ssize_t sent = ::writev(m_fd, &io[current_index], iovcnt);
        if (sent < 0) {
            if (errno == EINTR)
                continue;
            ws::log::error("TcpSocket::send (writev) error: {}",
                           std::strerror(errno));
            return std::unexpected("TcpSocket::send (writev) failed");
        }
        total_sent += sent;

        if (total_sent < total_size) {
            // Adjust the iovec structs for the next partial write
            if (total_sent >= meta_data.size() && current_index == 0) {
                // The metadata buffer is fully sent, move onto the payload
                // buffer
                current_index       = 1;
                iovcnt              = 1;
                size_t payload_sent = total_sent - meta_data.size();
                io[1].iov_base =
                    static_cast<char *>(io[1].iov_base) + payload_sent;
                io[1].iov_len -= payload_sent;
            } else if (current_index == 0) {
                // Still working on the metadata buffer
                io[0].iov_base = static_cast<char *>(io[0].iov_base) + sent;
                io[0].iov_len -= sent;
            } else {
                // Still working on the payload buffer
                io[1].iov_base = static_cast<char *>(io[1].iov_base) + sent;
                io[1].iov_len -= sent;
            }
        }
    }
    return total_sent;
}

std::expected<size_t, std::string_view>
TcpSocket::send(const std::span<const uint8_t> meta_data,
                std::string_view               payload) const {
    auto buf = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
    return send(meta_data, buf);
}

} // namespace ws
