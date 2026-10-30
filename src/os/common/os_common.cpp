#include "../os_internal.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>

namespace mdv::os {

namespace {
std::vector<std::string> g_arguments;
bool g_initialized = false;
} // namespace

bool init(const InitParams& params)
{
    if (g_initialized)
        return true;
    g_arguments.clear();
    g_initialized = detail::platformInit(params, g_arguments);
    return g_initialized;
}

void shutdown()
{
    if (!g_initialized)
        return;
    detail::platformShutdown();
    g_initialized = false;
}

const std::vector<std::string>& arguments()
{
    return g_arguments;
}

double now()
{
    using Clock = std::chrono::steady_clock;
    static const Clock::time_point start = Clock::now();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

namespace log {

void write(Level level, const char* fmt, ...)
{
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    detail::writeLogSink(level, buffer);
}

} // namespace log

namespace net {

bool supportsHttps()
{
    return detail::platformSupportsHttps();
}

} // namespace net

namespace detail {

std::string toLowerAscii(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}

std::string shellQuote(const std::string& s)
{
    std::string out = "'";
    for (char c : s) {
        if (c == '\'')
            out += "'\\''";
        else
            out += c;
    }
    out += "'";
    return out;
}

} // namespace detail

} // namespace mdv::os
