//
// #include "sync/client.hpp"
// #include "common/details/mask_payload.hpp"
// #include "common/details/prepare_meta.hpp"
// #include "common/error.hpp"
// #include "common/logger.hpp"
// #include "simdutf.h"
// #include "sync/handshake.hpp"
// #include <arpa/inet.h>
//
// namespace ws {
//
// std::expected<Client, Error> Client::create(const std::string_view host,
//                                             const std::string_view path,
//                                             const std::string_view port) {
//     auto s = TcpSocket::connect(host, port);
//     if (!s) {
//         return std::unexpected(s.error());
//     }
//     auto hk = perform_handshake(*s, host, path, port);
//     if (!hk) {
//         // ws::log::error("Handshake failed: {}", hk.error());
//         return std::unexpected(hk.error());
//     }
//     ws::log::info("Client created and handshake complete");
//     Client c = (std::move(*s));
//     if (!hk->leftover.empty()) {
//         c.current_read.data = std::move(hk->leftover);
//         // c.current_read.remaing_bytes = c.current_read.data.size();
//     }
//     return c;
// }
//
//
//
// std::expected<void, Error> Client::close(std::span<const uint8_t> payload) {
//     if (m_state != ConnectionState::Open) {
//         return std::unexpected(Error::InvalidState);
//     }
//     m_state = ConnectionState::Closing;
//     return send_close(payload);
// }
//
// // this should be an internal function of send control
// // and then the dispatching happens with each one seding its own opcode
// // and payload
//
// void Client::fail_connection(status_code reason) {
//     auto st = htons(static_cast<uint16_t>(reason));
//     if (auto send_res = send_close({reinterpret_cast<const uint8_t *>(&st),
//     2});
//         !send_res) {
//         ws::log::debug("unabe to send a close frame");
//     }
//     m_socket.close();
//     m_state = ConnectionState::Closed;
// }
//
//
//
//
// std::expected<Message, Error> Client::read_message(size_t max_size) {
//     if (m_state != ConnectionState::Open) {
//         return std::unexpected(Error::ConnectionClosed);
//     }
//
//     Message msg;
//     bool    first_chunk = true;
//
//     while (true) {
//         auto chunk_res = read_chunk();
//         if (!chunk_res)
//             return std::unexpected(chunk_res.error());
//         auto &chunk = *chunk_res;
//         if (first_chunk) {
//             if (chunk.type == opcode::continuation) {
//                 return std::unexpected(Error::ProtocolError);
//             }
//             msg.type    = chunk.type;
//             first_chunk = false;
//         } else {
//             if (chunk.type != opcode::continuation) {
//                 return std::unexpected(Error::ProtocolError);
//             }
//         }
//
//         if (msg.payload.size() + chunk.payload.size() > max_size) {
//             return std::unexpected(Error::MessageTooLarge);
//         }
//
//         msg.payload.insert(msg.payload.end(), chunk.payload.begin(),
//                            chunk.payload.end());
//
//         if (chunk.is_fin) {
//             break;
//         }
//     }
//
//     if (msg.type == opcode::text) {
//         if (!simdutf::validate_utf8(
//                 reinterpret_cast<const char *>(msg.payload.data()),
//                 msg.payload.size())) {
//             fail_connection(get_status_code(Error::InvalidUTF8));
//             return std::unexpected(Error::InvalidUTF8);
//         }
//     }
//
//     return msg;
// }
//
// } // namespace ws
