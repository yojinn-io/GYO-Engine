#include "gyo/voxel_map_editor/VoxelMap.hpp"

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
    "  gyo_voxel_map_editor --help\n"
    "\n"
    "build bakes a gyo.voxel_grid into a gyo.voxel_mesh that Engine accepts and\n"
    "writes it in place of <mesh>. verify builds again and compares the bytes\n"
    "with <mesh>. inspect summarizes a grid or a baked mesh. import-boxes makes a\n"
    "grid of cubes from the boxes at a JSON pointer (for example /walls), each\n"
    "{\"min\": [x, y, z], \"max\": [x, y, z]} in whole cells.\n"
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
    return Usage("unknown command " + command);
}

} // namespace

int main(int argc, char* argv[]) {
    return Gyo::Tools::Framework::RunHeadlessShell(argc, argv, Run, Usage);
}
