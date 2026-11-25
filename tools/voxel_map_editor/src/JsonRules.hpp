#pragma once

#include "voxel_map_editor/Grid.hpp"

#include "engine/json/Document.hpp"
#include "engine/json/Writer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

// The strict JSON reading shared by the tool's parsers (gyo.voxel_grid and box import), on the json Module. A rule
// that breaks gives a Problem (message and the path of the offending value), turned into the tool's Error at the end.
namespace voxel_map_editor::JsonRules {

namespace Json = GYO::Engine::Json;

struct Problem final {
    std::string message;
    std::string path;
};
using Check = std::optional<Problem>;

inline Check Broken(std::string message, std::string path) { return Problem{std::move(message), std::move(path)}; }

inline std::string At(const std::string& path, const std::size_t index) {
    return path + "[" + std::to_string(index) + "]";
}
inline std::string Field(const std::string& path, const std::string_view key) {
    return path.empty() ? std::string(key) : path + "." + std::string(key);
}

// The object at path with exactly these keys; repeated keys are refused by Json::Parse already.
inline Check ExactObject(const Json::Value& value, const std::initializer_list<std::string_view> keys,
                         const std::string& path, const std::string_view what, std::optional<Json::ObjectView>& out) {
    const auto object = value.AsObject();
    if (!object) return Broken(std::string(what) + " is an object", path);
    for (const std::string_view key : keys)
        if (!object->Find(key)) return Broken("missing field '" + std::string(key) + "'", path);
    for (const Json::Member member : *object)
        if (std::find(keys.begin(), keys.end(), member.key) == keys.end())
            return Broken("unknown field '" + std::string(member.key) + "'", path);
    out = *object;
    return std::nullopt;
}

inline Json::Value Get(const Json::ObjectView& object, const std::string_view key) { return *object.Find(key); }

// Raw integers are bounded before any arithmetic, so sums cannot overflow.
inline constexpr std::int64_t kRawIntegerLimit = std::int64_t{1} << 30;

inline Check Integer(const Json::Value& value, const std::string& path, std::int64_t& out) {
    if (value.GetKind() != Json::Kind::Number || !value.IsIntegerLiteral())
        return Broken("an integer is expected", path);
    const auto number = value.AsInt64();
    if (!number || *number < -kRawIntegerLimit || *number > kRawIntegerLimit)
        return Broken("the integer is too large", path);
    out = *number;
    return std::nullopt;
}

template <std::size_t N>
Check Integers(const Json::Value& value, const std::string& path, const std::string_view what,
               std::array<std::int64_t, N>& out) {
    const auto array = value.AsArray();
    if (!array || array->Size() != N) return Broken(std::string(what), path);
    std::size_t i = 0;
    for (const Json::Value item : *array) {
        if (Check broken = Integer(item, At(path, i), out[i])) return broken;
        ++i;
    }
    return std::nullopt;
}

inline Check Triple(const Json::Value& value, const std::string& path, std::array<std::int64_t, 3>& out) {
    return Integers(value, path, "three integers are expected", out);
}

// The same names as the map's palette names: 1 to 64 of a-z, 0-9, '_' and '.'.
inline bool IsName(const std::string_view name) {
    return !name.empty() && name.size() <= 64 && std::all_of(name.begin(), name.end(), [](const char c) {
               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
           });
}

inline Check Name(const Json::Value& value, const std::string& path, std::string& out) {
    const auto text = value.AsString();
    if (!text || !IsName(*text)) return Broken("a name is 1 to 64 characters of a-z, 0-9, '_' and '.'", path);
    out = std::string(*text);
    return std::nullopt;
}

inline Check Colour(const Json::Value& value, const std::string& path, std::array<std::uint8_t, 4>& out) {
    std::array<std::int64_t, 4> channels{};
    if (Check broken = Integers(value, path, "a colour is four integers", channels)) return broken;
    for (std::size_t channel = 0; channel < 4; ++channel) {
        if (channels[channel] < 0 || channels[channel] > 255) return Broken("a colour channel is 0 to 255", At(path, channel));
        out[channel] = static_cast<std::uint8_t>(channels[channel]);
    }
    return std::nullopt;
}

// A float as the shortest decimal of its double (the json Module's writer), which reads back (decimal to nearest
// double to nearest float) as the same float.
inline std::string FloatText(const float value) {
    std::string out;
    Json::Writer writer(out);
    writer.Double(static_cast<double>(value));
    // A finite float always writes; cell sizes are checked finite before they get here.
    (void)writer.Finish();
    return out;
}

// A JSON string of a name. Names are validated (IsName), so no character needs escaping; the json Module's writer
// is used all the same, so the text is right whatever it holds.
inline std::string Quoted(const std::string_view text) {
    std::string out;
    Json::Writer writer(out);
    writer.String(text);
    (void)writer.Finish();
    return out;
}

} // namespace voxel_map_editor::JsonRules
