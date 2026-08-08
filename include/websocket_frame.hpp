#ifndef WEBSOCKETFRAME_HPP
#define WEBSOCKETFRAME_HPP

#include <span>
#include <cstdint>

struct  WebSocketFrame{
    bool fin;
    uint8_t opcode;
    std::span<uint8_t>payload;
};

#endif  
