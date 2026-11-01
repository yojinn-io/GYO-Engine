#pragma once

#include "engine/asset/catalog/CatalogParser.hpp"
#include "gyo/framework/FrameworkError.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>

// Registering an asset in one catalog, as structure: the entries Engine's
// CatalogParser reads, the catalog-level rules of the content contract, and
// the caller's registration policy. No text format here; the JSON binding
// (GYO::ToolFrameworkJson) reads and rewrites catalog text.

namespace Gyo::Tools::Framework {

// One catalog entry, as Engine reads it: id, type, path.
using CatalogEntry = Engine::Asset::Catalog::RawCatalogEntry;

// What a caller allows when an entry meets an existing one. Both are refused
// by default; neither is a rule of the content contract (two ids may share a
// file, cases.json valid-shared-file-distinct-ids).
struct RegistrationPolicy final {
    bool allowTypeChange{};  // the id is already registered with another type
    bool allowSharedPath{};  // the path is already registered under another id
};

// A catalog after one registration, as text.
struct CatalogUpdate final {
    std::string text;                        // the whole catalog after the change
    bool added{};                            // false: the entry with this id was updated
    std::optional<std::string> previousPath; // an updated entry's old path when it changed
};

struct RegistrationPlan final {
    bool added{};                            // false: the entry at index is updated
    std::size_t index{};                     // the updated entry, or entries.size() for a new one
    std::optional<std::string> previousPath; // an updated entry's old path when it changed
};

// The catalog-level rules of the content contract for one catalog (the
// catalog-level cases of tests/common/fixtures/asset_contract/cases.json):
// ids are unique, and each path is one Engine::Asset::IsRelativeContentPath
// accepts and is not content.json. Across catalogs and against shader bundles
// ContentManifest decides at load. InvalidCatalog otherwise.
[[nodiscard]] Result<void, CatalogError> CheckCatalogEntries(std::span<const CatalogEntry> entries);

// Where entry goes in a catalog whose entries pass CheckCatalogEntries: the
// entry with its id is updated, or it is added. InvalidRegistration: an empty
// id or type, a path the catalog rules refuse, or what the policy refuses.
[[nodiscard]] Result<RegistrationPlan, CatalogError> PlanRegistration(std::span<const CatalogEntry> entries,
                                                                      const CatalogEntry& entry,
                                                                      const RegistrationPolicy& policy = {});

} // namespace Gyo::Tools::Framework
