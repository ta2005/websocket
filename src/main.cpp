#include <arpa/inet.h>
#include <cstdlib>
#include <netdb.h>
#include <print>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "opcode.hpp"

void usage() { std::print("USAGE"); }

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void send_close(int socketfd){
    uint8_t payload[6]={0};
    payload[0]=0x80|static_cast<uint8_t>(ws::opcode::close);
    payload[1]=0x80;
    write(socketfd,payload,6);
}


// this one should be used with std::expected 
// i will read more about them in the core guidline and learncpp
void send_message(int sockefd){
    const std::string_view msg="Hello World my name is Talel";
    // I will be sending 32bit then mask 16 bit then my msg + 28 = 40
    uint8_t payload[34]={0};
    memset((void *)payload,0,sizeof payload);
    payload[0]=0x1|0x80;
    payload[1]|=0x80;
    uint8_t l = (uint8_t)msg.size();
    payload[1]|=l|0x80;
    // a mask of zero does nothing
    memcpy(&payload[6],msg.data(),28);
    write(sockefd,payload,34);
}


int main(int argc, char **argv) {
    std::string_view s = "localhost";
    std::string_view port = "8765";
    if (argc > 2) {
        s = argv[1];
        if (argc == 3) {
            port = argv[2];
        }
    }
    addrinfo *res = NULL;
    addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int result;
    if ((result = getaddrinfo(s.data(), port.data(), &hints, &res)) != 0) {
        std::print("{}", gai_strerror(result));
    }
    int socketfd;
    for (addrinfo *p = res; p != NULL; p = (p)->ai_next) {
        if ((socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
            -1) {
            continue;
        }
        char host[INET6_ADDRSTRLEN];
        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
                  host, sizeof host);
        std::print("\033[32mconnection to {}\033[0m\n", host);
        if (connect(socketfd, p->ai_addr, p->ai_addrlen) == -1) {
            std::print("\033[31mfailed to connect to {}\033[0m\n", host);
            close(socketfd);
            continue;
        }
        break;
    }

    const char *message = (char *)"GET / HTTP/1.1\r\n"
                            "Connection: Upgrade\r\n"
                            "Upgrade: websocket\r\n"
                            "Host: echo.websocket.org\r\n"
                            "Origin: https://www.websocket.org\r\n"
			    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" 
                            "Sec-WebSocket-Version: 13\r\n"
			    "\r\n";
    //
    write(socketfd, message, strlen(message));
    std::print("Handshake sent. Waiting for response...\n\n");

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    
    int bytes_read = read(socketfd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        std::print("Server Response:\n{}\n", buffer);
    } else {
        std::print("Failed to read response from server.\n");
    }
    send_message(socketfd);

    // of coure the seq from the srv is valid 
    // 
    memset(buffer, 0, sizeof(buffer));
    bytes_read = read(socketfd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        std::print("Server Echoed Back (Raw Bytes): ");
        for (int i = 0; i < bytes_read; ++i) {
            std::print("{:02x} ", static_cast<uint8_t>(buffer[i]));
        }
        std::print("\n");
        
        // Skip the 2-byte unmasked server header and print text
        std::print("Decoded Text: {}\n", &buffer[2]);
    }
    sleep(2);
    send_close(socketfd);

    close(socketfd);
    freeaddrinfo(res);
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
