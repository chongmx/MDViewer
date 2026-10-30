// Top-level ImGui view bound to ViewerViewModel.
#pragma once

#include "mdv/view/fonts.h"
#include "mdv/view/markdown_renderer.h"
#include "mdv/view/theme.h"
#include "mdv/viewmodel/viewer_view_model.h"

#include <string>

namespace mdv::view {

class MainView {
public:
    MainView(viewmodel::ViewerViewModel& vm, const Fonts& fonts, float uiScale);

    // Builds the whole UI for one frame (between ImGui::NewFrame() and ImGui::Render()).
    void render();

    ImVec4 clearColor() const { return palette_.clearColor; }
    bool quitRequested() const { return quitRequested_; }
    // Extra text shown in the About box (e.g. the GL renderer string).
    void setRendererInfo(std::string info) { rendererInfo_ = std::move(info); }

private:
    void handleShortcuts();
    void renderMenuBar();
    void renderToolbar();
    void renderToc(float width, float height);
    void renderContent(float width, float height);
    void renderStatusBar();
    void renderOpenUrlPopup();
    void renderFileBrowserPopup();
    void renderAboutPopup();

    bool iconButton(const char* icon, const char* fallback, const char* tooltip, bool enabled = true);
    void commandButton(const char* icon, const char* fallback, const char* tooltip, const viewmodel::Command& command);
    const char* label(const char* icon, const char* fallback) const;
    bool narrow() const;

    viewmodel::ViewerViewModel& vm_;
    const Fonts& fonts_;
    float uiScale_;
    Palette palette_;
    MarkdownRenderer renderer_;

    std::string searchBuffer_;
    std::string urlBuffer_;
    std::string rendererInfo_;
    std::string pendingAnchor_;
    std::string lastLocation_;
    bool pendingMatchScroll_ = false;
    bool resetScroll_ = false;
    bool focusSearch_ = false;
    bool openUrlPopup_ = false;
    bool openFileBrowser_ = false;
    bool openAbout_ = false;
    bool quitRequested_ = false;
    bool tocOverlay_ = false;    // narrow layouts show the TOC instead of the content
    bool dragScrolling_ = false;
    float contentScrollY_ = 0.0f;
    int pendingAnchorFrames_ = 0;
    std::vector<viewmodel::Subscription> subscriptions_;
};

} // namespace mdv::view
