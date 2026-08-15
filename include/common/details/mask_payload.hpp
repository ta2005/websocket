#ifndef MASK_PAYLOAD_HPP_
#define MASK_PAYLOAD_HPP_

#include <arpa/inet.h>
#include <cstdint>
#include <span>

constexpr void mask_payload(std::span<uint8_t> payload, uint32_t mask) {
    // I have seen this pattern before
    // with stb printf when wirting data
    mask = htonl(mask);
    union u32_conv {
        int     a;
        uint8_t b[4];
    };
    u32_conv c;
    c.a = mask;
    for (size_t i = 0; i < payload.size(); i++) {
        payload[i] ^= c.b[i % 4];
    }
}

#endif
