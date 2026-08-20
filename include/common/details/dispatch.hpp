#ifndef WS_DISPATCH_HPP
#define WS_DISPATCH_HPP

#include "common/concepts.hpp"

namespace ws::detail {

// Executes async_fn if the Socket is async, otherwise executes sync_fn
template <typename Socket, typename AsyncFn, typename SyncFn>
constexpr decltype(auto) dispatch(AsyncFn &&async_fn, SyncFn &&sync_fn) {
    if constexpr (isAsyncSocket<Socket>) {
        return async_fn();
    } else {
        return sync_fn();
    }
}

} // namespace ws::detail

#endif
