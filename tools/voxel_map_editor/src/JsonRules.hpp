#pragma once

#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The strict JSON reading shared by the tool's parsers (gyo.voxel_grid and box
// import). Thrown inside parsing only and turned into VoxelMapError at the end.
namespace Gyo::Tools::VoxelMapEditor::JsonRules {

using Json = nlohmann::json;

struct Invalid final {
    std::string message;
    std::string path;
};

[[noreturn]] inline void Fail(std::string message, std::string path) {
    throw Invalid{std::move(message), std::move(path)};
}

inline std::string At(const std::string& path, const std::size_t index) {
    return path + "[" + std::to_string(index) + "]";
}

inline void RequireExactKeys(const Json& object, const std::set<std::string>& keys, const std::string& path) {
    for (const auto& key : keys)
        if (!object.contains(key)) Fail("missing field '" + key + "'", path);
    for (const auto& item : object.items())
        if (!keys.contains(item.key())) Fail("unknown field '" + item.key() + "'", path);
}

// The DOM keeps the last of repeated keys, so repeats are caught while parsing.
inline Json ParseWithoutRepeatedKeys(const std::string_view text) {
    std::vector<std::set<std::string>> open;
    Json::parser_callback_t callback = [&open](int, const Json::parse_event_t event, Json& parsed) {
        if (event == Json::parse_event_t::object_start) {
            open.emplace_back();
        } else if (event == Json::parse_event_t::object_end) {
            open.pop_back();
        } else if (event == Json::parse_event_t::key) {
            const auto key = parsed.get<std::string>();
            if (!open.back().insert(key).second) Fail("repeated key '" + key + "'", "");
        }
        return true;
    };
    try {
        return Json::parse(text.begin(), text.end(), callback);
    } catch (const Json::exception& failure) {
        Fail(std::string("not JSON: ") + failure.what(), "");
    }
}

// Raw integers are bounded before any arithmetic, so sums cannot overflow.
inline constexpr std::int64_t RawIntegerLimit = std::int64_t{1} << 30;

inline std::int64_t Integer(const Json& value, const std::string& path) {
    if (!value.is_number_integer()) Fail("an integer is expected", path);
    if (value.is_number_unsigned()) {
        const auto unsignedValue = value.get<std::uint64_t>();
        if (unsignedValue > static_cast<std::uint64_t>(RawIntegerLimit)) Fail("the integer is too large", path);
        return static_cast<std::int64_t>(unsignedValue);
    }
    const auto signedValue = value.get<std::int64_t>();
    if (signedValue < -RawIntegerLimit || signedValue > RawIntegerLimit) Fail("the integer is too large", path);
    return signedValue;
}

inline std::array<std::int64_t, 3> Triple(const Json& value, const std::string& path) {
    if (!value.is_array() || value.size() != 3) Fail("three integers are expected", path);
    return {Integer(value[0], At(path, 0)), Integer(value[1], At(path, 1)), Integer(value[2], At(path, 2))};
}

// The same names as Engine's palette names: 1 to 64 of a-z, 0-9, '_' and '.'.
inline std::string Name(const Json& value, const std::string& path) {
    const bool valid = value.is_string() && [&] {
        const auto& name = value.get_ref<const std::string&>();
        return !name.empty() && name.size() <= 64 && std::all_of(name.begin(), name.end(), [](const char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
        });
    }();
    if (!valid) Fail("a name is 1 to 64 characters of a-z, 0-9, '_' and '.'", path);
    return value.get<std::string>();
}

inline std::array<std::uint8_t, 4> Colour(const Json& value, const std::string& path) {
    if (!value.is_array() || value.size() != 4) Fail("a colour is four integers", path);
    std::array<std::uint8_t, 4> colour{};
    for (std::size_t channel = 0; channel < 4; ++channel) {
        const auto v = Integer(value[channel], At(path, channel));
        if (v < 0 || v > 255) Fail("a colour channel is 0 to 255", At(path, channel));
        colour[channel] = static_cast<std::uint8_t>(v);
    }
    return colour;
}

// A float as the shortest decimal of its double, which reads back (decimal to
// nearest double to nearest float) as the same float.
inline std::string FloatText(const float value) { return Json(static_cast<double>(value)).dump(); }

} // namespace Gyo::Tools::VoxelMapEditor::JsonRules
