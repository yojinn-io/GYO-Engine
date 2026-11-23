#include "object_connect/runtime/GameData.hpp"

#include "object_connect/data/Csv.hpp"

#include "engine/audio/WavLoader.hpp"
#include "engine/image/PngLoader.hpp"
#include "engine/text/FontLoader.hpp"

#include <algorithm>
#include <utility>

namespace object_connect::runtime {
namespace {

namespace A = GYO::Engine::Asset;
namespace B = GYO::Engine::Base;

using Failure = A::LoaderFailure<GameDataCode>;

// The non-empty values of one column, each once, in the order they first appear.
[[nodiscard]] B::Result<std::vector<std::string>, Failure> ColumnValues(const std::string& csv,
                                                                      const std::string_view column,
                                                                      const A::LoadContext& context) {
    const data::CsvParseResult parsed = data::Csv::Parse(csv);
    if (!parsed) {
        return B::Err(Failure::Make(GameDataCode::BadCsv, "the table is not valid CSV",
                                    context.Id().Text() + " (" + context.Path() + "): " + parsed.error));
    }
    const data::CsvDocument& document = *parsed.document;
    const auto found = std::find(document.header.begin(), document.header.end(), column);
    if (found == document.header.end()) {
        return B::Err(Failure::Make(GameDataCode::MissingColumn, "the table has no " + std::string(column) + " column",
                                    context.Id().Text() + " (" + context.Path() + ")"));
    }
    const auto index = static_cast<std::size_t>(found - document.header.begin());
    std::vector<std::string> values;
    for (const data::CsvRecord& record : document.records) {
        if (index >= record.fields.size() || record.fields[index].empty()) {
            continue;
        }
        if (std::find(values.begin(), values.end(), record.fields[index]) == values.end()) {
            values.push_back(record.fields[index]);
        }
    }
    return values;
}

[[nodiscard]] std::string ToString(const std::span<const std::byte> bytes) {
    return bytes.empty() ? std::string() : std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

} // namespace

std::string ReferenceFromTable(const std::string_view tablePath, const std::string_view contentPath) {
    std::string reference;
    for (const char c : tablePath) {
        if (c == '/') {
            reference += "../";
        }
    }
    reference += contentPath;
    return reference;
}

B::Result<GameDataSettings, Failure> LevelTableLoader::ParseSettings(const std::optional<std::string_view>) const {
    return GameDataSettings{};
}

B::Result<LevelTable, Failure> LevelTableLoader::Load(const std::span<const std::byte> bytes, const GameDataSettings&,
                                                     A::LoadContext& context) const {
    LevelTable table;
    table.csv = ToString(bytes);
    auto paths = ColumnValues(table.csv, "map_path", context);
    if (!paths) {
        return B::Err(std::move(paths).error());
    }
    for (std::string& path : *paths) {
        A::AssetRef<A::Text> asset = context.ReferencePath<A::Text>(ReferenceFromTable(context.Path(), path));
        table.maps.push_back({std::move(path), std::move(asset)});
    }
    return table;
}

B::Result<GameDataSettings, Failure> NodeTableLoader::ParseSettings(const std::optional<std::string_view>) const {
    return GameDataSettings{};
}

B::Result<NodeTable, Failure> NodeTableLoader::Load(const std::span<const std::byte> bytes, const GameDataSettings&,
                                                   A::LoadContext& context) const {
    NodeTable table;
    table.csv = ToString(bytes);
    auto paths = ColumnValues(table.csv, "texture_path", context);
    if (!paths) {
        return B::Err(std::move(paths).error());
    }
    for (std::string& path : *paths) {
        A::AssetRef<GYO::Engine::Image::ImageData> asset =
            context.ReferencePath<GYO::Engine::Image::ImageData>(ReferenceFromTable(context.Path(), path));
        table.textures.push_back({std::move(path), std::move(asset)});
    }
    return table;
}

std::optional<std::string> RegisterLoaders(const A::AssetLoaderRegistry& registry) {
    const auto failed = [](const char* name, const A::LoaderRegistrationResult& result) -> std::optional<std::string> {
        if (result) {
            return std::nullopt;
        }
        return std::string("the ") + name + " loader was refused: " + GYO::Engine::Base::Describe(result.error());
    };
    if (auto f = failed("level table", registry.Register<LevelTableLoader>())) return f;
    if (auto f = failed("node table", registry.Register<NodeTableLoader>())) return f;
    if (auto f = failed("png", registry.Register<GYO::Engine::Image::PngLoader>())) return f;
    if (auto f = failed("font", registry.Register<GYO::Engine::Text::FontLoader>())) return f;
    // sdl_audio registers it too when carried; the same loader again changes nothing.
    if (auto f = failed("wav", registry.Register<GYO::Engine::Audio::WavLoader>())) return f;
    return std::nullopt;
}

} // namespace object_connect::runtime
