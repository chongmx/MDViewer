// Minimal plain-HTTP backend over BSD sockets, used when no TLS-capable library is available.
#include "../os_internal.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace mdv::os::detail {

namespace {

constexpr int kMaxRedirects = 10;

std::string headerValue(const std::string& headers, const char* name)
{
    std::string lower = toLowerAscii(headers);
    std::string key = std::string("\r\n") + toLowerAscii(name) + ":";
    size_t pos = lower.find(key);
    if (pos == std::string::npos)
        return {};
    pos += key.size();
    size_t end = headers.find("\r\n", pos);
    std::string value = headers.substr(pos, end - pos);
    size_t first = value.find_first_not_of(" \t");
    return first == std::string::npos ? std::string() : value.substr(first);
}

std::string decodeChunked(const std::string& body)
{
    std::string out;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t lineEnd = body.find("\r\n", pos);
        if (lineEnd == std::string::npos)
            break;
        size_t chunk = std::strtoul(body.substr(pos, lineEnd - pos).c_str(), nullptr, 16);
        if (chunk == 0)
            break;
        pos = lineEnd + 2;
        out.append(body, pos, chunk);
        pos += chunk + 2;
    }
    return out;
}

net::HttpResponse fetchOnce(const net::Url& url, std::string& location)
{
    net::HttpResponse response;

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    int rc = ::getaddrinfo(url.host.c_str(), std::to_string(url.port).c_str(), &hints, &result);
    if (rc != 0) {
        response.error = std::string("DNS lookup failed: ") + gai_strerror(rc);
        return response;
    }
    int fd = -1;
    for (addrinfo* ai = result; ai; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;
        if (::connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
            break;
        ::close(fd);
        fd = -1;
    }
    ::freeaddrinfo(result);
    if (fd < 0) {
        response.error = "Could not connect to " + url.host;
        return response;
    }

    // HTTP/1.0 keeps the response un-chunked and closes the connection when done.
    std::string request = "GET " + url.path + " HTTP/1.0\r\nHost: " + url.host +
                          "\r\nUser-Agent: MDViewer/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n";
    size_t sent = 0;
    while (sent < request.size()) {
        ssize_t n = ::send(fd, request.data() + sent, request.size() - sent, 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            response.error = "send failed";
            ::close(fd);
            return response;
        }
        sent += size_t(n);
    }

    std::string raw;
    char buf[16 * 1024];
    for (;;) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        raw.append(buf, size_t(n));
    }
    ::close(fd);

    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        response.error = "Malformed HTTP response";
        return response;
    }
    std::string headers = raw.substr(0, headerEnd + 2);
    size_t sp = headers.find(' ');
    response.status = sp == std::string::npos ? 0 : std::atoi(headers.c_str() + sp + 1);
    response.contentType = headerValue(headers, "Content-Type");
    response.body = raw.substr(headerEnd + 4);
    if (toLowerAscii(headerValue(headers, "Transfer-Encoding")).find("chunked") != std::string::npos)
        response.body = decodeChunked(response.body);
    location = headerValue(headers, "Location");
    return response;
}

} // namespace

net::HttpResponse httpGetBlocking(const std::string& urlString)
{
    std::string current = urlString;
    for (int redirect = 0; redirect <= kMaxRedirects; ++redirect) {
        std::optional<net::Url> url = net::parseUrl(current);
        net::HttpResponse response;
        if (!url || url->scheme != "http") {
            response.error = (url && url->scheme == "https")
                                 ? "HTTPS is not supported in this build (rebuild with libcurl)"
                                 : "Unsupported URL: " + current;
            return response;
        }
        std::string location;
        response = fetchOnce(*url, location);
        if (!response.error.empty() || response.status < 300 || response.status >= 400 || location.empty())
            return response;
        current = net::resolveUrl(current, location);
    }
    net::HttpResponse tooMany;
    tooMany.error = "Too many redirects";
    return tooMany;
}

} // namespace mdv::os::detail
