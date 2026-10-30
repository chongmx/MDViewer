#include "mdv/services/platform_service.h"

#include "mdv/os/os.h"

namespace mdv::services {

bool PlatformService::hasNativeFilePicker() const { return os::dialog::hasNativeOpenFile(); }

void PlatformService::pickMarkdownFile(interfaces::PickFileCallback callback)
{
    os::dialog::openFile("Open Markdown document", {{"Markdown", {"md", "markdown", "mdown", "mkd", "txt"}}},
                         std::move(callback));
}

bool PlatformService::openExternal(const std::string& urlOrPath) { return os::shell::openExternal(urlOrPath); }

double PlatformService::now() const { return os::now(); }
std::string PlatformService::platformName() const { return os::osName(); }
std::string PlatformService::architectureName() const { return os::archName(); }
bool PlatformService::supportsHttps() const { return os::net::supportsHttps(); }
bool PlatformService::isDesktop() const { return os::isDesktop(); }
bool PlatformService::isMobile() const { return os::isMobile(); }

} // namespace mdv::services
