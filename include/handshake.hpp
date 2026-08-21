#ifndef HANDSHAKE_HPP
#define HANDSHAKE_HPP

#include "common/details/dispatch.hpp"
#include "common/error.hpp"
#include <algorithm>
#include <expected>
#include <format>
#include <string>
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
        return std::unexpected(Error::ParseError);
    }
    res.version = line.substr(0, sp1);
    // this should suffice for now but i will implement the comsume nbr fnct
    // later
    if (res.version != "HTTP/1.1") {
        return std::unexpected(Error::InvalidHttpVersion);
    }
    size_t sp2 = line.find(' ', sp1 + 1);
    if (sp2 == std::string_view::npos) {
        return std::unexpected(Error::ParseError);
    }
    auto st = line.substr(sp1 + 1, sp2 - sp1 - 1);
    if (((sp2 - sp1 - 1) != 3) || st < "100" || st > "599") {
        return std::unexpected(Error::InvalidStatusCode);
    }
    res.status = (st[2] - '0') + 10 * (st[1] - '0') + 100 * (st[0] - '0');
    // it can be empty i don't care
    res.reason = line.substr(sp2 + 1);
    return res;
}

constexpr std::expected<std::string_view, Error>
get_line(std::string_view &buf) {
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
constexpr std::expected<std::pair<HandskaheResult, size_t>, Error>
parse_headers(const std::string_view headers) {
    auto tmphd       = headers;
    auto status_line = get_line(tmphd).and_then(parse_status_line);
    // parse_status_line;
    if (!status_line) {
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
        } else if (iequals(key, "Sec-WebSocket-Extensions")) {
            res.extensions = value;
        } else if (iequals(key, "Sec-WebSocket-Protocol")) {
            res.protocol = value;
        }
    }
    size_t header_len = headers.size() - tmphd.size();
    return std::pair{res, header_len};
}

template <typename Socket>
decltype(auto) send_handshake(Socket &socket, const std::string_view host,
                              const std::string_view path,
                              const std::string_view port) {
    // this is just place holder that jsut works of course i can then use the
    // same stratgey as curl and get these fields from the command line

    return detail::dispatch<Socket>(
        [&socket, host, path,
         port]<typename S = Socket>() -> Task<std::expected<void, Error>> {
            auto req =
                std::format("GET {} HTTP/1.1\r\n"
                            "Connection: Upgrade\r\n"
                            "Upgrade: websocket\r\n"
                            "Host: {}:{}\r\n"
                            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                            "Sec-WebSocket-Version: 13\r\n"
                            "\r\n",
                            path, host, port);
            auto out = co_await static_cast<S &>(socket).write(
                std::span<const uint8_t>{},
                std::span<const uint8_t>{
                    reinterpret_cast<const uint8_t *>(req.data()), req.size()});
            if (!out) {
                co_return std::unexpected(out.error());
            }
            co_return {};
        },
        [&socket, host, path,
         port]<typename S = Socket>() -> std::expected<void, Error> {
            auto req =
                std::format("GET {} HTTP/1.1\r\n"
                            "Connection: Upgrade\r\n"
                            "Upgrade: websocket\r\n"
                            "Host: {}:{}\r\n"
                            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                            "Sec-WebSocket-Version: 13\r\n"
                            "\r\n",
                            path, host, port);
            auto out = socket.write(
                std::span<const uint8_t>{},
                std::span<const uint8_t>{
                    reinterpret_cast<const uint8_t *>(req.data()), req.size()});
            if (!out) {
                return std::unexpected(out.error());
            }
            return {};
        });
}

template <typename Socket>
decltype(auto) perform_handshake(Socket &socket, const std::string_view host,
                                 const std::string_view path,
                                 const std::string_view port = "80") {
    return detail::dispatch<Socket>(
        [&socket, host, path, port]<typename S = Socket>()
            -> Task<std::expected<HandskaheResult, Error>> {
            auto res = co_await send_handshake(static_cast<S &>(socket), host,
                                               path, port);
            if (!res) {
                co_return std::unexpected(res.error());
            }
            std::array<uint8_t, 1024 * 8> buffer;
            auto actual_size = co_await static_cast<S &>(socket).read(
                std::span{buffer.data(), buffer.size()});
            if (!actual_size) {
                co_return std::unexpected(actual_size.error());
            }
            std::string_view header_view{
                reinterpret_cast<const char *>(buffer.data()), *actual_size};
            auto parsed_header = parse_headers(header_view);
            if (!parsed_header) {
                co_return std::unexpected(parsed_header.error());
            }
            if (parsed_header->second != *actual_size) {
                size_t msg_len = *actual_size - parsed_header->second;
                parsed_header->first.leftover.reserve(msg_len);
                parsed_header->first.leftover.assign(
                    buffer.begin() + parsed_header->second,
                    buffer.begin() + *actual_size);
            }
            co_return parsed_header->first;
        },
        [&socket, host, path,
         port]<typename S = Socket>() -> std::expected<HandskaheResult, Error> {
            auto res = send_handshake(socket, host, path, port);
            if (!res) {
                return std::unexpected(res.error());
            }
            std::array<uint8_t, 1024 * 8> buffer;
            auto                          actual_size =
                socket.read(std::span{buffer.data(), buffer.size()});
            if (!actual_size) {
                return std::unexpected(actual_size.error());
            }
            std::string_view header_view{
                reinterpret_cast<const char *>(buffer.data()), *actual_size};
            auto parsed_header = parse_headers(header_view);
            if (!parsed_header) {
                return std::unexpected(parsed_header.error());
            }
            if (parsed_header->second != *actual_size) {
                size_t msg_len = *actual_size - parsed_header->second;
                parsed_header->first.leftover.reserve(msg_len);
                parsed_header->first.leftover.assign(
                    buffer.begin() + parsed_header->second,
                    buffer.begin() + *actual_size);
            }
            return parsed_header->first;
        });
}

} // namespace ws
#endif
