#include "client.hpp"
#include "details/prepare_meta.hpp"
#include "handshake.hpp"
#include "tcp_socket.hpp"
#include "details/mask_payload.hpp"
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
Client::send_impl(std::span<const uint8_t> msg, opcode first_op) {
    constexpr int chunk_size = 8 * 1024;
    bool          first      = true;
    std::array<uint8_t,chunk_size>tmp;

    while (msg.size() > chunk_size) {
        // First frame gets the actual opcode, the rest get continuation (0x0)
        opcode op = first ? first_op : opcode::continuation;

	uint32_t mask = m_rng(); 
        auto meta = detail::format_meta(false, true, op, chunk_size, mask);
	std::copy(msg.begin(),msg.begin()+chunk_size,tmp.begin());
	mask_payload(tmp,mask);

        auto l    = m_socket.send(meta.span(), tmp);
        if (!l) {
            return std::unexpected(l.error());
        }
        msg   = msg.subspan(chunk_size);
        first = false;
    }

    opcode op   = first ? first_op : opcode::continuation;
    uint32_t mask = m_rng(); 
    std::copy(msg.begin(),msg.begin()+msg.size(),tmp.begin());
    auto meta = detail::format_meta(true, true, op, msg.size(), mask);
    mask_payload(tmp,mask);

    auto l    = m_socket.send(meta.span(), {tmp.data(),msg.size()});
    if (!l) {
        return std::unexpected(l.error());
    }
    return {};
}

std::expected<void, std::string_view>
Client::send(std::span<const uint8_t> msg) {
    return send_impl(msg, opcode::binary);
}

std::expected<void, std::string_view>
Client::send(const std::string_view msg) {
    auto buf = std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(msg.data()), msg.size());
    return send_impl(buf, opcode::text);
}

std::expected<void, std::string_view> Client::close() const {
    auto meta = detail::format_meta(true, true, opcode::close, 0, 0);
    auto sz   = m_socket.send(meta.span());
    if (!sz) {
        return std::unexpected(sz.error());
    }
    return {};
}

} // namespace ws
