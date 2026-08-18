#ifndef ADVANCE_IOVEC_HPP
#define ADVANCE_IOVEC_HPP

#include <span>
#include <sys/uio.h>
namespace ws::detail {

constexpr void advance_iovec(std::span<iovec, 2> io, size_t step) {
    size_t total_size    = io[0].iov_len + io[1].iov_len;
    int    current_index = 0;

    if (step < total_size) {
        // Adjust the iovec structs for the next partial write
        if (step >= io[0].iov_len && current_index == 0) {
            // The metadata buffer is fully sent, move onto the payload
            // buffer
            current_index       = 1;
            size_t payload_sent = step - io[0].iov_len;
            io[1].iov_base = static_cast<char *>(io[1].iov_base) + payload_sent;
            io[1].iov_len -= payload_sent;
        } else if (current_index == 0) {
            // Still working on the metadata buffer
            io[0].iov_base = static_cast<char *>(io[0].iov_base) + step;
            io[0].iov_len -= step;
        } else {
            // Still working on the payload buffer
            io[1].iov_base = static_cast<char *>(io[1].iov_base) + step;
            io[1].iov_len -= step;
        }
    }
}
} // namespace ws::detail

#endif // INCLUDE/home/talel/Programming/Projects/websockadvance_iovecadvance_iovec.hpp_
