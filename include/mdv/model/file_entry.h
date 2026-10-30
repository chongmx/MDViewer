// File-browser entries and document type detection.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace mdv::model {

struct FileEntry {
    std::string name;
    bool isDirectory = false;
    std::uint64_t size = 0;
};

// True for file names / paths / URLs (query string ignored) with a markdown or text extension.
bool isMarkdownPath(std::string_view path);

} // namespace mdv::model
