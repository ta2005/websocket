#include "async/server.hpp"
#include "async/client.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdexcept>
#include <iostream>

namespace ws::async {

static int create_listener(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd == -1) throw std::runtime_error("socket failed");

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        throw std::runtime_error("bind failed");
    }
    if (::listen(fd, 10000) == -1) {
        throw std::runtime_error("listen failed");
    }
    return fd;
}

Server::Server(uint16_t port, EventLoop& loop) 
    : m_loop(loop), m_listener(create_listener(port), loop) 
{
    // Register the listener socket with the event loop
    m_loop.register_socket(m_listener);
}

task Server::run_accept_loop() {
    std::cout << "[Server] Starting accept loop...\n";
    while (true) {
        // Co_await pauses this coroutine until a new connection arrives
        TcpSocket client_sock = co_await m_listener.accept();
        
        std::cout << "[Server] Accepted new connection (FD: " << client_sock.get_fd() << ")\n";

        // Register the new client socket with the event loop
        m_loop.register_socket(client_sock);

        // Dynamically allocate a client session and fire-and-forget its run_session
        // Note: In production, store this in a std::unique_ptr inside a manager class
        auto* client = new Client(std::move(client_sock));
        client->run_session(); 
    }
}

} // namespace ws::async
