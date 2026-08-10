#ifndef OPCODE_HPP
#define OPCODE_HPP

#include <cstdint>

namespace ws {
enum class opcode : uint8_t {
    continuation = 0x0,
    text         = 0x1,
    binary       = 0x2,
    close        = 0x8,
    ping         = 0x9,
    pong         = 0xA,
    unsupported  = 0xFF
};

constexpr opcode get_opcode(uint8_t code) {
    switch (code) {
        case 0x0:
            return opcode::continuation;
        case 0x1:
            return opcode::text;
        case 0x2:
            return opcode::binary;
        case 0x8:
            return opcode::close;
        case 0x9:
            return opcode::ping;
        case 0xA:
            return opcode::pong;
        default:
            return opcode::unsupported;
    }
}

} // namespace ws

#endif
