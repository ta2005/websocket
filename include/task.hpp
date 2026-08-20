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
        std::optional<T>   value;
        std::exception_ptr exception;

        Task get_return_object() noexcept {
            return Task{Handle::from_promise(*this)};
        }

        // Don't automatically run the coroutine.
        std::suspend_always initial_suspend() noexcept { return {}; }

        // The coroutine stays alive until the Task destroys it.
        std::suspend_always final_suspend() noexcept { return {}; }

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

    bool done() const noexcept { return !handle_ || handle_.done(); }

    void resume() {
        if (handle_ && !handle_.done())
            handle_.resume();
    }

    T &result() {
        if (handle_ && handle_.promise().exception)
            std::rethrow_exception(handle_.promise().exception);

        return *handle_.promise().value;
    }

  private:
    Handle handle_{};
};

} // namespace ws
#endif // INCLUDE/home/talel/Programming/Projects/websock/includetasktask.hpp_
