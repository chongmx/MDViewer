// User preferences (data only; persisted by ISettingsService).
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace mdv::model {

struct Settings {
    bool darkTheme = true;
    bool showToc = true;
    float zoom = 1.0f;
    std::vector<std::string> recentDocuments; // most recent first

    static constexpr std::size_t kMaxRecent = 12;
    static constexpr float kMinZoom = 0.5f;
    static constexpr float kMaxZoom = 3.0f;

    void addRecent(const std::string& location);

    std::string serialize() const;                    // "key=value" lines
    static Settings deserialize(const std::string& text);
};

} // namespace mdv::model
