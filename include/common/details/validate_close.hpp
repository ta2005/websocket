#ifndef PARSE_CLOSE_CPP
#define PARSE_CLOSE_CPP

#include "common/details/parse_meta.hpp"
#include "common/error.hpp"
#include "common/status_code.hpp"
#include "simdutf.h"
#include <cassert>
#include <cstdint>
#include <expected>
#include <span>

namespace ws::detail {
constexpr std::expected<void, Error>
validate_close_frame(PayloadMetaData meta, std::span<uint8_t> payload) {
    assert(meta.len == payload.size());
    if (meta.len > 125) {
        return std::unexpected(Error::InvalidPayloadLength);
    }
    if (meta.len == 0) {
        return {};
    } else if (meta.len == 1) {
        return std::unexpected(Error::ProtocolError);
    } else {
        int status = payload[0] << 8 | payload[1];
        if (invalid(status) || reserved(status)) {
            return std::unexpected(Error::ProtocolError);
        }
        const char *reason_ptr =
            reinterpret_cast<const char *>(payload.data() + 2);
        size_t reason_len = meta.len - 2;
        if (!simdutf::validate_utf8(reason_ptr, reason_len)) {
            return std::unexpected(Error::InvalidUTF8);
        }
    }
    return {};
}
} // namespace ws::detail

#endif // INCLUDE/home/talel/Programming/Projects/websock/include/detailsparse_closeparse_close.cpp_
