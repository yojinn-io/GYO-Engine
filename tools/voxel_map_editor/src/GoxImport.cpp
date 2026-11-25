#include "voxel_map_editor/GoxImport.hpp"

#include "engine/asset/LoadContext.hpp"
#include "engine/image/PngLoader.hpp"

#include <algorithm>
#include <cstring>
#include <set>
#include <utility>

namespace voxel_map_editor {
namespace {

namespace Base = GYO::Engine::Base;
namespace Image = GYO::Engine::Image;

constexpr std::int32_t kGoxVersion = 2;
constexpr std::int32_t kBlockEdge = 16;          // a BL16 block is 16^3 voxels
constexpr std::uint32_t kBlockImageEdge = 64;     // stored as a 64 x 64 image
constexpr std::uint8_t kSolidAlpha = 127;         // alpha below this is empty
constexpr std::size_t kLayerBlockBytes = 20;      // index, x, y, z, 0
constexpr std::size_t kMaxColours = 256;          // a palette's entries

struct Problem final {
    std::string message;
    std::string detail;
};
using Check = std::optional<Problem>;
Check Broken(std::string message, std::string detail = {}) { return Problem{std::move(message), std::move(detail)}; }

// Little-endian reads inside one chunk's data or the file.
class Reader final {
public:
    explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}
    [[nodiscard]] std::size_t Position() const noexcept { return position_; }
    [[nodiscard]] std::size_t Left() const noexcept { return bytes_.size() - position_; }
    [[nodiscard]] bool Int32(std::int32_t& out) {
        if (Left() < 4) return false;
        std::uint32_t value = 0;
        for (std::size_t i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[position_ + i])) << (8 * i);
        out = static_cast<std::int32_t>(value);
        position_ += 4;
        return true;
    }
    [[nodiscard]] bool Bytes(const std::size_t count, std::span<const std::byte>& out) {
        if (Left() < count) return false;
        out = bytes_.subspan(position_, count);
        position_ += count;
        return true;
    }

private:
    std::span<const std::byte> bytes_;
    std::size_t position_ = 0;
};

std::string Text(const std::span<const std::byte> bytes) {
    // A DICT string may end with NUL padding.
    const auto text = AsText(bytes);
    return std::string(text.substr(0, text.find('\0')));
}

bool AnyNonZero(const std::span<const std::byte> bytes) {
    return std::any_of(bytes.begin(), bytes.end(), [](const std::byte b) { return b != std::byte{0}; });
}

struct LayerBlock final {
    std::int32_t index{};
    std::array<std::int32_t, 3> at{}; // Goxel's x, y, z
};

struct Layer final {
    std::vector<LayerBlock> blocks;
    std::string name;
    bool visible = true;
    bool ownBlocks = true; // false for a clone (base_id) or a procedural shape
};

Check ReadLayer(const std::span<const std::byte> data, const std::size_t blockCount, const std::size_t layerIndex,
                Layer& layer) {
    const auto where = "layer " + std::to_string(layerIndex);
    Reader reader(data);
    std::int32_t count = 0;
    if (!reader.Int32(count) || count < 0 || static_cast<std::size_t>(count) > reader.Left() / kLayerBlockBytes)
        return Broken("a layer's block list does not fit in its chunk", where);
    for (std::int32_t i = 0; i < count; ++i) {
        LayerBlock block;
        std::int32_t zero = 0;
        if (!reader.Int32(block.index) || !reader.Int32(block.at[0]) || !reader.Int32(block.at[1]) ||
            !reader.Int32(block.at[2]) || !reader.Int32(zero))
            return Broken("a layer's block list does not fit in its chunk", where);
        if (block.index < 0 || static_cast<std::size_t>(block.index) >= blockCount)
            return Broken("a layer names a block that no BL16 before it holds", where + ", block " + std::to_string(i));
        layer.blocks.push_back(block);
    }
    // The DICT: key size (0 ends it), key, value size, value. The end of the data ends it too.
    while (reader.Left() > 0) {
        std::int32_t keySize = 0, valueSize = 0;
        std::span<const std::byte> key, value;
        if (!reader.Int32(keySize) || keySize < 0) return Broken("a layer's dictionary does not fit in its chunk", where);
        if (keySize == 0) break;
        if (!reader.Bytes(static_cast<std::size_t>(keySize), key) || !reader.Int32(valueSize) || valueSize < 0 ||
            !reader.Bytes(static_cast<std::size_t>(valueSize), value))
            return Broken("a layer's dictionary does not fit in its chunk", where);
        const auto name = AsText(key);
        if (name == "name") layer.name = Text(value);
        else if (name == "visible") layer.visible = value.empty() || value[0] != std::byte{0};
        else if ((name == "base_id" || name == "shape") && AnyNonZero(value)) layer.ownBlocks = false;
    }
    return std::nullopt;
}

Check DecodeBlock(const std::span<const std::byte> data, const std::size_t blockIndex, Image::ImageData& out) {
    const Image::PngLoader loader;
    GYO::Engine::Asset::LoadContext context(GYO::Engine::Asset::AssetId("gox.block"),
                                            "BL16 " + std::to_string(blockIndex), Image::PngLoader::kDefaultMaxBytes);
    auto image = loader.Load(data, Image::PngSettings{}, context);
    if (!image)
        return Broken("a BL16 block is not a PNG the image Module decodes: " + image.error().message,
                      "block " + std::to_string(blockIndex));
    if (image->Width() != kBlockImageEdge || image->Height() != kBlockImageEdge)
        return Broken("a BL16 block is not a 64 x 64 image", "block " + std::to_string(blockIndex));
    out = std::move(image).value();
    return std::nullopt;
}

// The voxels so far, by gyo chunk: 0 is empty, otherwise 1 + 0xRRGGBB.
using ColourChunk = std::array<std::uint32_t, kChunkCells>;

std::int32_t FloorDiv(const std::int32_t value, const std::int32_t divisor) {
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

Check Paint(const Layer& layer, const std::vector<Image::ImageData>& blocks, std::map<Vec3i, ColourChunk>& voxels,
            GoxLayerReport& report) {
    for (const auto& block : layer.blocks) {
        const auto pixels = blocks[static_cast<std::size_t>(block.index)].Pixels();
        for (std::int32_t i = 0; i < kBlockEdge * kBlockEdge * kBlockEdge; ++i) {
            const auto* rgba = pixels.data() + static_cast<std::size_t>(i) * 4;
            if (rgba[3] < kSolidAlpha) continue;
            const std::array<std::int64_t, 3> goxel{std::int64_t{block.at[0]} + i % kBlockEdge,
                                                    std::int64_t{block.at[1]} + (i / kBlockEdge) % kBlockEdge,
                                                    std::int64_t{block.at[2]} + i / (kBlockEdge * kBlockEdge)};
            // gyo (x, y, z) = Goxel (x, z, y).
            const std::array<std::int64_t, 3> cell{goxel[0], goxel[2], goxel[1]};
            for (const auto c : cell)
                if (c < -kCellLimit || c > kCellLimit - 1)
                    return Broken("a voxel lies beyond the grid's [-2^16, 2^16 - 1] cells",
                                  "layer '" + layer.name + "' at " + std::to_string(goxel[0]) + ", " +
                                      std::to_string(goxel[1]) + ", " + std::to_string(goxel[2]));
            const Vec3i at{static_cast<std::int32_t>(cell[0]), static_cast<std::int32_t>(cell[1]),
                           static_cast<std::int32_t>(cell[2])};
            const Vec3i key{FloorDiv(at[0], kChunkSize), FloorDiv(at[1], kChunkSize), FloorDiv(at[2], kChunkSize)};
            auto found = voxels.find(key);
            if (found == voxels.end()) found = voxels.emplace(key, ColourChunk{}).first;
            const auto colour = (std::uint32_t{rgba[0]} << 16) | (std::uint32_t{rgba[1]} << 8) | rgba[2];
            auto& slot = found->second[CellIndex(at[0] - key[0] * kChunkSize, at[1] - key[1] * kChunkSize,
                                                 at[2] - key[2] * kChunkSize)];
            // A later layer's voxel is over an earlier one's.
            slot = colour + 1;
            ++report.voxels;
        }
    }
    return std::nullopt;
}

Rgb RgbOf(const std::uint32_t colour) {
    return {static_cast<std::uint8_t>(colour >> 16), static_cast<std::uint8_t>(colour >> 8),
            static_cast<std::uint8_t>(colour)};
}

Check Import(const std::span<const std::byte> bytes, const GoxImportOptions& options, GoxImport& result) {
    Reader file(bytes);
    std::span<const std::byte> magic;
    std::int32_t version = 0;
    if (!file.Bytes(4, magic) || AsText(magic) != "GOX ") return Broken("not a .gox file (no \"GOX \" magic)");
    if (!file.Int32(version) || version != kGoxVersion)
        return Broken("a .gox of version " + std::to_string(kGoxVersion) + " is expected", "version " + std::to_string(version));

    std::vector<Image::ImageData> blocks;
    std::vector<Layer> layers;
    while (file.Left() > 0) {
        std::span<const std::byte> type, data, crc;
        std::int32_t length = 0;
        const auto at = "offset " + std::to_string(file.Position());
        if (!file.Bytes(4, type) || !file.Int32(length) || length < 0 ||
            !file.Bytes(static_cast<std::size_t>(length), data) || !file.Bytes(4, crc))
            return Broken("a chunk does not fit in the file", at);
        const auto kind = AsText(type);
        if (kind == "BL16") {
            Image::ImageData image;
            if (Check broken = DecodeBlock(data, blocks.size(), image)) return broken;
            blocks.push_back(std::move(image));
        } else if (kind == "LAYR") {
            Layer layer;
            if (Check broken = ReadLayer(data, blocks.size(), layers.size(), layer)) return broken;
            layers.push_back(std::move(layer));
        }
    }

    std::map<Vec3i, ColourChunk> voxels;
    for (const auto& layer : layers) {
        GoxLayerReport report{layer.name, GoxLayerUse::Geometry, 0};
        const bool excluded = std::any_of(options.excludeLayers.begin(), options.excludeLayers.end(),
                                          [&](const std::string& pattern) { return MatchesPattern(pattern, layer.name); });
        if (!layer.visible) report.use = GoxLayerUse::Hidden;
        else if (!layer.ownBlocks) report.use = GoxLayerUse::NoBlocks;
        else if (excluded) report.use = GoxLayerUse::Excluded;
        if (report.use == GoxLayerUse::Geometry)
            if (Check broken = Paint(layer, blocks, voxels, report)) return broken;
        result.layers.push_back(std::move(report));
    }

    std::set<std::uint32_t> colours;
    for (const auto& [key, cells] : voxels)
        for (const auto value : cells)
            if (value != 0) colours.insert(value - 1);
    if (colours.empty()) return Broken("no geometry layer has a voxel");
    if (colours.size() > kMaxColours)
        return Broken("the geometry has " + std::to_string(colours.size()) + " colours; a palette holds 256");

    const auto shapes = DefaultShapes();
    std::map<std::uint32_t, std::uint16_t> blockOf;
    auto& grid = result.grid;
    grid.cellSize = options.cellSize;
    grid.shapes = shapes;
    for (const auto colour : colours) {
        const auto rgb = RgbOf(colour);
        std::uint32_t shape = 0;
        if (const auto named = options.blockShapes.find(rgb); named != options.blockShapes.end()) {
            const auto found = std::find_if(shapes.begin(), shapes.end(),
                                            [&](const Shape& s) { return s.name == named->second; });
            if (found == shapes.end()) return Broken("no shape is named '" + named->second + "'", ColourBlockName(rgb));
            shape = static_cast<std::uint32_t>(found - shapes.begin());
        }
        grid.blocks.push_back({ColourBlockName(rgb), shape, {rgb[0], rgb[1], rgb[2], 255}});
        blockOf.emplace(colour, static_cast<std::uint16_t>(grid.blocks.size()));
    }
    for (const auto& [rgb, shape] : options.blockShapes) {
        const auto colour = (std::uint32_t{rgb[0]} << 16) | (std::uint32_t{rgb[1]} << 8) | rgb[2];
        if (!colours.contains(colour)) {
            if (std::none_of(shapes.begin(), shapes.end(), [&](const Shape& s) { return s.name == shape; }))
                return Broken("no shape is named '" + shape + "'", ColourBlockName(rgb));
            result.unusedShapeColours.push_back(rgb);
        }
    }
    for (const auto& [key, cells] : voxels) {
        ChunkCellsArray chunk{};
        bool any = false;
        for (std::size_t i = 0; i < cells.size(); ++i) {
            if (cells[i] == 0) continue;
            chunk[i] = {blockOf.at(cells[i] - 1), 0};
            any = true;
            ++result.cells;
        }
        if (any) grid.chunks.emplace(key, chunk);
    }
    // What the tool writes, it reads back (as the bake does with the Module's parser).
    if (auto readBack = ParseGrid(WriteGrid(grid)); !readBack)
        return Broken("the imported grid does not read back: " + readBack.error().message, readBack.error().detail);
    return std::nullopt;
}

} // namespace

Result<GoxImport> ImportGox(const std::span<const std::byte> bytes, const GoxImportOptions& options) {
    GoxImport result;
    if (Check broken = Import(bytes, options, result))
        return Base::Err(Error::Make(ErrorCode::InvalidGox, ".gox import: " + std::move(broken->message),
                                     std::move(broken->detail)));
    return result;
}

bool MatchesPattern(const std::string_view pattern, const std::string_view name) noexcept {
    // Iterative wildcard match: on a mismatch, go back to the last '*' and let it take one more character.
    std::size_t p = 0, n = 0, star = std::string_view::npos, resume = 0;
    while (n < name.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == name[n])) {
            ++p;
            ++n;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            resume = n;
        } else if (star != std::string_view::npos) {
            p = star + 1;
            n = ++resume;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

std::optional<Rgb> ParseRgb(const std::string_view text) noexcept {
    if (text.size() != 6) return std::nullopt;
    Rgb colour{};
    for (std::size_t i = 0; i < 6; ++i) {
        const char c = text[i];
        std::uint8_t digit = 0;
        if (c >= '0' && c <= '9') digit = static_cast<std::uint8_t>(c - '0');
        else if (c >= 'a' && c <= 'f') digit = static_cast<std::uint8_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = static_cast<std::uint8_t>(c - 'A' + 10);
        else return std::nullopt;
        colour[i / 2] = static_cast<std::uint8_t>(colour[i / 2] * 16 + digit);
    }
    return colour;
}

std::string ColourBlockName(const Rgb& colour) {
    constexpr char kHex[] = "0123456789abcdef";
    std::string name = "c_";
    for (const auto channel : colour) {
        name += kHex[channel >> 4];
        name += kHex[channel & 15];
    }
    return name;
}

} // namespace voxel_map_editor
