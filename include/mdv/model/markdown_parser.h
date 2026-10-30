// CommonMark-style parser with the common GitHub extensions
// (tables, task lists, strikethrough, autolinks).
#pragma once

#include "mdv/model/document.h"

#include <string>
#include <string_view>

namespace mdv::model {

Document parseMarkdown(std::string_view source);

// GitHub-compatible heading slug: "Hello, World!" -> "hello-world".
std::string slugify(std::string_view text);

} // namespace mdv::model
