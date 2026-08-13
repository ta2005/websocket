#include "tcp_socket.hpp"
#include <coroutine>
#include <print>
#include <string_view>
#include <sys/epoll.h>
#include <unistd.h>
#include <utility>

using std::println;

// Context helper to bundle the FD and Coroutine Handle together
struct SocketContext {
    int                     fd;
    std::coroutine_handle<> handle;
};

struct EventLoop {
    int fd_;

    EventLoop() {
        fd_ = epoll_create1(0);
        if (fd_ == -1) {
            throw "epoll_create1 failed";
        }
    }

    ~EventLoop() {
        if (fd_ != -1)
            ::close(fd_);
    }

    void register_event(int socket_fd, uint32_t events, SocketContext *ctx) {
        epoll_event ev{};
        ev.events   = events | EPOLLONESHOT; // Fires once, then disarms
        ev.data.ptr = ctx;                   // Store pointer to context
        epoll_ctl(fd_, EPOLL_CTL_ADD, socket_fd, &ev);
    }

    void run_once() {
        epoll_event events[10];
        int         nfds = epoll_wait(fd_, events, 10, -1);
        for (int i = 0; i < nfds; i++) {
            auto *ctx = static_cast<SocketContext *>(events[i].data.ptr);

            // Clean up epoll registration
            epoll_ctl(fd_, EPOLL_CTL_DEL, ctx->fd, nullptr);

            // Wake up the coroutine!
            if (ctx->handle && !ctx->handle.done()) {
                ctx->handle.resume();
            }
        }
    }
};

template <class T = void> struct task {
    struct promise_type {
        void unhandled_exception() { println("{}", __PRETTY_FUNCTION__); }
        void return_void() {} // Required when coroutine ends

        std::suspend_always initial_suspend() noexcept { return {}; }

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

    // Task MUST be move-only to prevent double-destroying handle
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

struct AwaitableWrite {
    int           fd;
    EventLoop    &loop; // Passed BY REFERENCE
    std::string   data;
    SocketContext ctx{};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) noexcept {
        ctx = {fd, h};
        loop.register_event(fd, EPOLLOUT, &ctx);
    }

    void await_resume() noexcept {
        println("{}", __PRETTY_FUNCTION__);
        ::write(fd, data.c_str(), data.size());
    }
};

// 1. The Coroutine Function
task<void> run_client(int socket_fd, EventLoop &loop) {
    println("[Coroutine] Suspending until socket is writable...");

    // 2. co_await registers with epoll and suspends!
    co_await AwaitableWrite{socket_fd, loop,
                            "Hello World from Async Coroutine!\n"};

    println("[Coroutine] Resumed and write complete!");
}

int main() {
    auto t = ws::TcpSocket::connect("127.0.0.1", "8080");
    if (!t) {
        println("Connection failed!");
        return 1;
    }

    EventLoop loop{};

    // Start the coroutine (it suspends at initial_suspend)
    auto client_task = run_client(t->get_fd(), loop);

    // Kick off the coroutine until it hits co_await
    client_task.h_.resume();

    // Run epoll loop to catch the socket event and resume the coroutine
    println("[Main] Entering epoll event loop...");
    loop.run_once();

    return 0;
}
