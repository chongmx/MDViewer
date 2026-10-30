// OS abstraction layer: umbrella header and process-level services.
#pragma once

#include "mdv/os/platform.h"
#include "mdv/os/dialog.h"
#include "mdv/os/dispatch.h"
#include "mdv/os/fs.h"
#include "mdv/os/log.h"
#include "mdv/os/net.h"
#include "mdv/os/shell.h"

#include <optional>
#include <string>
#include <vector>

namespace mdv::os {

struct InitParams {
    int argc = 0;
    char** argv = nullptr;
    // Platform handle required by some backends: `android_app*` on Android, unused elsewhere.
    void* nativeHandle = nullptr;
};

bool init(const InitParams& params);
void shutdown();

// Launch arguments as UTF-8 (argv[0] excluded). On the web the `?doc=` query parameter
// is exposed as the first argument.
const std::vector<std::string>& arguments();

std::optional<std::string> getEnv(const std::string& name);

// Monotonic clock in seconds.
double now();

} // namespace mdv::os
