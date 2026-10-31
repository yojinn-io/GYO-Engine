#pragma once

#include "gyo/audio_editor/AudioEditorError.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace Gyo::Tools::AudioEditor {

struct SoundRegistration final {
    std::string id;   // the catalog id, for example "product.audio.reload"
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

// Adds or updates the "sound" entry with registration's id in a catalog v1
// text (Design Invariant 3). Only that entry's text changes: every other byte
// of the catalog, including the layout of other entries, stays as it was. A
// new entry follows the last one with its indentation.
//
// InvalidCatalog: the text is not a catalog v1 (an object with integer version
// 1 and an assets array of objects with nonempty string id, type and path,
// unique ids). InvalidRegistration: an invalid id or path, the id registered
// with another type, or the path registered under another id.
[[nodiscard]] Result<CatalogUpdate, AudioEditorError> RegisterSound(std::string_view catalogText,
                                                                    const SoundRegistration& registration);

} // namespace Gyo::Tools::AudioEditor
