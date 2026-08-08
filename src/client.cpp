#include "client.hpp"
#include "handshake.hpp"
#include "tcp_socket.hpp"
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
Client::send(const std::span<uint8_t> msg) const {
    auto tmp=std::span(
	    // is there a better way than const cast?
	    //
	    static_cast<uint8_t *>(msg.data()),
	    msg.size()
	);

    while(tmp.size()>8*1024){
	std::array<uint8_t,8>meta;
	auto l=m_socket.send(meta,tmp.subspan(0,8*1024));
	if(!l){
	    return std::unexpected(l.error());
	}
    }
    return {};
}

} // namespace ws
