#include "application.h"

#include "mdv/os/os.h"

namespace mdv::app {

Application::Application()
    : services_(services::ServiceContainer::createDefault())
{
    viewModel_ = std::make_unique<viewmodel::ViewerViewModel>(*services_.documents, *services_.settings,
                                                              *services_.fileSystem, *services_.platform);
}

Application::~Application()
{
    detachUi();
}

void Application::attachUi(float uiScale, std::string rendererInfo)
{
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // all persistent state lives in the settings service
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.ChildRounding = 0.0f;
    style.WindowRounding = 6.0f;
    style.ScrollbarSize = os::isMobile() ? 6.0f : 12.0f;
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(6.0f, 6.0f);
    style.TouchExtraPadding = os::isMobile() ? ImVec2(4.0f, 4.0f) : ImVec2(0.0f, 0.0f);
    style.ScaleAllSizes(uiScale);
    style.FontScaleDpi = uiScale;
    style.FontSizeBase = 16.0f;

    fonts_ = view::Fonts::load();
    view_ = std::make_unique<view::MainView>(*viewModel_, fonts_, uiScale);
    view_->setRendererInfo(std::move(rendererInfo));
}

void Application::detachUi()
{
    view_.reset();
}

void Application::openInitialDocument()
{
    const std::vector<std::string>& args = os::arguments();
    viewModel_->open(args.empty() ? viewmodel::ViewerViewModel::kWelcomeDocument : args.front());
}

void Application::openPath(const std::string& path)
{
    viewModel_->open(path);
}

bool Application::navigateBack()
{
    if (!viewModel_->backCommand.canExecute())
        return false;
    viewModel_->backCommand.execute();
    return true;
}

void Application::frame()
{
    os::dispatch::pump();
    viewModel_->tick();
    if (view_)
        view_->render();
}

ImVec4 Application::clearColor() const
{
    return view_ ? view_->clearColor() : ImVec4(0, 0, 0, 1);
}

bool Application::quitRequested() const
{
    return view_ && view_->quitRequested();
}

} // namespace mdv::app
