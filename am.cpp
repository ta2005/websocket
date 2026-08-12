#ifndef OPCODE_HPP
#define OPCODE_HPP

#include <cassert>
#include <cstdint>

namespace ws {
enum class opcode : uint8_t {
    continuation = 0x0,
    text         = 0x1,
    binary       = 0x2,
    close        = 0x8,
    ping         = 0x9,
    pong         = 0xA,
    unsupported  = 0xFF
};

constexpr opcode get_opcode(uint8_t code) {
    switch (code) {
        case 0x0:
            return opcode::continuation;
        case 0x1:
            return opcode::text;
        case 0x2:
            return opcode::binary;
        case 0x8:
            return opcode::close;
        case 0x9:
            return opcode::ping;
        case 0xA:
            return opcode::pong;
        default:
            return opcode::unsupported;
    }
}

constexpr bool is_supported(opcode op) {
    return get_opcode(static_cast<uint8_t>(op)) == opcode::unsupported;
}

constexpr bool is_control(opcode op) { return static_cast<uint8_t>(op) & 0x8; }

} // namespace ws

#endif
#ifndef WS_LOGGER_HPP
#define WS_LOGGER_HPP

#include <print>
#include <utility>

namespace ws::log {

// A simple compile-time toggle for debugging
#ifndef WS_DEBUG
#define WS_DEBUG 1
#endif

// Using C++23 std::print and std::println
template <typename... Args>
void debug(std::format_string<Args...> fmt, Args &&...args) {
    if constexpr (WS_DEBUG) {
        std::print("[DEBUG] ");
        std::println(fmt, std::forward<Args>(args)...);
    }
}

template <typename... Args>
void info(std::format_string<Args...> fmt, Args &&...args) {
    std::print("[INFO]  ");
    std::println(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args &&...args) {
    std::print(stderr, "[ERROR] ");
    std::println(stderr, fmt, std::forward<Args>(args)...);
}

} // namespace ws::log

#endif
#ifndef HANDSHAKE_HPP
#define HANDSHAKE_HPP

#include "tcp_socket.hpp"
#include <string_view>
#include <vector>

namespace ws {
// i will first assuem no erros then will see later on
struct StatusLine {
    std::string version;
    int         status;
    std::string reason;
};
struct HandskaheResult {
    StatusLine           line;
    std::string          accept_key;
    std::string          upgrade;
    std::string          connection;
    std::string          extensions;
    std::string          protocol;
    std::vector<uint8_t> leftover;
};

std::expected<HandskaheResult, Error>
perform_handshake(const TcpSocket &, const std::string_view host,
                  const std::string_view path,
                  const std::string_view port = "80");

} // namespace ws

#endif
#ifndef WEBSOCKETFRAME_HPP
#define WEBSOCKETFRAME_HPP

#include <cstdint>
#include <span>

struct WebSocketFrame {
    bool               fin;
    uint8_t            opcode;
    std::span<uint8_t> payload;
};

#endif
#ifndef WS_ERROR_HPP
#define WS_ERROR_HPP

#include <string_view>

namespace ws {

// Strong typing for errors makes handling edge cases (like partial reads) easy!
enum class Error {
    // I/O & Connection
    ConnectionFailed,
    ReadFailed,
    WriteFailed,
    ConnectionClosed,

    // Handshake
    InvalidHttpVersion,
    InvalidStatusCode,
    HandshakeRejected,
    MissingHeaders,
    DeduplicateHeaders,
    FoundNoNewLine,
    InvalidToken,
    ParseError,

    // Parsing & Framing
    NeedMoreBytes, // Return this when a read didn't contain a full frame header
                   // yet!
    PayloadTooShort,
    NonFinControlFrame,
    UnsupporOpcode,
    UnsupportedExtension, // RSV bits set without negotiation
    InvalidPayloadLength, // E.g. using 64-bit length for a 10-byte payload
    InvalidUTF8,
    ControlFrameTooLarge,   // Control payload > 125 bytes
    ControlFrameFragmented, // Control frames cannot have FIN=0
    UnmaskedServerPayload,  // Server to client must NOT be masked
    MessageTooLarge,         // Exceeded max_size in read_message
    ProtocolError,           // Generic protocol violation
    InvalidState,
};

// check if i must Fail the Websocket connection after error
constexpr bool is_fatal(Error e) {
    return e == Error::UnsupportedExtension || e == Error::UnsupporOpcode ||
           e == Error::InvalidUTF8 || e == Error::ConnectionFailed ||
           e == Error::ReadFailed;
}

constexpr std::string_view to_string(Error e) {
    switch (e) {
        case Error::ConnectionFailed:
            return "Connection failed";
        case Error::ReadFailed:
            return "Socket read failed";
        case Error::WriteFailed:
            return "Socket write failed";
        case Error::ConnectionClosed:
            return "Connection closed by peer";
        case Error::InvalidHttpVersion:
            return "Invalid HTTP version";
        case Error::InvalidStatusCode:
            return "Invalid status code";
        case Error::HandshakeRejected:
            return "Handshake rejected (expected 101)";
        case Error::MissingHeaders:
            return "Missing required headers";
        case Error::NeedMoreBytes:
            return "Need more bytes to parse frame";
        case Error::PayloadTooShort:
            return "Payload too short";
        case Error::UnsupportedExtension:
            return "Unsupported extension (RSV bits set)";
        case Error::InvalidPayloadLength:
            return "Invalid payload length encoding";
        case Error::InvalidUTF8:
            return "Invalid UTF-8 in text frame";
        case Error::ControlFrameTooLarge:
            return "Control frame payload > 125 bytes";
        case Error::ControlFrameFragmented:
            return "Control frames cannot be fragmented (FIN=0)";
        case Error::UnmaskedServerPayload:
            return "Payload from server must not be masked";
        case Error::MessageTooLarge:
            return "Message exceeded maximum allowed size";
        case Error::ProtocolError:
            return "Protocol violation";
        case Error::NonFinControlFrame:
            return "Control frames cannot be fragmented";
        default:
            return "Unknown Error";
    }
}

} // namespace ws

#endif
#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "opcode.hpp"
#include "tcp_socket.hpp"
#include <random>
#include <string_view>
#include <vector>

namespace ws {

struct ChunkView {
    std::span<uint8_t> payload;
    bool               is_fin;
    opcode             type;
};

struct Message {
    std::vector<uint8_t> payload;
    opcode               type;
};

enum class ConnectionState {
    Open,
    Closing,
    Closed
};
class Client {
  private:
    TcpSocket    m_socket;
    std::mt19937 m_rng;
    struct ReadSate {
        std::vector<uint8_t> data;
        size_t               remaing_bytes;
    } current_read;

    ConnectionState m_state = ConnectionState::Open;

    Client(TcpSocket s)
        : m_socket(std::move(s)), m_rng(std::random_device{}()) {};
    std::expected<void, Error> send_impl(std::span<const uint8_t>, opcode);

    std::expected<void, Error> send_control_frame(std::span<const uint8_t>,
                                                  opcode);
    std::expected<ChunkView, Error> read_chunk_impl(size_t);
    std::expected<void, Error> send_close(std::span<const uint8_t>);

  public:
    std::expected<void, Error> send(const std::string_view);
    std::expected<void, Error> send(std::span<const uint8_t>);
    std::expected<void, Error> send_ping(std::span<const uint8_t>);
    std::expected<void, Error> send_pong(std::span<const uint8_t>);
    // next major update this will need to be and enum with an explanation
    std::expected<void, Error>       close(std::span<const uint8_t>);
    std::expected<ChunkView, Error> read_chunk();
    std::expected<Message, Error> read_message(size_t max_size = 100 * 1024 * 1024); // 100MB limit to prevent OOM
    // this one should be uri
    static std::expected<Client, Error>
    create(const std::string_view host, const std::string_view path,
           const std::string_view port = "80");
};
} // namespace ws

#endif
#ifndef TCP_SOCKET_HPP
#define TCP_SOCKET_HPP

#include "error.hpp"
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <unistd.h>

namespace ws {
class TcpSocket {
  private:
    int m_fd = -1;
    TcpSocket(int fd) : m_fd{fd} {}

  public:
    TcpSocket(const std::string_view host, const std::string_view port);
    TcpSocket(TcpSocket &&);
    TcpSocket &operator=(TcpSocket &&);
    TcpSocket(const TcpSocket &)            = delete;
    TcpSocket &operator=(const TcpSocket &) = delete;
    ~TcpSocket();
    void close(){
	::close(m_fd);
    }
    int get_fd() const { return m_fd; }
        operator bool() { return m_fd != -1; }
    // i don't maybe i will change the interface later
    std::expected<size_t, Error> send(const std::span<const uint8_t>) const;
    std::expected<size_t, Error> send(const std::string_view) const;
    std::expected<size_t, Error>
    send(const std::span<const uint8_t> meta_data,
         const std::span<const uint8_t> payload) const;
    std::expected<size_t, Error> send(const std::span<const uint8_t> meta_data,
                                      std::string_view payload) const;
    std::string                  read(int max_len) const;
    std::expected<size_t, Error> read(std::span<uint8_t> buf) const;
    // I think i can return std::string as the getaddrinfo strings
    // are statically allocated but further testing is needed
    static std::expected<TcpSocket, Error> connect(const std::string_view host,
                                                   const std::string_view port);
};
} // namespace ws

#endif
// this  is a helper furciton that format a given the metadata
#ifndef PREPARE_META_HPP
#define PREPARE_META_HPP

#include "opcode.hpp"
#include <assert.h>
#include <cstring>
#include <span>

namespace ws::detail {

struct FrameHeader {
    std::array<uint8_t, 14> data;
    size_t                  size;

    // A convenient helper to return a span pointing only to the valid bytes
    std::span<const uint8_t> span() const { return {data.data(), size}; }
};

constexpr FrameHeader format_meta(bool fin, bool is_masked, opcode op,
                                  uint64_t len, uint32_t mask) {
    FrameHeader meta{};

    // FIN is the 8th bit (0x80). Opcode is the bottom 4 bits (0x0F).
    meta.data[0] = (fin ? 0x80 : 0x00) | (static_cast<uint8_t>(op) & 0x0F);

    // Mask flag is the 8th bit (0x80) of the second byte
    uint8_t mask_bit = is_masked ? 0x80 : 0x00;

    int mask_pos = 2;
    if (len <= 125) {
        meta.data[1] = mask_bit | static_cast<uint8_t>(len);
        meta.size    = 2;
    } else if (len <= 0xFFFF) {
        meta.data[1] = mask_bit | 126;
        // WebSocket requires Big Endian (Network Byte Order)
        meta.data[2] = (len >> 8) & 0xFF;
        meta.data[3] = len & 0xFF;
        meta.size    = 4;
        mask_pos     = 4;
    } else {
        meta.data[1] = mask_bit | 127;
        // 64-bit length in Big Endian
        for (int i = 0; i < 8; i++) {
            meta.data[2 + i] = (len >> ((7 - i) * 8)) & 0xFF;
        }
        meta.size = 10;
        mask_pos  = 10;
    }

    if (is_masked) {
        // Mask is 4 bytes. We write it sequentially.
        meta.data[mask_pos]     = (mask >> 24) & 0xFF;
        meta.data[mask_pos + 1] = (mask >> 16) & 0xFF;
        meta.data[mask_pos + 2] = (mask >> 8) & 0xFF;
        meta.data[mask_pos + 3] = mask & 0xFF;
        meta.size += 4;
    }

    return meta;
}

} // namespace ws::detail

#endif
#ifndef PARSE_META_HPP
#define PARSE_META_HPP

#include "error.hpp"
#include "opcode.hpp"
#include <cstdint>
#include <expected>
#include <netinet/in.h>
#include <span>

namespace ws::detail {
struct PayloadMetaData {
    uint64_t len;
    uint32_t mask;
    uint8_t  meta_size;
    bool     fin;
    bool     is_masked;
    opcode   op;
};

// this func i kind of useful
constexpr std::expected<std::span<uint8_t>, Error>
advance(std::span<uint8_t> &payload, size_t step = 1) {
    if (payload.size() < step) {
        return std::unexpected(Error::PayloadTooShort);
    }
    auto ret = payload.subspan(0, step);
    payload  = payload.subspan(step);
    return ret;
}

// yeah now i get it why i should have done
// the error type a and error code not a string view
// it allows me to easily change the error message
// and then get a get_String
// maybe i could have like some standard log(like the one in systemd
// next release ???
constexpr std::expected<PayloadMetaData, Error>
parse_meta(std::span<uint8_t> payload) {
    PayloadMetaData res;
    res.meta_size = 2;
    auto meta     = ws::detail::advance(payload, 2);
    if (!meta) {
        return std::unexpected(meta.error());
    }
    res.fin = ((*meta)[0]) & 0x80;
    if (((*meta)[0] & 0x70) != 0) {
        return std::unexpected(Error::UnsupportedExtension);
    }
    auto op = get_opcode(((*meta)[0]) & 0x0F);
    if (op == opcode::unsupported) {
        return std::unexpected(Error::UnsupporOpcode);
    }
    res.op        = op;
    res.is_masked = ((*meta)[1]) & 0x80;
    res.len       = ((*meta)[1]) & 0x7F;
    if (res.len == 126) {
        auto len_span = ws::detail::advance(payload, 2);
        if (!len_span)
            return std::unexpected(len_span.error());
        res.len = (static_cast<uint16_t>((*len_span)[0]) << 8) |
                  static_cast<uint16_t>((*len_span)[1]);

        res.meta_size += 2;
        if (res.len < 126) {
            return std::unexpected(Error::InvalidPayloadLength);
        }
    } else if (res.len == 127) {
        auto len_span = ws::detail::advance(payload, 8);
        if (!len_span) {
            return std::unexpected(len_span.error());
        }
        res.len = (static_cast<uint64_t>((*len_span)[0]) << 56) |
                  (static_cast<uint64_t>((*len_span)[1]) << 48) |
                  (static_cast<uint64_t>((*len_span)[2]) << 40) |
                  (static_cast<uint64_t>((*len_span)[3]) << 32) |
                  (static_cast<uint64_t>((*len_span)[4]) << 24) |
                  (static_cast<uint64_t>((*len_span)[5]) << 16) |
                  (static_cast<uint64_t>((*len_span)[6]) << 8) |
                  static_cast<uint64_t>((*len_span)[7]);

        if (res.len <= 0xFFFF) {
            return std::unexpected(Error::InvalidPayloadLength);
        }
        res.meta_size += 8;
    }
    if (res.is_masked) {

        auto mask_span = ws::detail::advance(payload, 4);
        if (!mask_span) {
            return std::unexpected(meta.error());
        }
        res.mask = (static_cast<uint32_t>((*mask_span)[0]) << 24) |
                   (static_cast<uint32_t>((*mask_span)[1]) << 16) |
                   (static_cast<uint32_t>((*mask_span)[2]) << 8) |
                   static_cast<uint32_t>((*mask_span)[3]);

        // i don't know whether i should reverse it or not
        res.meta_size += 4;
    }
    return res;
}

} // namespace ws::detail

#endif // INCLUDE/home/talel/Programming/Projects/websock/include/detailsparse_headerparse_header.hpp_
#ifndef MASK_PAYLOAD_HPP_
#define MASK_PAYLOAD_HPP_

#include <arpa/inet.h>
#include <cstdint>
#include <span>

constexpr void mask_payload(std::span<uint8_t> payload, uint32_t mask) {
    // I have seen this pattern before
    // with stb printf when wirting data
    mask = htonl(mask);
    switch (payload.size() % 4) {
        case 1:
            payload[0] ^= mask & 0xFF;
            payload = payload.subspan(1);
            break;
        case 2:
            payload[0] ^= mask & 0xFF;
            payload[1] ^= (mask << 8) & 0xFF;
            payload = payload.subspan(2);
            break;
        case 3:
            payload[0] ^= mask & 0xFF;
            payload[1] ^= (mask << 8) & 0xFF;
            payload[2] ^= (mask << 16) & 0xFF;
            payload = payload.subspan(3);
            break;
    }
    auto tmp = std::span<uint32_t>(reinterpret_cast<uint32_t *>(payload.data()),
                                   payload.size() / 4);
    for (size_t i = 0; i < tmp.size(); i++) {
        tmp[i] ^= mask;
    }
}

#endif
// #include <print>
// #include <string_view>
// #include <unistd.h>
//
// #include "client.hpp"
// #include "logger.hpp"
//
// int main(int argc, char **argv) {
//     std::string_view s       = "localhost";
//     std::string_view port    = "8765";
//     std::string_view message = "Hello world my name is Tale Zighni";
//
//     if (argc > 2) {
//         s = argv[1];
//         if (argc == 3)
//             port = argv[2];
//         if (argc == 4) {
//             message = argv[3];
//         }
//     }
//
//     auto client_res = ws::Client::create(s, "/", port);
//     if (!client_res) {
//         std::print("Failed to connect/handshake: {}\n", client_res.error());
//         return 1;
//     }
//     ws::Client client = std::move(*client_res);
//
//     ws::log::info("Sending message...");
//     auto send_res = client.send(message);
//     if (!send_res) {
//         std::print("Send failed: {}\n", send_res.error());
//         return 1;
//     }
//
//     ws::log::info("Waiting for server response...");
//     auto chunk_res = client.read_chunk();
//     if (!chunk_res) {
//         std::print("Read failed: {}\n", chunk_res.error());
//         return 1;
//     }
//
//     ws::ChunckView view = *chunk_res;
//
//     std::print("\n=== RECEIVED FRAME ===\n");
//     std::print("Opcode: {}\n", static_cast<int>(view.type));
//     std::print("FIN bit: {}\n", view.is_fin);
//     std::print("Payload Length: {} bytes\n", view.payload.size());
//
//     // Print the payload as a string
//     std::string_view payload_str(
//         reinterpret_cast<const char *>(view.payload.data()),
//         view.payload.size());
//     std::print("Payload Data: {}\n", payload_str);
//     std::print("======================\n\n");
//
//     if (!client.close()) {
//         return 1;
//     }
//     return 0;
// }
// /*curl --include \
//      --no-buffer \
//      --header "Connection: Upgrade" \
//      --header "Upgrade: websocket" \
//      --header "Host: echo.websocket.org" \
//      --header "Origin: https://www.websocket.org" \
//      --header "Sec-WebSocket-Key: SGVsbG8sIHdvcmxkIQ==" \
//      --header "Sec-WebSocket-Version: 13" \
//      https://echo.websocket.org
//     */

#include "client.hpp"
#include "details/mask_payload.hpp"
#include "details/parse_meta.hpp"
#include "details/prepare_meta.hpp"
#include "handshake.hpp"
#include "logger.hpp"
#include "simdutf.h"

namespace ws {

std::expected<Client, Error> Client::create(const std::string_view host,
                                            const std::string_view path,
                                            const std::string_view port) {
    auto s = TcpSocket::connect(host, port);
    if (!s) {
        return std::unexpected(s.error());
    }
    auto hk = perform_handshake(*s, host, path, port);
    if (!hk) {
        // ws::log::error("Handshake failed: {}", hk.error());
        return std::unexpected(hk.error());
    }
    ws::log::info("Client created and handshake complete");
    Client c = (std::move(*s));
    if (!hk->leftover.empty()) {
        c.current_read.data = std::move(hk->leftover);
        // c.current_read.remaing_bytes = c.current_read.data.size();
    }
    return c;
}

std::expected<void, Error>
Client::send_control_frame(std::span<const uint8_t> payload, opcode op) {
    if (m_state == ConnectionState::Closed) {
        return std::unexpected(Error::ConnectionClosed);
    }
    if (payload.size() > 125) {
        return std::unexpected(Error::InvalidPayloadLength);
    }
    auto meta = detail::format_meta(true, true, op, payload.size(), m_rng());
    if (auto sz = m_socket.send(meta.span(), payload); !sz) {
        return std::unexpected(sz.error());
    }
    return {};
}

std::expected<void, Error> Client::send_impl(std::span<const uint8_t> msg,
                                             opcode first_op) {
    if (m_state != ConnectionState::Open) {
        return std::unexpected(Error::InvalidState);
    }
    constexpr int                   chunk_size = 8 * 1024;
    bool                            first      = true;
    std::array<uint8_t, chunk_size> tmp;

    while (msg.size() > chunk_size) {
        // First frame gets the actual opcode, the rest get continuation (0x0)
        opcode op = first ? first_op : opcode::continuation;

        uint32_t mask = m_rng();
        auto     meta = detail::format_meta(false, true, op, chunk_size, mask);
        std::copy(msg.begin(), msg.begin() + chunk_size, tmp.begin());
        mask_payload(tmp, mask);

        ws::log::debug("Sending chunk of size {} bytes with opcode {}",
                       chunk_size, static_cast<int>(op));
        if (auto l = m_socket.send(meta.span(), tmp); !l) {
            // ws::log::error("Failed to send chunk: {}", l.error());
            return std::unexpected(l.error());
        }
        msg   = msg.subspan(chunk_size);
        first = false;
    }

    opcode   op   = first ? first_op : opcode::continuation;
    uint32_t mask = m_rng();
    std::copy(msg.begin(), msg.begin() + msg.size(), tmp.begin());
    auto meta = detail::format_meta(true, true, op, msg.size(), mask);
    mask_payload(tmp, mask);

    ws::log::debug("Sending final chunk of size {} bytes with opcode {}",
                   msg.size(), static_cast<int>(op));
    if (auto l = m_socket.send(meta.span(), {tmp.data(), msg.size()}); !l) {
        // ws::log::error("Failed to send final chunk: {}", l.error());
        return std::unexpected(l.error());
    }
    return {};
}

std::expected<void, Error> Client::send(std::span<const uint8_t> msg) {
    return send_impl(msg, opcode::binary);
}

std::expected<void, Error> Client::send(const std::string_view msg) {
    if (!simdutf::validate_utf8(msg.data(), msg.size())) {
        return std::unexpected(Error::InvalidUTF8);
    }
    auto buf = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t *>(msg.data()), msg.size());
    return send_impl(buf, opcode::text);
}

std::expected<void, Error>
Client::send_close(std::span<const uint8_t> payload) {
    return send_control_frame(payload, opcode::close);
}

std::expected<void, Error> Client::close(std::span<const uint8_t> payload) {
    if (m_state == ConnectionState::Open) {
	m_state = ConnectionState::Closing;
    }else if(m_state == ConnectionState::Closing){
	m_state=ConnectionState::Closed;
	m_socket.close();
	return {};
    }else{
	return std::unexpected(Error::InvalidState);
    }
    return send_close(payload);
}

// this should be an internal function of send control
// and then the dispatching happens with each one seding its own opcode
// and payload
std::expected<void, Error> Client::send_ping(std::span<const uint8_t> payload) {
    return send_control_frame(payload, opcode::ping);
}

std::expected<void, Error> Client::send_pong(std::span<const uint8_t> payload) {
    return send_control_frame(payload, opcode::pong);
}

std::expected<ChunkView, Error> Client::read_chunk_impl(size_t first_read) {
    constexpr int chunk_size  = 8 * 1024;
    auto         &buffer      = current_read.data;
    auto          parsed_meta = detail::parse_meta({buffer.data(), first_read});
    if (!parsed_meta) {
        // ws::log::error("[READ ERROR] Failed to parse frame metadata: {}",
        //                parsed_meta.error());
        return std::unexpected(parsed_meta.error());
    }
    // 1. Log Parsed Metadata
    ws::log::debug("=== [FRAME HEADER RECEIVED] ===");
    ws::log::debug("  Opcode:    0x{:01X} ({})",
                   static_cast<uint8_t>(parsed_meta->op),
                   static_cast<int>(parsed_meta->op));
    ws::log::debug("  FIN Bit:   {}", parsed_meta->fin);
    ws::log::debug("  Masked:    {}", parsed_meta->is_masked);
    ws::log::debug("  Payload L: {} bytes", parsed_meta->len);
    ws::log::debug("  Header Size: {} bytes", parsed_meta->meta_size);
    // TODO:this will then be converted into a validate function
    if (parsed_meta->is_masked) {
        return std::unexpected(Error::UnmaskedServerPayload);
    }
    if (is_control(parsed_meta->op) && !parsed_meta->fin) {
        return std::unexpected(Error::NonFinControlFrame);
    }
    if (is_control(parsed_meta->op) && parsed_meta->len > 125) {
        return std::unexpected(Error::InvalidPayloadLength);
    }
    buffer.erase(buffer.begin(), buffer.begin() + parsed_meta->meta_size);
    size_t total_read = first_read - parsed_meta->meta_size;
    while (total_read < parsed_meta->len) {
        std::array<uint8_t, chunk_size> tmp;
        auto read_size = m_socket.read({tmp.data(), tmp.size()});
        if (!read_size) {
            return std::unexpected(read_size.error());
        }
        buffer.insert(buffer.end(), tmp.begin(), tmp.begin() + *read_size);
        total_read += *read_size;
    }
    current_read.remaing_bytes = total_read;
    // 2. Log Received Data Payload
    std::string_view payload_view(reinterpret_cast<const char *>(buffer.data()),
                                  parsed_meta->len);

    if (m_state == ConnectionState::Closing && !is_control(parsed_meta->op)) {
        ws::log::debug("Dropping non-control frame received in CLOSING state");
        return read_chunk();
    }

    ws::log::debug("=== [FRAME PAYLOAD READ] ===");
    if (parsed_meta->op == opcode::text) {
        ws::log::debug("  Text Payload ({}) bytes: \"{}\"", parsed_meta->len,
                       payload_view);
    } else {
        ws::log::debug("  Binary/Control Payload ({}) bytes", parsed_meta->len);
    }
    switch (parsed_meta->op) {
        case opcode::ping: {
            if (auto pong_res = send_pong({buffer.data(), parsed_meta->len});
                !pong_res) {
                return std::unexpected(pong_res.error());
            }
            return read_chunk();
        } break;
        case opcode::close: {
            if(auto close_res=close(std::span{buffer.data(), parsed_meta->len});!close_res){
                return std::unexpected(close_res.error());
	    }
        } break;
        default:;
    }
    ChunkView res = {.payload = std::span{buffer.data(), parsed_meta->len},
                     .is_fin  = parsed_meta->fin,
                     .type    = parsed_meta->op};
    return res;
}

std::expected<ChunkView, Error> Client::read_chunk() {
    // i need to add a function to parse the header
    // detail::parse_meta();
    constexpr int chunk_size = 8 * 1024;
    auto         &buffer     = current_read.data;
    if (current_read.remaing_bytes > 0 &&
        current_read.remaing_bytes <= buffer.size()) {
        buffer.erase(buffer.begin(),
                     buffer.begin() + current_read.remaing_bytes);
        current_read.remaing_bytes = 0;
    }
    if (!buffer.empty()) {
        return read_chunk_impl(buffer.size());
    }
    buffer.resize(chunk_size);
    auto first_read = m_socket.read({buffer.data(), buffer.size()});
    if (!first_read) {
        return std::unexpected(first_read.error());
    }
    return read_chunk_impl(*first_read);
}

std::expected<Message, Error> Client::read_message(size_t max_size) {
    if (m_state != ConnectionState::Open) {
        return std::unexpected(Error::ConnectionClosed);
    }

    Message msg;
    bool    first_chunk = true;

    while (true) {
        auto chunk_res = read_chunk();
        if (!chunk_res)
            return std::unexpected(chunk_res.error());
        auto &chunk = *chunk_res;
        if (first_chunk) {
            if (chunk.type == opcode::continuation) {
                return std::unexpected(Error::ProtocolError);
            }
            msg.type    = chunk.type;
            first_chunk = false;
        } else {
            if (chunk.type != opcode::continuation) {
                return std::unexpected(Error::ProtocolError);
            }
        }

        if (msg.payload.size() + chunk.payload.size() > max_size) {
            return std::unexpected(Error::MessageTooLarge);
        }

        msg.payload.insert(msg.payload.end(), chunk.payload.begin(),
                           chunk.payload.end());

        if (chunk.is_fin) {
            break;
        }
    }

    if (msg.type == opcode::text) {
        if (!simdutf::validate_utf8(
                reinterpret_cast<const char *>(msg.payload.data()),
                msg.payload.size())) {
            return std::unexpected(Error::InvalidUTF8);
        }
    }

    return msg;
}

} // namespace ws
// #define WS_DEBUG 0

#include "handshake.hpp"
#include "logger.hpp"
#include <algorithm>
#include <cctype>
#include <expected>
#include <format>

bool iequals(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char c1, char c2) {
        return std::tolower(static_cast<unsigned char>(c1)) ==
               std::tolower(static_cast<unsigned char>(c2));
    });
}

// i stole this one from a cpp
// library that i cloned a year ago
bool is_token_char(char c) {
    return std::isalnum(c) || c == '!' || c == '#' || c == '$' || c == '%' ||
           c == '&' || c == '\'' || c == '*' || c == '+' || c == '-' ||
           c == '.' || c == '^' || c == '_' || c == '`' || c == '|' || c == '~';
}

bool istoken(std::string_view f) {
    return std::all_of(f.begin(), f.end(), is_token_char);
}

bool is_field_name(std::string_view f) { return istoken(f); }

bool is_txt(char c) { return c == ' ' || c == '\t' || (c > 32 && c != 127); }

std::string_view drop_space_and_tab(std::string_view s) {
    while (!s.empty() && (*s.begin() == ' ' || *s.begin() == '\t'))
        s.remove_prefix(1);
    return s;
}

bool is_field_value(const std::string_view v) {
    if (v.empty()) {
        return true;
    }
    return std::all_of(v.begin(), v.end(), is_txt);
}

namespace ws {

// Helper for case-insensitive string comparison
bool iequals(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char c1, char c2) {
        return std::tolower(static_cast<unsigned char>(c1)) ==
               std::tolower(static_cast<unsigned char>(c2));
    });
}

std::expected<void, Error> send_handshake(const TcpSocket       &socket,
                                          const std::string_view host,
                                          const std::string_view path,
                                          const std::string_view port) {
    // this is just place holder that jsut works of course i can then use the
    // same stratgey as curl and get these fields from the command line

    auto out = socket.send(
        std::format("GET {} HTTP/1.1\r\n"
                    "Connection: Upgrade\r\n"
                    "Upgrade: websocket\r\n"
                    "Host: {}:{}\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    "\r\n",
                    path, host, port));
    if (!out) {
        return std::unexpected(out.error());
    }
    return {};
}

// and of course nothing is a easy as it seems
// i could have done this one ocaml sytle with
// line * rest
// but i would need to make the other function recursive
std::expected<std::string_view, Error> get_line(std::string_view &buf) {
    auto pos = buf.find('\n');
    if (pos == std::string_view::npos) {
        // put all the rest into line;
        return std::unexpected(Error::FoundNoNewLine);
    }
    std::string_view line = buf.substr(0, pos);
    buf                   = buf.substr(pos + 1);
    if (line.back() == '\r') {
        line.remove_suffix(1);
    }
    return line;
}

std::expected<StatusLine, Error>
parse_status_line(const std::string_view line) {
    // the line i get should never have \r or \r in it
    StatusLine res{};
    if (line.find('\r') != std::string_view::npos ||
        line.find('\n') != std::string_view::npos) {
        return std::unexpected(Error::InvalidToken);
    }
    size_t sp1 = line.find(' ');
    if (sp1 == std::string_view::npos) {
        ws::log::error("Invalid status line: Unable to find the HTTP version");
        return std::unexpected(Error::ParseError);
    }
    res.version = line.substr(0, sp1);
    // this should suffice for now but i will implement the comsume nbr fnct
    // later
    if (res.version != "HTTP/1.1") {
        ws::log::error("Invalid HTTP version: Expected HTTP/1.1, got {}",
                       res.version);
        return std::unexpected(Error::InvalidHttpVersion);
    }
    size_t sp2 = line.find(' ', sp1 + 1);
    if (sp2 == std::string_view::npos) {
        ws::log::error("Invalid status line: Unable to find the status code");
        return std::unexpected(Error::ParseError);
    }
    auto st = line.substr(sp1 + 1, sp2 - sp1 - 1);
    if (((sp2 - sp1 - 1) != 3) || st < "100" || st > "599") {
        ws::log::error("Invalid status line: Invalid status number '{}'", st);
        return std::unexpected(Error::InvalidStatusCode);
    }
    res.status = (st[2] - '0') + 10 * (st[1] - '0') + 100 * (st[0] - '0');
    // it can be empty i don't care
    res.reason = line.substr(sp2 + 1);
    return res;
}

std::expected<std::pair<HandskaheResult, size_t>, Error>
parse_headers(const std::string_view headers) {
    auto tmphd       = headers;
    auto status_line = get_line(tmphd).and_then(parse_status_line);
    // parse_status_line;
    if (!status_line) {
        // ws::log::error("Failed to parse status line: {}",
        // status_line.error());
        return std::unexpected(status_line.error());
    }
    HandskaheResult res;
    res.line = *status_line;
    while (!tmphd.empty()) {
        auto current_line = get_line(tmphd);
        if (!current_line) {
            return std::unexpected(current_line.error());
        }
        if (current_line->empty()) {
            break;
        }
        // i should have just create a wrapper around
        // current_line->find
        // and then used and_then
        auto colon_pos = current_line->find(':');
        if (colon_pos == std::string_view::npos) {
            return std::unexpected(Error::ParseError);
        }
        auto key = current_line->substr(0, colon_pos);
        if (!is_field_name(key)) {
            return std::unexpected(Error::ParseError);
        }
        auto value = current_line->substr(colon_pos + 1);
        value      = drop_space_and_tab(value);
        if (!is_field_value(value)) {
            return std::unexpected(Error::ParseError);
        }
        ws::log::debug("Parsed header: [{}] = [{}]", key, value);

        if (iequals(key, "Upgrade")) {
            if (!res.upgrade.empty())
                return std::unexpected(Error::DeduplicateHeaders);
            res.upgrade = value;
        } else if (iequals(key, "Connection")) {
            if (!res.connection.empty())
                return std::unexpected(Error::DeduplicateHeaders);
            res.connection = value;
        } else if (iequals(key, "Sec-WebSocket-Accept")) {
            if (!res.accept_key.empty())
                return std::unexpected(Error::DeduplicateHeaders);
            res.accept_key = value;
        }
        // Extensions and protocol could technically be comma-separated, but we
        // keep it simple for now
        else if (iequals(key, "Sec-WebSocket-Extensions")) {
            res.extensions = value;
        } else if (iequals(key, "Sec-WebSocket-Protocol")) {
            res.protocol = value;
        }
    }
    ws::log::debug("Handshake successfully parsed");
    size_t header_len = headers.size() - tmphd.size();
    return std::pair{res, header_len};
}
std::expected<HandskaheResult, Error>
perform_handshake(const TcpSocket &socket, const std::string_view host,
                  const std::string_view path, const std::string_view port) {
    auto res = send_handshake(socket, host, path, port);
    if (!res) {
        return std::unexpected(res.error());
    }
    std::array<uint8_t, 1024 * 8> buffer;
    auto                          actual_size = socket.read(buffer);
    if (!actual_size) {
        return std::unexpected(actual_size.error());
    }
    std::string_view header_view{reinterpret_cast<const char *>(buffer.data()),
                                 *actual_size};
    auto             parsed_header = parse_headers(header_view);
    if (!parsed_header) {
        return std::unexpected(parsed_header.error());
    }
    if (parsed_header->second != *actual_size) {
        size_t msg_len = *actual_size - parsed_header->second;
        parsed_header->first.leftover.reserve(msg_len);
        parsed_header->first.leftover.assign(buffer.begin() +
                                                 parsed_header->second,
                                             buffer.begin() + *actual_size);
    }
    return parsed_header->first;
}

} // namespace ws
#include <cerrno>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "logger.hpp"
#include "tcp_socket.hpp"

namespace ws {
TcpSocket::~TcpSocket() {
    if (m_fd != -1) {
        close();
    }
}
std::expected<TcpSocket, Error>
TcpSocket::connect(const std::string_view host, const std::string_view port) {
    addrinfo *res = NULL;
    addrinfo  hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (int result = getaddrinfo(host.data(), port.data(), &hints, &res);
        result != 0) {
        ws::log::error("getaddrinfo failed: {}", gai_strerror(result));
        return std::unexpected(Error::ConnectionClosed);
    }
    int socketfd = -1;
    for (addrinfo *p = res; p != NULL; p = (p)->ai_next) {
        if ((socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
            -1) {
            continue;
        }
        // maybe add some loging capability through some uniform interface later
        // char h[INET6_ADDRSTRLEN];
        // inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
        //           host, sizeof host);
        // std::print("\033[32mconnection to {}\033[0m\n", host);
        if (::connect(socketfd, p->ai_addr, p->ai_addrlen) == -1) {
            ws::log::error("Failed to connect to {}:{} using protocol {}", host,
                           port, p->ai_protocol);
            ::close(socketfd);
            socketfd = -1;
            continue;
        }
        ws::log::info("Successfully connected to {}:{}", host, port);
        break;
    }
    //
    //    if (socketfd == -1) {
    //        return std::unexpected("unable to connect");
    //    }
    //    auto flags = fcntl(socketfd,F_GETFL);
    //    if(flags==-1){
    // return std::unexpected("unable to get the flags to socket");
    //    }
    //    flags=fcntl(socketfd,F_SETFL,flags|O_NONBLOCK);
    //    if(flags==-1){
    // return std::unexpected("unable to set non blocking socket");
    //    }

    freeaddrinfo(res);
    return TcpSocket(socketfd);
}
// the move opertors
TcpSocket::TcpSocket(TcpSocket &&other) : m_fd(other.m_fd) { other.m_fd = -1; }
TcpSocket &TcpSocket::operator=(TcpSocket &&other) {
    if (this != &other) {
        if (m_fd != -1)
            close();
        m_fd       = other.m_fd;
        other.m_fd = -1;
    }
    return *this;
}
// both of these functions are shit
// but they do the job
std::expected<size_t, Error>
TcpSocket::send(std::span<const uint8_t> data) const {
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        ssize_t sent =
            ::write(m_fd, data.data() + total_sent, data.size() - total_sent);
        if (sent < 0) {
            if (errno == EINTR)
                continue; // Interrupted by signal, try again
            ws::log::error("TcpSocket::send error: {}", std::strerror(errno));
            return std::unexpected(Error::ConnectionFailed);
        }
        total_sent += sent;
    }
    return total_sent;
}

std::expected<size_t, Error> TcpSocket::send(const std::string_view buf) const {
    auto bytes = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t *>(buf.data()), buf.size());
    return send(bytes);
}

std::string TcpSocket::read(int max_len) const {
    std::string s;
    s.resize(max_len);
    ::read(m_fd, s.data(), max_len);
    return s;
}

std::expected<size_t, Error> TcpSocket::read(std::span<uint8_t> buf) const {
    while (true) {
        ssize_t bytes_read = ::read(m_fd, buf.data(), buf.size());
        if (bytes_read < 0) {
            if (errno == EINTR)
                continue; // Interrupted by signal, try again
            ws::log::error("TcpSocket::read error: {}", std::strerror(errno));
            return std::unexpected(Error::ReadFailed);
        }
        if (bytes_read == 0) {
            return std::unexpected(Error::ConnectionClosed);
        }
        return static_cast<size_t>(bytes_read);
    }
}

// Question: do i really need the const uint8_t here ?
// Answer: You don't need `const` on the span itself (i.e. `const std::span`),
// because a span is just a lightweight view (pointer + size).
// Passing `std::span<const uint8_t>` by value is the standard C++ way!
std::expected<size_t, Error>
TcpSocket::send(std::span<const uint8_t> meta_data,
                std::span<const uint8_t> payload) const {
    size_t       total_sent = 0;
    size_t       total_size = meta_data.size() + payload.size();
    struct iovec io[2];

    io[0].iov_base =
        const_cast<void *>(static_cast<const void *>(meta_data.data()));
    io[0].iov_len = meta_data.size();

    io[1].iov_base =
        const_cast<void *>(static_cast<const void *>(payload.data()));
    io[1].iov_len = payload.size();

    int current_index = 0;
    int iovcnt        = 2;

    while (total_sent < total_size) {
        // BUG FIX: The 3rd argument is the number of buffers (iovcnt), not the
        // byte size!
        ssize_t sent = ::writev(m_fd, &io[current_index], iovcnt);
        if (sent < 0) {
            if (errno == EINTR)
                continue;
            ws::log::error("TcpSocket::send (writev) error: {}",
                           std::strerror(errno));
            return std::unexpected(Error::WriteFailed);
        }
        total_sent += sent;

        if (total_sent < total_size) {
            // Adjust the iovec structs for the next partial write
            if (total_sent >= meta_data.size() && current_index == 0) {
                // The metadata buffer is fully sent, move onto the payload
                // buffer
                current_index       = 1;
                iovcnt              = 1;
                size_t payload_sent = total_sent - meta_data.size();
                io[1].iov_base =
                    static_cast<char *>(io[1].iov_base) + payload_sent;
                io[1].iov_len -= payload_sent;
            } else if (current_index == 0) {
                // Still working on the metadata buffer
                io[0].iov_base = static_cast<char *>(io[0].iov_base) + sent;
                io[0].iov_len -= sent;
            } else {
                // Still working on the payload buffer
                io[1].iov_base = static_cast<char *>(io[1].iov_base) + sent;
                io[1].iov_len -= sent;
            }
        }
    }
    return total_sent;
}

std::expected<size_t, Error>
TcpSocket::send(const std::span<const uint8_t> meta_data,
                std::string_view               payload) const {
    auto buf = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
    return send(meta_data, buf);
}

} // namespace ws
#include "client.hpp"
#include <format>
#include <iostream>
#include <string>

void run_test_case(int case_num, const std::string &host,
                   const std::string &port) {
    std::string path =
        std::format("/runCase?case={}&agent=MyCppClient", case_num);

    auto client_res = ws::Client::create(host, path, port);
    if (!client_res)
        return;

    auto &client = *client_res;

    // Autobahn Echo Loop
    while (true) {
        auto msg = client.read_message();
        if (!msg) {
            // Connection closed or protocol error encountered
            break;
        }

        auto &chunk = *msg;

        // Autobahn expects the client to echo back Text or Binary frames
        if (chunk.type == ws::opcode::text) {
            std::string_view text(
                reinterpret_cast<const char *>(chunk.payload.data()),
                chunk.payload.size());
            if (!client.send(text))
                break;
        } else if (chunk.type == ws::opcode::binary) {
            if (!client.send(chunk.payload))
                break;
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return 1;
    }
    const std::string host = "127.0.0.1";
    const std::string port = "9001";

    // 1. Get total case count from Autobahn
    int total_cases = std::stoi(argv[1]);
    // {
    //     auto client = ws::Client::create(host, "/getCaseCount", port);
    //     if (client) {
    //         auto chunk = client->read_chunk();
    //  // chunk->payload
    //  //
    //         if (chunk) {
    //             std::string count_str(
    //                 reinterpret_cast<const char *>(chunk->payload.data()),
    //                 chunk->payload.size());
    //             total_cases = std::stoi(count_str);
    //         }
    //     }
    // }

    if (total_cases == 0) {
        std::cerr << "Failed to fetch test case count from Autobahn server at "
                  << host << ":" << port << std::endl;
        // return 1;
    }

    std::cout << "Starting Autobahn test suite (" << total_cases << " cases)..."
              << std::endl;

    // 2. Loop through all test cases
    for (int i = 1; i <= total_cases; ++i) {
        std::cout << "Running case " << i << "/" << total_cases << "...\r"
                  << std::flush;
        run_test_case(i, host, port);
    }

    std::cout << "\nTest cases finished. Generating HTML report..."
              << std::endl;

    // 3. Trigger report generation
    {
        auto client =
            ws::Client::create(host, "/updateReports?agent=MyCppClient", port);
        if (client) {
            if (!client->read_chunk()) {
                return 1;
            }
        }
    }

    std::cout
        << "Done! Open autobahn/reports/clients/index.html to view results."
        << std::endl;
    return 0;
}
