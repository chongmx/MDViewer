// Native dialogs.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace mdv::os::dialog {

struct FileFilter {
    std::string name;                    // "Markdown"
    std::vector<std::string> extensions; // {"md", "markdown"}
};

using OpenFileCallback = std::function<void(std::optional<std::string> path)>;

// False where no native picker exists (Android NativeActivity); callers then fall back
// to an in-app browser built on fs::listDirectory().
bool hasNativeOpenFile();

// Shows an open-file dialog. The callback runs on the main thread with the chosen path,
// or std::nullopt if cancelled. On the web the chosen file is copied into the in-memory
// file system and that path is returned.
void openFile(const std::string& title, const std::vector<FileFilter>& filters, OpenFileCallback callback);

} // namespace mdv::os::dialog
