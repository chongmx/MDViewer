#include "mdv/model/file_entry.h"

#include <cctype>

namespace mdv::model {

bool isMarkdownPath(std::string_view path)
{
    path = path.substr(0, path.find_first_of("?#"));
    size_t slash = path.find_last_of("/\\");
    std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
    size_t dot = name.find_last_of('.');
    if (dot == std::string_view::npos || dot == 0)
        return false;
    std::string ext;
    for (char c : name.substr(dot))
        ext += char(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".md" || ext == ".markdown" || ext == ".mdown" || ext == ".mkd" || ext == ".mkdn" || ext == ".txt";
}

} // namespace mdv::model
