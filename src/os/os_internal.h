// Private interface between the portable OS-layer code and the per-platform backends.
#pragma once

#include "mdv/os/os.h"

#include <string>
#include <vector>

namespace mdv::os::detail {

// Implemented by every platform backend.
bool platformInit(const InitParams& params, std::vector<std::string>& outArguments);
void platformShutdown();
void writeLogSink(log::Level level, const char* message);
bool platformSupportsHttps();

#if defined(MDV_HAS_THREADS)
// Blocking GET used by the threaded httpGet() implementation. Must follow redirects.
net::HttpResponse httpGetBlocking(const std::string& url);
#endif

// Shared helpers.
std::string toLowerAscii(std::string s);
std::string shellQuote(const std::string& s); // single-quote for /bin/sh

} // namespace mdv::os::detail
