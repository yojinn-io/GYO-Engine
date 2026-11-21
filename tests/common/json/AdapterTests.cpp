// The adapter's own tests: they reach inside engine/modules/json/src/adapters, which
// only tests/common/json may do (Design Invariant J-1).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstddef>
#include <string_view>

#include <yyjson.h>

#include "adapters/yyjson/ParseWith.hpp"

using GYO::Engine::Json::JsonErrorCode;

namespace {

void* FailMalloc(void*, std::size_t) { return nullptr; }
void* FailRealloc(void*, void*, std::size_t, std::size_t) { return nullptr; }
void NoFree(void*, void*) {}

} // namespace

TEST_CASE("ParseWith: an allocation failure in yyjson is OutOfMemory, not an exception") {
    const yyjson_alc failing{FailMalloc, FailRealloc, NoFree, nullptr};
    auto document = GYO::Engine::Json::Detail::ParseWith(R"({"a":[1,2,3]})", &failing);
    REQUIRE_FALSE(document.has_value());
    CHECK(document.error().code == JsonErrorCode::OutOfMemory);
}
