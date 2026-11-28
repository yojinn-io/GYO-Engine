#pragma once

#include "engine/json/Document.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace object_fps_pvp::data {

// Reading the members of the game's JSON data contracts. Each function throws
// std::runtime_error naming what was wrong; a parser catches it once and turns
// it into its error text, so a contract's reader stays a straight list of the
// members it takes.

[[nodiscard]] GYO::Engine::Json::Document ParseDocument(std::string_view text);
[[nodiscard]] GYO::Engine::Json::ObjectView AsObject(const GYO::Engine::Json::Value& value, std::string_view what);
// The member key of object; missing is an error.
[[nodiscard]] GYO::Engine::Json::Value Member(const GYO::Engine::Json::ObjectView& object, std::string_view key);
[[nodiscard]] bool HasMember(const GYO::Engine::Json::ObjectView& object, std::string_view key);
[[nodiscard]] double Number(const GYO::Engine::Json::Value& value, std::string_view what);
[[nodiscard]] std::int64_t Integer(const GYO::Engine::Json::Value& value, std::string_view what);
[[nodiscard]] std::uint64_t Unsigned(const GYO::Engine::Json::Value& value, std::string_view what);
[[nodiscard]] std::string String(const GYO::Engine::Json::Value& value, std::string_view what);
[[nodiscard]] bool Boolean(const GYO::Engine::Json::Value& value, std::string_view what);
// A number that is finite and positive as a float.
[[nodiscard]] float Positive(const GYO::Engine::Json::Value& value, std::string_view what);
// The member key as Positive; absent: fallback.
[[nodiscard]] float PositiveOr(const GYO::Engine::Json::ObjectView& object, std::string_view key, float fallback);
// A file under the content root: a relative path of plain names separated by '/' (none empty, "." or "..", none with
// a backslash or ':').
[[nodiscard]] std::string ContentPath(const GYO::Engine::Json::Value& value, std::string_view what);

} // namespace object_fps_pvp::data
