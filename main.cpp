#include <arpa/inet.h>
#include <cstdlib>
#include <string.h>
#include <netdb.h>
#include <print>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

void usage() { std::print("USAGE"); }

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc,char ** argv) {
    std::string_view s = "echo.websocket.org";
    std::string_view port="80";
    if(argc>2){
	s=argv[1];
	if(argc==3){
	    port=argv[2];
	}
    }
    addrinfo *res = NULL;
    addrinfo hints;
    memset(&hints, 0,sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int result;
    if ((result = getaddrinfo(s.data(),port.data(), NULL, &res)) != 0) {
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
    freeaddrinfo(res);
    // now I will try to connect 
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
