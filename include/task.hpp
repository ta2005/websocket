#ifndef TASK_HPP
#define TASK_HPP

#include <coroutine>
#include <utility>

namespace ws {
struct task {
    struct promise_type {
        void unhandled_exception() {}
        void return_void() {} // Required when coroutine ends

        std::suspend_never initial_suspend() noexcept { return {}; }

        task get_return_object() {
            return task(
                std::coroutine_handle<promise_type>::from_promise(*this));
        }

        std::suspend_always final_suspend() noexcept { return {}; }
    };

    using Handle = std::coroutine_handle<promise_type>;
    Handle h_;

    explicit task(Handle h) : h_(h) {}

    ~task() {
        if (h_)
            h_.destroy();
    }

    task(const task &)            = delete;
    task &operator=(const task &) = delete;

    task(task &&o) noexcept : h_(std::exchange(o.h_, nullptr)) {}
    task &operator=(task &&o) noexcept {
        if (this != &o) {
            if (h_)
                h_.destroy();
            h_ = std::exchange(o.h_, nullptr);
        }
        return *this;
    }
};

} // namespace ws
#endif
