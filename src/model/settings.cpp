#include "mdv/model/settings.h"

#include <algorithm>
#include <cstdlib>

namespace mdv::model {

void Settings::addRecent(const std::string& location)
{
    if (location.rfind("asset:", 0) == 0)
        return;
    recentDocuments.erase(std::remove(recentDocuments.begin(), recentDocuments.end(), location), recentDocuments.end());
    recentDocuments.insert(recentDocuments.begin(), location);
    if (recentDocuments.size() > kMaxRecent)
        recentDocuments.resize(kMaxRecent);
}

std::string Settings::serialize() const
{
    std::string out;
    out += "dark_theme=" + std::string(darkTheme ? "1" : "0") + "\n";
    out += "show_toc=" + std::string(showToc ? "1" : "0") + "\n";
    out += "zoom=" + std::to_string(zoom) + "\n";
    for (const std::string& r : recentDocuments)
        out += "recent=" + r + "\n";
    return out;
}

Settings Settings::deserialize(const std::string& text)
{
    Settings s;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (key == "dark_theme")
            s.darkTheme = value == "1";
        else if (key == "show_toc")
            s.showToc = value == "1";
        else if (key == "zoom")
            s.zoom = std::clamp(float(std::atof(value.c_str())), kMinZoom, kMaxZoom);
        else if (key == "recent" && !value.empty() && s.recentDocuments.size() < kMaxRecent)
            s.recentDocuments.push_back(value);
    }
    return s;
}

} // namespace mdv::model
