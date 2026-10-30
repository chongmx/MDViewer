#include "mdv/view/fonts.h"

#include "mdv/os/os.h"

#include "IconsFontAwesome6.h"

#include <cstring>

namespace mdv::view {

namespace {

constexpr float kBaseSize = 16.0f;

// ImGui takes ownership of TTF data and frees it with IM_FREE, so copy into IM_ALLOC'd memory.
ImFont* addFont(const char* asset, const ImFontConfig& config, const ImWchar* ranges = nullptr)
{
    std::optional<std::string> data = os::fs::readAsset(asset);
    if (!data || data->empty())
        return nullptr;
    void* buffer = IM_ALLOC(data->size());
    std::memcpy(buffer, data->data(), data->size());
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(buffer, int(data->size()), kBaseSize, &config, ranges);
}

} // namespace

Fonts Fonts::load()
{
    Fonts fonts;
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;

    ImFontConfig textConfig;
    textConfig.OversampleH = 2;
    fonts.body = addFont("fonts/Roboto-Medium.ttf", textConfig);
    if (!fonts.body)
        fonts.body = atlas->AddFontDefaultVector();

    static const ImWchar iconRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
    ImFontConfig iconConfig;
    iconConfig.MergeMode = true;
    iconConfig.PixelSnapH = true;
    iconConfig.GlyphMinAdvanceX = kBaseSize; // fixed-width icons align nicely in buttons
    fonts.hasIcons = addFont("fonts/" FONT_ICON_FILE_NAME_FAS, iconConfig, iconRanges) != nullptr;

    fonts.mono = addFont("fonts/Cousine-Regular.ttf", ImFontConfig());
    if (!fonts.mono)
        fonts.mono = fonts.body;

    return fonts;
}

} // namespace mdv::view
