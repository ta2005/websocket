#include <print>
#include <string.h>
#include <unistd.h>

#include "handshake.hpp"
#include "opcode.hpp"
#include "tcp_socket.hpp"

void usage() { std::print("USAGE"); }

// i need to disable implicit conversion like wtf
void send_close(const ws::TcpSocket &socket) {
    uint8_t payload[6] = {0};
    payload[0]         = 0x80 | static_cast<uint8_t>(ws::opcode::close);
    payload[1]         = 0x80;
    auto out           = socket.send(payload);
    if (!out) {
        std::print("{}", out.error());
    }
}

// this one should be used with std::expected
// i will read more about them in the core guidline and learncpp
void send_message(const ws::TcpSocket &socket) {
    const std::string_view msg = "Hello World my name is Talel";
    // I will be sending 32bit then mask 16 bit then my msg + 28 = 40
    uint8_t payload[34] = {0};
    memset((void *)payload, 0, sizeof payload);
    payload[0] = 0x1 | 0x80;
    payload[1] |= 0x80;
    uint8_t l = (uint8_t)msg.size();
    payload[1] |= l | 0x80;
    // a mask of zero does nothing
    memcpy(&payload[6], msg.data(), 28);
    auto out = socket.send(payload);
    if (!out) {
        std::print("{}", out.error());
    }
}

int main(int argc, char **argv) {
    std::string_view s    = "localhost";
    std::string_view port = "8765";
    if (argc > 2) {
        s = argv[1];
        if (argc == 3) {
            port = argv[2];
        }
    }
    auto socket = ws::TcpSocket::connect(s, port);
    if (!socket) {
        std::print("{}", socket.error());
        return 1;
    }

    auto hc=ws::perform_handshake(*socket, s, "/");
    if(!hc){
        std::print("{}", hc.error());
        return 1;
    }
    // send_message(*socket);
    // std::print("Server Response:\n{}\n", socket->read(1024));
    //
    // // of coure the seq from the srv is valid
    // //
    sleep(2);
    send_close(*socket);

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
