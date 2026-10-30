// Contract for persisting user preferences.
#pragma once

#include "mdv/model/settings.h"

namespace mdv::interfaces {

class ISettingsService {
public:
    virtual ~ISettingsService() = default;
    virtual model::Settings load() = 0;
    virtual bool save(const model::Settings& settings) = 0;
};

} // namespace mdv::interfaces
