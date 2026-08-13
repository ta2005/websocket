#ifndef SOCKET_CTX_HPP
#define SOCKET_CTX_HPP

#include <coroutine>

using Handle = std::coroutine_handle<>;

namespace ws::async {
struct SocketCtx {
    int    fd;
    Handle read_handle;
    Handle write_handle;
};
} // namespace ws::async

#endif
