// Main view-model: navigation, history, link handling, search and preferences.
// Depends only on service interfaces and the model; the ImGui view binds to it.
#pragma once

#include "mdv/interfaces/i_document_service.h"
#include "mdv/interfaces/i_file_system_service.h"
#include "mdv/interfaces/i_platform_service.h"
#include "mdv/interfaces/i_settings_service.h"
#include "mdv/model/document.h"
#include "mdv/model/settings.h"
#include "mdv/viewmodel/file_browser_view_model.h"
#include "mdv/viewmodel/observable.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mdv::viewmodel {

class ViewerViewModel {
public:
    static constexpr const char* kWelcomeDocument = "asset:docs/welcome.md";

    ViewerViewModel(interfaces::IDocumentService& documents, interfaces::ISettingsService& settings,
                    interfaces::IFileSystemService& fileSystem, interfaces::IPlatformService& platform);
    ~ViewerViewModel();
    ViewerViewModel(const ViewerViewModel&) = delete;
    ViewerViewModel& operator=(const ViewerViewModel&) = delete;

    // ---- Bindable state ---------------------------------------------------------------
    Property<model::DocumentPtr> document;
    Property<bool> loading{false};
    Property<std::string> statusText;
    Property<std::string> errorText;

    Property<bool> darkTheme{true};
    Property<bool> showToc{true};
    Property<float> zoom{1.0f};

    Property<std::string> searchQuery;
    Property<int> matchCount{0};
    Property<int> currentMatch{-1}; // index into document-order matches, -1 when none

    // ---- One-shot requests to the view -----------------------------------------------
    Event<std::string> scrollToAnchorRequested;
    Event<> scrollToMatchRequested;
    Event<> fileBrowserRequested;
    Event<> openUrlRequested;

    // ---- Commands ----------------------------------------------------------------------
    Command openFileCommand;
    Command openUrlCommand;
    Command reloadCommand;
    Command backCommand;
    Command forwardCommand;
    Command homeCommand;
    Command zoomInCommand;
    Command zoomOutCommand;
    Command zoomResetCommand;
    Command nextMatchCommand;
    Command previousMatchCommand;

    // Opens a file path, URL or "asset:" location (optionally with a #fragment).
    void open(const std::string& location);
    // Handles a click on a link inside the current document.
    void followLink(const std::string& href);
    void setSearchQuery(const std::string& query);
    void clearRecentDocuments();

    const std::vector<std::string>& recentDocuments() const { return settings_.recentDocuments; }
    const std::string& currentLocation() const { return currentLocation_; }
    FileBrowserViewModel& fileBrowser() { return fileBrowser_; }
    const interfaces::IPlatformService& platform() const { return platform_; }

    // Called once per frame (watches the open file for changes).
    void tick();

private:
    void navigate(const std::string& location, bool addToHistory);
    void onLoaded(const std::string& location, const std::string& fragment, interfaces::LoadResult result);
    void updateSearch();
    void saveSettings();

    interfaces::IDocumentService& documents_;
    interfaces::ISettingsService& settingsService_;
    interfaces::IFileSystemService& fileSystem_;
    interfaces::IPlatformService& platform_;

    model::Settings settings_;
    FileBrowserViewModel fileBrowser_;

    std::vector<std::string> history_;
    int historyIndex_ = -1;
    std::string currentLocation_; // without fragment
    std::uint64_t requestId_ = 0;

    double nextWatchCheck_ = 0.0;
    std::int64_t watchedModifiedTime_ = 0;
    std::vector<Subscription> subscriptions_;
    // Async callbacks hold a weak reference so they are dropped if the view-model is gone.
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace mdv::viewmodel
