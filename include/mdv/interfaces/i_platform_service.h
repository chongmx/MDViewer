// Contract for host-environment interactions (dialogs, external apps, clock, platform info).
#pragma once

#include <functional>
#include <optional>
#include <string>

namespace mdv::interfaces {

using PickFileCallback = std::function<void(std::optional<std::string> path)>;

class IPlatformService {
public:
    virtual ~IPlatformService() = default;

    // False where no native picker exists; the UI then shows its in-app file browser.
    virtual bool hasNativeFilePicker() const = 0;
    // Callback runs on the main thread; nullopt when cancelled.
    virtual void pickMarkdownFile(PickFileCallback callback) = 0;
    // Opens a URL or file with the system's default handler.
    virtual bool openExternal(const std::string& urlOrPath) = 0;

    virtual double now() const = 0; // monotonic seconds
    virtual std::string platformName() const = 0;     // "Windows", "Web", ...
    virtual std::string architectureName() const = 0; // "x86_64", "aarch64", "wasm32", ...
    virtual bool supportsHttps() const = 0;
    virtual bool isDesktop() const = 0;
    virtual bool isMobile() const = 0;
};

} // namespace mdv::interfaces
