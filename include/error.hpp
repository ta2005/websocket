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
    MessageTooLarge,        // Exceeded max_size in read_message
    ProtocolError,          // Generic protocol violation
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
