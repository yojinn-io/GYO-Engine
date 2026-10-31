#pragma once

#include "gyo/framework/FrameworkError.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace Gyo::Tools::Framework {

struct AssetRegistration final {
    std::string id;   // the catalog id
    std::string type; // the asset type, chosen by the tool (Design Invariant 3)
    std::string path; // relative to the asset root, '/' separated
};

struct CatalogUpdate final {
    std::string text;                        // the whole catalog after the change
    bool added{};                            // false: the entry with this id was updated
    std::optional<std::string> previousPath; // an updated entry's old path when it changed
};

// Whether path is one the content contract accepts for an asset: relative, '/'
// separated, no empty, '.' or '..' segment, and not content.json.
[[nodiscard]] bool IsValidAssetPath(std::string_view path) noexcept;

// Adds or updates the entry with registration's id in a catalog v1 text
// (Design Invariant 4). Only that entry's text changes: every other byte of
// the catalog, including the layout of other entries, stays as it was. A new
// entry follows the last one with its indentation. The caller writes the text
// back (by atomic replace).
//
// InvalidCatalog: the text is not a catalog v1 (an object with integer version
// 1 and an assets array of objects with nonempty string id, type and path,
// unique ids). InvalidRegistration: an empty id or type, an invalid path, the
// id registered with another type, or the path registered under another id.
[[nodiscard]] Result<CatalogUpdate, CatalogError> RegisterAsset(std::string_view catalogText,
                                                                const AssetRegistration& registration);

} // namespace Gyo::Tools::Framework
