#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/math/linear/VecInt.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Engine::MapMarkers {

// gyo.map_markers version 1 (docs/architecture/map-markers.md): named points,
// boxes and paths placed on a map. Ids, kinds and tags are opaque: what they
// mean, which kinds are allowed and whether a marker makes sense where it
// stands belong to the product that reads the file. This module checks form
// only.

inline constexpr std::string_view MapMarkersSchema = "gyo.map_markers";
inline constexpr int MapMarkersVersion = 1;
inline constexpr std::size_t MaxTextBytes = std::size_t{4} * 1024 * 1024;
inline constexpr std::size_t MaxMarkers = 65536;
inline constexpr std::size_t MaxNameLength = 64;      // id, kind and tag keys
inline constexpr std::size_t MaxTags = 32;
inline constexpr std::size_t MaxTagValueLength = 256;
inline constexpr std::size_t MinPathPoints = 2;
inline constexpr std::size_t MaxPathPoints = 4096;
// Coordinates are integers in 1/16 of a cell from the map's origin, within
// +-2^20 units.
inline constexpr std::int64_t CoordinateLimit = std::int64_t{1} << 20;

// Failure classes of the module. Zero is not a valid code; the numeric values
// are not a data contract.
enum class MapMarkersErrorCode : std::uint8_t {
    // A gyo.map_markers document breaks its contract.
    InvalidMapMarkers = 1,
};

[[nodiscard]] constexpr const char* ToString(const MapMarkersErrorCode code) noexcept {
    switch (code) {
    case MapMarkersErrorCode::InvalidMapMarkers: return "InvalidMapMarkers";
    }
    return "Unknown";
}

using MapMarkersError = Base::Error<MapMarkersErrorCode>;
static_assert(Base::CodedError<MapMarkersError>);

template <class T>
using MapMarkersResult = Base::Result<T, MapMarkersError>;

// A point with a heading: yaw is a finite angle in radians whose meaning
// belongs to the product.
struct MarkerPoint final {
    Math::Vec3i position{};
    float yaw{};
};

// An axis-aligned box; minimum < maximum on every axis.
struct MarkerBox final {
    Math::Vec3i minimum{};
    Math::Vec3i maximum{};
};

// Two or more points in order.
struct MarkerPath final {
    std::vector<Math::Vec3i> points;
};

struct Marker final {
    std::string id;   // unique in the document
    std::string kind; // opaque
    std::vector<std::pair<std::string, std::string>> tags; // opaque, in key order
    std::variant<MarkerPoint, MarkerBox, MarkerPath> shape;
};

// A validated document: markers in id order. Coordinates stay integers; a
// consumer turns them into world positions with the map they belong to (for
// a gyo.voxel_map, Engine::VoxelMesh::MapUnitsToWorld).
struct MapMarkersDocument final {
    std::vector<Marker> markers;
};

// Checks every rule of version 1. Anything the contract does not allow is an
// InvalidMapMarkers error with the offending path as its detail; nothing is
// ignored or repaired.
[[nodiscard]] MapMarkersResult<MapMarkersDocument> ParseMapMarkers(std::string_view text);

} // namespace Engine::MapMarkers
