// Contract for loading documents and resolving locations/links between them.
#pragma once

#include "mdv/model/document.h"

#include <functional>
#include <string>

namespace mdv::interfaces {

struct LoadResult {
    model::DocumentPtr document; // null on failure
    std::string error;
};

using LoadCallback = std::function<void(LoadResult)>;

// Locations:
//   "asset:docs/welcome.md"         bundled resource
//   "https://host/path/README.md"   remote (http/https)
//   "/abs/path.md", "C:/x.md"        local file
class IDocumentService {
public:
    virtual ~IDocumentService() = default;

    // Asynchronous; the callback runs on the main thread.
    virtual void load(const std::string& location, LoadCallback callback) = 0;

    // Turns user input (relative path, URL, asset) into a canonical location.
    virtual std::string normalizeLocation(const std::string& input) const = 0;
    // Resolves a link target found in the document at `base` (no fragment handling).
    virtual std::string resolveLink(const std::string& base, const std::string& href) const = 0;

    virtual bool isLocalFile(const std::string& location) const = 0;
    virtual bool isRemote(const std::string& location) const = 0;
    virtual bool isAsset(const std::string& location) const = 0;
    // True for schemes the viewer does not handle itself (mailto:, ftp://, ...).
    virtual bool isExternalScheme(const std::string& location) const = 0;
};

} // namespace mdv::interfaces
