#include "mdv/services/file_system_service.h"

#include "mdv/os/fs.h"

namespace mdv::services {

std::vector<model::FileEntry> FileSystemService::listDirectory(const std::string& path, std::string* error)
{
    std::vector<model::FileEntry> out;
    for (os::fs::DirEntry& e : os::fs::listDirectory(path, error))
        out.push_back({std::move(e.name), e.isDirectory, e.size});
    return out;
}

std::vector<std::string> FileSystemService::roots() { return os::fs::rootDirectories(); }

std::string FileSystemService::join(const std::string& directory, const std::string& name) const
{
    return os::fs::path::join(directory, name);
}

std::string FileSystemService::parent(const std::string& path) const { return os::fs::path::parent(path); }

std::string FileSystemService::normalize(const std::string& path) const { return os::fs::path::normalize(path); }

std::optional<std::int64_t> FileSystemService::modifiedTime(const std::string& path)
{
    os::fs::FileInfo info = os::fs::stat(path);
    if (!info.exists)
        return std::nullopt;
    return info.modifiedTime;
}

} // namespace mdv::services
