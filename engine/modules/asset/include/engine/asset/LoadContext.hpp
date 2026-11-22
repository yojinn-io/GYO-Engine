#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetRef.hpp"
#include "engine/asset/AssetTypeName.hpp"

// What a loader is given besides the bytes and the settings (declaration 5.16,
// 5.17): the id and path of the entry being loaded (for messages), the byte
// limit, and the way to declare references to other assets.
//
// A reference is only recorded here, in declaration order. Mapping a path to an
// id through the catalog, holding the referenced asset and judging the rest of
// a reference (UnlistedReference, AmbiguousReference, UnknownId, TypeMismatch,
// cycles) are the store's. A loader called directly (a tool checking its
// output) gets the list from References().

namespace GYO::Engine::Asset {

enum class ReferenceKind : std::uint8_t { Id = 1, Path };

// A reference this context can already refuse. Zero is not a valid code.
enum class ReferenceFailureCode : std::uint8_t {
    // A path that is absolute, has a '\\', goes above the content root with
    // "..", or does not resolve to a relative content path.
    BadReference = 1,
};

[[nodiscard]] constexpr const char* ToString(const ReferenceFailureCode code) noexcept {
    switch (code) {
    case ReferenceFailureCode::BadReference: return "BadReference";
    }
    return "Unknown";
}

struct DeclaredReference final {
    ReferenceKind kind;
    // AssetTypeName of the type the reference loads as.
    std::string typeName;
    // Kind Id: the id named.
    AssetId id;
    // Kind Path: the path as the loader wrote it, and the content path it
    // resolves to (empty with BadReference).
    std::string writtenPath;
    std::string path;
    std::optional<ReferenceFailureCode> failure;

    [[nodiscard]] friend bool operator==(const DeclaredReference&, const DeclaredReference&) = default;
};

class LoadContext final {
public:
    // path is the entry's content path; references by path resolve against
    // its directory.
    LoadContext(AssetId id, std::string path, std::size_t maxBytes);

    [[nodiscard]] const AssetId& Id() const noexcept { return id_; }
    [[nodiscard]] const std::string& Path() const noexcept { return path_; }
    [[nodiscard]] std::size_t MaxBytes() const noexcept { return maxBytes_; }

    // A reference to the asset with this id, loading as U.
    template <class U>
        requires HasAssetTypeName<U>
    [[nodiscard]] AssetRef<U> Reference(AssetId id) {
        RecordId(AssetTypeName<U>::value, id);
        return AssetRef<U>(std::move(id));
    }

    // A reference to the file at this path, relative to the directory of this
    // entry's file (for example a texture file name inside a model file).
    // Recorded with BadReference when the path cannot name a content file.
    template <class U>
        requires HasAssetTypeName<U>
    [[nodiscard]] AssetRef<U> ReferencePath(std::string_view relativePath) {
        RecordPath(AssetTypeName<U>::value, relativePath);
        return AssetRef<U>();
    }

    [[nodiscard]] const std::vector<DeclaredReference>& References() const noexcept { return references_; }

private:
    void RecordId(std::string_view typeName, const AssetId& id);
    void RecordPath(std::string_view typeName, std::string_view relativePath);

    AssetId id_;
    std::string path_;
    std::size_t maxBytes_;
    std::vector<DeclaredReference> references_;
};

} // namespace GYO::Engine::Asset
