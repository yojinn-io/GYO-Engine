// Reads the bundled level data under assets/object_connect (the original tests
// loaded it from disk through PuzzleCatalogLoader::Load, which the port removed:
// runtime code reads files through the asset Module). Here the test reads the
// files itself and hands their text to Parse, so the assertions are the
// original ones on the same data.
#include "TestSupport.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace object_connect::tests {
namespace {

[[nodiscard]] std::filesystem::path Root() {
    return std::filesystem::path{OBJECT_CONNECT_TEST_RESOURCE_ROOT};
}

[[nodiscard]] bool ReadText(const std::filesystem::path& path, std::string& text, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open " + path.generic_string();
        return false;
    }
    text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

} // namespace

bool LoadBundledCatalog(PuzzleCatalog& catalog, std::string& error) {
    std::string levels;
    std::string nodes;
    if (!ReadText(Root() / "data/levels.csv", levels, error) || !ReadText(Root() / "data/nodes.csv", nodes, error)) {
        return false;
    }
    std::vector<std::string> paths;
    for (const auto& entry : std::filesystem::directory_iterator(Root() / "data/maps")) {
        if (entry.is_regular_file() && entry.path().extension() == ".csv") {
            paths.push_back("data/maps/" + entry.path().filename().generic_string());
        }
    }
    std::vector<std::string> contents(paths.size());
    std::vector<PuzzleMapCsvSource> maps;
    for (std::size_t index = 0; index < paths.size(); ++index) {
        if (!ReadText(Root() / paths[index], contents[index], error)) {
            return false;
        }
    }
    for (std::size_t index = 0; index < paths.size(); ++index) {
        maps.push_back({paths[index], contents[index]});
    }
    return PuzzleCatalogLoader::Parse({levels, nodes, maps}, catalog, error);
}

bool LoadBundledNodePresets(NodePresetCatalog& catalog, std::string& error) {
    std::string nodes;
    if (!ReadText(Root() / "data/nodes.csv", nodes, error)) {
        return false;
    }
    return NodePresetCatalogLoader::Parse(nodes, catalog, error);
}

} // namespace object_connect::tests
