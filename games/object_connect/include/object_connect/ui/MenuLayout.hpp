#pragma once

#include "object_connect/game/GameFlow.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The menus' layout and the page state of the level select and the final
// results, in playfield coordinates (1280 x 720). Moved out of the original
// GameUiRenderer (Object_Connect ca4482c, src/ObjectConnect/Rendering/
// GameUiRenderer.cpp): the layout, the hit test and the page navigation are the
// game's rules; drawing them waits for the render batches and reads the same
// layout. The values and the order of the checks are the original's.

namespace object_connect::ui {

struct UiPoint final {
    float x = 0.0f;
    float y = 0.0f;
};

struct UiRect final {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    // Left and top edges inside, right and bottom outside; a non-finite point is outside.
    [[nodiscard]] bool Contains(UiPoint point) const noexcept;
};

struct MenuLayout final {
    enum class EntryVisual : std::uint8_t {
        Standard,
        LevelTile,
        LevelBack,
    };

    struct Entry final {
        // The GameFlow item this entry selects.
        std::size_t logicalItemIndex = 0;
        std::string label;
        UiRect bounds;
        EntryVisual visual = EntryVisual::Standard;
    };

    std::vector<Entry> entries;
    std::optional<UiRect> previousPage;
    std::optional<UiRect> nextPage;
    std::size_t levelPage = 0;
    std::size_t levelPageCount = 1;
};

inline constexpr float kUiWidth = 1280.0f;
inline constexpr float kUiHeight = 720.0f;
inline constexpr std::size_t kLevelGridColumns = 5;
inline constexpr std::size_t kLevelGridRows = 2;
inline constexpr std::size_t kLevelsPerPage = kLevelGridColumns * kLevelGridRows;
// One wheel notch in the original's unit (Win32 WHEEL_DELTA).
inline constexpr std::int64_t kWheelDeltaPerPage = 120;

inline constexpr UiRect kPreviousPageBounds{126.0f, 284.0f, 88.0f, 88.0f};
inline constexpr UiRect kNextPageBounds{1066.0f, 284.0f, 88.0f, 88.0f};
inline constexpr UiRect kFinalResultsPreviousPageBounds{42.0f, 278.0f, 64.0f, 82.0f};
inline constexpr UiRect kFinalResultsNextPageBounds{1174.0f, 278.0f, 64.0f, 82.0f};

[[nodiscard]] std::size_t GetLevelPageCount(std::size_t puzzleCount) noexcept;

// The entries of a screen's menu (none while Playing). requestedLevelPage is
// clamped to the level select's pages.
[[nodiscard]] MenuLayout MakeMenuLayout(GameScreen screen, std::size_t puzzleCount, bool hasNextPuzzle,
                                        std::size_t requestedLevelPage);

// The page state the original kept in its renderer. Present() is the part of
// the original Draw() that moved the pages; the game calls it once a frame,
// after the frame's logic, with what the frame ends on, so the next frame's
// navigation sees the screen as last shown, as in the original.
class MenuPages final {
public:
    void Present(GameScreen screen, std::size_t selectedItem, std::size_t puzzleCount,
                 std::size_t finalResultsStageCount) noexcept;

    // The menu item under the point, as the original GameUiRenderer::HitTest.
    [[nodiscard]] std::optional<std::size_t> HitTest(GameScreen screen, UiPoint point, std::size_t puzzleCount,
                                                     bool hasNextPuzzle, bool solvedMenuReady) const;
    // A menu item or an enabled page arrow is under the point (the cursor icon).
    [[nodiscard]] bool IsPointerOverAction(GameScreen screen, UiPoint point, std::size_t puzzleCount,
                                           bool hasNextPuzzle, bool solvedMenuReady) const;
    // After the hit test and before GameFlow. Page arrows never act as menu
    // items; when the page changes, the first level index of the new page is
    // returned for GameFlow's selection.
    [[nodiscard]] std::optional<std::size_t> ApplyLevelSelectNavigation(UiPoint point, bool mousePrimaryPressed,
                                                                        int wheelDelta, bool keyboardNavigated,
                                                                        bool activationRequested,
                                                                        GameScreen screen) noexcept;
    // Final-result pages only change what is shown; true when the page changed.
    [[nodiscard]] bool ApplyFinalResultsNavigation(UiPoint point, bool mousePrimaryPressed, int wheelDelta,
                                                   bool keyboardNavigated, bool activationRequested,
                                                   GameScreen screen) noexcept;

    [[nodiscard]] std::size_t LevelSelectPage() const noexcept { return levelSelectPage_; }
    [[nodiscard]] std::size_t FinalResultsPage() const noexcept { return finalResultsPage_; }

private:
    std::size_t levelSelectPage_ = 0;
    std::size_t lastLevelSelectPuzzleCount_ = 0;
    std::optional<std::size_t> lastLevelSelectSelection_;
    std::optional<GameScreen> lastPresentedScreen_;
    bool manualLevelPageChange_ = false;
    std::int64_t levelPageWheelRemainder_ = 0;
    std::size_t finalResultsPage_ = 0;
    std::size_t finalResultsStageCount_ = 0;
    std::int64_t finalResultsPageWheelRemainder_ = 0;
};

} // namespace object_connect::ui
