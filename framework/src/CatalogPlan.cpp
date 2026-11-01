#include "gyo/framework/CatalogPlan.hpp"

#include "engine/asset/ContentManifest.hpp"

#include <set>
#include <string_view>
#include <utility>

namespace Gyo::Tools::Framework {
namespace {

CatalogError InvalidCatalog(std::string message, std::string detail = {}) {
    return CatalogError::Make(CatalogErrorCode::InvalidCatalog, std::move(message), std::move(detail));
}

CatalogError RegistrationError(std::string message, std::string detail = {}) {
    return CatalogError::Make(CatalogErrorCode::InvalidRegistration, std::move(message), std::move(detail));
}

// A path the content contract accepts for an asset in a catalog: Engine's
// rule, and not the content descriptor itself (D-1, b-lite).
bool IsAssetPath(const std::string_view path) noexcept {
    return Engine::Asset::IsRelativeContentPath(path) && path != "content.json";
}

} // namespace

Result<void, CatalogError> CheckCatalogEntries(const std::span<const CatalogEntry> entries) {
    std::set<std::string_view> ids;
    for (const CatalogEntry& entry : entries) {
        if (!ids.insert(entry.id).second) {
            return Engine::Base::Err(InvalidCatalog("duplicate asset id", entry.id));
        }
        if (!IsAssetPath(entry.path)) {
            return Engine::Base::Err(InvalidCatalog("not a content path", entry.path));
        }
    }
    return {};
}

Result<RegistrationPlan, CatalogError> PlanRegistration(const std::span<const CatalogEntry> entries,
                                                        const CatalogEntry& entry,
                                                        const RegistrationPolicy& policy) {
    if (entry.id.empty()) {
        return Engine::Base::Err(RegistrationError("the asset id is empty"));
    }
    if (entry.type.empty()) {
        return Engine::Base::Err(RegistrationError("the asset type is empty", entry.id));
    }
    if (!IsAssetPath(entry.path)) {
        return Engine::Base::Err(RegistrationError("not a content path", entry.path));
    }
    RegistrationPlan plan{true, entries.size(), std::nullopt};
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const CatalogEntry& existing = entries[index];
        if (existing.id == entry.id) {
            if (existing.type != entry.type && !policy.allowTypeChange) {
                return Engine::Base::Err(
                    RegistrationError("the id is registered with another type", entry.id + " is " + existing.type));
            }
            plan.added = false;
            plan.index = index;
            if (existing.path != entry.path) plan.previousPath = existing.path;
        } else if (existing.path == entry.path && !policy.allowSharedPath) {
            return Engine::Base::Err(
                RegistrationError("the path is registered under another id", entry.path + " is " + existing.id));
        }
    }
    return plan;
}

} // namespace Gyo::Tools::Framework
