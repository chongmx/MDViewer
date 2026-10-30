// IPlatformService implementation over the OS layer.
#pragma once

#include "mdv/interfaces/i_platform_service.h"

namespace mdv::services {

class PlatformService final : public interfaces::IPlatformService {
public:
    bool hasNativeFilePicker() const override;
    void pickMarkdownFile(interfaces::PickFileCallback callback) override;
    bool openExternal(const std::string& urlOrPath) override;

    double now() const override;
    std::string platformName() const override;
    std::string architectureName() const override;
    bool supportsHttps() const override;
    bool isDesktop() const override;
    bool isMobile() const override;
};

} // namespace mdv::services
