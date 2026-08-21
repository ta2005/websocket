#ifndef TASK_HPP
#define TASK_HPP

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

namespace ws {

template <typename T> class Task {
  public:
    struct promise_type;
    using Handle = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::optional<T>        value;
        std::exception_ptr      exception;
        std::coroutine_handle<> continuation;

        Task get_return_object() noexcept {
            return Task{Handle::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(Handle h) noexcept {
                return h.promise().continuation ? h.promise().continuation
                                                : std::noop_coroutine();
            }
            void await_resume() const noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }

        void return_value(T v) noexcept { value = std::move(v); }
        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    Task() noexcept = default;
    explicit Task(Handle h) noexcept : handle_(h) {}
    ~Task() {
        if (handle_)
            handle_.destroy();
    }

    Task(const Task &)            = delete;
    Task &operator=(const Task &) = delete;

    Task(Task &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}
    Task &operator=(Task &&other) noexcept {
        if (this != &other) {
            if (handle_)
                handle_.destroy();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    // --- AWAITABLE INTERFACE ---
    bool await_ready() const noexcept { return !handle_ || handle_.done(); }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> caller) noexcept {
        handle_.promise().continuation = caller;
        return handle_; // Symmetric transfer: suspends caller and resumes this
                        // task immediately!
    }

    T await_resume() {
        if (handle_.promise().exception)
            std::rethrow_exception(handle_.promise().exception);
        return std::move(*handle_.promise().value);
    }

    bool done() const noexcept { return !handle_ || handle_.done(); }

  private:
    Handle handle_{};
};

} // namespace ws
#endif // INCLUDE/home/talel/Programming/Projects/websock/includetasktask.hpp_
