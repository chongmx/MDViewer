// Platform-independent application shell: composes services, view-model and view,
// and runs one UI frame at a time for the platform main loops.
#pragma once

#include "mdv/services/service_container.h"
#include "mdv/view/fonts.h"
#include "mdv/view/main_view.h"
#include "mdv/viewmodel/viewer_view_model.h"

#include <memory>
#include <string>

namespace mdv::app {

class Application {
public:
    // Requires os::init() to have been called.
    Application();
    ~Application();

    // Creates UI resources for the current ImGui context (fonts, style, view).
    // On Android this happens again whenever the native window is recreated.
    void attachUi(float uiScale, std::string rendererInfo);
    void detachUi();

    // Opens the first launch argument, or the welcome document.
    void openInitialDocument();
    void openPath(const std::string& path);
    // Android back button: navigates back; returns false when there is no history.
    bool navigateBack();

    // Pumps background results and builds the UI (between ImGui::NewFrame() and ImGui::Render()).
    void frame();

    ImVec4 clearColor() const;
    bool quitRequested() const;

private:
    services::ServiceContainer services_;
    std::unique_ptr<viewmodel::ViewerViewModel> viewModel_;
    view::Fonts fonts_;
    std::unique_ptr<view::MainView> view_;
};

} // namespace mdv::app
