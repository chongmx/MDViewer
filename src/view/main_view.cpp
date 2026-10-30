#include "mdv/view/main_view.h"

#include "IconsFontAwesome6.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mdv::view {

namespace {

constexpr float kTocWidth = 260.0f;
constexpr float kNarrowWidth = 720.0f;
constexpr float kBodyScale = 1.05f;     // body text relative to the UI font
constexpr float kMaxColumnEms = 50.0f;  // readable line length

} // namespace

MainView::MainView(viewmodel::ViewerViewModel& vm, const Fonts& fonts, float uiScale)
    : vm_(vm), fonts_(fonts), uiScale_(uiScale), renderer_(fonts)
{
    palette_ = applyTheme(vm_.darkTheme.get());
    lastLocation_ = vm_.document.get() ? vm_.document.get()->location : std::string();
    subscriptions_.push_back(vm_.darkTheme.subscribe([this](const bool& dark) { palette_ = applyTheme(dark); }));
    subscriptions_.push_back(vm_.document.subscribe([this](const model::DocumentPtr& doc) {
        renderer_.invalidate();
        if (doc && doc->location != lastLocation_) {
            lastLocation_ = doc->location;
            resetScroll_ = true;
        }
    }));
    subscriptions_.push_back(vm_.scrollToAnchorRequested.subscribe([this](const std::string& anchor) {
        pendingAnchor_ = anchor;
        pendingAnchorFrames_ = 0;
    }));
    subscriptions_.push_back(vm_.scrollToMatchRequested.subscribe([this]() { pendingMatchScroll_ = true; }));
    subscriptions_.push_back(vm_.fileBrowserRequested.subscribe([this]() { openFileBrowser_ = true; }));
    subscriptions_.push_back(vm_.openUrlRequested.subscribe([this]() { openUrlPopup_ = true; }));
}

const char* MainView::label(const char* icon, const char* fallback) const
{
    return fonts_.hasIcons ? icon : fallback;
}

bool MainView::narrow() const
{
    return ImGui::GetMainViewport()->WorkSize.x < kNarrowWidth * uiScale_;
}

bool MainView::iconButton(const char* icon, const char* fallback, const char* tooltip, bool enabled)
{
    ImGui::BeginDisabled(!enabled);
    bool clicked = ImGui::Button(label(icon, fallback));
    ImGui::EndDisabled();
    if (tooltip && !vm_.platform().isMobile())
        ImGui::SetItemTooltip("%s", tooltip);
    return clicked;
}

void MainView::commandButton(const char* icon, const char* fallback, const char* tooltip, const viewmodel::Command& command)
{
    if (iconButton(icon, fallback, tooltip, command.canExecute()))
        command.execute();
}

// ---- Frame ------------------------------------------------------------------------------------

void MainView::render()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("##main", nullptr, flags);
    ImGui::PopStyleVar(3);

    handleShortcuts();
    renderMenuBar();
    renderToolbar();

    if (!vm_.errorText.get().empty()) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImGui::GetStyle().FramePadding);
        ImGui::Indent(ImGui::GetStyle().WindowPadding.x + 8.0f * uiScale_);
        ImGui::PushStyleColor(ImGuiCol_Text, palette_.error);
        ImGui::TextWrapped("%s %s", label(ICON_FA_TRIANGLE_EXCLAMATION, "!"), vm_.errorText.get().c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::SmallButton(label(ICON_FA_XMARK "##dismiss", "x##dismiss")))
            vm_.errorText.set({});
        ImGui::Unindent(ImGui::GetStyle().WindowPadding.x + 8.0f * uiScale_);
        ImGui::PopStyleVar();
    }

    const float statusHeight = ImGui::GetFrameHeight();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float bodyHeight = std::max(1.0f, avail.y - statusHeight);
    const model::DocumentPtr& doc = vm_.document.get();
    const bool hasToc = doc && !doc->headings.empty();

    if (hasToc && narrow() && tocOverlay_) {
        renderToc(avail.x, bodyHeight);
    } else {
        float contentWidth = avail.x;
        if (hasToc && !narrow() && vm_.showToc.get()) {
            const float tocWidth = std::min(kTocWidth * uiScale_, avail.x * 0.4f);
            renderToc(tocWidth, bodyHeight);
            ImGui::SameLine(0.0f, 0.0f);
            contentWidth -= tocWidth;
        }
        renderContent(contentWidth, bodyHeight);
    }
    renderStatusBar();

    renderOpenUrlPopup();
    renderFileBrowserPopup();
    renderAboutPopup();
    ImGui::End();
}

// ---- Input --------------------------------------------------------------------------------------

void MainView::handleShortcuts()
{
    const ImGuiInputFlags global = ImGuiInputFlags_RouteGlobal;
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, global))
        vm_.openFileCommand.execute();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_L, global))
        vm_.openUrlCommand.execute();
    if (ImGui::Shortcut(ImGuiKey_F5, global) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_R, global))
        vm_.reloadCommand.execute();
    if (ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_LeftArrow, global) || ImGui::IsMouseClicked(3))
        vm_.backCommand.execute();
    if (ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_RightArrow, global) || ImGui::IsMouseClicked(4))
        vm_.forwardCommand.execute();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F, global))
        focusSearch_ = true;
    if (ImGui::Shortcut(ImGuiKey_F3, global))
        vm_.nextMatchCommand.execute();
    if (ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_F3, global))
        vm_.previousMatchCommand.execute();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Equal, global) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadAdd, global))
        vm_.zoomInCommand.execute();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Minus, global) ||
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadSubtract, global))
        vm_.zoomOutCommand.execute();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_0, global))
        vm_.zoomResetCommand.execute();
}

// ---- Chrome ---------------------------------------------------------------------------------------

void MainView::renderMenuBar()
{
    if (!ImGui::BeginMenuBar())
        return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open...", "Ctrl+O"))
            vm_.openFileCommand.execute();
        if (ImGui::MenuItem("Open URL...", "Ctrl+L"))
            vm_.openUrlCommand.execute();
        if (ImGui::BeginMenu("Open Recent", !vm_.recentDocuments().empty())) {
            std::string selected;
            for (const std::string& recent : vm_.recentDocuments())
                if (ImGui::MenuItem(recent.c_str()))
                    selected = recent;
            ImGui::Separator();
            if (ImGui::MenuItem("Clear list"))
                vm_.clearRecentDocuments();
            ImGui::EndMenu();
            if (!selected.empty())
                vm_.open(selected);
        }
        if (ImGui::MenuItem("Reload", "F5", false, vm_.reloadCommand.canExecute()))
            vm_.reloadCommand.execute();
        if (vm_.platform().isDesktop()) {
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
                quitRequested_ = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        bool toc = vm_.showToc.get();
        if (ImGui::MenuItem("Table of Contents", nullptr, &toc))
            vm_.showToc.set(toc);
        bool dark = vm_.darkTheme.get();
        if (ImGui::MenuItem("Dark Theme", nullptr, &dark))
            vm_.darkTheme.set(dark);
        ImGui::Separator();
        if (ImGui::MenuItem("Zoom In", "Ctrl+=", false, vm_.zoomInCommand.canExecute()))
            vm_.zoomInCommand.execute();
        if (ImGui::MenuItem("Zoom Out", "Ctrl+-", false, vm_.zoomOutCommand.canExecute()))
            vm_.zoomOutCommand.execute();
        if (ImGui::MenuItem("Actual Size", "Ctrl+0"))
            vm_.zoomResetCommand.execute();
        ImGui::Separator();
        if (ImGui::MenuItem("Find...", "Ctrl+F"))
            focusSearch_ = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Go")) {
        if (ImGui::MenuItem("Back", "Alt+Left", false, vm_.backCommand.canExecute()))
            vm_.backCommand.execute();
        if (ImGui::MenuItem("Forward", "Alt+Right", false, vm_.forwardCommand.canExecute()))
            vm_.forwardCommand.execute();
        if (ImGui::MenuItem("Welcome Page"))
            vm_.homeCommand.execute();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About MDViewer"))
            openAbout_ = true;
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

void MainView::renderToolbar()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const float height = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * uiScale_, style.WindowPadding.y));
    ImGui::BeginChild("##toolbar", ImVec2(0.0f, height), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    const bool isNarrow = narrow();
    commandButton(ICON_FA_ARROW_LEFT, "<", "Back (Alt+Left)", vm_.backCommand);
    ImGui::SameLine();
    commandButton(ICON_FA_ARROW_RIGHT, ">", "Forward (Alt+Right)", vm_.forwardCommand);
    ImGui::SameLine();
    commandButton(ICON_FA_ROTATE_RIGHT, "Reload", "Reload (F5)", vm_.reloadCommand);
    ImGui::SameLine();
    if (!isNarrow) {
        commandButton(ICON_FA_HOUSE, "Home", "Welcome page", vm_.homeCommand);
        ImGui::SameLine();
    }
    commandButton(ICON_FA_FOLDER_OPEN, "Open", "Open file (Ctrl+O)", vm_.openFileCommand);
    ImGui::SameLine();
    if (!isNarrow) {
        commandButton(ICON_FA_GLOBE, "URL", "Open URL (Ctrl+L)", vm_.openUrlCommand);
        ImGui::SameLine();
    }
    const bool tocActive = isNarrow ? tocOverlay_ : vm_.showToc.get();
    if (tocActive)
        ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
    if (iconButton(ICON_FA_LIST, "TOC", "Table of contents")) {
        if (isNarrow)
            tocOverlay_ = !tocOverlay_;
        else
            vm_.showToc.set(!vm_.showToc.get());
    }
    if (tocActive)
        ImGui::PopStyleColor();
    ImGui::SameLine();

    // Right-aligned group: zoom + theme (desktop widths only).
    const float buttonW = ImGui::GetFrameHeight() + style.FramePadding.x;
    const float rightGroup = isNarrow ? 0.0f : buttonW * 3.0f + style.ItemSpacing.x * 3.0f;

    // Search box takes the remaining space.
    char countText[32] = "";
    if (!vm_.searchQuery.get().empty())
        std::snprintf(countText, sizeof(countText), "%d/%d", vm_.matchCount.get() ? vm_.currentMatch.get() + 1 : 0,
                      vm_.matchCount.get());
    const float countW = ImGui::CalcTextSize("0000/0000").x;
    const float searchW = ImGui::GetContentRegionAvail().x - rightGroup - buttonW * 2.0f - countW - style.ItemSpacing.x * 4.0f;
    ImGui::SetNextItemWidth(std::max(60.0f * uiScale_, searchW));
    if (focusSearch_) {
        ImGui::SetKeyboardFocusHere();
        focusSearch_ = false;
    }
    std::string hint = std::string(label(ICON_FA_MAGNIFYING_GLASS "  ", "")) + "Search";
    const bool enter = ImGui::InputTextWithHint("##search", hint.c_str(), &searchBuffer_, ImGuiInputTextFlags_EnterReturnsTrue);
    const bool searchActive = ImGui::IsItemActive();
    if (searchActive && ImGui::IsKeyPressed(ImGuiKey_Escape))
        searchBuffer_.clear();
    if (searchBuffer_ != vm_.searchQuery.get()) {
        if (searchActive || enter || searchBuffer_.empty())
            vm_.setSearchQuery(searchBuffer_);
        else
            searchBuffer_ = vm_.searchQuery.get();
    }
    if (enter) {
        if (ImGui::GetIO().KeyShift)
            vm_.previousMatchCommand.execute();
        else
            vm_.nextMatchCommand.execute();
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::SameLine();
    commandButton(ICON_FA_CHEVRON_UP, "^", "Previous match (Shift+F3)", vm_.previousMatchCommand);
    ImGui::SameLine();
    commandButton(ICON_FA_CHEVRON_DOWN, "v", "Next match (F3)", vm_.nextMatchCommand);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", countText);

    if (!isNarrow) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - rightGroup + style.ItemSpacing.x));
        commandButton(ICON_FA_MAGNIFYING_GLASS_MINUS, "-", "Zoom out (Ctrl+-)", vm_.zoomOutCommand);
        ImGui::SameLine();
        commandButton(ICON_FA_MAGNIFYING_GLASS_PLUS, "+", "Zoom in (Ctrl+=)", vm_.zoomInCommand);
        ImGui::SameLine();
        if (iconButton(vm_.darkTheme.get() ? ICON_FA_SUN : ICON_FA_MOON, "Theme", "Toggle light/dark theme"))
            vm_.darkTheme.set(!vm_.darkTheme.get());
    }
    ImGui::EndChild();
}

void MainView::renderToc(float width, float height)
{
    const model::DocumentPtr& doc = vm_.document.get();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * uiScale_, 10.0f * uiScale_));
    ImGui::BeginChild("##toc", ImVec2(width, height), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    ImGui::TextDisabled("CONTENTS");
    ImGui::Separator();

    // The active section is the last heading above the top of the viewport.
    int active = -1;
    for (size_t i = 0; i < doc->headings.size(); ++i)
        if (std::optional<float> y = renderer_.anchorY(doc->headings[i].anchor); y && *y <= contentScrollY_ + 4.0f)
            active = int(i);

    int minLevel = 6;
    for (const model::HeadingRef& h : doc->headings)
        minLevel = std::min(minLevel, h.level);
    std::string target;
    for (size_t i = 0; i < doc->headings.size(); ++i) {
        const model::HeadingRef& h = doc->headings[i];
        const float indent = float(h.level - minLevel) * 12.0f * uiScale_;
        if (indent > 0.0f)
            ImGui::Indent(indent);
        ImGui::PushID(int(i));
        if (h.level == minLevel)
            ImGui::PushStyleColor(ImGuiCol_Text, palette_.heading);
        else
            ImGui::PushStyleColor(ImGuiCol_Text, palette_.textDim);
        if (ImGui::Selectable(h.text.empty() ? "(untitled)" : h.text.c_str(), int(i) == active))
            target = h.anchor;
        ImGui::PopStyleColor();
        ImGui::PopID();
        if (indent > 0.0f)
            ImGui::Unindent(indent);
    }
    ImGui::EndChild();

    if (!target.empty()) {
        tocOverlay_ = false;
        vm_.followLink("#" + target);
    }
}

void MainView::renderContent(float width, float height)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, palette_.contentBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f * uiScale_, 0.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    if (io.KeyCtrl)
        flags |= ImGuiWindowFlags_NoScrollWithMouse; // Ctrl+wheel zooms instead
    ImGui::BeginChild("##content", ImVec2(width, height), ImGuiChildFlags_AlwaysUseWindowPadding, flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (resetScroll_) {
        ImGui::SetScrollY(0.0f);
        resetScroll_ = false;
    }

    const float fontSize = ImGui::GetFontSize() * kBodyScale * vm_.zoom.get();
    const model::DocumentPtr doc = vm_.document.get();
    if (!doc) {
        const char* text = vm_.loading.get() ? "Loading..." : "No document open. Use File > Open to view a Markdown file.";
        ImVec2 size = ImGui::CalcTextSize(text);
        ImGui::SetCursorPos(ImVec2(std::max(0.0f, (width - size.x) * 0.5f), height * 0.4f));
        ImGui::TextDisabled("%s", text);
    } else {
        MarkdownRenderer::Params params;
        params.palette = &palette_;
        params.fontSize = fontSize;
        params.maxWidth = fontSize * kMaxColumnEms;
        params.search = vm_.searchQuery.get();
        params.currentMatch = vm_.currentMatch.get();
        params.fullLayout = !pendingAnchor_.empty() || pendingMatchScroll_;
        params.showTooltips = !vm_.platform().isMobile();
        renderer_.render(*doc, params);

        if (int link = renderer_.clickedLink(); link >= 0)
            vm_.followLink(doc->links[size_t(link)].url);

        if (!pendingAnchor_.empty()) {
            if (std::optional<float> y = renderer_.anchorY(pendingAnchor_)) {
                ImGui::SetScrollY(std::max(0.0f, *y - fontSize * 0.5f));
                pendingAnchor_.clear();
            } else if (++pendingAnchorFrames_ > 2) {
                vm_.statusText.set("Section not found: #" + pendingAnchor_);
                pendingAnchor_.clear();
            }
        }
        if (pendingMatchScroll_) {
            if (std::optional<float> y = renderer_.currentMatchY())
                ImGui::SetScrollY(std::max(0.0f, *y - ImGui::GetWindowHeight() * 0.35f));
            pendingMatchScroll_ = false;
        }
    }

    // Ctrl + wheel zoom.
    if (io.KeyCtrl && io.MouseWheel != 0.0f && ImGui::IsWindowHovered()) {
        if (io.MouseWheel > 0.0f)
            vm_.zoomInCommand.execute();
        else
            vm_.zoomOutCommand.execute();
    }

    // Drag-to-scroll (touch screens, and a "hand tool" with the mouse).
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        dragScrolling_ = ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered();
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        dragScrolling_ = false;
    if (dragScrolling_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);

    // Keyboard scrolling when no widget has focus.
    if (!io.WantTextInput && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        const float page = ImGui::GetWindowHeight() * 0.9f;
        const float lineStep = fontSize * 3.0f;
        float scroll = ImGui::GetScrollY();
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown) || ImGui::IsKeyPressed(ImGuiKey_Space))
            scroll += page;
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
            scroll -= page;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            scroll += lineStep;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            scroll -= lineStep;
        if (ImGui::IsKeyPressed(ImGuiKey_Home))
            scroll = 0.0f;
        if (ImGui::IsKeyPressed(ImGuiKey_End))
            scroll = ImGui::GetScrollMaxY();
        if (scroll != ImGui::GetScrollY())
            ImGui::SetScrollY(std::clamp(scroll, 0.0f, ImGui::GetScrollMaxY()));
    }
    contentScrollY_ = ImGui::GetScrollY();
    ImGui::EndChild();
}

void MainView::renderStatusBar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * uiScale_, 0.0f));
    ImGui::BeginChild("##status", ImVec2(0.0f, ImGui::GetFrameHeight()), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::AlignTextToFramePadding();
    if (vm_.loading.get()) {
        static const char* const frames[] = {".  ", ".. ", "..."};
        ImGui::TextDisabled("Loading%s", frames[int(ImGui::GetTime() * 3.0) % 3]);
        ImGui::SameLine();
    }
    std::string platform = vm_.platform().platformName() + " " + vm_.platform().architectureName() + "  " +
                           std::to_string(int(std::lround(vm_.zoom.get() * 100.0f))) + "%";
    const float platformW = ImGui::CalcTextSize(platform.c_str()).x;
    const float statusW = ImGui::GetContentRegionAvail().x - platformW - 16.0f * uiScale_;
    ImGui::PushClipRect(ImGui::GetCursorScreenPos(),
                        ImVec2(ImGui::GetCursorScreenPos().x + std::max(0.0f, statusW), ImGui::GetWindowPos().y + ImGui::GetWindowHeight()),
                        true);
    ImGui::TextDisabled("%s", vm_.statusText.get().c_str());
    ImGui::PopClipRect();
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - platformW));
    ImGui::TextDisabled("%s", platform.c_str());
    ImGui::EndChild();
}

// ---- Popups ------------------------------------------------------------------------------------------

void MainView::renderOpenUrlPopup()
{
    if (openUrlPopup_) {
        ImGui::OpenPopup("Open URL");
        openUrlPopup_ = false;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal("Open URL", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::TextUnformatted("Markdown document URL:");
    ImGui::SetNextItemWidth(std::min(560.0f * uiScale_, viewport->WorkSize.x * 0.8f));
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    bool submit = ImGui::InputTextWithHint("##url", "https://raw.githubusercontent.com/user/repo/main/README.md", &urlBuffer_,
                                           ImGuiInputTextFlags_EnterReturnsTrue);
    if (!vm_.platform().supportsHttps())
        ImGui::TextDisabled("This build only supports http:// URLs.");
    if (vm_.platform().platformName() == "Web")
        ImGui::TextDisabled("The server must allow cross-origin (CORS) requests.");
    submit |= ImGui::Button("Open");
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
    if (submit && !urlBuffer_.empty()) {
        vm_.open(urlBuffer_);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void MainView::renderFileBrowserPopup()
{
    if (openFileBrowser_) {
        ImGui::OpenPopup("Open File");
        openFileBrowser_ = false;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x * 0.85f, viewport->WorkSize.y * 0.8f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Open File", nullptr))
        return;

    viewmodel::FileBrowserViewModel& browser = vm_.fileBrowser();
    if (iconButton(ICON_FA_ARROW_UP, "Up", "Parent folder"))
        browser.goUp();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::min(220.0f * uiScale_, ImGui::GetContentRegionAvail().x * 0.4f));
    if (ImGui::BeginCombo("##roots", "Locations")) {
        for (const std::string& root : browser.roots())
            if (ImGui::Selectable(root.c_str()))
                browser.navigateTo(root);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    bool showAll = browser.showAllFiles.get();
    if (ImGui::Checkbox("All files", &showAll))
        browser.showAllFiles.set(showAll);
    ImGui::TextWrapped("%s", browser.currentDirectory.get().c_str());
    if (!browser.error().empty())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette_.error), "%s", browser.error().c_str());

    const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("##entries", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders);
    std::optional<model::FileEntry> activated;
    for (const model::FileEntry& entry : browser.entries()) {
        std::string text = std::string(entry.isDirectory ? label(ICON_FA_FOLDER "  ", "[dir] ")
                                                         : label(ICON_FA_FILE_LINES "  ", "")) + entry.name;
        if (ImGui::Selectable(text.c_str())) {
            activated = entry; // activating may rebuild the list, so act after the loop
            break;
        }
    }
    ImGui::EndChild();
    if (activated) {
        if (std::optional<std::string> file = browser.activate(*activated)) {
            vm_.open(*file);
            ImGui::CloseCurrentPopup();
        }
    }
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void MainView::renderAboutPopup()
{
    if (openAbout_) {
        ImGui::OpenPopup("About MDViewer");
        openAbout_ = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal("About MDViewer", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::Text("MDViewer %s", MDV_VERSION);
    ImGui::Separator();
    ImGui::Text("Platform:  %s (%s)", vm_.platform().platformName().c_str(), vm_.platform().architectureName().c_str());
    ImGui::Text("Dear ImGui %s", IMGUI_VERSION);
    if (!rendererInfo_.empty())
        ImGui::Text("Renderer:  %s", rendererInfo_.c_str());
    ImGui::Text("HTTPS:     %s", vm_.platform().supportsHttps() ? "yes" : "no");
    ImGui::Spacing();
    if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

} // namespace mdv::view
