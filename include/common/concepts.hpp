#ifndef WS_CONCEPTS_HPP
#define WS_CONCEPTS_HPP

#include "common/error.hpp"
#include "task.hpp"
#include <coroutine>
#include <cstdint>
#include <expected>
#include <span>

namespace ws {

template <typename T>
concept Awaitable = requires(T t, std::coroutine_handle<> h) {
    { t.await_ready() } -> std::same_as<bool>;
    t.await_suspend(h);
    t.await_resume();
};
template <typename T>
concept isAsyncSocket = requires(T t, std::span<uint8_t> buffer) {
    { t.read(buffer) } -> Awaitable;
};

template <typename T>
concept IsSocket =
    requires(T t, std::span<uint8_t> mut_buf,
             std::span<const uint8_t> const_buf) {
        // Must have read and write methods
        t.read(mut_buf);
        t.write(const_buf, const_buf);

        // Must have a close method
        { t.close() } -> std::same_as<void>;
    } &&
    std::movable<T> && !std::copy_constructible<T>;

template <typename socket>
    requires(!isAsyncSocket<socket>)
std::expected<size_t, Error> read(socket &s, std::span<uint8_t> buffer) {
    return s.read(buffer);
}
template <typename socket>
    requires(isAsyncSocket<socket>)
auto read(socket &s, std::span<uint8_t> buffer) {
    s.read(buffer);
}
} // namespace ws

#endif
