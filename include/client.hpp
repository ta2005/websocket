#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "common/concepts.hpp"
#include "common/roles.hpp"
#include "common/details/dispatch.hpp"
#include "handshake.hpp"
#include "session.hpp"
#include "task.hpp"
#include <string_view>

namespace ws {

template <typename Socket> 
class Client : public Session<Socket, Role::Client> {
    template <typename T>
    using Result =
        std::conditional_t<isAsyncSocket<Socket>, Task<std::expected<T, Error>>,
                           std::expected<T, Error>>;

  protected:
    Client(Socket s) : Session<Socket, Role::Client>(std::move(s)) {}

  public:
    static decltype(auto) create(Socket s, const std::string_view host,
                                 const std::string_view path,
                                 const std::string_view port = "80");
};

template <typename Socket>
decltype(auto) Client<Socket>::create(Socket s, const std::string_view host,
                                      const std::string_view path,
                                      const std::string_view port) {
    return detail::dispatch<Socket>(
        [host, path, port, s = std::move(s)]<typename S = Socket>() mutable
            -> Task<std::expected<Client<Socket>, Error>> {
            auto hs_res = co_await perform_handshake(static_cast<S &>(s), host,
                                                     path, port);
            if (!hs_res) {
                co_return std::unexpected(hs_res.error());
            }
            Client<Socket> client(std::move(s));
            if (!hs_res->leftover.empty()) {
                client.current_read.data = std::move(hs_res->leftover);
                client.current_read.remaing_bytes = 0;
            }
            co_return client;
        },
        [host, path, port, s = std::move(s)]<typename S = Socket>() mutable
            -> std::expected<Client<Socket>, Error> {
            auto hs_res = perform_handshake(s, host, path, port);
            if (!hs_res) {
                return std::unexpected(hs_res.error());
            }
            Client<Socket> client(std::move(s));
            if (!hs_res->leftover.empty()) {
                client.current_read.data          = std::move(hs_res->leftover);
                client.current_read.remaing_bytes = 0;
            }
            return client;
        });
}

} // namespace ws

#endif
