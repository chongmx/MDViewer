// Font loading (bundled TTFs read through the OS layer, Font Awesome icons merged in).
#pragma once

#include "imgui.h"

namespace mdv::view {

struct Fonts {
    ImFont* body = nullptr; // proportional text + icons (also used for the UI chrome)
    ImFont* mono = nullptr; // code
    bool hasIcons = false;

    // Must be called after ImGui::CreateContext() and before the first frame.
    static Fonts load();
};

} // namespace mdv::view
