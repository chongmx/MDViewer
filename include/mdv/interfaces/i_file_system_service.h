// Contract for browsing and watching the file system.
#pragma once

#include "mdv/model/file_entry.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mdv::interfaces {

class IFileSystemService {
public:
    virtual ~IFileSystemService() = default;

    virtual std::vector<model::FileEntry> listDirectory(const std::string& path, std::string* error) = 0;
    virtual std::vector<std::string> roots() = 0;
    virtual std::string join(const std::string& directory, const std::string& name) const = 0;
    virtual std::string parent(const std::string& path) const = 0;
    virtual std::string normalize(const std::string& path) const = 0;
    // Modification time in seconds since the epoch, or nullopt when the file does not exist.
    virtual std::optional<std::int64_t> modifiedTime(const std::string& path) = 0;
};

} // namespace mdv::interfaces
