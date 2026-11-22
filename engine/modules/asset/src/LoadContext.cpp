#include "engine/asset/LoadContext.hpp"

#include <utility>

#include "engine/asset/ContentPath.hpp"

namespace GYO::Engine::Asset {

LoadContext::LoadContext(AssetId id, std::string path, const std::size_t maxBytes, const ContentPathIndex* paths)
    : id_(std::move(id)), path_(std::move(path)), maxBytes_(maxBytes), paths_(paths) {}

void LoadContext::RecordId(const std::string_view typeName, const AssetId& id) {
    references_.push_back(DeclaredReference{
        .kind = ReferenceKind::Id, .typeName = std::string(typeName), .id = id, .writtenPath = {}, .path = {},
        .failure = std::nullopt});
}

AssetId LoadContext::RecordPath(const std::string_view typeName, const std::string_view relativePath) {
    std::optional<std::string> resolved = ResolveReferencePath(path_, relativePath);
    DeclaredReference reference{.kind = ReferenceKind::Path,
                                .typeName = std::string(typeName),
                                .id = {},
                                .writtenPath = std::string(relativePath),
                                .path = {},
                                .failure = std::nullopt};
    if (!resolved) {
        reference.failure = ReferenceFailureCode::BadReference;
    } else {
        reference.path = std::move(*resolved);
        if (paths_ != nullptr) {
            const std::span<const AssetId> ids = paths_->IdsAt(reference.path);
            if (ids.empty()) {
                reference.failure = ReferenceFailureCode::UnlistedReference;
            } else if (ids.size() > 1) {
                reference.failure = ReferenceFailureCode::AmbiguousReference;
            } else {
                reference.id = ids.front();
            }
        }
    }
    AssetId id = reference.id;
    references_.push_back(std::move(reference));
    return id;
}

} // namespace GYO::Engine::Asset
