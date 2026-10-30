// Interaction with the desktop shell / host environment.
#pragma once

#include <string>

namespace mdv::os::shell {

// Opens a URL (or a local file) with the system's default handler / browser tab.
bool openExternal(const std::string& urlOrPath);

} // namespace mdv::os::shell
