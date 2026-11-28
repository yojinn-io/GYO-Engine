#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace object_fps_pvp::present {

// The labels over the heads of what the player sees (the other players' names, a target's or a monster's name and
// health bar): how they look and when a health bar shows. Presentation only: no rule reads them.

// When a thing's health bar shows (a name shows whenever the thing is seen).
// Always: whenever it is seen (a boss). Engaged: while the own player is engaged with it: a hit of the own lately, or
// its health at or below the low mark; after the last hit it holds, then fades.
enum class OverheadPolicy : std::uint8_t { Always, Engaged };

struct OverheadSettings final {
    // Metres above the thing's anchor (a player's head, the top of a target) the label stands at.
    float headClearanceMetres{};
    float textPixels{};
    std::array<float, 3> nameColor{};
    // The bar under the name: its size, the gap between the name and the bar, its back, its fill and its fill at or
    // below lowHealth.
    float barWidthPixels{}, barHeightPixels{}, gapPixels{};
    std::array<float, 3> backColor{};
    float backAlpha{};
    std::array<float, 3> fillColor{};
    std::array<float, 3> lowColor{};
    // A fraction of the whole health in (0, 1).
    float lowHealth{};
    double holdSeconds{};
    double fadeSeconds{};
};

// Parses ui/overhead.json (version 1); nullopt with an error naming the member otherwise.
[[nodiscard]] std::optional<OverheadSettings> ParseOverheadSettings(const std::string& text, std::string& error);

// The fraction of a health bar for milli-points of a whole (both in milli-points): the whole points a player reads
// (sim::DisplayPoints), so a thing with any health left never shows an empty bar. 0 when whole is 0.
[[nodiscard]] float HealthFraction(std::uint32_t milliPoints, std::uint32_t wholeMilliPoints) noexcept;

// The alpha of a health bar this frame. health: the bar's fraction (none: no bar). lastOwnHit: when the own player last
// hit the thing (none: never), on the clock of now.
[[nodiscard]] float OverheadBarAlpha(OverheadPolicy policy, std::optional<float> health, std::optional<double> lastOwnHit,
                                     double nowSeconds, const OverheadSettings& settings) noexcept;

} // namespace object_fps_pvp::present
