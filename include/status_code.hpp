#ifndef STATUS_CODE_HPP
#define STATUS_CODE_HPP

#include <cstdint>

enum class status_code : uint16_t {
    normal_closure = 1000,
    going_away     = 1001,

    protocol_error  = 1002,
    unknown_data    = 1003,
    reserved1       = 1004,
    no_status       = 1005,
    abnormal        = 1006,
    bad_payload     = 1007,
    policy_error    = 1008,
    payload_too_big = 1009,

    needs_extention = 1010,

    internal_error = 1011,

    reserved2 = 1015,
};

inline constexpr int rsv_start = 1016;
inline constexpr int rsv_end   = 2999;
inline bool          reserved(int code) {
    return ((code >= rsv_start && code <= rsv_end) ||
            code == static_cast<int>(status_code::reserved1) ||
            code == static_cast<int>(status_code::reserved2));
}

inline constexpr int invalid_low  = 999;
inline constexpr int invalid_high = 5000;
inline bool          invalid(int code) {
    return (code <= invalid_low || code >= invalid_high ||
            code == static_cast<int>(status_code::no_status) ||
            code == static_cast<int>(status_code::abnormal));
}

#endif // INCLUDE/home/talel/Programming/Projects/websock/includestatus_codestatus_code.hpp_
