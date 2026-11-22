#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
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
// A reference is only recorded here, in declaration order. Holding the
// referenced asset and judging the rest of a reference (UnknownId,
// TypeMismatch, skipping, cycles) are the store's. A loader called directly (a
// tool checking its output) gets the list from References().
//
// A reference by path maps to an id through a ContentPathIndex when the context
// has one (the store gives its catalog's): then ReferencePath returns a
// reference with the id, or records UnlistedReference or AmbiguousReference.
// Without one (a tool calling a loader directly) only the path is recorded and
// the returned reference has an empty id.

namespace GYO::Engine::Asset {

enum class ReferenceKind : std::uint8_t { Id = 1, Path };

// A reference this context can already refuse. Zero is not a valid code.
enum class ReferenceFailureCode : std::uint8_t {
    // A path that is absolute, has a '\\', goes above the content root with
    // "..", or does not resolve to a relative content path.
    BadReference = 1,
    // A path no catalog entry has (with a ContentPathIndex only).
    UnlistedReference,
    // A path two or more catalog entries have (with a ContentPathIndex only).
    AmbiguousReference,
};

[[nodiscard]] constexpr const char* ToString(const ReferenceFailureCode code) noexcept {
    switch (code) {
    case ReferenceFailureCode::BadReference: return "BadReference";
    case ReferenceFailureCode::UnlistedReference: return "UnlistedReference";
    case ReferenceFailureCode::AmbiguousReference: return "AmbiguousReference";
    }
    return "Unknown";
}

struct DeclaredReference final {
    ReferenceKind kind;
    // AssetTypeName of the type the reference loads as.
    std::string typeName;
    // Kind Id: the id named. Kind Path: the id the path maps to through a
    // ContentPathIndex, empty without one or when the mapping failed.
    AssetId id;
    // Kind Path: the path as the loader wrote it, and the content path it
    // resolves to (empty with BadReference).
    std::string writtenPath;
    std::string path;
    std::optional<ReferenceFailureCode> failure;

    [[nodiscard]] friend bool operator==(const DeclaredReference&, const DeclaredReference&) = default;
};

// Maps a content path to the ids of the catalog entries whose path it is.
class ContentPathIndex {
public:
    virtual ~ContentPathIndex() = default;
    // None, one, or more when two entries name one file.
    [[nodiscard]] virtual std::span<const AssetId> IdsAt(std::string_view path) const = 0;
};

class LoadContext final {
public:
    // path is the entry's content path; references by path resolve against
    // its directory. paths, when given, must outlive the context.
    LoadContext(AssetId id, std::string path, std::size_t maxBytes, const ContentPathIndex* paths = nullptr);

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
    // Recorded with BadReference when the path cannot name a content file;
    // with a ContentPathIndex, with UnlistedReference or AmbiguousReference
    // when it names no entry's file or more than one's. The reference has the
    // id the path maps to, or an empty id.
    template <class U>
        requires HasAssetTypeName<U>
    [[nodiscard]] AssetRef<U> ReferencePath(std::string_view relativePath) {
        return AssetRef<U>(RecordPath(AssetTypeName<U>::value, relativePath));
    }

    [[nodiscard]] const std::vector<DeclaredReference>& References() const noexcept { return references_; }

private:
    void RecordId(std::string_view typeName, const AssetId& id);
    AssetId RecordPath(std::string_view typeName, std::string_view relativePath);

    AssetId id_;
    std::string path_;
    std::size_t maxBytes_;
    const ContentPathIndex* paths_;
    std::vector<DeclaredReference> references_;
};

} // namespace GYO::Engine::Asset
