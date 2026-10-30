#include "mdv/services/document_service.h"

#include "mdv/model/markdown_parser.h"
#include "mdv/os/os.h"

#include <utility>

namespace mdv::services {

namespace {

constexpr const char* kAssetPrefix = "asset:";
constexpr size_t kAssetPrefixLength = 6;

interfaces::LoadResult makeResult(const std::string& location, const std::string& source)
{
    auto doc = std::make_shared<model::Document>(model::parseMarkdown(source));
    doc->location = location;
    if (doc->title.empty()) {
        std::string name = os::fs::path::filename(location);
        doc->title = name.empty() ? location : name;
    }
    return {std::move(doc), {}};
}

} // namespace

bool DocumentService::isAsset(const std::string& location) const { return location.rfind(kAssetPrefix, 0) == 0; }

bool DocumentService::isRemote(const std::string& location) const
{
    return location.rfind("http://", 0) == 0 || location.rfind("https://", 0) == 0;
}

bool DocumentService::isExternalScheme(const std::string& location) const
{
    return os::net::isUrl(location) && !isRemote(location);
}

bool DocumentService::isLocalFile(const std::string& location) const
{
    return !location.empty() && !isAsset(location) && !os::net::isUrl(location);
}

std::string DocumentService::toRawUrl(const std::string& url)
{
    const std::string prefix = "https://github.com/";
    if (url.rfind(prefix, 0) != 0)
        return url;
    size_t blob = url.find("/blob/", prefix.size());
    if (blob == std::string::npos)
        return url;
    return "https://raw.githubusercontent.com/" + url.substr(prefix.size(), blob - prefix.size()) + url.substr(blob + 5);
}

std::string DocumentService::normalizeLocation(const std::string& input) const
{
    if (input.empty() || os::net::isUrl(input) || isAsset(input))
        return input;
    if (input.rfind("file://", 0) == 0)
        return normalizeLocation(os::net::percentDecode(input.substr(7)));
    return os::fs::path::isAbsolute(input) ? os::fs::path::normalize(input)
                                           : os::fs::path::join(os::fs::currentDirectory(), input);
}

std::string DocumentService::resolveLink(const std::string& base, const std::string& href) const
{
    if (os::net::isUrl(href) || base.empty())
        return href;
    if (isRemote(base))
        return os::net::resolveUrl(base, href);

    std::string decoded = os::net::percentDecode(href);
    if (isAsset(base)) {
        std::string dir = os::fs::path::parent(base.substr(kAssetPrefixLength));
        return kAssetPrefix + os::fs::path::join(dir == "." ? "" : dir, decoded);
    }
    if (os::fs::path::isAbsolute(decoded))
        return os::fs::path::normalize(decoded);
    return os::fs::path::join(os::fs::path::parent(base), decoded);
}

void DocumentService::load(const std::string& location, interfaces::LoadCallback callback)
{
    if (isRemote(location)) {
        os::net::httpGet(toRawUrl(location), [location, callback = std::move(callback)](os::net::HttpResponse response) {
            if (!response.ok()) {
                std::string error = !response.error.empty() ? response.error : "HTTP " + std::to_string(response.status);
                callback({nullptr, "Could not fetch " + location + ": " + error});
                return;
            }
            callback(makeResult(location, response.body));
        });
        return;
    }

    // Local files and assets: read and parse off the UI thread.
    const bool asset = isAsset(location);
    os::dispatch::runAsync([location, asset, callback = std::move(callback)]() mutable {
        interfaces::LoadResult result;
        std::string error;
        std::optional<std::string> data = asset ? os::fs::readAsset(location.substr(kAssetPrefixLength))
                                                : os::fs::readFile(location, &error);
        if (data)
            result = makeResult(location, *data);
        else
            result.error = "Could not open " + location + (error.empty() ? std::string() : ": " + error);
        os::dispatch::postToMain([callback = std::move(callback), result = std::move(result)]() mutable {
            callback(std::move(result));
        });
    });
}

} // namespace mdv::services
