// The menus' and HUD's picture: the original GameUiRenderer::Draw (Object_Connect ca4482c,
// src/ObjectConnect/Rendering/GameUiRenderer.cpp:599-1130) on a Canvas, the texts as requests. The
// rectangles, colours, texts, sizes and order are the original's; the page bookkeeping at the top of
// the original Draw is MenuPages::Present (PresentMenus), which ran earlier in the frame.
#include "object_connect/runtime/Drawing.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace object_connect::runtime {
namespace {

using ui::MenuLayout;
using ui::UiRect;

constexpr float kMenuTextPixelSize = 28.0f;
constexpr float kLevelNumberBackdropWidth = 72.0f;
constexpr float kLevelNumberBackdropHeight = 68.0f;
constexpr float kLevelNumberMaximumPixelSize = 32.0f;
constexpr std::array<float, 2> kFinalResultsCardLefts = {132.0f, 696.0f};
constexpr float kFinalResultsCardTop = 142.0f;
constexpr float kFinalResultsCardWidth = 452.0f;
constexpr float kFinalResultsCardHeight = 58.0f;
constexpr float kFinalResultsCardGap = 8.0f;
constexpr float kFinalResultsCardInset = 3.0f;
constexpr float kFinalResultsIconSize = 48.0f;
constexpr Color kGrey{0.50f, 0.50f, 0.50f, 1.0f};

struct DecorationSpec final {
    UiRect bounds;
    Color color;
};

constexpr std::array<DecorationSpec, 11> kLevelDecorationSpecs{{
    {{142.0f, 48.0f, 996.0f, 624.0f}, {0.53f, 0.45f, 0.39f, 1.0f}},
    {{154.0f, 60.0f, 972.0f, 600.0f}, {0.19f, 0.15f, 0.13f, 1.0f}},
    {{169.0f, 75.0f, 942.0f, 570.0f}, {0.39f, 0.32f, 0.28f, 1.0f}},
    {{182.0f, 88.0f, 916.0f, 544.0f}, {0.25f, 0.20f, 0.17f, 1.0f}},
    {{194.0f, 100.0f, 892.0f, 520.0f}, {0.32f, 0.26f, 0.23f, 1.0f}},
    {{244.0f, 174.0f, 792.0f, 314.0f}, {0.18f, 0.14f, 0.12f, 1.0f}},
    {{254.0f, 184.0f, 772.0f, 294.0f}, {0.43f, 0.36f, 0.31f, 1.0f}},
    {{265.0f, 195.0f, 750.0f, 272.0f}, {0.28f, 0.22f, 0.19f, 1.0f}},
    {{414.0f, 44.0f, 452.0f, 108.0f}, {0.55f, 0.47f, 0.40f, 1.0f}},
    {{425.0f, 55.0f, 430.0f, 86.0f}, {0.19f, 0.15f, 0.13f, 1.0f}},
    {{438.0f, 67.0f, 404.0f, 62.0f}, {0.38f, 0.31f, 0.27f, 1.0f}},
}};

void Rect(Canvas& canvas, const UiRect& r, const Color color) { canvas.Rect(r.left, r.top, r.width, r.height, color); }

[[nodiscard]] std::string LengthText(const float value) {
    return std::to_string(std::lround((std::max)(0.0f, value)));
}

[[nodiscard]] UiRect FinalResultsCardBounds(const std::size_t visibleSlot) noexcept {
    const std::size_t column = visibleSlot % 2;
    const std::size_t row = visibleSlot / 2;
    return {kFinalResultsCardLefts[column],
            kFinalResultsCardTop + static_cast<float>(row) * (kFinalResultsCardHeight + kFinalResultsCardGap),
            kFinalResultsCardWidth, kFinalResultsCardHeight};
}

[[nodiscard]] float FinalResultsFillRatio(const std::size_t connected, const std::size_t total) noexcept {
    if (total == 0) {
        return 0.0f;
    }
    return (std::min)(1.0f, static_cast<float>(connected) / static_cast<float>(total));
}

struct Texts final {
    std::vector<TextRequest>& list;
    void operator()(std::string text, const Vec2 position, const float pixelSize,
                    const TextAlignH h = TextAlignH::Left, const TextAlignV v = TextAlignV::Top,
                    const Color color = {}) const {
        list.push_back({std::move(text), position, pixelSize, h, v, color});
    }
};

} // namespace

void DrawUi(Canvas& canvas, std::vector<TextRequest>& texts, const UiFrame& frame,
            const std::function<float(std::string_view, float)>& measure) {
    const Texts text{texts};
    const GameScreen screen = frame.screen;
    const std::size_t puzzleCount = frame.catalog->GetPuzzles().size();
    const MenuLayout layout = ui::MakeMenuLayout(screen, puzzleCount, frame.hasNextPuzzle, frame.levelSelectPage);
    const bool showFullBackground =
        screen == GameScreen::MainMenu || screen == GameScreen::LevelSelect || screen == GameScreen::FinalResults;
    const bool showOverlay = screen == GameScreen::Paused || (screen == GameScreen::Solved && frame.solvedMenuReady);
    Color background{0.03f, 0.01f, 0.02f, 0.94f};
    if (screen == GameScreen::LevelSelect) {
        background = {0.24f, 0.20f, 0.18f, 1.0f};
    } else if (screen == GameScreen::FinalResults) {
        background = {0.03f, 0.01f, 0.02f, 1.0f};
    } else if (showFullBackground) {
        background = {0.03f, 0.01f, 0.02f, 0.96f};
    } else if (showOverlay) {
        background = {0.03f, 0.01f, 0.02f, 0.72f};
    }
    const bool menuVisible = screen != GameScreen::Playing && (screen != GameScreen::Solved || frame.solvedMenuReady);
    const PuzzleBoardSnapshot* const board = frame.board;
    const bool showLengthWarning =
        screen != GameScreen::FinalResults && board != nullptr && board->lengthExhausted && !board->solved;
    const auto selectedEntry =
        std::find_if(layout.entries.begin(), layout.entries.end(),
                     [&](const MenuLayout::Entry& entry) { return entry.logicalItemIndex == frame.selectedItem; });
    const bool hasSelection = menuVisible && selectedEntry != layout.entries.end();
    UiRect selection{};
    if (hasSelection) {
        const float border = selectedEntry->visual == MenuLayout::EntryVisual::Standard ? 0.0f : 5.0f;
        const UiRect& bounds = selectedEntry->bounds;
        selection = {bounds.left - border, bounds.top - border, bounds.width + border * 2.0f,
                     bounds.height + border * 2.0f};
    }

    // ---- The texts, in the original's order (drawn after every rectangle).
    if (screen != GameScreen::FinalResults && board != nullptr && frame.currentPuzzleIndex &&
        *frame.currentPuzzleIndex < puzzleCount) {
        const PuzzleDefinition& puzzle = frame.catalog->GetPuzzles()[*frame.currentPuzzleIndex];
        for (std::size_t nodeIndex = 0; nodeIndex < puzzle.nodes.size(); ++nodeIndex) {
            const NodeDefinition& node = puzzle.nodes[nodeIndex];
            const std::optional<Vec2> center = node.GetCenterPosition();
            if (!center || node.displayName.empty()) {
                continue;
            }
            const Vec2 labelPosition{center->x, center->y + node.GetPixelSize().y * 0.5f + 4.0f};
            const bool active = nodeIndex < board->nodeStates.size() && board->nodeStates[nodeIndex].active;
            constexpr Color kLabelShadow{0.10f, 0.04f, 0.05f, 0.92f};
            constexpr Color kActiveLabel{1.0f, 0.93f, 0.82f, 1.0f};
            constexpr Color kInactiveLabel{0.70f, 0.68f, 0.65f, 1.0f};
            text(node.displayName, {labelPosition.x + 1.0f, labelPosition.y + 1.0f}, 18.0f, TextAlignH::Center,
                 TextAlignV::Top, kLabelShadow);
            text(node.displayName, labelPosition, 18.0f, TextAlignH::Center, TextAlignV::Top,
                 active ? kActiveLabel : kInactiveLabel);
        }
        text("残り " + LengthText(board->remainingLength) + " / " + LengthText(board->totalLength), {28.0f, 24.0f},
             24.0f);
        if (board->lengthExhausted && !board->solved) {
            text("長さが足りません", {ui::kUiWidth * 0.5f, 74.0f}, 25.0f, TextAlignH::Center);
        }
    }

    switch (screen) {
    case GameScreen::MainMenu:
        text("ブラッドライン", {ui::kUiWidth * 0.5f, 118.0f}, 45.0f, TextAlignH::Center);
        text("血流を取り戻せ", {ui::kUiWidth * 0.5f, 180.0f}, 23.0f, TextAlignH::Center);
        break;
    case GameScreen::LevelSelect:
        text("ステージ選択", {ui::kUiWidth * 0.5f, 78.0f}, 41.0f, TextAlignH::Center);
        break;
    case GameScreen::Paused:
        text("一時停止", {ui::kUiWidth * 0.5f, 100.0f}, 45.0f, TextAlignH::Center);
        break;
    case GameScreen::Solved:
        if (frame.solvedMenuReady) {
            text("血流回復", {ui::kUiWidth * 0.5f, 112.0f}, 41.0f, TextAlignH::Center);
        }
        break;
    case GameScreen::FinalResults: {
        text("最終結果", {ui::kUiWidth * 0.5f, 18.0f}, 38.0f, TextAlignH::Center);
        text("目標：すべての臓器をつなぐ", {ui::kUiWidth * 0.5f, 64.0f}, 19.0f, TextAlignH::Center);
        const std::size_t completed = frame.finalResults != nullptr ? frame.finalResults->stages.size() : 0;
        text("クリアステージ " + std::to_string(completed), {ui::kUiWidth * 0.5f, 96.0f}, 24.0f, TextAlignH::Center);
        const std::size_t pageCount = GetFinalResultsPageCount(completed);
        const std::size_t page = ClampFinalResultsPage(frame.finalResultsPage, completed);
        const std::size_t firstStage = page * kFinalResultsStagesPerPage;
        const std::size_t visible =
            firstStage < completed ? (std::min)(kFinalResultsStagesPerPage, completed - firstStage) : 0;
        for (std::size_t slot = 0; slot < visible; ++slot) {
            const StageResultEntry& stage = frame.finalResults->stages[firstStage + slot];
            const UiRect bounds = FinalResultsCardBounds(slot);
            text(stage.stageTitle.empty() ? stage.puzzleId : stage.stageTitle,
                 {bounds.left + 68.0f, bounds.top + bounds.height * 0.5f}, 21.0f, TextAlignH::Left,
                 TextAlignV::Middle);
            text("【臓器 " + std::to_string(stage.connectedOrganCount) + "/" +
                     std::to_string(stage.totalOrganCount) + "】",
                 {bounds.left + bounds.width - 12.0f, bounds.top + bounds.height * 0.5f}, 20.0f, TextAlignH::Right,
                 TextAlignV::Middle);
        }
        text("ページ " + std::to_string(page + 1) + " / " + std::to_string(pageCount), {ui::kUiWidth * 0.5f, 494.0f},
             17.0f, TextAlignH::Center);
        const UiRect& previous = ui::kFinalResultsPreviousPageBounds;
        const UiRect& next = ui::kFinalResultsNextPageBounds;
        text("<", {previous.left + previous.width * 0.5f, previous.top + previous.height * 0.5f}, 38.0f,
             TextAlignH::Center, TextAlignV::Middle, page > 0 ? Color{} : kGrey);
        text(">", {next.left + next.width * 0.5f, next.top + next.height * 0.5f}, 38.0f, TextAlignH::Center,
             TextAlignV::Middle, page + 1 < pageCount ? Color{} : kGrey);
        break;
    }
    case GameScreen::Playing:
        break;
    }

    if (menuVisible) {
        for (const MenuLayout::Entry& entry : layout.entries) {
            const UiRect& bounds = entry.bounds;
            const Vec2 center{bounds.left + bounds.width * 0.5f, bounds.top + bounds.height * 0.5f};
            if (entry.visual == MenuLayout::EntryVisual::LevelTile) {
                // The number shrinks to fit its backdrop (GetLevelNumberPixelSize).
                float pixelSize = kLevelNumberMaximumPixelSize;
                const float maximumWidth = kLevelNumberBackdropWidth - 10.0f;
                const float width = entry.label.empty() ? 0.0f : measure(entry.label, kLevelNumberMaximumPixelSize);
                if (width > maximumWidth) {
                    pixelSize = (std::max)(1.0f, std::floor(kLevelNumberMaximumPixelSize * maximumWidth / width));
                }
                text(entry.label, center, pixelSize, TextAlignH::Center, TextAlignV::Middle);
                continue;
            }
            if (entry.visual == MenuLayout::EntryVisual::LevelBack) {
                text(entry.label, center, 19.0f, TextAlignH::Center, TextAlignV::Middle);
                continue;
            }
            if (entry.logicalItemIndex == frame.selectedItem) {
                text(">", {bounds.left + 18.0f, center.y}, kMenuTextPixelSize, TextAlignH::Left, TextAlignV::Middle);
            }
            text(entry.label, {bounds.left + 58.0f, center.y}, kMenuTextPixelSize, TextAlignH::Left,
                 TextAlignV::Middle);
        }
        constexpr const char* kHint = "W/S または上下キー：選択    Enter/左クリック：決定";
        if (screen == GameScreen::LevelSelect) {
            text("ページ " + std::to_string(layout.levelPage + 1) + " / " + std::to_string(layout.levelPageCount),
                 {ui::kUiWidth * 0.5f, 508.0f}, 17.0f, TextAlignH::Center);
            const UiRect& previous = ui::kPreviousPageBounds;
            const UiRect& next = ui::kNextPageBounds;
            text("<", {previous.left + previous.width * 0.5f, previous.top + previous.height * 0.5f}, 41.0f,
                 TextAlignH::Center, TextAlignV::Middle);
            text(">", {next.left + next.width * 0.5f, next.top + next.height * 0.5f}, 41.0f, TextAlignH::Center,
                 TextAlignV::Middle);
            text(kHint, {ui::kUiWidth * 0.5f, 684.0f}, 17.0f, TextAlignH::Center);
        } else if (screen != GameScreen::FinalResults) {
            text(kHint, {ui::kUiWidth * 0.5f, 662.0f}, 17.0f, TextAlignH::Center);
        }
    }

    // ---- The rectangles, in the original's order.
    if (showFullBackground || showOverlay) {
        canvas.Rect(0.0f, 0.0f, 1280.0f, 720.0f, background);
    }
    if (screen == GameScreen::FinalResults && frame.finalResults != nullptr) {
        const std::size_t stageCount = frame.finalResults->stages.size();
        const std::size_t page = ClampFinalResultsPage(frame.finalResultsPage, stageCount);
        const std::size_t firstStage = page * kFinalResultsStagesPerPage;
        const std::size_t visible =
            firstStage < stageCount ? (std::min)(kFinalResultsStagesPerPage, stageCount - firstStage) : 0;
        for (std::size_t slot = 0; slot < visible; ++slot) {
            const StageResultEntry& stage = frame.finalResults->stages[firstStage + slot];
            const UiRect bounds = FinalResultsCardBounds(slot);
            const float fillRatio = FinalResultsFillRatio(stage.connectedOrganCount, stage.totalOrganCount);
            const bool complete = stage.totalOrganCount > 0 && stage.connectedOrganCount >= stage.totalOrganCount;
            Rect(canvas, bounds, complete ? Color{0.86f, 0.63f, 0.24f, 1.0f} : Color{0.40f, 0.24f, 0.24f, 1.0f});
            const UiRect inner{bounds.left + kFinalResultsCardInset, bounds.top + kFinalResultsCardInset,
                               bounds.width - kFinalResultsCardInset * 2.0f,
                               bounds.height - kFinalResultsCardInset * 2.0f};
            Rect(canvas, inner, {0.12f, 0.055f, 0.065f, 0.96f});
            if (fillRatio > 0.0f) {
                Rect(canvas, {inner.left, inner.top, inner.width * fillRatio, inner.height},
                     complete ? Color{0.78f, 0.49f, 0.13f, 0.38f} : Color{0.58f, 0.18f, 0.20f, 0.34f});
            }
            if (frame.organTexture) {
                canvas.Image(*frame.organTexture, bounds.left + 8.0f,
                             bounds.top + (bounds.height - kFinalResultsIconSize) * 0.5f, kFinalResultsIconSize,
                             kFinalResultsIconSize);
            }
        }
    }
    if (screen == GameScreen::LevelSelect) {
        for (const DecorationSpec& spec : kLevelDecorationSpecs) Rect(canvas, spec.bounds, spec.color);
    }
    constexpr Color kSelection{0.65f, 0.12f, 0.20f, 0.48f};
    if (hasSelection && selectedEntry->visual != MenuLayout::EntryVisual::Standard) {
        Rect(canvas, selection, kSelection);
    }
    if (screen == GameScreen::LevelSelect) {
        for (const MenuLayout::Entry& entry : layout.entries) {
            if (entry.visual == MenuLayout::EntryVisual::Standard) {
                continue;
            }
            const UiRect& bounds = entry.bounds;
            Rect(canvas, bounds, {0.94f, 0.90f, 0.82f, 1.0f});
            const bool tile = entry.visual == MenuLayout::EntryVisual::LevelTile;
            const float width = tile ? kLevelNumberBackdropWidth : bounds.width - 18.0f;
            const float height = tile ? kLevelNumberBackdropHeight : bounds.height - 22.0f;
            canvas.Rect(bounds.left + (bounds.width - width) * 0.5f, bounds.top + (bounds.height - height) * 0.5f,
                        width, height, {0.36f, 0.28f, 0.24f, 1.0f});
        }
        const auto pageButton = [&](const UiRect bounds, const bool enabled) {
            const float b = enabled ? 1.0f : 0.58f;
            Rect(canvas, bounds, {0.94f * b, 0.90f * b, 0.82f * b, 1.0f});
            canvas.Rect(bounds.left + 9.0f, bounds.top + 11.0f, bounds.width - 18.0f, bounds.height - 22.0f,
                        {0.36f * b, 0.28f * b, 0.24f * b, 1.0f});
        };
        pageButton(*layout.previousPage, layout.levelPage > 0);
        pageButton(*layout.nextPage, layout.levelPage + 1 < layout.levelPageCount);
    }
    if (screen == GameScreen::FinalResults) {
        const std::size_t stageCount = frame.finalResults != nullptr ? frame.finalResults->stages.size() : 0;
        const std::size_t pageCount = GetFinalResultsPageCount(stageCount);
        const std::size_t page = ClampFinalResultsPage(frame.finalResultsPage, stageCount);
        const auto pageButton = [&](const UiRect bounds, const bool enabled) {
            const float b = enabled ? 0.92f : 0.38f;
            Rect(canvas, bounds, {0.86f * b, 0.63f * b, 0.42f * b, 1.0f});
            canvas.Rect(bounds.left + 7.0f, bounds.top + 9.0f, bounds.width - 14.0f, bounds.height - 18.0f,
                        {0.29f * b, 0.11f * b, 0.13f * b, 1.0f});
        };
        pageButton(ui::kFinalResultsPreviousPageBounds, page > 0);
        pageButton(ui::kFinalResultsNextPageBounds, page + 1 < pageCount);
    }
    if (hasSelection && selectedEntry->visual == MenuLayout::EntryVisual::Standard) {
        Rect(canvas, selection, kSelection);
    }
    if (showLengthWarning) {
        constexpr Color kWarning{0.75f, 0.02f, 0.06f, 0.72f};
        canvas.Rect(18.0f, 14.0f, 390.0f, 46.0f, kWarning);
        canvas.Rect(468.0f, 65.0f, 344.0f, 43.0f, kWarning);
    }
}

} // namespace object_connect::runtime
