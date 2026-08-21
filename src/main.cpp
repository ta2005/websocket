#include "client.hpp"
#include "io/tcp_socket.hpp"
#include "task.hpp"
#include <iostream>

// using namespace ws;

// --- ASYNC TEST ---
// Task<void> run_async_client(async::EventLoop& loop) {
//     std::cout << "[ASYNC] Connecting to ws://echo.websocket.events/ ...\n";
//
//     // We would resolve the DNS and connect the TCP socket.
//     // For this test, we assume the user has a connect function, or we can
//     just mock it.
//     // We'll mock the fd with 0 just to see if the code compiles.
//     int dummy_fd = 0;
//     async::TcpSocket socket(dummy_fd, loop);
//
//     auto client_res = co_await
//     Client<async::TcpSocket>::create(std::move(socket),
//     "echo.websocket.events", "/"); if (!client_res) {
//         std::cout << "[ASYNC] Failed to create client: " <<
//         static_cast<int>(client_res.error()) << "\n"; co_return;
//     }
//
//     auto& client = *client_res;
//
//     auto send_res = co_await client.send("Hello from async websockets!");
//     if (!send_res) {
//         std::cout << "[ASYNC] Failed to send: " <<
//         static_cast<int>(send_res.error()) << "\n"; co_return;
//     }
//
//     auto chunk = co_await client.read_chunk();
//     if (!chunk) {
//         std::cout << "[ASYNC] Failed to read chunk: " <<
//         static_cast<int>(chunk.error()) << "\n"; co_return;
//     }
//
//     std::cout << "[ASYNC] Read " << chunk->payload.size() << " bytes.\n";
//     co_return;
// }

// --- SYNC TEST ---
void run_sync_client() {
    std::cout << "[SYNC] Connecting to ws://echo.websocket.events/ ...\n";

    // Again, assume the socket is created and connected
    auto socket_res = ws::TcpSocket::connect("echo.websocket.events", "80");
    if (!socket_res) {
        std::cout << "[SYNC] Connection failed\n";
        return;
    }

    auto client_res = ws::Client<ws::TcpSocket>::create(
        std::move(*socket_res), "echo.websocket.events", "/");
    if (!client_res) {
        std::cout << "[SYNC] Failed to create client: "
                  << static_cast<int>(client_res.error()) << "\n";
        return;
    }

    auto &client = *client_res;

    auto send_res = client.send("Hello from sync websockets!");
    if (!send_res) {
        std::cout << "[SYNC] Failed to send: "
                  << static_cast<int>(send_res.error()) << "\n";
        return;
    }

    auto chunk = client.read_chunk();
    if (!chunk) {
        std::cout << "[SYNC] Failed to read chunk: "
                  << static_cast<int>(chunk.error()) << "\n";
        return;
    }

    std::cout << "[SYNC] Read " << chunk->payload.size() << " bytes.\n";
}

int main() {
    // Test the sync path
    std::cout << "Hello world";

    // Test the async path
    // async::EventLoop loop;
    // auto task = run_async_client(loop);
    // loop.run();

    return 0;
}
