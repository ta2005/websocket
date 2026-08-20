#ifndef ADVANCE_SPAN_HPP
#define ADVANCE_SPAN_HPP

#include <span>
#include <cstdint>
namespace ws::detail {

constexpr void advance_span(std::span<uint8_t>& buffer1,std::span<uint8_t>& buffer2, size_t step) {
    size_t total_size    = buffer1.size()+buffer2.size();
    int    current_index = 0;
    if (step < total_size) {
        // Adjust the iovec structs for the next partial write
        if (step >= buffer1.size() && current_index == 0) {
            // The metadata buffer is fully sent, move onto the payload
            // buffer
            current_index       = 1;
            size_t payload_sent = step - buffer1.size();
	    buffer2=buffer2.subspan(payload_sent);
        } else if (current_index == 0) {
            // Still working on the metadata buffer
	    buffer1=buffer1.subspan(step);
        } else {
            // Still working on the payload buffer
	    buffer2=buffer2.subspan(step);
        }
    }
}
} // namespace ws::detail

#endif // INCLUDE/home/talel/Programming/Projects/websockadvance_iovecadvance_iovec.hpp_
