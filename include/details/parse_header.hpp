#ifndef PARSE_HEADER_HPP
#define PARSE_HEADER_HPP

#include "opcode.hpp"
#include <cstdint>
#include <expected>>

namespace ws::detail {
struct PayloadMetaData {
    bool     fin;
    bool     is_masked;
    opcode   op;
    uint64_t len;
    uint32_t mask;
};

// yeah now i get it
constexpr std::expected<PayloadMetaData, std::string_view>

} // namespace ws::detail

#endif // INCLUDE/home/talel/Programming/Projects/websock/include/detailsparse_headerparse_header.hpp_
