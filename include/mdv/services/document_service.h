// IDocumentService implementation: files and assets via os::fs, remote via os::net.
#pragma once

#include "mdv/interfaces/i_document_service.h"

namespace mdv::services {

class DocumentService final : public interfaces::IDocumentService {
public:
    void load(const std::string& location, interfaces::LoadCallback callback) override;

    std::string normalizeLocation(const std::string& input) const override;
    std::string resolveLink(const std::string& base, const std::string& href) const override;

    bool isLocalFile(const std::string& location) const override;
    bool isRemote(const std::string& location) const override;
    bool isAsset(const std::string& location) const override;
    bool isExternalScheme(const std::string& location) const override;

    // Rewrites github.com ".../blob/<ref>/<path>" links to raw.githubusercontent.com.
    static std::string toRawUrl(const std::string& url);
};

} // namespace mdv::services
