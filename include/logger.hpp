#ifndef WS_LOGGER_HPP
#define WS_LOGGER_HPP

#include <print>
#include <utility>

namespace ws::log {

// A simple compile-time toggle for debugging
#ifndef WS_DEBUG
#define WS_DEBUG 1
#endif

// Using C++23 std::print and std::println
template <typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
    if constexpr (WS_DEBUG) {
        std::print("[DEBUG] ");
        std::println(fmt, std::forward<Args>(args)...);
    }
}

template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    std::print("[INFO]  ");
    std::println(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    std::print(stderr, "[ERROR] ");
    std::println(stderr, fmt, std::forward<Args>(args)...);
}

} // namespace ws::log

#endif
