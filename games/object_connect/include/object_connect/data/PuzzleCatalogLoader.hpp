#pragma once

#include "object_connect/data/PuzzleData.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace object_connect {

struct PuzzleMapCsvSource final {
    std::string_view path;
    std::string_view contents;
};

struct PuzzleCsvSources final {
    std::string_view levels;
    std::string_view nodes;
    std::span<const PuzzleMapCsvSource> maps;
};

class PuzzleCatalogLoader final {
public:
    [[nodiscard]] static bool Parse(const PuzzleCsvSources& sources,
                                    PuzzleCatalog& catalog,
                                    std::string& error,
                                    std::vector<std::string>* warnings = nullptr);
};

class NodePresetCatalogLoader final {
public:
    [[nodiscard]] static bool Parse(std::string_view nodesCsv,
                                    NodePresetCatalog& catalog,
                                    std::string& error);
};

} // namespace object_connect
