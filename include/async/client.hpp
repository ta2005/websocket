#ifndef ASYNC_CLIENT_HPP
#define ASYNC_CLIENT_HPP

#include "async/tcp_socket.hpp"
#include "async/task.hpp"
#include "common/websocket_frame.hpp"
#include <string_view>

namespace ws::async {

class Client {
  public:
    // Takes ownership of a connected TcpSocket via move semantics
    Client(TcpSocket socket);

    // The main coroutine loop that runs for the lifetime of the connection
    task run_session();

    // Sends text or binary frames (returns a fire-and-forget task)
    // Note: Since this returns `task` (which is void/fire-and-forget),
    // you must ensure the buffers outlive the async write loop!
    task send_text(std::string_view text);
    task send_binary(std::span<const uint8_t> data);

  private:
    TcpSocket m_socket;
};

} // namespace ws::async

#endif
