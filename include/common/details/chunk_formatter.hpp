#ifndef CHUNK_FORMATTER_HPP
#define CHUNK_FORMATTER_HPP

#include <random>
#include <span>
#include "common/opcode.hpp"
#include "common/details/prepare_meta.hpp"
#include "common/details/mask_payload.hpp"

namespace ws::detail {
struct ChunkFormatter {
    static constexpr int chunk_size = 8 * 1024;
    std::span<const uint8_t> remaining;
    opcode                   first_op;
    std::mt19937&            rng;
    bool                     is_first = true;
    std::array<uint8_t, chunk_size> tmp_buf;
    bool has_next(){
	return !remaining.empty() || is_first;
    }
    auto next(){
	bool is_final=remaining.size()<=chunk_size;
	size_t chunk_len = is_final?remaining.size():chunk_size;
	opcode op = is_first?first_op:opcode::continuation;
	is_first=false;
	uint32_t mask=rng();
        auto     meta = format_meta(is_final, true, op, chunk_len, mask);
	std::copy(remaining.begin(), remaining.begin() + chunk_len, tmp_buf.begin());
        auto payload_span = std::span<uint8_t>{tmp_buf.data(), chunk_len};
        mask_payload(payload_span, mask);

        remaining = remaining.subspan(chunk_len);

        return std::make_pair(meta, std::span<const uint8_t>{payload_span});
    }
};

} // namespace ws::detail

#endif
