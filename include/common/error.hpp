#ifndef WS_ERROR_HPP
#define WS_ERROR_HPP

#include "common/status_code.hpp"
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
    // should server be 
    // a class of it's own???
    // should it and it's constrocot 
    // calls to bind and listen 
    // I should also then have a client class
    // where i can then store the data retruned but socket
    UnsupportedExtension, // RSV bits set without negotiation
    InvalidPayloadLength, // E.g. using 64-bit length for a 10-byte payload
    InvalidUTF8,
    ControlFrameTooLarge,   // Control payload > 125 bytes
    ControlFrameFragmented, // Control frames cannot have FIN=0
    UnmaskedServerPayload,  // Server to client must NOT be masked
    MessageTooLarge,        // Exceeded max_size in read_message
    ProtocolError,          // Generic protocol violation
    InvalidState,
    // Async
    EventLoopInitFailed
};

constexpr status_code get_status_code(Error e) {
    switch (e) {
        case Error::InvalidUTF8:
            return status_code::bad_payload; // 1007
        case Error::MessageTooLarge:
            return status_code::payload_too_big; // 1009
        case Error::UnsupportedExtension:
            return status_code::needs_extention; // 1010
        case Error::ConnectionClosed:
            return status_code::normal_closure; // 1000
        case Error::ConnectionFailed:
        case Error::ReadFailed:
        case Error::WriteFailed:
            return status_code::abnormal; // 1006
        case Error::InvalidHttpVersion:
        case Error::InvalidStatusCode:
        case Error::HandshakeRejected:
        case Error::MissingHeaders:
        case Error::DeduplicateHeaders:
        case Error::FoundNoNewLine:
        case Error::InvalidToken:
        case Error::ParseError:
        case Error::NonFinControlFrame:
        case Error::UnsupporOpcode:
        case Error::InvalidPayloadLength:
        case Error::ControlFrameTooLarge:
        case Error::ControlFrameFragmented:
        case Error::UnmaskedServerPayload:
        case Error::ProtocolError:
        default:
            return status_code::protocol_error; // 1002
    }
}

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
