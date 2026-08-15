#include "async/event_loop.hpp"
#include "async/socket_ctx.hpp"
#include "async/task.hpp"
#include "async/read_awaitable.hpp"
#include "async/write_awaitable.hpp"

#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/uio.h>
#include <span>

using namespace ws::async;

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Example Coroutine for an individual connected client
task handle_client(int client_fd, EventLoop& loop) {
    // Note: In a real app, SocketCtx should be dynamically allocated or stored securely
    // so it doesn't get destroyed while registered in Epoll.
    auto* ctx = new SocketCtx{client_fd, nullptr, nullptr, 0};
    loop.register_socket(*ctx);

    std::vector<uint8_t> buffer(1024);
    std::cout << "[Client " << client_fd << "] Connected.\n";

    while (true) {
        // 1. Asynchronous Read
        ReadAwaitable reader{*ctx, buffer, loop};
        ssize_t bytes_read = co_await reader;

        if (bytes_read <= 0) {
            std::cout << "[Client " << client_fd << "] Disconnected.\n";
            close(client_fd);
            delete ctx;
            co_return;
        }

        std::cout << "[Client " << client_fd << "] Read " << bytes_read << " bytes.\n";

        // 2. Asynchronous Writev (Echo back)
        std::string meta_str = "ECHO: ";
        std::span<const uint8_t> meta(reinterpret_cast<const uint8_t*>(meta_str.data()), meta_str.size());
        std::span<const uint8_t> payload(buffer.data(), bytes_read);

        iovec io[2];
        io[0].iov_base = const_cast<void*>(static_cast<const void*>(meta.data()));
        io[0].iov_len  = meta.size();
        io[1].iov_base = const_cast<void*>(static_cast<const void*>(payload.data()));
        io[1].iov_len  = payload.size();

        size_t total_to_send = meta.size() + payload.size();
        size_t total_sent = 0;
        int iovcnt = 2;

        while (total_sent < total_to_send) {
            ssize_t sent = ::writev(client_fd, io, iovcnt);
            
            if (sent < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // SUSPEND COROUTINE UNTIL WRITABLE!
                    co_await WaitWritable{*ctx, loop};
                    continue; // Try writing again
                }
                std::cerr << "[Client " << client_fd << "] Write error.\n";
                close(client_fd);
                delete ctx;
                co_return;
            }

            total_sent += sent;

            if (total_sent < total_to_send) {
                // Adjust iovecs for partial write
                if (io[0].iov_len > 0) {
                    if (static_cast<size_t>(sent) >= io[0].iov_len) {
                        sent -= io[0].iov_len;
                        io[0].iov_len = 0;
                        io[1].iov_base = static_cast<char*>(io[1].iov_base) + sent;
                        io[1].iov_len -= sent;
                        io[0] = io[1]; // Shift payload down
                        iovcnt = 1;
                    } else {
                        io[0].iov_base = static_cast<char*>(io[0].iov_base) + sent;
                        io[0].iov_len -= sent;
                    }
                } else {
                    io[0].iov_base = static_cast<char*>(io[0].iov_base) + sent;
                    io[0].iov_len -= sent;
                }
            }
        }
    }
}

int main() {
    EventLoop loop;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8083);

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10000);
    set_nonblocking(server_fd);

    auto* server_ctx = new SocketCtx{server_fd, nullptr, nullptr, 0};
    loop.register_socket(*server_ctx);

    std::cout << "Server listening on port 8083...\n";

    // Create a dummy task to accept connections
    auto accept_task = [&]() -> task {
        while (true) {
            ReadAwaitable waiter{*server_ctx, {}, loop}; 
            co_await waiter; // Wait for incoming connection

            while (true) {
                int client_fd = accept(server_fd, nullptr, nullptr);
                if (client_fd == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                    continue;
                }
                set_nonblocking(client_fd);
                
                // Fire and forget the coroutine!
                handle_client(client_fd, loop); 
            }
        }
    }();

    // Start the event loop
    while (true) {
        loop.run();
    }

    return 0;
}
