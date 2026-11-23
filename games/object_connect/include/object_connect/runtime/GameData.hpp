#pragma once

#include "engine/asset/AssetLoader.hpp"
#include "engine/asset/AssetRef.hpp"
#include "engine/asset/AssetStore.hpp"
#include "engine/asset/AssetTypeName.hpp"
#include "engine/asset/LoadContext.hpp"
#include "engine/asset/Text.hpp"
#include "engine/base/Result.hpp"
#include "engine/image/ImageData.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The game's two table files as assets. The original read its catalog from
// disk (PuzzleCatalogLoader::Load, removed in the port); here each table is a
// catalog entry with a loader of the game's own, and the files a table names
// are the loader's declared references (asset declaration 5.17), so a table is
// Complete only with every file it names. The tables name files by their path
// from the content root (the original's resourceRoot); a loader turns that into
// a reference relative to the table's own file.
//
// levels.csv -> LevelTable: the text, and its map_path column as text assets.
// nodes.csv  -> NodeTable:  the text, and its texture_path column as images.

namespace object_connect::runtime {

template <class T>
struct NamedFile final {
    // As written in the table (the content-root path the catalog parser expects).
    std::string path;
    GYO::Engine::Asset::AssetRef<T> asset;
};

struct LevelTable final {
    std::string csv;
    std::vector<NamedFile<GYO::Engine::Asset::Text>> maps;
};

struct NodeTable final {
    std::string csv;
    std::vector<NamedFile<GYO::Engine::Image::ImageData>> textures;
};

enum class GameDataCode : std::uint8_t {
    // The table is not valid CSV.
    BadCsv = 1,
    // The table has no column naming its files.
    MissingColumn,
};

[[nodiscard]] constexpr const char* ToString(const GameDataCode code) noexcept {
    switch (code) {
    case GameDataCode::BadCsv: return "BadCsv";
    case GameDataCode::MissingColumn: return "MissingColumn";
    }
    return "Unknown";
}

struct GameDataSettings final {
    [[nodiscard]] friend bool operator==(const GameDataSettings&, const GameDataSettings&) = default;
};

class LevelTableLoader final {
public:
    using Output = LevelTable;
    using Settings = GameDataSettings;
    using Code = GameDataCode;

    static constexpr std::string_view kTypeName = "object_connect.levels";
    static constexpr std::array<std::string_view, 1> kFormats{"csv"};
    static constexpr std::size_t kDefaultMaxBytes = std::size_t{4} * 1024u * 1024u;

    [[nodiscard]] GYO::Engine::Base::Result<Settings, GYO::Engine::Asset::LoaderFailure<Code>> ParseSettings(
        std::optional<std::string_view> settingsJson) const;
    [[nodiscard]] GYO::Engine::Base::Result<LevelTable, GYO::Engine::Asset::LoaderFailure<Code>> Load(
        std::span<const std::byte> bytes, const Settings& settings, GYO::Engine::Asset::LoadContext& context) const;
};

class NodeTableLoader final {
public:
    using Output = NodeTable;
    using Settings = GameDataSettings;
    using Code = GameDataCode;

    static constexpr std::string_view kTypeName = "object_connect.nodes";
    static constexpr std::array<std::string_view, 1> kFormats{"csv"};
    static constexpr std::size_t kDefaultMaxBytes = std::size_t{1} * 1024u * 1024u;

    [[nodiscard]] GYO::Engine::Base::Result<Settings, GYO::Engine::Asset::LoaderFailure<Code>> ParseSettings(
        std::optional<std::string_view> settingsJson) const;
    [[nodiscard]] GYO::Engine::Base::Result<NodeTable, GYO::Engine::Asset::LoaderFailure<Code>> Load(
        std::span<const std::byte> bytes, const Settings& settings, GYO::Engine::Asset::LoadContext& context) const;
};

// The reference a table at tablePath writes for a file at contentPath (both
// from the content root): "../" for each directory of the table, then the path.
[[nodiscard]] std::string ReferenceFromTable(std::string_view tablePath, std::string_view contentPath);

// Registers the game's loaders and the Engine loaders the game's content
// needs (image's png, text's font, audio's wav) with the Product's store. Call from the
// product's Configure (ProductDescription::configure).
[[nodiscard]] std::optional<std::string> RegisterLoaders(const GYO::Engine::Asset::AssetLoaderRegistry& registry);

} // namespace object_connect::runtime

template <>
struct GYO::Engine::Asset::AssetTypeName<object_connect::runtime::LevelTable> {
    static constexpr std::string_view value = "object_connect.levels";
};

template <>
struct GYO::Engine::Asset::AssetTypeName<object_connect::runtime::NodeTable> {
    static constexpr std::string_view value = "object_connect.nodes";
};

static_assert(GYO::Engine::Asset::AssetLoader<object_connect::runtime::LevelTableLoader>);
static_assert(GYO::Engine::Asset::AssetLoader<object_connect::runtime::NodeTableLoader>);
