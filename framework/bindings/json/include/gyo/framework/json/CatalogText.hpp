#pragma once

#include "gyo/framework/CatalogPlan.hpp"
#include "gyo/framework/FrameworkError.hpp"

#include <string_view>
#include <vector>

// The JSON binding (framework/bindings/json, target GYO::ToolFrameworkJson):
// catalog v1 text. Whether a text is a catalog v1 is decided by Engine's
// CatalogParser; the catalog-level rules are the core's CheckCatalogEntries.

namespace Gyo::Tools::Framework {

// The entries of a catalog v1 text. InvalidCatalog when Engine's
// CatalogParser refuses it or CheckCatalogEntries does.
[[nodiscard]] Result<std::vector<CatalogEntry>, CatalogError> ReadCatalog(std::string_view catalogText);

// Adds or updates the entry with entry's id in a catalog v1 text, as
// PlanRegistration places it (Design Invariant 4). Every other byte of the
// catalog, including the layout of other entries, stays as it was: an update
// changes only the entry's type and path values and keeps its other fields
// (the content contract keeps unknown optional metadata); a new entry follows
// the last one with its indentation. The result is read again and compared
// with the expected entries as data. The caller writes the text back (by atomic replace).
// InvalidCatalog: the text is not a catalog ReadCatalog accepts.
// InvalidRegistration: what PlanRegistration refuses.
[[nodiscard]] Result<CatalogUpdate, CatalogError> RegisterAsset(std::string_view catalogText, const CatalogEntry& entry,
                                                                const RegistrationPolicy& policy = {});

} // namespace Gyo::Tools::Framework
