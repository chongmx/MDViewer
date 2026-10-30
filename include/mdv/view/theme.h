// Colour palettes for the ImGui chrome and the markdown content.
#pragma once

#include "imgui.h"

namespace mdv::view {

struct Palette {
    ImU32 text;
    ImU32 textDim;
    ImU32 heading;
    ImU32 link;
    ImU32 linkHover;
    ImU32 codeText;
    ImU32 codeBg;
    ImU32 codeBorder;
    ImU32 quoteBar;
    ImU32 quoteText;
    ImU32 rule;
    ImU32 tableHeaderBg;
    ImU32 highlight;
    ImU32 highlightCurrent;
    ImU32 checkbox;
    ImU32 syntaxKeyword;
    ImU32 syntaxString;
    ImU32 syntaxComment;
    ImU32 syntaxNumber;
    ImU32 error;
    ImVec4 contentBg;
    ImVec4 clearColor;
};

// Applies ImGui style colours for the theme and returns the matching content palette.
Palette applyTheme(bool dark);

} // namespace mdv::view
