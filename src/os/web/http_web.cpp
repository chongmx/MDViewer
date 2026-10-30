// Web HTTP backend using the browser's fetch() via emscripten_fetch (subject to CORS).
#include "../os_internal.h"

#include <emscripten/fetch.h>

#include <cstring>

namespace mdv::os::net {

namespace {

struct Request {
    HttpCallback callback;
};

void complete(emscripten_fetch_t* fetch, bool success)
{
    auto* request = static_cast<Request*>(fetch->userData);
    HttpResponse response;
    response.status = fetch->status;
    if (fetch->data && fetch->numBytes > 0)
        response.body.assign(fetch->data, size_t(fetch->numBytes));
    if (!success && response.status == 0)
        response.error = "Network error (the server may not allow cross-origin requests)";
    emscripten_fetch_close(fetch);

    dispatch::postToMain([callback = std::move(request->callback), response = std::move(response)]() mutable {
        callback(std::move(response));
    });
    delete request;
}

} // namespace

void httpGet(const std::string& url, HttpCallback callback)
{
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    std::strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.userData = new Request{std::move(callback)};
    attr.onsuccess = [](emscripten_fetch_t* f) { complete(f, true); };
    attr.onerror = [](emscripten_fetch_t* f) { complete(f, false); };
    emscripten_fetch(&attr, url.c_str());
}

} // namespace mdv::os::net
