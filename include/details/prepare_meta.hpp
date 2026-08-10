// this  is a helper furciton that format a given the metadata
#ifndef PREPARE_META_HPP
#define PREPARE_META_HPP

#include "opcode.hpp"
#include <assert.h>
#include <cstring>
#include <span>

namespace ws::detail {

struct FrameHeader {
    std::array<uint8_t, 14> data;
    size_t                  size;

    // A convenient helper to return a span pointing only to the valid bytes
    std::span<const uint8_t> span() const { return {data.data(), size}; }
};

constexpr FrameHeader format_meta(bool fin, bool is_masked, opcode op,
                                  uint64_t len, uint32_t mask) {
    FrameHeader meta{};

    // FIN is the 8th bit (0x80). Opcode is the bottom 4 bits (0x0F).
    meta.data[0] = (fin ? 0x80 : 0x00) | (static_cast<uint8_t>(op) & 0x0F);

    // Mask flag is the 8th bit (0x80) of the second byte
    uint8_t mask_bit = is_masked ? 0x80 : 0x00;

    int mask_pos = 2;
    if (len <= 125) {
        meta.data[1] = mask_bit | static_cast<uint8_t>(len);
        meta.size    = 2;
    } else if (len <= 0xFFFF) {
        meta.data[1] = mask_bit | 126;
        // WebSocket requires Big Endian (Network Byte Order)
        meta.data[2] = (len >> 8) & 0xFF;
        meta.data[3] = len & 0xFF;
        meta.size    = 4;
        mask_pos     = 4;
    } else {
        meta.data[1] = mask_bit | 127;
        // 64-bit length in Big Endian
        for (int i = 0; i < 8; i++) {
            meta.data[2 + i] = (len >> ((7 - i) * 8)) & 0xFF;
        }
        meta.size = 10;
        mask_pos  = 10;
    }

    if (is_masked) {
        // Mask is 4 bytes. We write it sequentially.
        meta.data[mask_pos]     = (mask >> 24) & 0xFF;
        meta.data[mask_pos + 1] = (mask >> 16) & 0xFF;
        meta.data[mask_pos + 2] = (mask >> 8) & 0xFF;
        meta.data[mask_pos + 3] = mask & 0xFF;
        meta.size += 4;
    }

    return meta;
}

} // namespace ws::detail

#endif
