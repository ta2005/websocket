#include "client.hpp"
// #include "io/tcp_socket.hpp"
#include "io/async/tcp_socket.hpp"
#include "session.hpp"
#include "task.hpp"
#include <iostream>
#include <string_view>

using namespace ws;

int main() {
    std::cout << "=========================================\n";
    std::cout << "   WebSocket Client Test (Synchronous)\n";
    std::cout << "=========================================\n\n";

    std::string_view host = "127.0.0.1";
    std::string_view port = "8765";

    std::cout << "[1] Connecting to " << host << ":" << port << " ...\n";

    // Connect the raw TCP socket
    auto sock_res = TcpSocket::connect(host, port);
    if (!sock_res) {
        std::cerr << "[-] Failed to connect TCP socket. Is the Python server"
                     "running?\n";
        return 1;
    }
    std::cout << "[+] TCP Connected!\n";

    std::cout << "[2] Performing WebSocket Handshake ...\n";
    // Upgrade to WebSocket
    auto client_res =
        Client<TcpSocket>::create(std::move(*sock_res), host, "/", port);
    if (!client_res) {
        std::cerr << "[-] Handshake failed! Error code: "
                  << static_cast<int>(client_res.error()) << "\n";
        return 1;
    }
    std::cout << "[+] Handshake successful! We are speaking WebSocket.\n\n";

    auto &client = *client_res;

    // Send a message
    std::string_view msg = "Hello from C++ Client!";
    std::cout << "[3] Sending payload: \"" << msg << "\"\n";

    auto send_res = client.send(msg);
    if (!send_res) {
        std::cerr << "[-] Failed to send message.\n";
        return 1;
    }
    std::cout << "[+] Message sent successfully.\n\n";

    // Wait for the echo response
    std::cout << "[4] Waiting for response from server ...\n";
    auto chunk = client.read_message();
    if (!chunk) {
        std::cerr << "[-] Failed to read chunk from server.\n";
        return 1;
    }

    std::string_view received_payload(
        reinterpret_cast<const char *>(chunk->payload.data()),
        chunk->payload.size());

    std::cout << "[+] Server replied: \"" << received_payload << "\"\n";
    std::cout << "[+] Frame FIN: " << std::boolalpha << chunk->is_fin <<
    "\n";
    // client.close();

    std::cout << "\n=========================================\n";
    std::cout << "   Test Completed Successfully!\n";
    std::cout << "=========================================\n";

    return 0;
}
