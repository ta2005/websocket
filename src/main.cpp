#include <print>
#include <unistd.h>

#include "client.hpp"

void usage() { std::print("USAGE"); }

// i need to disable implicit conversion like wtf
// this one should be used with std::expected
// i will read more about them in the core guidline and learncpp


int main(int argc, char **argv) {
    std::string_view s    = "localhost";
    std::string_view port = "8765";
    if (argc > 2) {
        s = argv[1];
        if (argc == 3) {
            port = argv[2];
        }
    }
    auto cl = ws::Client::create(s, "/", port);
    if(!cl){
	return 1;
    }
    auto res=cl->send("Hello world my name is Talel Zigni");
    if(!res){
	return 1;
    }
    res=cl->close();


    // send_message(*socket);
    // std::print("Server Response:\n{}\n", socket->read(1024));
    //
    // // of coure the seq from the srv is valid
    // //
    sleep(2);

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
