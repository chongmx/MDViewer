// Windows HTTP(S) backend using WinHTTP (follows redirects automatically).
#include "../os_internal.h"

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

namespace mdv::os::detail {

namespace {

std::wstring widen(const std::string& s)
{
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring out(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), out.data(), n);
    return out;
}

struct Handle {
    HINTERNET h = nullptr;
    explicit Handle(HINTERNET handle) : h(handle) {}
    ~Handle()
    {
        if (h)
            WinHttpCloseHandle(h);
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    explicit operator bool() const { return h != nullptr; }
};

std::string winHttpError(const char* what)
{
    return std::string(what) + " failed (WinHTTP error " + std::to_string(GetLastError()) + ")";
}

} // namespace

net::HttpResponse httpGetBlocking(const std::string& url)
{
    net::HttpResponse response;
    std::optional<net::Url> parsed = net::parseUrl(url);
    if (!parsed || (parsed->scheme != "http" && parsed->scheme != "https")) {
        response.error = "Unsupported URL: " + url;
        return response;
    }

    Handle session(WinHttpOpen(L"MDViewer/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        response.error = winHttpError("WinHttpOpen");
        return response;
    }
    Handle connection(WinHttpConnect(session.h, widen(parsed->host).c_str(), INTERNET_PORT(parsed->port), 0));
    if (!connection) {
        response.error = winHttpError("WinHttpConnect");
        return response;
    }
    const DWORD flags = parsed->scheme == "https" ? WINHTTP_FLAG_SECURE : 0;
    Handle request(WinHttpOpenRequest(connection.h, L"GET", widen(parsed->path).c_str(), nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request) {
        response.error = winHttpError("WinHttpOpenRequest");
        return response;
    }
    if (!WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.h, nullptr)) {
        response.error = winHttpError("HTTP request");
        return response;
    }

    DWORD status = 0;
    DWORD size = sizeof(status);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &size, WINHTTP_NO_HEADER_INDEX);
    response.status = int(status);

    wchar_t contentType[256] = {};
    size = sizeof(contentType);
    if (WinHttpQueryHeaders(request.h, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX, contentType, &size,
                            WINHTTP_NO_HEADER_INDEX)) {
        for (const wchar_t* p = contentType; *p; ++p)
            response.contentType += char(*p < 128 ? *p : '?');
    }

    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.h, &available)) {
            response.error = winHttpError("WinHttpQueryDataAvailable");
            break;
        }
        if (available == 0)
            break;
        size_t offset = response.body.size();
        response.body.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.h, response.body.data() + offset, available, &read)) {
            response.error = winHttpError("WinHttpReadData");
            break;
        }
        response.body.resize(offset + read);
    }
    return response;
}

} // namespace mdv::os::detail
