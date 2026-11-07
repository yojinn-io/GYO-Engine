#pragma once

#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "engine/map_markers/MapMarkers.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

// Markers of a map (gyo.map_markers) and the kinds a product lists for them
// (gyo.marker_kinds), read and written only through Engine's GYO::MapMarkers
// (v8-arch-big-map-tool-chunked, tc-markers). The editor places and names
// markers; what a kind or a tag means is the product's, and the editor
// interprets none of them.
namespace Gyo::Tools::VoxelMapEditor {

// ParseMapMarkers; InvalidMarkers with Engine's message and path.
[[nodiscard]] Result<Engine::MapMarkers::MapMarkersDocument> ReadMarkers(std::string_view text);
// ParseMarkerKinds; InvalidMarkers with Engine's message and path.
[[nodiscard]] Result<std::vector<std::string>> ReadMarkerKinds(std::string_view text);

// The editor's text of a markers document: markers in the document's order,
// tags in their order, yaw as the shortest decimal of the float's double,
// with spaces after commas. The text is read back with ParseMapMarkers and
// compared with the document; BuildRefused when Engine refuses it (a document
// out of id order; a text over 4 MiB, which a compact file of about 3 MiB of
// paths can reach once written this way) or it reads back different (a name
// that is not UTF-8, whose bytes are dropped).
[[nodiscard]] Result<std::string> WriteMarkers(const Engine::MapMarkers::MapMarkersDocument& document);

} // namespace Gyo::Tools::VoxelMapEditor
