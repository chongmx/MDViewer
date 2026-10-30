#include "mdv/services/settings_service.h"

#include "mdv/os/os.h"

namespace mdv::services {

namespace {

std::string settingsPath() { return os::fs::path::join(os::fs::appDataDirectory(), "settings.ini"); }

} // namespace

model::Settings SettingsService::load()
{
    std::optional<std::string> data = os::fs::readFile(settingsPath());
    return data ? model::Settings::deserialize(*data) : model::Settings();
}

bool SettingsService::save(const model::Settings& settings)
{
    return os::fs::writeFile(settingsPath(), settings.serialize());
}

} // namespace mdv::services
