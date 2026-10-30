#include "mdv/services/service_container.h"

#include "mdv/services/document_service.h"
#include "mdv/services/file_system_service.h"
#include "mdv/services/platform_service.h"
#include "mdv/services/settings_service.h"

namespace mdv::services {

ServiceContainer ServiceContainer::createDefault()
{
    ServiceContainer c;
    c.documents = std::make_unique<DocumentService>();
    c.settings = std::make_unique<SettingsService>();
    c.fileSystem = std::make_unique<FileSystemService>();
    c.platform = std::make_unique<PlatformService>();
    return c;
}

} // namespace mdv::services
