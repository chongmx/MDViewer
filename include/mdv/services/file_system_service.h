// IFileSystemService implementation over os::fs.
#pragma once

#include "mdv/interfaces/i_file_system_service.h"

namespace mdv::services {

class FileSystemService final : public interfaces::IFileSystemService {
public:
    std::vector<model::FileEntry> listDirectory(const std::string& path, std::string* error) override;
    std::vector<std::string> roots() override;
    std::string join(const std::string& directory, const std::string& name) const override;
    std::string parent(const std::string& path) const override;
    std::string normalize(const std::string& path) const override;
    std::optional<std::int64_t> modifiedTime(const std::string& path) override;
};

} // namespace mdv::services
