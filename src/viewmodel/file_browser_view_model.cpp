#include "mdv/viewmodel/file_browser_view_model.h"

#include <algorithm>

namespace mdv::viewmodel {

FileBrowserViewModel::FileBrowserViewModel(interfaces::IFileSystemService& fileSystem)
    : fileSystem_(fileSystem), roots_(fileSystem.roots())
{
    showAllFilesSubscription_ = showAllFiles.subscribe([this](const bool&) { refresh(); });
}

void FileBrowserViewModel::navigateTo(const std::string& directory)
{
    currentDirectory.set(fileSystem_.normalize(directory));
    refresh();
}

void FileBrowserViewModel::goUp()
{
    navigateTo(fileSystem_.parent(currentDirectory.get()));
}

void FileBrowserViewModel::refresh()
{
    if (currentDirectory.get().empty() && !roots_.empty())
        currentDirectory.set(roots_.front());
    error_.clear();
    entries_ = fileSystem_.listDirectory(currentDirectory.get(), &error_);
    if (!showAllFiles.get()) {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [](const model::FileEntry& e) { return !e.isDirectory && !model::isMarkdownPath(e.name); }),
                       entries_.end());
    }
}

std::optional<std::string> FileBrowserViewModel::activate(const model::FileEntry& entry)
{
    std::string full = fileSystem_.join(currentDirectory.get(), entry.name);
    if (entry.isDirectory) {
        navigateTo(full);
        return std::nullopt;
    }
    return full;
}

} // namespace mdv::viewmodel
