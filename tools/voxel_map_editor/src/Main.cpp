#include "gyo/voxel_map_editor/ChunkedMap.hpp"
#include "gyo/voxel_map_editor/MarkersFile.hpp"
#include "gyo/voxel_map_editor/VoxelMap.hpp"
#include "window/EditorWindow.hpp"

#include "MapMeasure.hpp"

#include "gyo/framework/HeadlessShell.hpp"

#include "engine/base/Error.hpp"
#include "engine/base/Sha256.hpp"
#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <span>
#include <string>
#include <string_view>

namespace {

using namespace Gyo::Tools::VoxelMapEditor;

constexpr std::string_view kHelp =
    "Usage:\n"
    "  gyo_voxel_map_editor build   <grid> <mesh>\n"
    "  gyo_voxel_map_editor verify  <grid> <mesh>\n"
    "  gyo_voxel_map_editor inspect <file>\n"
    "  gyo_voxel_map_editor import-boxes <json> <pointer> <grid>\n"
    "  gyo_voxel_map_editor edit <grid> <map-dir> [--gpu-driver <driver>]\n"
    "  gyo_voxel_map_editor measure-preview (--grid <grid> | --synthetic | --synthetic-map <name>) --csv <file>\n"
    "                       [--no-edit] [--frames <n>] [--draw-distance <cells>] [--gpu-driver <driver>]\n"
    "  gyo_voxel_map_editor build-map   <grid> <map-dir>\n"
    "  gyo_voxel_map_editor verify-map  <grid> <map-dir>\n"
    "  gyo_voxel_map_editor inspect-map <map-dir>\n"
    "  gyo_voxel_map_editor measure-map <out-dir> [<grid>...]\n"
    "  gyo_voxel_map_editor measure-map <out-dir> --scale km2-ground|km2-building\n"
    "  gyo_voxel_map_editor check-markers <markers> --marker-kinds <kinds>\n"
    "  gyo_voxel_map_editor --help\n"
    "\n"
    "build bakes a gyo.voxel_grid into a gyo.voxel_mesh that Engine accepts and\n"
    "writes it in place of <mesh>. verify builds again and compares the bytes\n"
    "with <mesh>. inspect summarizes a grid or a baked mesh. import-boxes makes a\n"
    "grid of cubes from the boxes at a JSON pointer (for example /walls), each\n"
    "{\"min\": [x, y, z], \"max\": [x, y, z]} in whole cells.\n"
    "edit opens the window (gui variant): a new grid when <grid> does not exist;\n"
    "every save writes <grid> and then the chunked map in <map-dir>. measure-preview\n"
    "times the window's preview, with an edit per frame or (--no-edit) drawing\n"
    "only (gui variant).\n"
    "build-map bakes a grid into Engine's chunked map: <map-dir>/map.json and its\n"
    "region files under <map-dir>/regions (region files of the old map that the\n"
    "new index does not list are removed). verify-map bakes again and compares\n"
    "map.json and every region file it lists. inspect-map summarizes a map.\n"
    "measure-map writes the declared measurement of the chunked bake (a synthetic\n"
    "building-like grid, a per-chunk CSV and a summary) into <out-dir>.\n"
    "check-markers reads a gyo.map_markers file and the product's gyo.marker_kinds\n"
    "list with Engine's parsers and lists every marker whose kind is not in it.\n"
    "Exit status: 0 done, 1 an input the formats refuse, 2 usage, 3 a file could\n"
    "not be read or written, 4 verify found a different mesh.\n";

constexpr std::size_t kMaxRead = 64U * 1024U * 1024U;

int Fail(const VoxelMapError& error) {
    std::cerr << "error: " << Engine::Base::Describe(error) << '\n';
    switch (error.code) {
    case VoxelMapErrorCode::ReadFailed:
    case VoxelMapErrorCode::WriteFailed: return 3;
    default: return 1;
    }
}

int Usage(const std::string_view message) {
    std::cerr << "error: " << message << "\n\n" << kHelp;
    return 2;
}

std::string_view Text(const std::vector<std::byte>& bytes) {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

// The document's own "schema" field; empty when the text has none.
std::string SchemaOf(const std::string_view text) {
    const auto document = nlohmann::json::parse(text.begin(), text.end(), nullptr, false);
    if (!document.is_object() || !document.contains("schema") || !document["schema"].is_string()) return {};
    return document["schema"].get<std::string>();
}

Result<BuildOutput> BuildFrom(const std::string& gridPath) {
    // Larger than the format allows, so ParseGrid is the one that refuses a too-large grid.
    auto bytes = ReadFile(gridPath, kMaxRead);
    if (!bytes) return Engine::Base::Err(std::move(bytes).error());
    auto grid = ParseGrid(Text(*bytes));
    if (!grid) return Engine::Base::Err(std::move(grid).error());
    return BuildMesh(*grid, *bytes);
}

int Build(const std::string& gridPath, const std::string& meshPath) {
    auto built = BuildFrom(gridPath);
    if (!built) return Fail(built.error());
    if (auto written = WriteFileReplacing(meshPath, built->text); !written) return Fail(written.error());
    std::cout << "built " << meshPath << ": " << built->stats.cells << " cells, " << built->stats.quads << " quads, "
              << built->stats.colliders << " colliders\n";
    return 0;
}

int Verify(const std::string& gridPath, const std::string& meshPath) {
    auto built = BuildFrom(gridPath);
    if (!built) return Fail(built.error());
    auto existing = ReadFile(meshPath, kMaxRead);
    if (!existing) return Fail(existing.error());
    if (Text(*existing) != built->text) {
        std::cerr << "different: " << meshPath << " is not what " << gridPath << " builds\n";
        return 4;
    }
    std::cout << "same: " << meshPath << '\n';
    return 0;
}

int Inspect(const std::string& path) {
    auto bytes = ReadFile(path, kMaxRead);
    if (!bytes) return Fail(bytes.error());
    const auto text = Text(*bytes);
    if (SchemaOf(text) == Engine::VoxelMesh::VoxelMeshSchema) {
        auto mesh = Engine::VoxelMesh::ParseVoxelMesh(text);
        if (!mesh) {
            std::cerr << "error: " << Engine::Base::Describe(mesh.error()) << '\n';
            return 1;
        }
        std::cout << "gyo.voxel_mesh: " << mesh->palette.size() << " palette entries, " << mesh->quads.size()
                  << " quads, " << mesh->colliders.size() << " colliders\n"
                  << "source_sha256: " << mesh->sourceSha256 << "\nbuild_version: " << mesh->buildVersion << '\n';
        return 0;
    }
    auto grid = ParseGrid(text);
    if (!grid) return Fail(grid.error());
    std::size_t cells = 0;
    for (const auto& [at, chunk] : grid->chunks)
        for (const auto& cell : chunk) cells += cell.block != 0 ? 1 : 0;
    std::cout << "gyo.voxel_grid: " << grid->shapes.size() << " shapes, " << grid->blocks.size() << " blocks, "
              << grid->chunks.size() << " chunks, " << cells << " cells\nsha256: " << Engine::Base::Sha256(*bytes)
              << '\n';
    return 0;
}

int ImportBoxesCommand(const std::string& jsonPath, const std::string& pointer, const std::string& gridPath) {
    auto bytes = ReadFile(jsonPath, kMaxRead);
    if (!bytes) return Fail(bytes.error());
    auto grid = ImportBoxes(Text(*bytes), pointer, {128, 128, 128, 255});
    if (!grid) return Fail(grid.error());
    if (auto written = WriteFileReplacing(gridPath, WriteGrid(*grid)); !written) return Fail(written.error());
    std::cout << "imported " << gridPath << '\n';
    return 0;
}

struct BakedFiles final {
    std::size_t chunks{};
    MapText text;
};

Result<BakedFiles> BakeFrom(const std::string& gridPath) {
    auto bytes = ReadFile(gridPath, kMaxRead);
    if (!bytes) return Engine::Base::Err(std::move(bytes).error());
    auto grid = ParseGrid(Text(*bytes));
    if (!grid) return Engine::Base::Err(std::move(grid).error());
    auto map = BakeMap(*grid);
    if (!map) return Engine::Base::Err(std::move(map).error());
    auto text = WriteMap(*map, *bytes);
    if (!text) return Engine::Base::Err(std::move(text).error());
    return BakedFiles{map->chunks.size(), std::move(text).value()};
}

int BuildMap(const std::string& gridPath, const std::string& directory) {
    auto baked = BakeFrom(gridPath);
    if (!baked) return Fail(baked.error());
    if (auto saved = SaveMap(directory, baked->text); !saved) return Fail(saved.error());
    std::cout << "built " << directory << ": " << baked->text.regions.size() << " regions, " << baked->chunks
              << " chunks\n";
    return 0;
}

int VerifyMap(const std::string& gridPath, const std::string& directory) {
    auto baked = BakeFrom(gridPath);
    if (!baked) return Fail(baked.error());
    const auto* text = &baked->text;
    const auto same = [](const std::string& path, const std::string& expected) -> Result<bool> {
        auto bytes = ReadFile(path, kMaxRead);
        if (!bytes) return Engine::Base::Err(std::move(bytes).error());
        return Text(*bytes) == expected;
    };
    auto index = same(directory + "/" + std::string(MapIndexFile), text->index);
    if (!index) return Fail(index.error());
    bool all = *index;
    for (const auto& region : text->regions) {
        if (!all) break;
        auto file = same(directory + "/" + region.file, region.text);
        if (!file) return Fail(file.error());
        all = *file;
    }
    if (!all) {
        std::cerr << "different: " << directory << " is not what " << gridPath << " builds\n";
        return 4;
    }
    std::cout << "same: " << directory << '\n';
    return 0;
}

int InspectMap(const std::string& directory) {
    auto bytes = ReadFile(directory + "/" + std::string(MapIndexFile), kMaxRead);
    if (!bytes) return Fail(bytes.error());
    auto index = Engine::VoxelMesh::ParseVoxelMap(Text(*bytes));
    if (!index) {
        std::cerr << "error: " << Engine::Base::Describe(index.error()) << '\n';
        return 1;
    }
    std::size_t chunks = 0, quads = 0, colliders = 0;
    for (const auto& entry : index->regions) {
        auto regionBytes = ReadFile(directory + "/" + entry.file, kMaxRead);
        if (!regionBytes) return Fail(regionBytes.error());
        auto region = Engine::VoxelMesh::ParseVoxelRegion(Text(*regionBytes), *index, entry.at);
        if (!region) {
            std::cerr << "error: " << entry.file << ": " << Engine::Base::Describe(region.error()) << '\n';
            return 1;
        }
        for (const auto& chunk : region->chunks) {
            ++chunks;
            quads += chunk.quads.size();
            colliders += chunk.colliders.size();
        }
    }
    std::cout << "gyo.voxel_map: " << index->palette.size() << " palette entries, " << index->regions.size()
              << " regions, " << chunks << " chunks, " << quads << " quads, " << colliders << " colliders\n"
              << "source_sha256: " << index->sourceSha256 << "\nbuild_version: " << index->buildVersion << '\n';
    return 0;
}

int CheckMarkers(const std::string& markersPath, const std::string& kindsPath) {
    auto markersBytes = ReadFile(markersPath, kMaxRead);
    if (!markersBytes) return Fail(markersBytes.error());
    auto kindsBytes = ReadFile(kindsPath, kMaxRead);
    if (!kindsBytes) return Fail(kindsBytes.error());
    auto markers = ReadMarkers(Text(*markersBytes));
    if (!markers) return Fail(markers.error());
    auto kinds = ReadMarkerKinds(Text(*kindsBytes));
    if (!kinds) return Fail(kinds.error());
    const auto unknown = Engine::MapMarkers::UnknownMarkerKinds(*markers, *kinds);
    if (!unknown.empty()) {
        for (const auto& path : unknown) std::cerr << "unknown kind: " << path << '\n';
        return Fail(VoxelMapError::Make(VoxelMapErrorCode::UnknownKinds,
                                        std::to_string(unknown.size()) + " markers have a kind not in " + kindsPath,
                                        unknown.front()));
    }
    std::cout << "markers: " << markers->markers.size() << " markers, every kind in " << kinds->size() << " kinds\n";
    return 0;
}

int Run(const std::span<const std::string> arguments) {
    if (arguments.size() < 2) return Usage("a command is required");
    const std::string& command = arguments[1];
    if (command == "--help" || command == "-h") {
        std::cout << kHelp;
        return 0;
    }
    if (command == "build" || command == "verify") {
        if (arguments.size() != 4) return Usage(command + " takes <grid> <mesh>");
        return command == "build" ? Build(arguments[2], arguments[3]) : Verify(arguments[2], arguments[3]);
    }
    if (command == "inspect") {
        if (arguments.size() != 3) return Usage("inspect takes <file>");
        return Inspect(arguments[2]);
    }
    if (command == "import-boxes") {
        if (arguments.size() != 5) return Usage("import-boxes takes <json> <pointer> <grid>");
        return ImportBoxesCommand(arguments[2], arguments[3], arguments[4]);
    }
    if (command == "edit") {
        if (arguments.size() != 4 && !(arguments.size() == 6 && arguments[4] == "--gpu-driver"))
            return Usage("edit takes <grid> <map-dir> [--gpu-driver <driver>]");
        WindowOptions window;
        window.gridPath = arguments[2];
        window.mapDirectory = arguments[3];
        if (arguments.size() == 6) window.gpuDriver = arguments[5];
        return RunEditorWindow(window);
    }
    if (command == "measure-preview") {
        MeasureOptions measure;
        for (std::size_t i = 2; i < arguments.size(); ++i) {
            const std::string& option = arguments[i];
            if (option == "--synthetic") {
                measure.synthetic = true;
                continue;
            }
            if (option == "--no-edit") {
                measure.edit = false;
                continue;
            }
            if (i + 1 >= arguments.size()) return Usage(option + " needs a value");
            const std::string& value = arguments[++i];
            if (option == "--grid") measure.gridPath = value;
            else if (option == "--synthetic-map") measure.syntheticMap = value;
            else if (option == "--csv") measure.csvPath = value;
            else if (option == "--gpu-driver") measure.gpuDriver = value;
            else if (option == "--frames") {
                if (value.empty() || value.size() > 6 || value.find_first_not_of("0123456789") != std::string::npos ||
                    std::stoul(value) == 0)
                    return Usage("--frames takes a whole number from 1 to 999999");
                measure.frames = static_cast<std::uint32_t>(std::stoul(value));
            } else if (option == "--draw-distance") {
                if (value.empty() || value.size() > 4 || value.find_first_not_of("0123456789") != std::string::npos ||
                    std::stoul(value) < static_cast<unsigned long>(MinDrawDistanceCells) ||
                    std::stoul(value) > static_cast<unsigned long>(MaxDrawDistanceCells))
                    return Usage("--draw-distance takes a whole number of cells from 64 to 1024");
                measure.drawDistanceCells = static_cast<float>(std::stoul(value));
            } else return Usage("unknown option " + option);
        }
        const int sources = (measure.synthetic ? 1 : 0) + (measure.gridPath.empty() ? 0 : 1) + (measure.syntheticMap.empty() ? 0 : 1);
        if (measure.csvPath.empty() || sources != 1)
            return Usage("measure-preview takes one of --grid <grid>, --synthetic and --synthetic-map <name>, and --csv <file>");
        return RunPreviewMeasure(measure);
    }
    if (command == "build-map" || command == "verify-map") {
        if (arguments.size() != 4) return Usage(command + " takes <grid> <map-dir>");
        return command == "build-map" ? BuildMap(arguments[2], arguments[3]) : VerifyMap(arguments[2], arguments[3]);
    }
    if (command == "inspect-map") {
        if (arguments.size() != 3) return Usage("inspect-map takes <map-dir>");
        return InspectMap(arguments[2]);
    }
    if (command == "check-markers") {
        if (arguments.size() != 5 || arguments[3] != "--marker-kinds")
            return Usage("check-markers takes <markers> --marker-kinds <kinds>");
        return CheckMarkers(arguments[2], arguments[4]);
    }
    if (command == "measure-map") {
        if (arguments.size() < 3) return Usage("measure-map takes <out-dir> [<grid>...]");
        if (arguments.size() >= 4 && arguments[3] == "--scale") {
            if (arguments.size() != 5) return Usage("measure-map takes <out-dir> --scale km2-ground|km2-building");
            return MeasureScale(arguments[2], arguments[4]);
        }
        return MeasureMap(arguments[2], std::vector<std::string>(arguments.begin() + 3, arguments.end()));
    }
    return Usage("unknown command " + command);
}

} // namespace

int main(int argc, char* argv[]) {
    return Gyo::Tools::Framework::RunHeadlessShell(argc, argv, Run, Usage);
}
