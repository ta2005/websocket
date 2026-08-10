#ifndef PARSE_META_HPP
#define PARSE_META_HPP

#include "opcode.hpp"
#include <cstdint>
#include <expected>
#include <netinet/in.h>
#include <span>
#include <string_view>

namespace ws::detail {
struct PayloadMetaData {
    uint64_t len;
    uint32_t mask;
    uint8_t  meta_size;
    bool     fin;
    bool     is_masked;
    opcode   op;
};

// this func i kind of useful
constexpr std::expected<std::span<uint8_t>, std::string_view>
advance(std::span<uint8_t> &payload, size_t step = 1) {
    if (payload.size() < step) {
        return std::unexpected("payload too short");
    }
    auto ret = payload.subspan(0, step);
    payload  = payload.subspan(step);
    return ret;
}

// yeah now i get it why i should have done
// the error type a and error code not a string view
// it allows me to easily change the error message
// and then get a get_String
// maybe i could have like some standard log(like the one in systemd
// next release ???
constexpr std::expected<PayloadMetaData, std::string_view>
parse_meta(std::span<uint8_t> payload) {
    PayloadMetaData res;
    res.meta_size = 2;
    auto meta     = ws::detail::advance(payload, 2);
    if (!meta) {
        return std::unexpected(meta.error());
    }
    res.fin = ((*meta)[0]) & 0x80;
    if (((*meta)[0] & 0x70) != 0) {
        return std::unexpected("unspported extenstion");
    }
    auto op       = get_opcode(((*meta)[0]) * 0x0F);
    res.op        = op;
    res.is_masked = ((*meta)[1]) & 0x80;
    res.len       = ((*meta)[1]) & 0x7F;
    if (res.len == 126) {
        auto len_span = ws::detail::advance(payload, 2);
        if (!len_span)
            return std::unexpected(len_span.error());
        res.len = (static_cast<uint16_t>((*len_span)[0]) << 8) |
                  static_cast<uint16_t>((*len_span)[1]);

        res.meta_size += 2;
        if (res.len < 126) {
            return std::unexpected("inappropirate len");
        }
    } else if (res.len == 127) {
        auto len_span = ws::detail::advance(payload, 8);
        if (!len_span) {
            return std::unexpected(len_span.error());
        }
        res.len = (static_cast<uint64_t>((*len_span)[0]) << 56) |
                  (static_cast<uint64_t>((*len_span)[1]) << 48) |
                  (static_cast<uint64_t>((*len_span)[2]) << 40) |
                  (static_cast<uint64_t>((*len_span)[3]) << 32) |
                  (static_cast<uint64_t>((*len_span)[4]) << 24) |
                  (static_cast<uint64_t>((*len_span)[5]) << 16) |
                  (static_cast<uint64_t>((*len_span)[6]) << 8) |
                  static_cast<uint64_t>((*len_span)[7]);

        if (res.len <= 0xFFFF) {
            return std::unexpected("inappropirate len");
        }
        res.meta_size += 8;
    }
    if (res.is_masked) {

        auto mask_span = ws::detail::advance(payload, 4);
        if (!mask_span) {
            return std::unexpected(meta.error());
        }
        res.mask = (static_cast<uint32_t>((*mask_span)[0]) << 24) |
                   (static_cast<uint32_t>((*mask_span)[1]) << 16) |
                   (static_cast<uint32_t>((*mask_span)[2]) << 8) |
                   static_cast<uint32_t>((*mask_span)[3]);

        // i don't know whether i should reverse it or not
        res.meta_size += 4;
    }
    return res;
}

} // namespace ws::detail

#endif // INCLUDE/home/talel/Programming/Projects/websock/include/detailsparse_headerparse_header.hpp_
