// ISettingsService implementation: settings.ini in the per-user app-data directory.
#pragma once

#include "mdv/interfaces/i_settings_service.h"

namespace mdv::services {

class SettingsService final : public interfaces::ISettingsService {
public:
    model::Settings load() override;
    bool save(const model::Settings& settings) override;
};

} // namespace mdv::services
