// The level's picture: the original PuzzleRenderer::Draw (Object_Connect ca4482c,
// src/ObjectConnect/Rendering/PuzzleRenderer.cpp:671-1001) on a Canvas. The shapes,
// colours and layers are the original's. Left out: its fixed vertex budget (65,536
// vertices, with the reservations that dropped optional vessel copies when it was
// full): a Pass2D's lists grow, so nothing is dropped.
#include "object_connect/runtime/Drawing.hpp"

#include "object_connect/geometry/Geometry2D.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace object_connect::runtime {
namespace {

constexpr float kFleshCoreInset = 4.0f;
constexpr float kFleshPixelSpacing = 12.0f;
constexpr std::size_t kMaximumFleshSamplesPerSegment = 256;
constexpr float kInactiveNodeTint = 0.55f;
constexpr float kBoneSpotSize = 4.0f;
constexpr Color kBoneAshFill{98.0f / 255.0f, 95.0f / 255.0f, 88.0f / 255.0f, 1.0f};
constexpr Color kBoneAshOutline{48.0f / 255.0f, 46.0f / 255.0f, 44.0f / 255.0f, 1.0f};
constexpr Color kBoneAshLight{190.0f / 255.0f, 185.0f / 255.0f, 174.0f / 255.0f, 1.0f};
constexpr Color kBoneAshDark{65.0f / 255.0f, 62.0f / 255.0f, 59.0f / 255.0f, 1.0f};

struct NodePalette final {
    Color fill{};
    Color outline{};
};

[[nodiscard]] constexpr NodePalette GetNodePalette(const NodeType type) noexcept {
    switch (type) {
    case NodeType::Root: return {{0.63f, 0.12f, 0.20f, 1.0f}, {0.96f, 0.35f, 0.42f, 1.0f}};
    case NodeType::Follow: return {{0.49f, 0.22f, 0.30f, 1.0f}, {0.79f, 0.49f, 0.57f, 1.0f}};
    case NodeType::End: return {{0.44f, 0.24f, 0.49f, 1.0f}, {0.79f, 0.51f, 0.83f, 1.0f}};
    case NodeType::Dead: return {kBoneAshFill, kBoneAshOutline};
    }
    return {};
}

void OutlinedRectangle(Canvas& canvas, const Vec2 topLeft, const Vec2 size, const Color fill, const Color outline) {
    canvas.Rect(topLeft.x, topLeft.y, size.x, size.y, outline);
    const float inset = (std::min)(3.0f, (std::min)(size.x, size.y) * 0.2f);
    if (inset <= 0.0f || size.x <= inset * 2.0f || size.y <= inset * 2.0f) {
        return;
    }
    canvas.Rect(topLeft.x + inset, topLeft.y + inset, size.x - inset * 2.0f, size.y - inset * 2.0f, fill);
}

[[nodiscard]] float ResolvePixelGridSize(const TentacleStyle& style) noexcept {
    return std::isfinite(style.pixelGridSize) ? (std::max)(0.0f, style.pixelGridSize) : kDefaultTentaclePixelGridSize;
}

[[nodiscard]] Vec2 SnapToPixelGrid(const Vec2 point, const float gridSize) noexcept {
    return {std::round(point.x / gridSize) * gridSize, std::round(point.y / gridSize) * gridSize};
}

[[nodiscard]] std::uint32_t MixBits(std::uint32_t value) noexcept {
    value ^= value >> 16u;
    value *= 0x7FEB352Du;
    value ^= value >> 15u;
    value *= 0x846CA68Bu;
    value ^= value >> 16u;
    return value;
}

[[nodiscard]] std::uint32_t StableNodeIdHash(const std::string& nodeId) noexcept {
    std::uint32_t hash = 2166136261u;
    for (const char character : nodeId) {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 16777619u;
    }
    return MixBits(hash);
}

[[nodiscard]] std::uint32_t BoneTileHash(const std::uint32_t nodeHash, const std::uint32_t tileX,
                                         const std::uint32_t tileY) noexcept {
    std::uint32_t hash = MixBits(nodeHash ^ (tileX + 0x9E3779B9u));
    hash = MixBits(hash ^ (tileY + 0x85EBCA6Bu));
    return hash;
}

[[nodiscard]] TentacleStyle MakeFleshCoreStyle(TentacleStyle style) noexcept {
    const auto insetWidth = [](const float outerWidth) noexcept {
        const float finiteOuter = std::isfinite(outerWidth) ? (std::max)(0.0f, outerWidth) : 0.0f;
        return std::clamp(finiteOuter - kFleshCoreInset, 0.0f, finiteOuter);
    };
    style.baseWidth = insetWidth(style.baseWidth);
    style.tipWidth = insetWidth(style.tipWidth);
    return style;
}

// The chunky flesh pixels along a vessel (PuzzleRenderer.cpp:258-360).
void FleshPixels(Canvas& canvas, const std::vector<Vec2>& centerline, const TentacleStyle& outerStyle,
                 const Vec2 translation) {
    if (centerline.size() < 2 || !IsFinite(translation)) {
        return;
    }
    const TentacleStyle coreStyle = MakeFleshCoreStyle(outerStyle);
    const float gridSize = ResolvePixelGridSize(coreStyle);
    if (gridSize <= 0.0f) {
        return;
    }
    const float minimumCoreWidth = (std::min)(coreStyle.baseWidth, coreStyle.tipWidth);
    if (minimumCoreWidth < gridSize) {
        return;
    }
    const float maximumOffset = (std::max)(0.0f, minimumCoreWidth * 0.5f - gridSize);
    const Color highlight = ScaleRgb(coreStyle.color, 1.55f);
    const Color shadow = ScaleRgb(coreStyle.color, 0.48f);
    const float finitePhase = std::isfinite(coreStyle.widthPhase) ? std::fabs(coreStyle.widthPhase) : 0.0f;
    const auto phaseSeed = static_cast<std::uint32_t>(finitePhase * 4096.0f);

    for (std::size_t segmentIndex = 0; segmentIndex + 1 < centerline.size(); ++segmentIndex) {
        const Vec2 start = centerline[segmentIndex];
        const Vec2 end = centerline[segmentIndex + 1];
        if (!IsFinite(start) || !IsFinite(end)) {
            continue;
        }
        const Vec2 delta = end - start;
        const float segmentLength = Length(delta);
        if (!std::isfinite(segmentLength) || segmentLength <= kFleshPixelSpacing) {
            continue;
        }
        const Vec2 normal = Perpendicular(NormalizeOr(delta, {1.0f, 0.0f}));
        const double requestedSamples =
            std::floor(static_cast<double>(segmentLength) / static_cast<double>(kFleshPixelSpacing));
        const bool samplesWereCapped = requestedSamples > static_cast<double>(kMaximumFleshSamplesPerSegment);
        const std::size_t sampleCount =
            samplesWereCapped ? kMaximumFleshSamplesPerSegment : static_cast<std::size_t>(requestedSamples);
        for (std::size_t sampleIndex = 1; sampleIndex <= sampleCount; ++sampleIndex) {
            const float distance =
                samplesWereCapped
                    ? segmentLength * (static_cast<float>(sampleIndex) / static_cast<float>(sampleCount + 1))
                    : static_cast<float>(sampleIndex) * kFleshPixelSpacing;
            if (distance >= segmentLength) {
                break;
            }
            const std::uint32_t seed = phaseSeed ^ static_cast<std::uint32_t>(segmentIndex * 73856093u) ^
                                       static_cast<std::uint32_t>(sampleIndex * 19349663u);
            const std::uint32_t hash = MixBits(seed);
            if ((hash & 3u) == 0u) {
                continue;
            }
            const float along = distance / segmentLength;
            const int offsetStep = static_cast<int>((hash >> 8u) % 5u) - 2;
            const float offset = std::clamp(static_cast<float>(offsetStep) * gridSize, -maximumOffset, maximumOffset);
            const bool widePixel = (hash & 16u) != 0u;
            const float pixelWidth = widePixel ? gridSize * 2.0f : gridSize;
            const Color pixelColor = (hash & 1u) != 0u ? highlight : shadow;
            const Vec2 pixelCenter = start + delta * along + normal * offset;
            const Vec2 pixelCorner =
                SnapToPixelGrid({pixelCenter.x - pixelWidth * 0.5f, pixelCenter.y - gridSize * 0.5f}, gridSize);
            canvas.Rect(pixelCorner.x + translation.x, pixelCorner.y + translation.y, pixelWidth, gridSize,
                        pixelColor);
        }
    }
}

[[nodiscard]] std::vector<RibbonVertex> Translated(const std::vector<RibbonVertex>& ribbon, const Vec2 translation) {
    std::vector<RibbonVertex> translated;
    translated.reserve(ribbon.size());
    for (const RibbonVertex& vertex : ribbon) {
        translated.push_back({vertex.position + translation, vertex.color});
    }
    return translated;
}

struct Projection final {
    std::vector<RibbonVertex> outline;
    std::vector<RibbonVertex> core;
    const TentacleRenderSnapshot* tentacle = nullptr;
    Vec2 translation{};
};

} // namespace

void DrawPuzzle(Canvas& canvas, const PuzzleDefinition& definition, const PuzzleBoardSnapshot& snapshot,
                const float elapsedSeconds,
                const std::function<std::optional<GYO::Engine::Render::TextureId>(std::string_view)>& texture) {
    AxisAlignedBox canvasBounds{{0.0f, 0.0f}, {ui::kUiWidth, ui::kUiHeight}};
    if (snapshot.playfieldBounds && IsValidAxisAlignedBox(*snapshot.playfieldBounds) &&
        snapshot.playfieldBounds->minimum.x < snapshot.playfieldBounds->maximum.x &&
        snapshot.playfieldBounds->minimum.y < snapshot.playfieldBounds->maximum.y) {
        canvasBounds = *snapshot.playfieldBounds;
    }
    const Vec2 canvasExtent = canvasBounds.maximum - canvasBounds.minimum;
    canvas.Rect(canvasBounds.minimum.x, canvasBounds.minimum.y, canvasExtent.x, canvasExtent.y,
                definition.backgroundColor);

    const auto pictureOf = [&](const std::size_t index) -> std::optional<GYO::Engine::Render::TextureId> {
        const NodeDefinition& node = definition.nodes[index];
        if (!node.GetTopLeftPosition() || node.texturePath.empty()) {
            return std::nullopt;
        }
        return texture(node.texturePath);
    };
    const auto drawable = [&](const std::size_t index) {
        return index < snapshot.nodeStates.size() && snapshot.nodeStates[index].drawable;
    };

    // Vessels: a dark outline strip, the crimson core inside the gameplay width, the flesh pixels.
    std::vector<Projection> projections;
    for (const TentacleRenderSnapshot& tentacle : snapshot.tentacles) {
        TentacleStyle outline = tentacle.style;
        outline.color = ScaleRgb(outline.color, 0.32f);
        const std::vector<RibbonVertex> outlineStrip = BuildRibbonStrip(tentacle.points, outline);
        const std::vector<RibbonVertex> coreStrip = BuildRibbonStrip(tentacle.points, MakeFleshCoreStyle(tentacle.style));
        if (outlineStrip.size() < 4 || coreStrip.size() < 4) {
            continue;
        }
        for (const Vec2 translation : tentacle.drawTranslations) {
            if (!IsFinite(translation)) {
                continue;
            }
            projections.push_back({Translated(outlineStrip, translation), Translated(coreStrip, translation),
                                   &tentacle, translation});
        }
    }
    for (const Projection& projection : projections) canvas.Strip(projection.outline);
    for (const Projection& projection : projections) canvas.Strip(projection.core);
    for (const Projection& projection : projections) {
        FleshPixels(canvas, projection.tentacle->points, projection.tentacle->style, projection.translation);
    }

    // Dead nodes cover the vessels: procedural bone where there is no picture, then the pictures.
    std::vector<std::size_t> proceduralDead;
    for (std::size_t index = 0; index < definition.nodes.size(); ++index) {
        const NodeDefinition& node = definition.nodes[index];
        if (drawable(index) && node.type == NodeType::Dead && !pictureOf(index) && node.GetTopLeftPosition()) {
            proceduralDead.push_back(index);
        }
    }
    for (const std::size_t index : proceduralDead) {
        const NodeDefinition& node = definition.nodes[index];
        const NodePalette palette = GetNodePalette(node.type);
        OutlinedRectangle(canvas, *node.GetTopLeftPosition(), node.GetPixelSize(), palette.fill, palette.outline);
    }
    for (const std::size_t index : proceduralDead) {
        const NodeDefinition& node = definition.nodes[index];
        const Vec2 topLeft = *node.GetTopLeftPosition();
        const std::uint32_t nodeHash = StableNodeIdHash(node.id);
        for (std::uint32_t tileY = 0; tileY < node.heightTiles; ++tileY) {
            for (std::uint32_t tileX = 0; tileX < node.widthTiles; ++tileX) {
                const std::uint32_t hash =
                    BoneTileHash(nodeHash, node.tilePosition->x + tileX, node.tilePosition->y + tileY);
                if (hash % 100u >= 35u) {
                    continue;
                }
                constexpr float kSpotInset = 2.0f;
                constexpr float kSpotStride = 4.0f;
                const float spotX = topLeft.x + static_cast<float>(tileX) * kPuzzleTileSize + kSpotInset +
                                    static_cast<float>((hash >> 8u) % 3u) * kSpotStride;
                const float spotY = topLeft.y + static_cast<float>(tileY) * kPuzzleTileSize + kSpotInset +
                                    static_cast<float>((hash >> 12u) % 3u) * kSpotStride;
                canvas.Rect(spotX, spotY, kBoneSpotSize, kBoneSpotSize,
                            (hash & 0x80000000u) != 0u ? kBoneAshLight : kBoneAshDark);
            }
        }
    }
    const auto pictures = [&](const bool deadLayer) {
        for (std::size_t index = 0; index < definition.nodes.size(); ++index) {
            const NodeDefinition& node = definition.nodes[index];
            const bool isDead = node.type == NodeType::Dead;
            const std::optional<GYO::Engine::Render::TextureId> picture = pictureOf(index);
            if (isDead != deadLayer || !drawable(index) || !picture) {
                continue;
            }
            const float tint = !isDead && !snapshot.nodeStates[index].active ? kInactiveNodeTint : 1.0f;
            const Vec2 topLeft = *node.GetTopLeftPosition();
            const Vec2 size = node.GetPixelSize();
            canvas.Image(*picture, topLeft.x, topLeft.y, size.x, size.y, Color{tint, tint, tint, 1.0f});
        }
    };
    pictures(true);

    // A quiet halo around every active interactive node.
    for (std::size_t index = 0; index < definition.nodes.size(); ++index) {
        const NodeDefinition& node = definition.nodes[index];
        if (!drawable(index) || !snapshot.nodeStates[index].active || node.type == NodeType::Dead) {
            continue;
        }
        const std::optional<Vec2> center = node.GetCenterPosition();
        if (!center) {
            continue;
        }
        const Vec2 size = node.GetPixelSize();
        const float radius = (std::max)(size.x, size.y) * 0.62f + 2.0f;
        canvas.Circle(*center, radius, WithAlpha(ScaleRgb(GetNodePalette(node.type).outline, 1.15f), 0.22f));
    }
    // Available sources pulse; the selected one faster and brighter.
    if (!snapshot.solved) {
        for (std::size_t index = 0; index < definition.nodes.size(); ++index) {
            const NodeDefinition& node = definition.nodes[index];
            if (!drawable(index) || node.type == NodeType::Dead || !snapshot.nodeStates[index].availableSource) {
                continue;
            }
            const std::optional<Vec2> center = node.GetCenterPosition();
            if (!center) {
                continue;
            }
            const bool selected = snapshot.selectedSourceNodeIndex && *snapshot.selectedSourceNodeIndex == index;
            const float wave = 0.5f + 0.5f * std::sin(elapsedSeconds * (selected ? 8.0f : 5.5f));
            const Vec2 size = node.GetPixelSize();
            const float radius = (std::max)(size.x, size.y) * 0.62f + (selected ? 7.0f : 4.0f) + wave * 3.0f;
            canvas.Circle(*center, radius,
                          selected ? WithAlpha(ScaleRgb(definition.vesselColor, 1.55f), 0.78f)
                                   : WithAlpha(definition.vesselColor, 0.48f));
        }
    }
    // Placeholders for interactive nodes without a picture.
    for (std::size_t index = 0; index < definition.nodes.size(); ++index) {
        const NodeDefinition& node = definition.nodes[index];
        if (!drawable(index) || node.type == NodeType::Dead || pictureOf(index)) {
            continue;
        }
        const std::optional<Vec2> topLeft = node.GetTopLeftPosition();
        if (!topLeft) {
            continue;
        }
        NodePalette palette = GetNodePalette(node.type);
        if (!snapshot.nodeStates[index].active) {
            palette.fill = ScaleRgb(palette.fill, kInactiveNodeTint);
            palette.outline = ScaleRgb(palette.outline, kInactiveNodeTint);
        }
        if (snapshot.selectedSourceNodeIndex && *snapshot.selectedSourceNodeIndex == index) {
            palette.outline = ScaleRgb(definition.vesselColor, 1.65f);
        }
        OutlinedRectangle(canvas, *topLeft, node.GetPixelSize(), palette.fill, palette.outline);
    }
    pictures(false);
}

} // namespace object_connect::runtime
