#ifndef WEBSOCKETFRAME_HPP
#define WEBSOCKETFRAME_HPP

#include <vector>
#include <cstdint>

struct  WebSocketFrame{
    bool fin;
    uint8_t opcode;
    std::vector<uint8_t>payload;
};

#endif  
