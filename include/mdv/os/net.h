// Networking: asynchronous HTTP(S) GET plus URL helpers.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace mdv::os::net {

struct HttpResponse {
    int status = 0;
    std::string body;
    std::string contentType;
    std::string error; // transport-level error (DNS, TLS, unsupported scheme, ...)

    bool ok() const { return error.empty() && status >= 200 && status < 300; }
};

using HttpCallback = std::function<void(HttpResponse)>;

// Starts an HTTP GET in the background, following redirects.
// The callback is always invoked on the main thread (from dispatch::pump()).
void httpGet(const std::string& url, HttpCallback callback);

// Whether this build can fetch https:// URLs.
bool supportsHttps();

struct Url {
    std::string scheme;   // lower-case, e.g. "https"
    std::string host;
    int port = 0;         // explicit or scheme default
    std::string path;     // path + query, always starts with '/'
    std::string fragment; // without '#'
};

std::optional<Url> parseUrl(std::string_view url);
bool isUrl(std::string_view s); // "<scheme>://..." or "mailto:"
// Resolves a relative reference against an absolute base URL (RFC 3986, simplified).
std::string resolveUrl(std::string_view base, std::string_view relative);
std::string percentDecode(std::string_view s);

} // namespace mdv::os::net
