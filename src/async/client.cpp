#include "async/client.hpp"
#include <iostream>
#include <vector>

namespace ws::async {

Client::Client(TcpSocket socket) : m_socket(std::move(socket)) {}

task Client::run_session() {
    std::cout << "[Client " << m_socket.get_fd() << "] Starting session...\n";
    std::vector<uint8_t> buffer(4096);

    while (true) {
        // 1. Await incoming data
        auto bytes_read = co_await m_socket.read_some(buffer);

        if (bytes_read <= 0) {
            std::cout << "[Client " << m_socket.get_fd() << "] Disconnected.\n";
            // The TcpSocket destructor will automatically close the FD
            delete this; // Clean up (if we were new'd by the server)
            co_return;
        }

        std::cout << "[Client " << m_socket.get_fd() << "] Received " << bytes_read << " bytes.\n";

        // 2. Here you will add your Autobahn parsing logic!
        // parse_header();
        // parse_payload();
        // send_pong();
    }
}

task Client::send_text(std::string_view text) {
    // Note: Since you're using write_some inside a coroutine loop,
    // you would prepare the WebSocket headers and then write them:
    // 
    // std::span<uint8_t> meta = ...;
    // std::span<uint8_t> payload = ...;
    //
    // while (total_sent < total_to_send) {
    //     ssize_t sent = co_await m_socket.write_some(meta, payload);
    //     // shift buffers...
    // }
    co_return;
}

task Client::send_binary(std::span<const uint8_t> data) {
    co_return;
}

} // namespace ws::async
