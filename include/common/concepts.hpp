#ifndef WS_CONCEPTS_HPP
#define WS_CONCEPTS_HPP

#include <concepts>
#include <string_view>
#include <span>
#include <cstdint>

namespace ws {

// A C++20 Concept defining the unified interface for both Sync and Async clients
template<typename T>
concept WebSocketClient = requires(T client, std::string_view text, std::span<const uint8_t> data) {
    
    // The client must be able to send text messages
    { client.send_text(text) };

    // The client must be able to send binary messages
    { client.send_binary(data) };

    // The client must be able to initiate a close sequence
    { client.close() };
};

} // namespace ws

#endif
