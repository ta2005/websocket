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
// std::expected<detail::PayloadMetaData, Error>
// Client::read_header(size_t first_read) {
//     auto &buffer      = current_read.data;
//     auto  parsed_meta = detail::parse_meta({buffer.data(), first_read});
//     while (!parsed_meta) {
//         if (parsed_meta.error() == Error::PayloadTooShort) {
//             std::array<uint8_t, chunk_size> tmp;
//             auto read_size = m_socket.read({tmp.data(), tmp.size()});
//             if (!read_size) {
//                 return std::unexpected(read_size.error());
//             }
//             buffer.insert(buffer.end(), tmp.begin(), tmp.begin() +
//             *read_size); parsed_meta = detail::parse_meta({buffer.data(),
//             buffer.size()});
//         } else {
//             return std::unexpected(parsed_meta.error());
//         }
//     }
//     if (parsed_meta->is_masked) {
//         return std::unexpected(Error::UnmaskedServerPayload);
//     }
//     if (is_control(parsed_meta->op) && !parsed_meta->fin) {
//         return std::unexpected(Error::NonFinControlFrame);
//     }
//     if (is_control(parsed_meta->op) && parsed_meta->len > 125) {
//         return std::unexpected(Error::InvalidPayloadLength);
//     }
//     buffer.erase(buffer.begin(), buffer.begin() + parsed_meta->meta_size);
//     return *parsed_meta;
// }
//
// std::expected<detail::PayloadMetaData, Error>
// Client::read_payload(detail::PayloadMetaData meta) {
//     auto  &buffer     = current_read.data;
//     size_t total_read = buffer.size();
//     while (total_read < meta.len) {
//         std::array<uint8_t, chunk_size> tmp;
//         auto read_size = m_socket.read({tmp.data(), tmp.size()});
//         if (!read_size) {
//             return std::unexpected(read_size.error());
//         }
//         buffer.insert(buffer.end(), tmp.begin(), tmp.begin() + *read_size);
//         total_read += *read_size;
//     }
//     current_read.remaing_bytes = meta.len;
//     return meta;
// }
//
// std::expected<ChunkView, Error> Client::read_chunk_impl(size_t first_read) {
//     auto &buffer = current_read.data;
//     auto  parsed_meta =
//         read_header(first_read).and_then([this](const auto &meta) {
//             return this->read_payload(meta);
//         });
//     if (!parsed_meta) {
//         fail_connection(get_status_code(parsed_meta.error()));
//         return std::unexpected(parsed_meta.error());
//     }
//     if (m_state == ConnectionState::Closing && !is_control(parsed_meta->op))
//     {
//         return read_chunk();
//     }
//     switch (parsed_meta->op) {
//         case opcode::ping: {
//             if (auto pong_res = send_pong({buffer.data(), parsed_meta->len});
//                 !pong_res) {
//                 fail_connection(get_status_code(pong_res.error()));
//                 return std::unexpected(pong_res.error());
//             }
//             return read_chunk();
//         } break;
//         case opcode::pong: {
//             return read_chunk();
//         } break;
//         case opcode::close: {
//             if (m_state == ConnectionState::Open) {
//
//                 if (auto close_res =
//                         close(std::span{buffer.data(), parsed_meta->len});
//                     !close_res) {
//                     fail_connection(get_status_code(close_res.error()));
//                     return std::unexpected(close_res.error());
//                 }
//                 m_state = ConnectionState::Closed;
//                 m_socket.close();
//             } else if (m_state == ConnectionState::Closing) {
//                 m_state = ConnectionState::Closed;
//                 m_socket.close();
//             }
//         } break;
//         default:;
//     }
//     ChunkView res = {.payload = std::span{buffer.data(), parsed_meta->len},
//                      .is_fin  = parsed_meta->fin,
//                      .type    = parsed_meta->op};
//     return res;
// }
//
// std::expected<ChunkView, Error> Client::read_chunk() {
//     // i need to add a function to parse the header
//     // detail::parse_meta();
//     auto &buffer = current_read.data;
//     if (current_read.remaing_bytes > 0 &&
//         current_read.remaing_bytes <= buffer.size()) {
//         buffer.erase(buffer.begin(),
//                      buffer.begin() + current_read.remaing_bytes);
//         current_read.remaing_bytes = 0;
//     }
//     if (!buffer.empty()) {
//         return read_chunk_impl(buffer.size());
//     }
//     buffer.resize(chunk_size);
//     auto first_read = m_socket.read({buffer.data(), buffer.size()});
//     if (!first_read) {
//         return std::unexpected(first_read.error());
//     }
//     buffer.resize(*first_read);
//     return read_chunk_impl(*first_read);
// }
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
