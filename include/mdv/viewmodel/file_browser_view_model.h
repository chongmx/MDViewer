// In-app file browser for platforms without a native picker (Android) or as an alternative.
#pragma once

#include "mdv/interfaces/i_file_system_service.h"
#include "mdv/model/file_entry.h"
#include "mdv/viewmodel/observable.h"

#include <optional>
#include <string>
#include <vector>

namespace mdv::viewmodel {

class FileBrowserViewModel {
public:
    explicit FileBrowserViewModel(interfaces::IFileSystemService& fileSystem);

    Property<std::string> currentDirectory;
    Property<bool> showAllFiles{false};

    const std::vector<model::FileEntry>& entries() const { return entries_; }
    const std::vector<std::string>& roots() const { return roots_; }
    const std::string& error() const { return error_; }

    void navigateTo(const std::string& directory);
    void goUp();
    void refresh();

    // Directories are entered (invalidating entries()); for files the full path is returned.
    std::optional<std::string> activate(const model::FileEntry& entry);

private:
    interfaces::IFileSystemService& fileSystem_;
    std::vector<model::FileEntry> entries_;
    std::vector<std::string> roots_;
    std::string error_;
    Subscription showAllFilesSubscription_;
};

} // namespace mdv::viewmodel
