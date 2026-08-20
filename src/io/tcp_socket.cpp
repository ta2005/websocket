#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "common/details/advance_iovec.hpp"
#include "common/logger.hpp"
#include "io/tcp_socket.hpp"

namespace ws {
TcpSocket::~TcpSocket() {
    if (m_fd != -1) {
        close();
        m_fd = -1;
    }
}
std::expected<TcpSocket, Error>
TcpSocket::connect(const std::string_view host, const std::string_view port) {
    addrinfo *res = NULL;
    addrinfo  hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (int result = getaddrinfo(host.data(), port.data(), &hints, &res);
        result != 0) {
        ws::log::error("getaddrinfo failed: {}", gai_strerror(result));
        return std::unexpected(Error::ConnectionClosed);
    }
    int socketfd = -1;
    for (addrinfo *p = res; p != NULL; p = (p)->ai_next) {
        if ((socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
            -1) {
            continue;
        }
        if (::connect(socketfd, p->ai_addr, p->ai_addrlen) == -1) {
            ws::log::error("Failed to connect to {}:{} using protocol {}", host,
                           port, p->ai_protocol);
            ::close(socketfd);
            socketfd = -1;
            continue;
        }
        ws::log::info("Successfully connected to {}:{}", host, port);
        break;
    }
    freeaddrinfo(res);
    return TcpSocket(socketfd);
}
// the move opertors
TcpSocket::TcpSocket(TcpSocket &&other) : m_fd(other.m_fd) { other.m_fd = -1; }
TcpSocket &TcpSocket::operator=(TcpSocket &&other) {
    if (this != &other) {
        if (m_fd != -1)
            close();
        m_fd       = other.m_fd;
        other.m_fd = -1;
    }
    return *this;
}

std::expected<size_t, Error> TcpSocket::read(std::span<uint8_t> buf) const {
    while (true) {
        ssize_t bytes_read = ::read(m_fd, buf.data(), buf.size());
        if (bytes_read < 0) {
            if (errno == EINTR)
                continue; // Interrupted by signal, try again
            ws::log::error("TcpSocket::read error: {}", std::strerror(errno));
            return std::unexpected(Error::ReadFailed);
        }
        if (bytes_read == 0) {
            return std::unexpected(Error::ConnectionClosed);
        }
        return static_cast<size_t>(bytes_read);
    }
}

// Question: do i really need the const uint8_t here ?
// Answer: You don't need `const` on the span itself (i.e. `const std::span`),
// because a span is just a lightweight view (pointer + size).
// Passing `std::span<const uint8_t>` by value is the standard C++ way!
std::expected<size_t, Error>
TcpSocket::send(std::span<const uint8_t> meta_data,
                std::span<const uint8_t> payload) const {
    size_t               total_sent = 0;
    size_t               total_size = meta_data.size() + payload.size();
    std::array<iovec, 2> io         = {{

        {.iov_base =
             const_cast<void *>(static_cast<const void *>(meta_data.data())),
         .iov_len = meta_data.size()},

        {.iov_base =
             const_cast<void *>(static_cast<const void *>(payload.data())),
         .iov_len = payload.size()},
    }};

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
            return std::unexpected(Error::WriteFailed);
        }
        total_sent += sent;
        detail::advance_iovec(io, total_sent);
    }
    return total_sent;
}

std::expected<size_t, Error>
TcpSocket::send(const std::span<const uint8_t> meta_data,
                std::string_view               payload) const {
    auto buf = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
    return send(meta_data, buf);
}

} // namespace ws
