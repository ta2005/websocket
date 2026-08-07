#include "handshake.hpp"
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

namespace ws::client {
std::expected<void, std::string_view>
send_handshake(const TcpSocket &socket, const std::string_view host,
               const std::string_view path) {
    // this is just place holder that jsut works of course i can then use the
    // same stratgey as curl and get these fields from the command line

    auto out = socket.send(
        std::format("GET {} HTTP/1.1\r\n"
                    "Connection: Upgrade\r\n"
                    "Upgrade: websocket\r\n"
                    "Host: {}\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    "\r\n",
                    path, host));
    if (!out) {
        return std::unexpected(out.error());
    }
    return {};
}

// and of course nothing is a easy as it seems
// i could have done this one ocaml sytle with
// line * rest
// but i would need to make the other function recursive
std::expected<std::string_view, std::string_view>
get_line(std::string_view &buf) {
    auto pos = buf.find('\n');
    if (pos == std::string_view::npos) {
        // put all the rest into line;
        return std::unexpected("found no \n");
    }
    std::string_view line = buf.substr(0, pos);
    buf                   = buf.substr(pos + 1);
    if (line.back() == '\r') {
        line.remove_suffix(1);
    }
    return line;
}

std::expected<StatusLine, std::string_view>
parse_status_line(const std::string_view line) {
    // the line i get should never have \r or \r in it
    StatusLine res{};
    if (line.find('\r') != std::string_view::npos ||
        line.find('\n') != std::string_view::npos) {
        return std::unexpected("unexpected line feed in stautus-line");
    }
    size_t sp1 = line.find(' ');
    if (sp1 == std::string_view::npos) {
        return std::unexpected("invalid line:Unable to find the version");
    }
    res.version = line.substr(0, sp1);
    // this should suffice for now but i will implement the comsume nbr fnct
    // later
    if (res.version.starts_with("HTTP/1.1")) {
        return std::unexpected("Unsupported http version expected 1.1");
    }
    size_t sp2 = line.find(' ', sp1 + 1);
    if (sp2 == std::string_view::npos) {
        return std::unexpected("invalid line:Unable to find the status code");
    }
    auto st = line.substr(sp1 + 1, sp2 - sp1 + 1);
    if (((sp2 - sp1 + 1) != 3) || st < "100" || st > "599") {
        return std::unexpected("invalide line:invalid status nbr");
    }
    res.status = (st[2] - '0') + 10 * (st[1] - '0') + 100 * (st[0] - '0');
    // it can be empty i don't care
    res.reason = line.substr(sp2 + 1);
    return res;
}

std::expected<HandskaheResult, std::string_view>
parse_headers(const std::string_view headers) {
    auto tmphd       = headers;
    auto status_line = get_line(tmphd).and_then(parse_status_line);
    if (!status_line) {
        return std::unexpected(status_line.error());
    }
    if (status_line->status != 101) {
        return std::unexpected("3asab lik t3ib");
    }
    HandskaheResult res;
    res.line = *status_line;
    while (!tmphd.empty()) {
        auto l = get_line(tmphd);
        if (!l) {
            return std::unexpected(l.error());
        }
        if (l->empty()) {
            break;
        }
        // i should have just create a wrapper around
        // l->find
        // and then used and_then
        auto colon_pos = l->find(':');
        if (colon_pos == std::string_view::npos) {
            return std::unexpected("invalid line header no colon was found");
        }
        auto key = l->substr(0, colon_pos);
        if (!is_field_name(key)) {
            return std::unexpected("invalid header:not a field name");
        }
        if (!is_field_name(key)) {
            return std::unexpected("invalid header:not a field name");
        }
        auto value = l->substr(colon_pos + 1);
        value      = drop_space_and_tab(value);
        if (!is_field_value(value)) {
            return std::unexpected("invalid header:not a value name");
        }
        if (iequals(key, "Upgrade")) {
            if (!res.upgrade.empty())
                return std::unexpected("Duplicate Upgrade header");
            res.upgrade = value;
        } else if (iequals(key, "Connection")) {
            if (!res.connection.empty())
                return std::unexpected("Duplicate Connection header");
            res.connection = value;
        } else if (iequals(key, "Sec-WebSocket-Accept")) {
            if (!res.accept_key.empty())
                return std::unexpected("Duplicate Sec-WebSocket-Accept header");
            res.accept_key = value;
        }
    }
    return res;
}
std::expected<HandskaheResult, std::string_view>
perform_handshake(const TcpSocket &socket, const std::string_view host,
                  const std::string_view path) {
    auto res = send_handshake(socket, host, path);
    if (!res) {
        return std::unexpected(res.error());
    }
    std::array<uint8_t, 1024 * 8> buffer;
    auto                          actual_size = socket.read(buffer);
    if (!actual_size) {
        return std::unexpected(res.error());
    }
    std::string_view header_view{reinterpret_cast<const char *>(buffer.data()),
                                 *actual_size};
    return parse_headers(header_view);
}

} // namespace ws::client
