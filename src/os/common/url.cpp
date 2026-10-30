#include "../os_internal.h"

#include <cctype>
#include <cstdlib>
#include <vector>

namespace mdv::os::net {

namespace {

int defaultPort(const std::string& scheme)
{
    if (scheme == "http")
        return 80;
    if (scheme == "https")
        return 443;
    return 0;
}

int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// Removes "." and ".." segments from an absolute URL path.
std::string removeDotSegments(const std::string& path)
{
    std::string query;
    std::string p = path;
    size_t q = p.find('?');
    if (q != std::string::npos) {
        query = p.substr(q);
        p.resize(q);
    }
    std::vector<std::string> parts;
    size_t i = 0;
    while (i <= p.size()) {
        size_t j = p.find('/', i);
        if (j == std::string::npos)
            j = p.size();
        std::string seg = p.substr(i, j - i);
        if (seg == "..") {
            if (!parts.empty())
                parts.pop_back();
        } else if (seg != "." && !(seg.empty() && j < p.size())) {
            parts.push_back(seg);
        }
        i = j + 1;
    }
    std::string out;
    for (const std::string& s : parts)
        out += "/" + s;
    // Keep a trailing slash for paths ending in "/", "/." or "/..".
    if (!p.empty() && (p.back() == '/' || (p.size() >= 2 && p.compare(p.size() - 2, 2, "/.") == 0) ||
                       (p.size() >= 3 && p.compare(p.size() - 3, 3, "/..") == 0)))
        if (out.empty() || out.back() != '/')
            out += '/';
    if (out.empty())
        out = "/";
    return out + query;
}

} // namespace

bool isUrl(std::string_view s)
{
    if (s.rfind("mailto:", 0) == 0)
        return true;
    size_t pos = s.find("://");
    if (pos == std::string_view::npos || pos == 0)
        return false;
    for (size_t i = 0; i < pos; ++i) {
        char c = s[i];
        if (!(std::isalnum((unsigned char)c) || c == '+' || c == '-' || c == '.'))
            return false;
    }
    return true;
}

std::optional<Url> parseUrl(std::string_view url)
{
    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string_view::npos || schemeEnd == 0)
        return std::nullopt;

    Url out;
    out.scheme = detail::toLowerAscii(std::string(url.substr(0, schemeEnd)));
    std::string_view rest = url.substr(schemeEnd + 3);

    size_t hash = rest.find('#');
    if (hash != std::string_view::npos) {
        out.fragment = std::string(rest.substr(hash + 1));
        rest = rest.substr(0, hash);
    }

    size_t pathStart = rest.find_first_of("/?");
    std::string_view authority = rest.substr(0, pathStart);
    out.path = pathStart == std::string_view::npos ? "/" : std::string(rest.substr(pathStart));
    if (!out.path.empty() && out.path[0] == '?')
        out.path = "/" + out.path;

    // Strip user-info.
    size_t at = authority.rfind('@');
    if (at != std::string_view::npos)
        authority = authority.substr(at + 1);

    out.port = defaultPort(out.scheme);
    if (!authority.empty() && authority[0] == '[') { // IPv6 literal
        size_t close = authority.find(']');
        if (close == std::string_view::npos)
            return std::nullopt;
        out.host = std::string(authority.substr(1, close - 1));
        authority = authority.substr(close + 1);
        if (!authority.empty() && authority[0] == ':')
            out.port = std::atoi(std::string(authority.substr(1)).c_str());
    } else {
        size_t colon = authority.rfind(':');
        if (colon != std::string_view::npos) {
            out.port = std::atoi(std::string(authority.substr(colon + 1)).c_str());
            authority = authority.substr(0, colon);
        }
        out.host = std::string(authority);
    }
    if (out.host.empty() && out.scheme != "file")
        return std::nullopt;
    return out;
}

std::string resolveUrl(std::string_view base, std::string_view relative)
{
    if (relative.empty())
        return std::string(base);
    if (isUrl(relative))
        return std::string(relative);

    std::optional<Url> b = parseUrl(base);
    if (!b)
        return std::string(relative);

    std::string origin = b->scheme + "://" + b->host;
    if (b->port != defaultPort(b->scheme))
        origin += ":" + std::to_string(b->port);

    if (relative.rfind("//", 0) == 0)
        return b->scheme + ":" + std::string(relative);
    if (relative[0] == '#')
        return origin + b->path + std::string(relative);

    std::string basePath = b->path;
    if (relative[0] == '?') {
        size_t q = basePath.find('?');
        return origin + basePath.substr(0, q) + std::string(relative);
    }

    std::string fragment;
    std::string rel(relative);
    size_t hash = rel.find('#');
    if (hash != std::string::npos) {
        fragment = rel.substr(hash);
        rel.resize(hash);
    }

    std::string path;
    if (rel[0] == '/') {
        path = rel;
    } else {
        size_t q = basePath.find('?');
        std::string dir = basePath.substr(0, q);
        dir = dir.substr(0, dir.find_last_of('/') + 1);
        path = dir + rel;
    }
    return origin + removeDotSegments(path) + fragment;
}

std::string percentDecode(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = hexValue(s[i + 1]);
            int lo = hexValue(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += char(hi * 16 + lo);
                i += 2;
                continue;
            }
        }
        out += s[i];
    }
    return out;
}

} // namespace mdv::os::net
