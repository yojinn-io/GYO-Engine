#include "object_connect/ui/MenuLayout.hpp"

#include "object_connect/game/FinalResults.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <string_view>

namespace object_connect::ui {
namespace {

constexpr float kMenuLeft = 390.0f;
constexpr float kMenuWidth = 500.0f;
constexpr float kItemHeight = 54.0f;
constexpr float kItemGap = 16.0f;
constexpr float kLevelGridTop = 210.0f;
constexpr float kLevelTileSize = 96.0f;
constexpr float kLevelTileGap = 28.0f;
constexpr float kLevelBackSize = 88.0f;
constexpr float kLevelBackLeft = 596.0f;
constexpr float kLevelBackTop = 558.0f;

[[nodiscard]] float GetCenteredRowLeft(const std::size_t itemCount) noexcept {
    if (itemCount == 0) {
        return kUiWidth * 0.5f;
    }
    const float width =
        static_cast<float>(itemCount) * kLevelTileSize + static_cast<float>(itemCount - 1) * kLevelTileGap;
    return (kUiWidth - width) * 0.5f;
}

void AddVerticalEntries(MenuLayout& layout, const std::span<const std::string_view> labels, const float top) {
    layout.entries.reserve(layout.entries.size() + labels.size());
    for (const std::string_view label : labels) {
        const std::size_t index = layout.entries.size();
        layout.entries.push_back({
            index,
            std::string{label},
            {kMenuLeft, top + static_cast<float>(index) * (kItemHeight + kItemGap), kMenuWidth, kItemHeight},
            MenuLayout::EntryVisual::Standard,
        });
    }
}

} // namespace

bool UiRect::Contains(const UiPoint point) const noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y) && point.x >= left && point.x < left + width &&
           point.y >= top && point.y < top + height;
}

std::size_t GetLevelPageCount(const std::size_t puzzleCount) noexcept {
    if (puzzleCount == 0) {
        return 1;
    }
    return 1 + (puzzleCount - 1) / kLevelsPerPage;
}

MenuLayout MakeMenuLayout(const GameScreen screen, const std::size_t puzzleCount, const bool hasNextPuzzle,
                          const std::size_t requestedLevelPage) {
    MenuLayout layout;
    switch (screen) {
    case GameScreen::MainMenu: {
        constexpr std::array labels = {std::string_view{"ゲーム開始"}, std::string_view{"終了"}};
        AddVerticalEntries(layout, labels, 280.0f);
        break;
    }
    case GameScreen::LevelSelect: {
        layout.levelPageCount = GetLevelPageCount(puzzleCount);
        layout.levelPage = (std::min)(requestedLevelPage, layout.levelPageCount - 1);
        layout.previousPage = kPreviousPageBounds;
        layout.nextPage = kNextPageBounds;

        const std::size_t firstIndex = layout.levelPage * kLevelsPerPage;
        const std::size_t itemCount =
            firstIndex < puzzleCount ? (std::min)(kLevelsPerPage, puzzleCount - firstIndex) : 0;
        layout.entries.reserve(itemCount + 1);
        const std::size_t visibleRows =
            itemCount / kLevelGridColumns + (itemCount % kLevelGridColumns == 0 ? 0 : 1);
        for (std::size_t row = 0; row < visibleRows; ++row) {
            const std::size_t rowFirstIndex = firstIndex + row * kLevelGridColumns;
            const std::size_t rowOffset = row * kLevelGridColumns;
            const std::size_t count = (std::min)(kLevelGridColumns, itemCount - rowOffset);
            const float left = GetCenteredRowLeft(count);
            const float top = kLevelGridTop + static_cast<float>(row) * (kLevelTileSize + kLevelTileGap);
            for (std::size_t column = 0; column < count; ++column) {
                const std::size_t puzzleIndex = rowFirstIndex + column;
                layout.entries.push_back({
                    puzzleIndex,
                    std::to_string(puzzleIndex + 1),
                    {left + static_cast<float>(column) * (kLevelTileSize + kLevelTileGap), top, kLevelTileSize,
                     kLevelTileSize},
                    MenuLayout::EntryVisual::LevelTile,
                });
            }
        }
        layout.entries.push_back({
            puzzleCount,
            "戻る",
            {kLevelBackLeft, kLevelBackTop, kLevelBackSize, kLevelBackSize},
            MenuLayout::EntryVisual::LevelBack,
        });
        break;
    }
    case GameScreen::Paused: {
        constexpr std::array labels = {std::string_view{"再開"}, std::string_view{"リトライ"},
                                       std::string_view{"ステージ選択"}, std::string_view{"メインメニュー"},
                                       std::string_view{"ゲーム終了"}};
        AddVerticalEntries(layout, labels, 220.0f);
        break;
    }
    case GameScreen::Solved: {
        std::vector<std::string_view> labels;
        if (hasNextPuzzle) {
            labels.push_back("次のステージ");
        }
        labels.push_back("ステージ選択");
        labels.push_back("リトライ");
        AddVerticalEntries(layout, labels, 280.0f);
        break;
    }
    case GameScreen::FinalResults: {
        constexpr std::array labels = {std::string_view{"ステージ選択"}, std::string_view{"メインメニュー"}};
        AddVerticalEntries(layout, labels, 548.0f);
        break;
    }
    case GameScreen::Playing:
        break;
    }
    return layout;
}

void MenuPages::Present(const GameScreen screen, const std::size_t selectedItem, const std::size_t puzzleCount,
                        const std::size_t finalResultsStageCount) noexcept {
    if (screen == GameScreen::LevelSelect) {
        lastLevelSelectPuzzleCount_ = puzzleCount;
        const bool enteringLevelSelect = !lastPresentedScreen_ || *lastPresentedScreen_ != GameScreen::LevelSelect;
        levelSelectPage_ = (std::min)(levelSelectPage_, GetLevelPageCount(puzzleCount) - 1);
        if (enteringLevelSelect) {
            levelSelectPage_ = selectedItem < puzzleCount ? selectedItem / kLevelsPerPage : 0;
            lastLevelSelectSelection_ = selectedItem;
            manualLevelPageChange_ = false;
            levelPageWheelRemainder_ = 0;
        } else {
            const bool selectionChanged = !lastLevelSelectSelection_ || *lastLevelSelectSelection_ != selectedItem;
            if (selectionChanged && !manualLevelPageChange_ && selectedItem < puzzleCount) {
                levelSelectPage_ = selectedItem / kLevelsPerPage;
            }
            lastLevelSelectSelection_ = selectedItem;
        }
    } else {
        levelSelectPage_ = 0;
        lastLevelSelectPuzzleCount_ = 0;
        lastLevelSelectSelection_.reset();
        manualLevelPageChange_ = false;
        levelPageWheelRemainder_ = 0;
    }

    if (screen == GameScreen::FinalResults) {
        const bool enteringFinalResults = !lastPresentedScreen_ || *lastPresentedScreen_ != GameScreen::FinalResults;
        finalResultsStageCount_ = finalResultsStageCount;
        if (enteringFinalResults) {
            finalResultsPage_ = GetFinalResultsPageCount(finalResultsStageCount) - 1;
            finalResultsPageWheelRemainder_ = 0;
        } else {
            finalResultsPage_ = ClampFinalResultsPage(finalResultsPage_, finalResultsStageCount);
        }
    } else {
        finalResultsPageWheelRemainder_ = 0;
    }
    lastPresentedScreen_ = screen;
}

std::optional<std::size_t> MenuPages::HitTest(const GameScreen screen, const UiPoint point,
                                              const std::size_t puzzleCount, const bool hasNextPuzzle,
                                              const bool solvedMenuReady) const {
    if (screen == GameScreen::Playing || (screen == GameScreen::Solved && !solvedMenuReady)) {
        return std::nullopt;
    }
    const MenuLayout layout = MakeMenuLayout(screen, puzzleCount, hasNextPuzzle, levelSelectPage_);
    for (const MenuLayout::Entry& entry : layout.entries) {
        if (entry.bounds.Contains(point)) {
            return entry.logicalItemIndex;
        }
    }
    return std::nullopt;
}

bool MenuPages::IsPointerOverAction(const GameScreen screen, const UiPoint point, const std::size_t puzzleCount,
                                    const bool hasNextPuzzle, const bool solvedMenuReady) const {
    if (HitTest(screen, point, puzzleCount, hasNextPuzzle, solvedMenuReady).has_value()) {
        return true;
    }
    if (screen == GameScreen::FinalResults) {
        const std::size_t pageCount = GetFinalResultsPageCount(finalResultsStageCount_);
        const std::size_t page = ClampFinalResultsPage(finalResultsPage_, finalResultsStageCount_);
        return (page > 0 && kFinalResultsPreviousPageBounds.Contains(point)) ||
               (page + 1 < pageCount && kFinalResultsNextPageBounds.Contains(point));
    }
    if (screen != GameScreen::LevelSelect) {
        return false;
    }
    const std::size_t pageCount = GetLevelPageCount(puzzleCount);
    const std::size_t page = (std::min)(levelSelectPage_, pageCount - 1);
    return (page > 0 && kPreviousPageBounds.Contains(point)) ||
           (page + 1 < pageCount && kNextPageBounds.Contains(point));
}

std::optional<std::size_t> MenuPages::ApplyLevelSelectNavigation(const UiPoint point, const bool mousePrimaryPressed,
                                                                 const int wheelDelta, const bool keyboardNavigated,
                                                                 const bool activationRequested,
                                                                 const GameScreen screen) noexcept {
    if (screen != GameScreen::LevelSelect || !lastPresentedScreen_ ||
        *lastPresentedScreen_ != GameScreen::LevelSelect) {
        return std::nullopt;
    }

    const std::size_t pageCount = GetLevelPageCount(lastLevelSelectPuzzleCount_);
    levelSelectPage_ = (std::min)(levelSelectPage_, pageCount - 1);
    const std::size_t previousPage = levelSelectPage_;

    bool pageButtonClicked = false;
    int pageButtonDirection = 0;
    if (mousePrimaryPressed) {
        if (kPreviousPageBounds.Contains(point)) {
            pageButtonClicked = true;
            pageButtonDirection = -1;
        } else if (kNextPageBounds.Contains(point)) {
            pageButtonClicked = true;
            pageButtonDirection = 1;
        }
    }

    if (pageButtonClicked) {
        // A page button takes the frame even on the first or last page, so a
        // wheel turn in the same frame does not also change the page.
        levelPageWheelRemainder_ = 0;
        if (pageButtonDirection < 0 && levelSelectPage_ > 0) {
            --levelSelectPage_;
        } else if (pageButtonDirection > 0 && levelSelectPage_ + 1 < pageCount) {
            ++levelSelectPage_;
        }
    } else {
        if (keyboardNavigated || activationRequested) {
            levelPageWheelRemainder_ = 0;
            return std::nullopt;
        }
        if (wheelDelta == 0) {
            return std::nullopt;
        }
        levelPageWheelRemainder_ += static_cast<std::int64_t>(wheelDelta);
        const std::int64_t pageDelta = levelPageWheelRemainder_ / kWheelDeltaPerPage;
        levelPageWheelRemainder_ %= kWheelDeltaPerPage;
        if (pageDelta > 0) {
            const auto magnitude = static_cast<std::size_t>(pageDelta);
            levelSelectPage_ = magnitude >= levelSelectPage_ ? 0 : levelSelectPage_ - magnitude;
        } else if (pageDelta < 0) {
            const std::uint64_t magnitude = static_cast<std::uint64_t>(-(pageDelta + 1)) + 1u;
            const std::size_t maximumPage = pageCount - 1;
            const std::size_t remainingPages = maximumPage - levelSelectPage_;
            levelSelectPage_ =
                magnitude >= remainingPages ? maximumPage : levelSelectPage_ + static_cast<std::size_t>(magnitude);
        }
    }

    if (levelSelectPage_ == previousPage) {
        return std::nullopt;
    }
    manualLevelPageChange_ = true;
    return levelSelectPage_ * kLevelsPerPage;
}

bool MenuPages::ApplyFinalResultsNavigation(const UiPoint point, const bool mousePrimaryPressed, const int wheelDelta,
                                            const bool keyboardNavigated, const bool activationRequested,
                                            const GameScreen screen) noexcept {
    if (screen != GameScreen::FinalResults || !lastPresentedScreen_ ||
        *lastPresentedScreen_ != GameScreen::FinalResults) {
        return false;
    }

    finalResultsPage_ = ClampFinalResultsPage(finalResultsPage_, finalResultsStageCount_);
    const std::size_t previousPage = finalResultsPage_;

    if (keyboardNavigated || activationRequested) {
        finalResultsPageWheelRemainder_ = 0;
        return false;
    }

    bool pageButtonClicked = false;
    std::ptrdiff_t pageButtonDelta = 0;
    if (mousePrimaryPressed) {
        if (kFinalResultsPreviousPageBounds.Contains(point)) {
            pageButtonClicked = true;
            pageButtonDelta = -1;
        } else if (kFinalResultsNextPageBounds.Contains(point)) {
            pageButtonClicked = true;
            pageButtonDelta = 1;
        }
    }

    if (pageButtonClicked) {
        finalResultsPageWheelRemainder_ = 0;
        finalResultsPage_ = MoveFinalResultsPage(finalResultsPage_, pageButtonDelta, finalResultsStageCount_);
    } else if (wheelDelta != 0) {
        finalResultsPageWheelRemainder_ += static_cast<std::int64_t>(wheelDelta);
        const std::int64_t wheelPageDelta = finalResultsPageWheelRemainder_ / kWheelDeltaPerPage;
        finalResultsPageWheelRemainder_ %= kWheelDeltaPerPage;
        if (wheelPageDelta > 0) {
            finalResultsPage_ = MoveFinalResultsPage(finalResultsPage_, -static_cast<std::ptrdiff_t>(wheelPageDelta),
                                                     finalResultsStageCount_);
        } else if (wheelPageDelta < 0) {
            const std::ptrdiff_t magnitude = static_cast<std::ptrdiff_t>(-(wheelPageDelta + 1)) + 1;
            finalResultsPage_ = MoveFinalResultsPage(finalResultsPage_, magnitude, finalResultsStageCount_);
        }
    }

    return finalResultsPage_ != previousPage;
}

} // namespace object_connect::ui
