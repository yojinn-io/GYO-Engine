#include "engine/asset/LoadContext.hpp"

#include <utility>

#include "engine/asset/ContentPath.hpp"

namespace GYO::Engine::Asset {

LoadContext::LoadContext(AssetId id, std::string path, const std::size_t maxBytes)
    : id_(std::move(id)), path_(std::move(path)), maxBytes_(maxBytes) {}

void LoadContext::RecordId(const std::string_view typeName, const AssetId& id) {
    references_.push_back(DeclaredReference{
        .kind = ReferenceKind::Id, .typeName = std::string(typeName), .id = id, .writtenPath = {}, .path = {},
        .failure = std::nullopt});
}

void LoadContext::RecordPath(const std::string_view typeName, const std::string_view relativePath) {
    std::optional<std::string> resolved = ResolveReferencePath(path_, relativePath);
    DeclaredReference reference{.kind = ReferenceKind::Path,
                                .typeName = std::string(typeName),
                                .id = {},
                                .writtenPath = std::string(relativePath),
                                .path = {},
                                .failure = std::nullopt};
    if (resolved) {
        reference.path = std::move(*resolved);
    } else {
        reference.failure = ReferenceFailureCode::BadReference;
    }
    references_.push_back(std::move(reference));
}

} // namespace GYO::Engine::Asset
