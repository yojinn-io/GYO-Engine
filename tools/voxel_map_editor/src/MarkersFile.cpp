#include "gyo/voxel_map_editor/MarkersFile.hpp"

#include "BuildParts.hpp"
#include "JsonRules.hpp"

#include <algorithm>
#include <cstring>
#include <utility>
#include <variant>

namespace Gyo::Tools::VoxelMapEditor {
namespace {

namespace MM = Engine::MapMarkers;

VoxelMapError Invalid(const MM::MapMarkersError& error) {
    return VoxelMapError::Make(VoxelMapErrorCode::InvalidMarkers, error.message, error.detail);
}

// Bytes that are not UTF-8 are dropped instead of thrown about: Engine then
// refuses the text or it reads back different, and nothing escapes as an
// exception.
std::string Quoted(const std::string& text) {
    return JsonRules::Json(text).dump(-1, ' ', false, JsonRules::Json::error_handler_t::ignore);
}

std::string Point(const Engine::Math::Vec3i& p) { return BuildParts::Ints({p.x, p.y, p.z}); }

bool SamePoint(const Engine::Math::Vec3i& a, const Engine::Math::Vec3i& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool SameBits(const float a, const float b) {
    std::uint32_t x{}, y{};
    std::memcpy(&x, &a, sizeof x);
    std::memcpy(&y, &b, sizeof y);
    return x == y;
}

bool SameShape(const MM::Marker& a, const MM::Marker& b) {
    if (a.shape.index() != b.shape.index()) return false;
    if (const auto* p = std::get_if<MM::MarkerPoint>(&a.shape)) {
        const auto& q = std::get<MM::MarkerPoint>(b.shape);
        return SamePoint(p->position, q.position) && SameBits(p->yaw, q.yaw);
    }
    if (const auto* box = std::get_if<MM::MarkerBox>(&a.shape)) {
        const auto& other = std::get<MM::MarkerBox>(b.shape);
        return SamePoint(box->minimum, other.minimum) && SamePoint(box->maximum, other.maximum);
    }
    const auto& path = std::get<MM::MarkerPath>(a.shape).points;
    const auto& other = std::get<MM::MarkerPath>(b.shape).points;
    return std::equal(path.begin(), path.end(), other.begin(), other.end(), SamePoint);
}

bool SameMarker(const MM::Marker& a, const MM::Marker& b) {
    return a.id == b.id && a.kind == b.kind && a.tags == b.tags && SameShape(a, b);
}

} // namespace

Result<MM::MapMarkersDocument> ReadMarkers(const std::string_view text) {
    auto document = MM::ParseMapMarkers(text);
    if (!document) return Engine::Base::Err(Invalid(document.error()));
    return std::move(document).value();
}

Result<std::vector<std::string>> ReadMarkerKinds(const std::string_view text) {
    auto kinds = MM::ParseMarkerKinds(text);
    if (!kinds) return Engine::Base::Err(Invalid(kinds.error()));
    return std::move(kinds).value();
}

Result<std::string> WriteMarkers(const MM::MapMarkersDocument& document) {
    std::string out = "{\n  \"schema\": \"" + std::string(MM::MapMarkersSchema) + "\",\n";
    out += "  \"version\": " + std::to_string(MM::MapMarkersVersion) + ",\n";
    out += "  \"markers\": [";
    for (std::size_t i = 0; i < document.markers.size(); ++i) {
        const auto& marker = document.markers[i];
        out += i ? ",\n    " : "\n    ";
        out += "{ \"id\": " + Quoted(marker.id) + ", \"kind\": " + Quoted(marker.kind) + ", \"tags\": {";
        for (std::size_t t = 0; t < marker.tags.size(); ++t) {
            out += t ? ", " : " ";
            out += Quoted(marker.tags[t].first) + ": " + Quoted(marker.tags[t].second);
        }
        out += marker.tags.empty() ? "}, " : " }, ";
        if (const auto* point = std::get_if<MM::MarkerPoint>(&marker.shape)) {
            out += "\"point\": " + Point(point->position) + ", \"yaw\": " + JsonRules::FloatText(point->yaw);
        } else if (const auto* box = std::get_if<MM::MarkerBox>(&marker.shape)) {
            out += "\"box\": { \"min\": " + Point(box->minimum) + ", \"max\": " + Point(box->maximum) + " }";
        } else {
            out += "\"path\": [";
            const auto& points = std::get<MM::MarkerPath>(marker.shape).points;
            for (std::size_t p = 0; p < points.size(); ++p) out += (p ? ", " : "") + Point(points[p]);
            out += "]";
        }
        out += " }";
    }
    out += document.markers.empty() ? "]\n}\n" : "\n  ]\n}\n";

    // One validator (Design Invariant 2): what Engine refuses or reads back
    // different is not returned.
    const auto readBack = MM::ParseMapMarkers(out);
    if (!readBack)
        return Engine::Base::Err(VoxelMapError::Make(VoxelMapErrorCode::BuildRefused,
                                                     "Engine refuses the markers text: " + readBack.error().message,
                                                     readBack.error().detail));
    if (!std::equal(readBack->markers.begin(), readBack->markers.end(), document.markers.begin(),
                    document.markers.end(), SameMarker))
        return Engine::Base::Err(
            VoxelMapError::Make(VoxelMapErrorCode::BuildRefused, "the markers text reads back different", ""));
    return out;
}

} // namespace Gyo::Tools::VoxelMapEditor
