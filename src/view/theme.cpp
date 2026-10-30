#include "mdv/view/theme.h"

namespace mdv::view {

namespace {

ImU32 rgba(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }
ImVec4 vec(int r, int g, int b, int a = 255) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f); }

} // namespace

Palette applyTheme(bool dark)
{
    ImGuiStyle& style = ImGui::GetStyle();
    Palette p{};
    if (dark) {
        ImGui::StyleColorsDark(&style);
        ImVec4* c = style.Colors;
        c[ImGuiCol_WindowBg] = vec(22, 27, 34);
        c[ImGuiCol_ChildBg] = vec(13, 17, 23);
        c[ImGuiCol_PopupBg] = vec(22, 27, 34, 250);
        c[ImGuiCol_MenuBarBg] = vec(22, 27, 34);
        c[ImGuiCol_Border] = vec(48, 54, 61);
        c[ImGuiCol_FrameBg] = vec(13, 17, 23);
        c[ImGuiCol_FrameBgHovered] = vec(33, 38, 45);
        c[ImGuiCol_Button] = vec(33, 38, 45);
        c[ImGuiCol_ButtonHovered] = vec(48, 54, 61);
        c[ImGuiCol_ButtonActive] = vec(56, 139, 253, 160);
        c[ImGuiCol_Header] = vec(56, 139, 253, 70);
        c[ImGuiCol_HeaderHovered] = vec(56, 139, 253, 110);
        c[ImGuiCol_HeaderActive] = vec(56, 139, 253, 160);
        c[ImGuiCol_TableBorderStrong] = vec(48, 54, 61);
        c[ImGuiCol_TableBorderLight] = vec(48, 54, 61);
        c[ImGuiCol_TableRowBgAlt] = vec(255, 255, 255, 8);

        p.text = rgba(230, 237, 243);
        p.textDim = rgba(139, 148, 158);
        p.heading = rgba(240, 246, 252);
        p.link = rgba(88, 166, 255);
        p.linkHover = rgba(121, 192, 255);
        p.codeText = rgba(230, 237, 243);
        p.codeBg = rgba(110, 118, 129, 60);
        p.codeBorder = rgba(48, 54, 61);
        p.quoteBar = rgba(61, 68, 77);
        p.quoteText = rgba(139, 148, 158);
        p.rule = rgba(48, 54, 61);
        p.tableHeaderBg = rgba(22, 27, 34);
        p.highlight = rgba(187, 128, 9, 110);
        p.highlightCurrent = rgba(255, 166, 0, 200);
        p.checkbox = rgba(88, 166, 255);
        p.syntaxKeyword = rgba(255, 123, 114);
        p.syntaxString = rgba(165, 214, 255);
        p.syntaxComment = rgba(139, 148, 158);
        p.syntaxNumber = rgba(121, 192, 255);
        p.error = rgba(248, 81, 73);
        p.contentBg = vec(13, 17, 23);
        p.clearColor = vec(13, 17, 23);
    } else {
        ImGui::StyleColorsLight(&style);
        ImVec4* c = style.Colors;
        c[ImGuiCol_WindowBg] = vec(246, 248, 250);
        c[ImGuiCol_ChildBg] = vec(255, 255, 255);
        c[ImGuiCol_PopupBg] = vec(255, 255, 255, 252);
        c[ImGuiCol_MenuBarBg] = vec(246, 248, 250);
        c[ImGuiCol_Border] = vec(208, 215, 222);
        c[ImGuiCol_FrameBg] = vec(255, 255, 255);
        c[ImGuiCol_FrameBgHovered] = vec(243, 244, 246);
        c[ImGuiCol_Button] = vec(240, 242, 245);
        c[ImGuiCol_ButtonHovered] = vec(225, 228, 232);
        c[ImGuiCol_ButtonActive] = vec(9, 105, 218, 120);
        c[ImGuiCol_Header] = vec(9, 105, 218, 40);
        c[ImGuiCol_HeaderHovered] = vec(9, 105, 218, 70);
        c[ImGuiCol_HeaderActive] = vec(9, 105, 218, 110);
        c[ImGuiCol_TableBorderStrong] = vec(208, 215, 222);
        c[ImGuiCol_TableBorderLight] = vec(208, 215, 222);
        c[ImGuiCol_TableRowBgAlt] = vec(0, 0, 0, 8);

        p.text = rgba(31, 35, 40);
        p.textDim = rgba(101, 109, 118);
        p.heading = rgba(31, 35, 40);
        p.link = rgba(9, 105, 218);
        p.linkHover = rgba(5, 80, 174);
        p.codeText = rgba(31, 35, 40);
        p.codeBg = rgba(175, 184, 193, 50);
        p.codeBorder = rgba(208, 215, 222);
        p.quoteBar = rgba(208, 215, 222);
        p.quoteText = rgba(101, 109, 118);
        p.rule = rgba(208, 215, 222);
        p.tableHeaderBg = rgba(246, 248, 250);
        p.highlight = rgba(255, 223, 93, 160);
        p.highlightCurrent = rgba(255, 150, 0, 200);
        p.checkbox = rgba(9, 105, 218);
        p.syntaxKeyword = rgba(207, 34, 46);
        p.syntaxString = rgba(10, 48, 105);
        p.syntaxComment = rgba(110, 119, 129);
        p.syntaxNumber = rgba(5, 80, 174);
        p.error = rgba(207, 34, 46);
        p.contentBg = vec(255, 255, 255);
        p.clearColor = vec(255, 255, 255);
    }
    return p;
}

} // namespace mdv::view
