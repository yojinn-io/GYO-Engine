#pragma once

#include <string_view>

#include <yyjson.h>

#include "engine/json/Document.hpp"

namespace GYO::Engine::Json::Detail {

// Parse with an explicit yyjson allocator (nullptr: the default one). Parse
// calls it with nullptr; the adapter's own tests pass a failing allocator to
// reach OutOfMemory. Lives under src/adapters/yyjson because its parameter is
// a yyjson type (Design Invariant J-1).
[[nodiscard]] JsonResult<Document> ParseWith(std::string_view text, const yyjson_alc* allocator);

} // namespace GYO::Engine::Json::Detail
