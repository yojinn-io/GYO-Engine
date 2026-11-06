#include "engine/map_markers/MapMarkers.hpp"

#include "engine/base/StrictJson.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Engine::MapMarkers {
namespace {
// Ordered: rule K6 asks for tag keys in increasing order in the text, which a
// DOM that sorts its keys cannot see.
using Json = nlohmann::ordered_json;

// Raw integers are bounded before any arithmetic.
constexpr std::int64_t RawIntegerLimit = std::int64_t{1} << 30;

// Thrown inside parsing only, and turned into the module's error at its end.
struct Invalid final {
    std::string message;
    std::string path;
};

[[noreturn]] void Fail(std::string message, std::string path) {
    throw Invalid{std::move(message), std::move(path)};
}

std::string At(const std::string& path, const std::size_t index) {
    return path + "[" + std::to_string(index) + "]";
}

void RequireExactKeys(const Json& object, const std::set<std::string>& keys, const std::string& path) {
    for (const auto& key : keys)
        if (!object.contains(key)) Fail("missing field '" + key + "'", path);
    for (const auto& item : object.items())
        if (!keys.contains(item.key())) Fail("unknown field '" + item.key() + "'", path);
}

std::int64_t Integer(const Json& value, const std::string& path) {
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

// Three integers within +-2^20 units (rule K10).
Math::Vec3i Coordinates(const Json& value, const std::string& path) {
    if (!value.is_array() || value.size() != 3) Fail("three integers are expected", path);
    std::array<std::int32_t, 3> result{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto units = Integer(value[axis], At(path, axis));
        if (units < -CoordinateLimit || units > CoordinateLimit)
            Fail("a coordinate lies beyond +-2^20 units", At(path, axis));
        result[axis] = static_cast<std::int32_t>(units);
    }
    return {result[0], result[1], result[2]};
}

// ids and kinds: 1 to 64 characters of a-z 0-9 _ . - (rule K5).
bool ValidName(const std::string& name) {
    if (name.empty() || name.size() > MaxNameLength) return false;
    return std::all_of(name.begin(), name.end(), [](const char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
    });
}

// Tag keys: 1 to 64 characters of a-z 0-9 _ . (rule K6).
bool ValidTagKey(const std::string& key) {
    if (key.empty() || key.size() > MaxNameLength) return false;
    return std::all_of(key.begin(), key.end(), [](const char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
    });
}

bool ValidTagValue(const std::string& value) {
    if (value.size() > MaxTagValueLength) return false;
    return std::all_of(value.begin(), value.end(), [](const char c) { return c >= 0x20 && c <= 0x7E; });
}

// No object of a valid document has more keys than rule K6 allows tags.
// ordered_json inserts each key with a linear search, so a larger object would
// cost quadratic time before any rule rejects it; this scan rejects it first.
// It changes when an error is found, not which documents are valid: the path
// it reports is the one the rules would report for that object.
constexpr std::size_t MaxObjectKeys = MaxTags;

class KeyCountScan final {
public:
    using number_integer_t = Json::number_integer_t;
    using number_unsigned_t = Json::number_unsigned_t;
    using number_float_t = Json::number_float_t;
    using string_t = Json::string_t;
    using binary_t = Json::binary_t;

    bool null() { return Value(); }
    bool boolean(bool) { return Value(); }
    bool number_integer(number_integer_t) { return Value(); }
    bool number_unsigned(number_unsigned_t) { return Value(); }
    bool number_float(number_float_t, const string_t&) { return Value(); }
    bool string(string_t&) { return Value(); }
    bool binary(binary_t&) { return Value(); }
    bool start_object(std::size_t) {
        frames_.push_back({ChildPath(), false, 0, {}});
        return true;
    }
    bool key(string_t& key) {
        auto& frame = frames_.back();
        frame.key = key;
        if (++frame.count <= MaxObjectKeys) return true;
        tooManyKeys_ = true;
        path_ = frame.path;
        return false;
    }
    bool end_object() {
        frames_.pop_back();
        return Value();
    }
    bool start_array(std::size_t) {
        frames_.push_back({ChildPath(), true, 0, {}});
        return true;
    }
    bool end_array() {
        frames_.pop_back();
        return Value();
    }
    // Text that is not JSON is the strict parse's to report.
    bool parse_error(std::size_t, const std::string&, const Json::exception&) { return false; }

    [[nodiscard]] bool TooManyKeys() const noexcept { return tooManyKeys_; }
    [[nodiscard]] const std::string& Path() const noexcept { return path_; }

private:
    // An open container: its path, whether it is an array, and its count of
    // elements so far (an array) or keys so far (an object, whose last key is
    // the member being read).
    struct Frame final {
        std::string path;
        bool array{};
        std::size_t count{};
        std::string key;
    };

    // The path of the value that starts now, written as the rules write it.
    [[nodiscard]] std::string ChildPath() const {
        if (frames_.empty()) return "";
        const auto& frame = frames_.back();
        if (frame.array) return At(frame.path, frame.count);
        return frame.path.empty() ? frame.key : frame.path + "." + frame.key;
    }

    // A value of the current container ended: the next array element follows.
    bool Value() {
        if (!frames_.empty() && frames_.back().array) ++frames_.back().count;
        return true;
    }

    std::vector<Frame> frames_;
    bool tooManyKeys_{};
    std::string path_;
};

std::vector<std::pair<std::string, std::string>> ParseTags(const Json& tags, const std::string& path) {
    if (!tags.is_object()) Fail("tags is an object", path);
    // At most 32 entries (rule K6): the key-count scan rejects a larger object
    // before the document is built, at this same path.
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& item : tags.items()) {
        const auto& key = item.key();
        if (!ValidTagKey(key)) Fail("a tag key is 1 to 64 characters of a-z, 0-9, '_' and '.'", path);
        if (!result.empty() && !(result.back().first < key)) Fail("tag keys are in strictly increasing order", path);
        const auto& value = item.value();
        if (!value.is_string() || !ValidTagValue(value.get<std::string>()))
            Fail("a tag value is a string of 0 to 256 printable ASCII characters", path + "." + key);
        result.emplace_back(key, value.get<std::string>());
    }
    return result;
}

Marker ParseMarker(const Json& entry, const std::string& path) {
    if (!entry.is_object()) Fail("a marker is an object", path);
    const auto shapes = static_cast<int>(entry.contains("point")) + static_cast<int>(entry.contains("box")) +
                        static_cast<int>(entry.contains("path"));
    if (shapes != 1) Fail("a marker has exactly one of point, box and path", path);
    if (entry.contains("point"))
        RequireExactKeys(entry, {"id", "kind", "tags", "point", "yaw"}, path);
    else if (entry.contains("box"))
        RequireExactKeys(entry, {"id", "kind", "tags", "box"}, path);
    else
        RequireExactKeys(entry, {"id", "kind", "tags", "path"}, path);

    Marker marker;
    const auto& id = entry.at("id");
    if (!id.is_string() || !ValidName(id.get<std::string>()))
        Fail("id is 1 to 64 characters of a-z, 0-9, '_', '.' and '-'", path + ".id");
    marker.id = id.get<std::string>();
    const auto& kind = entry.at("kind");
    if (!kind.is_string() || !ValidName(kind.get<std::string>()))
        Fail("kind is 1 to 64 characters of a-z, 0-9, '_', '.' and '-'", path + ".kind");
    marker.kind = kind.get<std::string>();
    marker.tags = ParseTags(entry.at("tags"), path + ".tags");

    if (entry.contains("point")) {
        MarkerPoint point;
        point.position = Coordinates(entry.at("point"), path + ".point");
        const auto& yaw = entry.at("yaw");
        if (!yaw.is_number()) Fail("yaw is a number", path + ".yaw");
        point.yaw = static_cast<float>(yaw.get<double>());
        if (!std::isfinite(point.yaw)) Fail("yaw is finite as a float", path + ".yaw");
        marker.shape = point;
    } else if (entry.contains("box")) {
        const auto& box = entry.at("box");
        const auto boxPath = path + ".box";
        if (!box.is_object()) Fail("box is an object", boxPath);
        RequireExactKeys(box, {"min", "max"}, boxPath);
        MarkerBox result{Coordinates(box.at("min"), boxPath + ".min"), Coordinates(box.at("max"), boxPath + ".max")};
        if (!(result.minimum.x < result.maximum.x) || !(result.minimum.y < result.maximum.y) ||
            !(result.minimum.z < result.maximum.z))
            Fail("min is below max on every axis", boxPath);
        marker.shape = result;
    } else {
        const auto& points = entry.at("path");
        const auto pointsPath = path + ".path";
        if (!points.is_array() || points.size() < MinPathPoints || points.size() > MaxPathPoints)
            Fail("path lists 2 to 4096 points", pointsPath);
        MarkerPath result;
        for (std::size_t i = 0; i < points.size(); ++i) result.points.push_back(Coordinates(points[i], At(pointsPath, i)));
        marker.shape = std::move(result);
    }
    return marker;
}

MapMarkersDocument ParseDocument(const std::string_view text) {
    if (text.size() > MaxTextBytes) Fail("the text exceeds 4 MiB", "");
    KeyCountScan keys;
    if (!Json::sax_parse(text.begin(), text.end(), &keys) && keys.TooManyKeys())
        Fail("an object has more than " + std::to_string(MaxObjectKeys) + " keys", keys.Path());
    std::string failure;
    const auto parsed = Base::ParseStrictJson<Json>(text, failure);
    if (!parsed) Fail(failure, "");
    const auto& document = *parsed;
    if (!document.is_object()) Fail("the document is an object", "");
    RequireExactKeys(document, {"schema", "version", "markers"}, "");
    const auto& schema = document.at("schema");
    if (!schema.is_string() || schema.get<std::string>() != MapMarkersSchema)
        Fail("schema is '" + std::string(MapMarkersSchema) + "'", "schema");
    const auto& version = document.at("version");
    if (!version.is_number_integer() || version.get<long long>() != MapMarkersVersion)
        Fail("version is " + std::to_string(MapMarkersVersion), "version");

    const auto& markers = document.at("markers");
    if (!markers.is_array() || markers.size() > MaxMarkers) Fail("markers lists 0 to 65536 markers", "markers");
    MapMarkersDocument result;
    result.markers.reserve(markers.size());
    for (std::size_t i = 0; i < markers.size(); ++i) {
        auto marker = ParseMarker(markers[i], At("markers", i));
        if (!result.markers.empty() && !(result.markers.back().id < marker.id))
            Fail("markers are in strictly increasing order of id", At("markers", i) + ".id");
        result.markers.push_back(std::move(marker));
    }
    return result;
}

} // namespace

MapMarkersResult<MapMarkersDocument> ParseMapMarkers(const std::string_view text) {
    try {
        return ParseDocument(text);
    } catch (Invalid& invalid) {
        return Base::Err(MapMarkersError::Make(MapMarkersErrorCode::InvalidMapMarkers,
                                               "gyo.map_markers: " + std::move(invalid.message),
                                               std::move(invalid.path)));
    }
}

} // namespace Engine::MapMarkers
