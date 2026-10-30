// Composition root: owns the concrete services and hands out their interfaces.
#pragma once

#include "mdv/interfaces/i_document_service.h"
#include "mdv/interfaces/i_file_system_service.h"
#include "mdv/interfaces/i_platform_service.h"
#include "mdv/interfaces/i_settings_service.h"

#include <memory>

namespace mdv::services {

struct ServiceContainer {
    std::unique_ptr<interfaces::IDocumentService> documents;
    std::unique_ptr<interfaces::ISettingsService> settings;
    std::unique_ptr<interfaces::IFileSystemService> fileSystem;
    std::unique_ptr<interfaces::IPlatformService> platform;

    // Production services backed by the OS layer.
    static ServiceContainer createDefault();
};

} // namespace mdv::services
