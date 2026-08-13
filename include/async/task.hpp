#ifndef TASK_HPP
#define TASK_HPP

#include <coroutine>

namespace async::ws {
struct task {
    struct promise_type {
        task               get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void               return_void() {}
        void               unhandled_exception() {}
    };
};

} // namespace async::ws
#endif 
