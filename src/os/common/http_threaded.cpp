// httpGet() for platforms with threads: run the blocking backend on a worker thread.
#include "../os_internal.h"

#include <utility>

namespace mdv::os::net {

void httpGet(const std::string& url, HttpCallback callback)
{
    dispatch::runAsync([url, callback = std::move(callback)]() mutable {
        HttpResponse response = detail::httpGetBlocking(url);
        dispatch::postToMain([callback = std::move(callback), response = std::move(response)]() mutable {
            callback(std::move(response));
        });
    });
}

} // namespace mdv::os::net
