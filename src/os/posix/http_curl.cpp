// libcurl HTTP(S) backend (Linux / macOS when libcurl is available).
#include "../os_internal.h"

#include <curl/curl.h>

#include <mutex>

namespace mdv::os::detail {

namespace {

size_t onWrite(char* data, size_t size, size_t count, void* user)
{
    static_cast<std::string*>(user)->append(data, size * count);
    return size * count;
}

} // namespace

net::HttpResponse httpGetBlocking(const std::string& url)
{
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });

    net::HttpResponse response;
    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "curl_easy_init failed";
        return response;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MDViewer/1.0");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &onWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        response.error = curl_easy_strerror(rc);
    } else {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        response.status = int(status);
        char* type = nullptr;
        if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &type) == CURLE_OK && type)
            response.contentType = type;
    }
    curl_easy_cleanup(curl);
    return response;
}

} // namespace mdv::os::detail
