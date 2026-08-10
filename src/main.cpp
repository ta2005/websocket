#include <print>
#include <string.h>
#include <string_view>
#include <unistd.h>

#include "client.hpp"
#include "logger.hpp"
#include "opcode.hpp"

int main(int argc, char **argv) {
    std::string_view s    = "localhost";
    std::string_view port = "8765";

    if (argc > 2) {
        s = argv[1];
        if (argc == 3)
            port = argv[2];
    }

    auto client_res = ws::Client::create(s, "/", port);
    if (!client_res) {
        std::print("Failed to connect/handshake: {}\n", client_res.error());
        return 1;
    }
    ws::Client client = std::move(*client_res);

    ws::log::info("Sending message...");
    auto send_res = client.send("Hello world my name is Talel Zigni");
    if (!send_res) {
        std::print("Send failed: {}\n", send_res.error());
        return 1;
    }

    ws::log::info("Waiting for server response...");
    auto chunk_res = client.read_chunk();
    if (!chunk_res) {
        std::print("Read failed: {}\n", chunk_res.error());
        return 1;
    }

    ws::ChunckView view = *chunk_res;

    std::print("\n=== RECEIVED FRAME ===\n");
    std::print("Opcode: {}\n", static_cast<int>(view.type));
    std::print("FIN bit: {}\n", view.is_fin);
    std::print("Payload Length: {} bytes\n", view.payload.size());

    // Print the payload as a string
    std::string_view payload_str(
        reinterpret_cast<const char *>(view.payload.data()),
        view.payload.size());
    std::print("Payload Data: {}\n", payload_str);
    std::print("======================\n\n");

    if (!client.close()) {
        return 1;
    }
    return 0;
}
/*curl --include \
     --no-buffer \
     --header "Connection: Upgrade" \
     --header "Upgrade: websocket" \
     --header "Host: echo.websocket.org" \
     --header "Origin: https://www.websocket.org" \
     --header "Sec-WebSocket-Key: SGVsbG8sIHdvcmxkIQ==" \
     --header "Sec-WebSocket-Version: 13" \
     https://echo.websocket.org
    */
