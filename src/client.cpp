#include "client.hpp"
#include "handshake.hpp"
#include "tcp_socket.hpp"
#include "details/prepare_meta.hpp"
#include <expected>

namespace ws {

std::expected<Client, std::string_view>
Client::create(const std::string_view host, const std::string_view path,
               const std::string_view port) {
    auto s = TcpSocket::connect(host, port);
    if (!s) {
        return std::unexpected(s.error());
    }
    auto hk = perform_handshake(*s, host, path);
    if (!hk) {
        return std::unexpected(hk.error());
    }
    return Client(std::move(*s));
}

std::expected<void, std::string_view>
Client::send(std::span<const uint8_t> msg) const {
    auto tmp = msg;
    constexpr int chunk_size = 8 * 1024;
    bool first = true;

    while (tmp.size() > chunk_size) {
        // First frame gets the actual opcode, the rest get continuation (0x0)
        opcode op = first ? opcode::binary : opcode::continuation;
        
        auto meta = detail::format_meta(false, true, op, chunk_size, 0);
        auto l = m_socket.send(meta.span(), tmp.subspan(0, chunk_size));
        if (!l) {
            return std::unexpected(l.error());
        }
        tmp = tmp.subspan(chunk_size); // Shift exactly chunk_size bytes!
        first = false;
    }
    
    // The final frame! Set FIN=true.
    opcode op = first ? opcode::binary : opcode::continuation;
    // CRITICAL BUG FIX: Pass tmp.size(), not chunk_size!
    auto meta = detail::format_meta(true, true, op, tmp.size(), 0); 
    auto l = m_socket.send(meta.span(), tmp);
    if (!l) {
        return std::unexpected(l.error());
    }
    return {};
}

std::expected<void, std::string_view>
Client::send(const std::string_view msg) const {
    auto tmp = msg;
    constexpr int chunk_size = 8 * 1024;
    bool first = true;

    while (tmp.size() > chunk_size) {
        opcode op = first ? opcode::text : opcode::continuation;
        
        auto meta = detail::format_meta(false, true, op, chunk_size, 0);
        auto payload_span = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(tmp.data()), chunk_size);
        auto l = m_socket.send(meta.span(), payload_span);
        if (!l) {
            return std::unexpected(l.error());
        }
        tmp = tmp.substr(chunk_size);
        first = false;
    }
    
    opcode op = first ? opcode::text : opcode::continuation;
    auto meta = detail::format_meta(true, true, op, tmp.size(), 0);
    auto payload_span = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(tmp.data()), tmp.size());
    auto l = m_socket.send(meta.span(), payload_span);
    if (!l) {
        return std::unexpected(l.error());
    }
    return {};
}

std::expected<void, std::string_view> Client::close() const{
    auto meta = detail::format_meta(true, true, opcode::close, 0, 0);
    auto sz=m_socket.send(meta.span());
    if(!sz){
	return std::unexpected(sz.error());
    }
    return {};
}




} // namespace ws
