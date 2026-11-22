#include <doctest/doctest.h>

// MSVC needs <ostream> for doctest to print std::string_view in CHECK.
#include <ostream>

#include <string>
#include <string_view>

#include "engine/base/Error.hpp"
#include "engine/json/JsonError.hpp"

using GYO::Engine::Json::JsonError;
using GYO::Engine::Json::JsonErrorCode;

TEST_CASE("JsonErrorCode: every code has its own name") {
    CHECK(std::string_view(ToString(JsonErrorCode::Syntax)) == "Syntax");
    CHECK(std::string_view(ToString(JsonErrorCode::RepeatedKey)) == "RepeatedKey");
    CHECK(std::string_view(ToString(JsonErrorCode::NumberOutOfRange)) == "NumberOutOfRange");
    CHECK(std::string_view(ToString(JsonErrorCode::WrongKind)) == "WrongKind");
    CHECK(std::string_view(ToString(JsonErrorCode::NotRepresentable)) == "NotRepresentable");
    CHECK(std::string_view(ToString(JsonErrorCode::OutOfMemory)) == "OutOfMemory");
}

TEST_CASE("JsonError: formatted through the one formatter") {
    const JsonError error = JsonError::Make(JsonErrorCode::Syntax, "not JSON", "line 1, column 3");
    CHECK(::GYO::Engine::Base::Describe(error) == "Syntax: not JSON (line 1, column 3)");
}
