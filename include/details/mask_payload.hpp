#ifndef MASK_PAYLOAD_HPP_
#define MASK_PAYLOAD_HPP_

#include <arpa/inet.h>
#include <cstdint>
#include <span>

constexpr void mask_payload(std::span<uint8_t> payload, uint32_t mask) {
    // I have seen this pattern before
    // with stb printf when wirting data
    mask = htonl(mask);
    switch (payload.size() % 4) {
        case 1:
            payload[0] ^= mask & 0xFF;
            payload = payload.subspan(1);
            break;
        case 2:
            payload[0] ^= mask & 0xFF;
            payload[1] ^= (mask << 8) & 0xFF;
            payload = payload.subspan(2);
            break;
        case 3:
            payload[0] ^= mask & 0xFF;
            payload[1] ^= (mask << 8) & 0xFF;
            payload[2] ^= (mask << 16) & 0xFF;
            payload = payload.subspan(3);
            break;
    }
    auto tmp = std::span<uint32_t>(reinterpret_cast<uint32_t *>(payload.data()),
                                   payload.size() / 4);
    for (size_t i = 0; i < tmp.size(); i++) {
        tmp[i] ^= mask;
    }
}

#endif
