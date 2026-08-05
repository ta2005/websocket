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
};
}

#endif
