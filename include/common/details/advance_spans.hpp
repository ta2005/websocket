#ifndef ADVANCE_SPAN_HPP
#define ADVANCE_SPAN_HPP

#include <cstdint>
#include <span>
namespace ws::detail {

template <typename T>
constexpr void advance_span(std::span<T> &buffer1, std::span<T> &buffer2,
                            size_t step) {
    if (step >= buffer1.size() + buffer2.size()) {
        buffer1 = {};
        buffer2 = {};
        return;
    }

    if (step >= buffer1.size()) {
        step -= buffer1.size();
        buffer1 = {}; // CLEAR IT
        buffer2 = buffer2.subspan(step);
    } else {
        buffer1 = buffer1.subspan(step);
    }
}
} // namespace ws::detail

#endif // INCLUDE/home/talel/Programming/Projects/websockadvance_iovecadvance_iovec.hpp_
