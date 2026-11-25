#include "voxel_map_editor/ChunkedMap.hpp"
#include "voxel_map_editor/GoxImport.hpp"
#include "voxel_map_editor/Grid.hpp"
#include "voxel_map_editor/Synthetic.hpp"

#include "engine/base/Sha256.hpp"
#include "engine/json/Document.hpp"
#include "engine/os/Console.hpp"

#include <charconv>
#include <chrono>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace voxel_map_editor;
namespace Base = GYO::Engine::Base;

constexpr std::string_view kHelp =
    "Usage:\n"
    "  gyo_voxel_map_editor build-map   <grid> <map-dir>\n"
    "  gyo_voxel_map_editor verify-map  <grid> <map-dir>\n"
    "  gyo_voxel_map_editor inspect     <grid>\n"
    "  gyo_voxel_map_editor inspect-map <map-dir>\n"
    "  gyo_voxel_map_editor import-boxes <json> <pointer> <grid>\n"
    "  gyo_voxel_map_editor import-gox <file.gox> <grid> [--exclude-layer <pattern>]...\n"
    "                       [--block <RRGGBB>=<shape>]... [--cell-size <metres>]\n"
    "  gyo_voxel_map_editor synthesize building|km2-ground|km2-building <map-dir> [--size <cells>] [--grid <grid>]\n"
    "  gyo_voxel_map_editor --help\n"
    "\n"
    "build-map bakes a gyo.voxel_grid into the chunked map: <map-dir>/map.json and its\n"
    "region files under <map-dir>/regions (region files of the old map that the new\n"
    "index does not list are removed). Every text is read back with the voxel_map\n"
    "Module first. verify-map bakes again and compares map.json and every region\n"
    "file it lists. inspect summarizes a grid; inspect-map reads a map with the\n"
    "voxel_map Module only and summarizes it.\n"
    "import-boxes makes a grid of cubes from the boxes at a JSON pointer (for\n"
    "example /walls), each {\"min\": [x, y, z], \"max\": [x, y, z]} in whole cells.\n"
    "import-gox makes a grid of a Goxel file: the visible layers with blocks of their\n"
    "own, merged in file order, minus the layers whose name matches an\n"
    "--exclude-layer pattern ('*' any run, '?' one character). Goxel's (x, y, z) is\n"
    "the cell (x, z, y). One block per colour, named c_rrggbb, a cube unless --block\n"
    "gives the colour a shape (cube, slab, stair). --cell-size is 0.5 by default.\n"
    "synthesize makes a synthetic grid in memory and bakes it into <map-dir>: building\n"
    "(256 x 48 x 256 cells), km2-ground and km2-building (--size x --size cells, 2000\n"
    "by default, a multiple of 16). --grid also writes the grid (source_sha256 is of\n"
    "that text). A km2 map takes gigabytes of memory and tens of seconds.\n"
    "Exit status: 0 done, 1 an input the formats refuse, 2 usage, 3 a file could\n"
    "not be read or written, 4 verify-map found a different map.\n";

// Larger than the formats allow, so their parsers are the ones that refuse a too-large file.
constexpr std::size_t kMaxRead = 64U * 1024U * 1024U;

int Fail(const Error& error) {
    std::cerr << "error: " << Base::Describe(error) << '\n';
    switch (error.code) {
    case ErrorCode::ReadFailed:
    case ErrorCode::WriteFailed: return 3;
    default: return 1;
    }
}

int Usage(const std::string_view message) {
    std::cerr << "error: " << message << "\n\n" << kHelp;
    return 2;
}

double MillisecondsSince(const std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

struct BakedFiles final {
    std::size_t chunks{};
    std::size_t quads{};
    std::size_t colliders{};
    std::size_t bytes{};
    MapText text;
};

Result<BakedFiles> Bake(const GridDocument& grid, const std::span<const std::byte> sourceBytes) {
    auto map = BakeMap(grid);
    if (!map) return Base::Err(std::move(map).error());
    auto text = WriteMap(*map, sourceBytes);
    if (!text) return Base::Err(std::move(text).error());
    BakedFiles files{map->chunks.size(), 0, 0, text->index.size(), std::move(text).value()};
    for (const auto& [at, chunk] : map->chunks) {
        files.quads += chunk.quads.size();
        files.colliders += chunk.colliders.size();
    }
    for (const auto& region : files.text.regions) files.bytes += region.text.size();
    return files;
}

Result<BakedFiles> BakeFrom(const std::string& gridPath) {
    auto bytes = ReadFile(gridPath, kMaxRead);
    if (!bytes) return Base::Err(std::move(bytes).error());
    auto grid = ParseGrid(AsText(*bytes));
    if (!grid) return Base::Err(std::move(grid).error());
    return Bake(*grid, *bytes);
}

void PrintBaked(const std::string& directory, const BakedFiles& baked) {
    std::cout << "built " << directory << ": " << baked.text.regions.size() << " regions, " << baked.chunks
              << " chunks, " << baked.quads << " quads, " << baked.colliders << " colliders, " << baked.bytes
              << " bytes\n";
}

int BuildMap(const std::string& gridPath, const std::string& directory) {
    auto baked = BakeFrom(gridPath);
    if (!baked) return Fail(baked.error());
    if (auto saved = SaveMap(directory, baked->text); !saved) return Fail(saved.error());
    PrintBaked(directory, *baked);
    return 0;
}

int VerifyMap(const std::string& gridPath, const std::string& directory) {
    auto baked = BakeFrom(gridPath);
    if (!baked) return Fail(baked.error());
    const auto same = [](const std::string& path, const std::string& expected) -> Result<bool> {
        auto bytes = ReadFile(path, kMaxRead);
        if (!bytes) return Base::Err(std::move(bytes).error());
        return AsText(*bytes) == expected;
    };
    auto index = same(directory + "/" + std::string(kMapIndexFile), baked->text.index);
    if (!index) return Fail(index.error());
    bool all = *index;
    for (const auto& region : baked->text.regions) {
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

int Inspect(const std::string& path) {
    auto bytes = ReadFile(path, kMaxRead);
    if (!bytes) return Fail(bytes.error());
    auto grid = ParseGrid(AsText(*bytes));
    if (!grid) return Fail(grid.error());
    std::size_t cells = 0;
    for (const auto& [at, chunk] : grid->chunks)
        for (const auto& cell : chunk) cells += cell.block != 0 ? 1 : 0;
    std::cout << "gyo.voxel_grid: " << grid->shapes.size() << " shapes, " << grid->blocks.size() << " blocks, "
              << grid->chunks.size() << " chunks, " << cells << " cells\nsha256: " << Base::Sha256(*bytes) << '\n';
    return 0;
}

int InspectMap(const std::string& directory) {
    auto summary = ReadMapSummary(directory);
    if (!summary) return Fail(summary.error());
    std::cout << "gyo.voxel_map: " << summary->index.palette.size() << " palette entries, "
              << summary->index.regions.size() << " regions, " << summary->chunks << " chunks, " << summary->quads
              << " quads, " << summary->colliders << " colliders, " << summary->bytes << " bytes\n"
              << "source_sha256: " << summary->index.sourceSha256 << "\nbuild_version: " << summary->index.buildVersion
              << '\n';
    return 0;
}

int ImportBoxesCommand(const std::string& jsonPath, const std::string& pointer, const std::string& gridPath) {
    auto bytes = ReadFile(jsonPath, kMaxRead);
    if (!bytes) return Fail(bytes.error());
    auto grid = ImportBoxes(AsText(*bytes), pointer, {128, 128, 128, 255});
    if (!grid) return Fail(grid.error());
    if (auto written = WriteFileReplacing(gridPath, WriteGrid(*grid)); !written) return Fail(written.error());
    std::cout << "imported " << gridPath << '\n';
    return 0;
}

// A positive number, the whole text in JSON's number syntax (read by the json Module, the same on every platform
// and in every locale).
bool ParseNumber(const std::string& text, double& out) {
    const auto document = GYO::Engine::Json::Parse(text);
    if (!document || document->Root().GetKind() != GYO::Engine::Json::Kind::Number) return false;
    const auto number = document->Root().AsDouble();
    if (!number || !(*number > 0)) return false;
    out = *number;
    return true;
}

bool ParseWhole(const std::string& text, std::int32_t& out) {
    const auto* end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, out);
    return error == std::errc{} && stop == end && !text.empty() && text.front() != '-';
}

int ImportGoxCommand(const std::span<const std::string> arguments) {
    if (arguments.size() < 4) return Usage("import-gox takes <file.gox> <grid> [options]");
    GoxImportOptions options;
    for (std::size_t i = 4; i < arguments.size(); ++i) {
        const std::string& option = arguments[i];
        if (i + 1 >= arguments.size()) return Usage(option + " needs a value");
        const std::string& value = arguments[++i];
        if (option == "--exclude-layer") {
            options.excludeLayers.push_back(value);
        } else if (option == "--block") {
            const auto equals = value.find('=');
            const auto colour = ParseRgb(std::string_view(value).substr(0, equals));
            if (equals == std::string::npos || !colour) return Usage("--block takes <RRGGBB>=<shape>");
            if (!options.blockShapes.emplace(*colour, value.substr(equals + 1)).second)
                return Usage("--block names the colour " + value.substr(0, equals) + " twice");
        } else if (option == "--cell-size") {
            double size = 0;
            if (!ParseNumber(value, size)) return Usage("--cell-size takes a positive number of metres");
            options.cellSize = static_cast<float>(size);
            if (!(options.cellSize >= kMinCellSize && options.cellSize <= kMaxCellSize))
                return Usage("--cell-size is within [2^-16, 2^16]");
        } else {
            return Usage("unknown option " + option);
        }
    }
    const auto start = std::chrono::steady_clock::now();
    auto bytes = ReadFile(arguments[2], kMaxRead);
    if (!bytes) return Fail(bytes.error());
    auto imported = ImportGox(*bytes, options);
    if (!imported) return Fail(imported.error());
    const auto text = WriteGrid(imported->grid);
    if (auto written = WriteFileReplacing(arguments[3], text); !written) return Fail(written.error());
    for (const auto& layer : imported->layers)
        std::cout << "layer \"" << layer.name << "\": " << ToString(layer.use) << ", " << layer.voxels << " voxels\n";
    for (const auto& colour : imported->unusedShapeColours)
        std::cout << "note: no geometry voxel has the colour of --block " << ColourBlockName(colour).substr(2) << '\n';
    std::cout << "imported " << arguments[3] << ": " << imported->grid.blocks.size() << " blocks, "
              << imported->grid.chunks.size() << " chunks, " << imported->cells << " cells, " << text.size()
              << " bytes, " << static_cast<long long>(MillisecondsSince(start)) << " ms\n";
    return 0;
}

int Synthesize(const std::span<const std::string> arguments) {
    if (arguments.size() < 4) return Usage("synthesize takes <name> <map-dir> [--size <cells>] [--grid <grid>]");
    std::int32_t size = kKm2Cells;
    std::string gridPath;
    for (std::size_t i = 4; i < arguments.size(); ++i) {
        const std::string& option = arguments[i];
        if (i + 1 >= arguments.size()) return Usage(option + " needs a value");
        const std::string& value = arguments[++i];
        if (option == "--size") {
            if (!ParseWhole(value, size)) return Usage("--size takes a whole number of cells");
        } else if (option == "--grid") {
            gridPath = value;
        } else {
            return Usage("unknown option " + option);
        }
    }
    const auto start = std::chrono::steady_clock::now();
    auto grid = SyntheticByName(arguments[2], size);
    if (!grid)
        return Usage("synthesize takes building, km2-ground or km2-building, and a --size of whole chunks from 16 to " +
                     std::to_string(kMaxSyntheticCells) + " cells");
    const auto made = MillisecondsSince(start);
    const auto text = WriteGrid(*grid);
    if (!gridPath.empty())
        if (auto written = WriteFileReplacing(gridPath, text); !written) return Fail(written.error());
    const auto bakeStart = std::chrono::steady_clock::now();
    auto baked = Bake(*grid, AsBytes(text));
    if (!baked) return Fail(baked.error());
    const auto bake = MillisecondsSince(bakeStart);
    if (auto saved = SaveMap(arguments[3], baked->text); !saved) return Fail(saved.error());
    PrintBaked(arguments[3], *baked);
    std::cout << "grid text: " << text.size() << " bytes; made in " << static_cast<long long>(made)
              << " ms, baked and read back in " << static_cast<long long>(bake) << " ms\n";
    return 0;
}

int Run(const std::span<const std::string> arguments) {
    if (arguments.size() < 2) return Usage("a command is required");
    const std::string& command = arguments[1];
    if (command == "--help" || command == "-h") {
        std::cout << kHelp;
        return 0;
    }
    if (command == "build-map" || command == "verify-map") {
        if (arguments.size() != 4) return Usage(command + " takes <grid> <map-dir>");
        return command == "build-map" ? BuildMap(arguments[2], arguments[3]) : VerifyMap(arguments[2], arguments[3]);
    }
    if (command == "inspect") {
        if (arguments.size() != 3) return Usage("inspect takes <grid>");
        return Inspect(arguments[2]);
    }
    if (command == "inspect-map") {
        if (arguments.size() != 3) return Usage("inspect-map takes <map-dir>");
        return InspectMap(arguments[2]);
    }
    if (command == "import-boxes") {
        if (arguments.size() != 5) return Usage("import-boxes takes <json> <pointer> <grid>");
        return ImportBoxesCommand(arguments[2], arguments[3], arguments[4]);
    }
    if (command == "import-gox") return ImportGoxCommand(arguments);
    if (command == "synthesize") return Synthesize(arguments);
    return Usage("unknown command " + command);
}

} // namespace

int main(int argc, char* argv[]) {
    namespace Console = GYO::Engine::OS::Console;
    const auto output = Console::Utf8ConsoleOutput::Enable();
    auto arguments = Console::CommandLineUtf8(argc, argv);
    if (!arguments) {
        std::cerr << "error: " << Base::Describe(arguments.error()) << '\n';
        return 2;
    }
    return Run(*arguments);
}
