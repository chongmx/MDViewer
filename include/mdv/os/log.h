// Logging routed to the platform's native sink (stderr, OutputDebugString, logcat, console).
#pragma once

namespace mdv::os::log {

enum class Level { Debug, Info, Warning, Error };

void write(Level level, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

} // namespace mdv::os::log

#define MDV_LOG_DEBUG(...) ::mdv::os::log::write(::mdv::os::log::Level::Debug, __VA_ARGS__)
#define MDV_LOG_INFO(...)  ::mdv::os::log::write(::mdv::os::log::Level::Info, __VA_ARGS__)
#define MDV_LOG_WARN(...)  ::mdv::os::log::write(::mdv::os::log::Level::Warning, __VA_ARGS__)
#define MDV_LOG_ERROR(...) ::mdv::os::log::write(::mdv::os::log::Level::Error, __VA_ARGS__)
