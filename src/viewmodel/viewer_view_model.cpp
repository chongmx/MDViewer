#include "mdv/viewmodel/viewer_view_model.h"

#include "mdv/model/file_entry.h"
#include "mdv/model/search.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mdv::viewmodel {

namespace {

constexpr float kZoomStep = 0.1f;
constexpr double kWatchInterval = 1.0;

void splitFragment(const std::string& location, std::string& base, std::string& fragment)
{
    size_t hash = location.find('#');
    if (hash == std::string::npos) {
        base = location;
        fragment.clear();
    } else {
        base = location.substr(0, hash);
        fragment = location.substr(hash + 1);
    }
}

std::string formatSize(std::size_t bytes)
{
    char buf[32];
    if (bytes < 1024)
        std::snprintf(buf, sizeof(buf), "%zu B", bytes);
    else
        std::snprintf(buf, sizeof(buf), "%.1f KB", double(bytes) / 1024.0);
    return buf;
}

} // namespace

ViewerViewModel::ViewerViewModel(interfaces::IDocumentService& documents, interfaces::ISettingsService& settings,
                                 interfaces::IFileSystemService& fileSystem, interfaces::IPlatformService& platform)
    : documents_(documents), settingsService_(settings), fileSystem_(fileSystem), platform_(platform),
      fileBrowser_(fileSystem)
{
    settings_ = settingsService_.load();
    darkTheme.set(settings_.darkTheme);
    showToc.set(settings_.showToc);
    zoom.set(settings_.zoom);

    subscriptions_.push_back(darkTheme.subscribe([this](const bool&) { saveSettings(); }));
    subscriptions_.push_back(showToc.subscribe([this](const bool&) { saveSettings(); }));

    openFileCommand = Command([this] {
        if (!platform_.hasNativeFilePicker()) {
            fileBrowser_.refresh();
            fileBrowserRequested.emit();
            return;
        }
        platform_.pickMarkdownFile([this, alive = std::weak_ptr<bool>(alive_)](std::optional<std::string> path) {
            if (path && !alive.expired())
                open(*path);
        });
    });
    openUrlCommand = Command([this] { openUrlRequested.emit(); });
    reloadCommand = Command([this] { navigate(currentLocation_, false); }, [this] { return !currentLocation_.empty(); });
    backCommand = Command(
        [this] {
            --historyIndex_;
            navigate(history_[size_t(historyIndex_)], false);
        },
        [this] { return historyIndex_ > 0; });
    forwardCommand = Command(
        [this] {
            ++historyIndex_;
            navigate(history_[size_t(historyIndex_)], false);
        },
        [this] { return historyIndex_ + 1 < int(history_.size()); });
    homeCommand = Command([this] { open(kWelcomeDocument); });

    auto setZoom = [this](float z) {
        zoom.set(std::clamp(std::round(z * 10.0f) / 10.0f, model::Settings::kMinZoom, model::Settings::kMaxZoom));
        saveSettings();
    };
    zoomInCommand = Command([this, setZoom] { setZoom(zoom.get() + kZoomStep); },
                            [this] { return zoom.get() < model::Settings::kMaxZoom; });
    zoomOutCommand = Command([this, setZoom] { setZoom(zoom.get() - kZoomStep); },
                             [this] { return zoom.get() > model::Settings::kMinZoom; });
    zoomResetCommand = Command([setZoom] { setZoom(1.0f); });

    nextMatchCommand = Command(
        [this] {
            currentMatch.set((currentMatch.get() + 1) % matchCount.get());
            scrollToMatchRequested.emit();
        },
        [this] { return matchCount.get() > 0; });
    previousMatchCommand = Command(
        [this] {
            int n = matchCount.get();
            currentMatch.set((currentMatch.get() - 1 + n) % n);
            scrollToMatchRequested.emit();
        },
        [this] { return matchCount.get() > 0; });
}

ViewerViewModel::~ViewerViewModel()
{
    saveSettings();
}

void ViewerViewModel::saveSettings()
{
    settings_.darkTheme = darkTheme.get();
    settings_.showToc = showToc.get();
    settings_.zoom = zoom.get();
    settingsService_.save(settings_);
}

void ViewerViewModel::clearRecentDocuments()
{
    settings_.recentDocuments.clear();
    saveSettings();
}

void ViewerViewModel::open(const std::string& location)
{
    if (!location.empty())
        navigate(documents_.normalizeLocation(location), true);
}

void ViewerViewModel::followLink(const std::string& href)
{
    if (href.empty())
        return;
    if (href[0] == '#') {
        scrollToAnchorRequested.emit(href.substr(1));
        return;
    }
    if (documents_.isExternalScheme(href)) {
        platform_.openExternal(href); // mailto:, ftp://, ...
        return;
    }

    std::string base, fragment;
    splitFragment(documents_.resolveLink(currentLocation_, href), base, fragment);
    if (model::isMarkdownPath(base)) {
        navigate(fragment.empty() ? base : base + "#" + fragment, true);
        return;
    }
    if (documents_.isAsset(base)) {
        statusText.set("Cannot open bundled resource " + base);
        return;
    }
    if (!platform_.openExternal(base))
        statusText.set("No application available to open " + base);
}

void ViewerViewModel::navigate(const std::string& location, bool addToHistory)
{
    std::string base, fragment;
    splitFragment(location, base, fragment);

    if (addToHistory && !(historyIndex_ >= 0 && history_[size_t(historyIndex_)] == location)) {
        history_.resize(size_t(historyIndex_ + 1));
        history_.push_back(location);
        historyIndex_ = int(history_.size()) - 1;
    }

    // Same document: only scroll.
    if (document.get() && base == currentLocation_ && !fragment.empty()) {
        scrollToAnchorRequested.emit(fragment);
        return;
    }

    const std::uint64_t request = ++requestId_;
    loading.set(true);
    errorText.set({});
    statusText.set("Loading " + base + " ...");
    documents_.load(base, [this, alive = std::weak_ptr<bool>(alive_), request, base, fragment](interfaces::LoadResult result) {
        if (alive.expired() || request != requestId_)
            return; // superseded by a newer navigation
        onLoaded(base, fragment, std::move(result));
    });
}

void ViewerViewModel::onLoaded(const std::string& location, const std::string& fragment, interfaces::LoadResult result)
{
    loading.set(false);
    if (!result.document) {
        errorText.set(result.error);
        statusText.set(result.error);
        return;
    }

    currentLocation_ = location;
    watchedModifiedTime_ = documents_.isLocalFile(location) ? fileSystem_.modifiedTime(location).value_or(0) : 0;
    settings_.addRecent(location);
    saveSettings();

    const model::Document& doc = *result.document;
    statusText.set(doc.title + "  \xE2\x80\x94  " + formatSize(doc.sourceBytes) + ", " +
                   std::to_string(doc.headings.size()) + " headings");
    document.set(result.document);
    updateSearch();
    if (!fragment.empty())
        scrollToAnchorRequested.emit(fragment);
}

void ViewerViewModel::setSearchQuery(const std::string& query)
{
    if (query == searchQuery.get())
        return;
    searchQuery.set(query);
    updateSearch();
    if (matchCount.get() > 0)
        scrollToMatchRequested.emit();
}

void ViewerViewModel::updateSearch()
{
    const model::DocumentPtr& doc = document.get();
    int count = doc && !searchQuery.get().empty() ? model::countMatches(*doc, searchQuery.get()) : 0;
    matchCount.set(count);
    currentMatch.set(count > 0 ? std::clamp(currentMatch.get(), 0, count - 1) : -1);
}

void ViewerViewModel::tick()
{
    const double now = platform_.now();
    if (now < nextWatchCheck_ || loading.get() || !documents_.isLocalFile(currentLocation_))
        return;
    nextWatchCheck_ = now + kWatchInterval;
    std::optional<std::int64_t> modified = fileSystem_.modifiedTime(currentLocation_);
    if (modified && *modified != watchedModifiedTime_) {
        watchedModifiedTime_ = *modified;
        navigate(currentLocation_, false);
    }
}

} // namespace mdv::viewmodel
