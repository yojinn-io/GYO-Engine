// The menus' layout, hit test and pages (object_connect/ui/MenuLayout.hpp),
// moved out of the original GameUiRenderer: the values are the original's.
#include "TestSupport.hpp"

#include "object_connect/game/FinalResults.hpp"
#include "object_connect/ui/MenuLayout.hpp"

#include <optional>

namespace object_connect::tests {
namespace {

using ui::MenuLayout;
using ui::MenuPages;
using ui::UiPoint;

constexpr std::size_t kLevels = 23;

void TestMainMenuLayout(TestContext& context) {
    const MenuLayout layout = ui::MakeMenuLayout(GameScreen::MainMenu, kLevels, false, 0);
    context.Expect(layout.entries.size() == 2, "the main menu has two entries");
    context.Expect(layout.entries[1].bounds.top == 350.0f, "main menu entries are 54 high with a 16 gap from 280");
    MenuPages pages;
    context.Expect(pages.HitTest(GameScreen::MainMenu, {400.0f, 290.0f}, kLevels, false, true) == std::size_t{0},
                   "the first entry is hit inside it");
    context.Expect(pages.HitTest(GameScreen::MainMenu, {400.0f, 360.0f}, kLevels, false, true) == std::size_t{1},
                   "the second entry is hit inside it");
    context.Expect(!pages.HitTest(GameScreen::MainMenu, {400.0f, 340.0f}, kLevels, false, true),
                   "the gap between entries hits nothing");
    context.Expect(!pages.HitTest(GameScreen::MainMenu, {890.0f, 290.0f}, kLevels, false, true),
                   "the right edge is outside");
    context.Expect(!pages.HitTest(GameScreen::Playing, {400.0f, 290.0f}, kLevels, false, true),
                   "nothing is hit while playing");
    context.Expect(!pages.HitTest(GameScreen::Solved, {400.0f, 290.0f}, kLevels, true, false),
                   "the solved menu takes no hit before its delay");
    context.Expect(pages.HitTest(GameScreen::Solved, {400.0f, 290.0f}, kLevels, true, true) == std::size_t{0},
                   "the solved menu takes hits after its delay");
}

void TestLevelSelectLayout(TestContext& context) {
    context.Expect(ui::GetLevelPageCount(0) == 1 && ui::GetLevelPageCount(10) == 1 && ui::GetLevelPageCount(23) == 3,
                   "ten levels a page, at least one page");
    const MenuLayout first = ui::MakeMenuLayout(GameScreen::LevelSelect, kLevels, false, 0);
    context.Expect(first.entries.size() == 11, "a full page has ten tiles and the back entry");
    context.Expect(first.entries[0].bounds.left == 344.0f && first.entries[0].bounds.top == 210.0f,
                   "a row of five tiles is centered");
    context.Expect(first.entries.back().logicalItemIndex == kLevels, "the back entry is the item after the levels");
    const MenuLayout last = ui::MakeMenuLayout(GameScreen::LevelSelect, kLevels, false, 7);
    context.Expect(last.levelPage == 2, "a page past the end is clamped to the last one");
    context.Expect(last.entries.size() == 4, "the last page has the three remaining levels and the back entry");
    context.Expect(last.entries[0].bounds.left == (1280.0f - (3.0f * 96.0f + 2.0f * 28.0f)) * 0.5f,
                   "a short row is centered on its own width");
}

void TestLevelSelectPages(TestContext& context) {
    MenuPages pages;
    context.Expect(!pages.ApplyLevelSelectNavigation({1070.0f, 290.0f}, true, 0, false, false, GameScreen::LevelSelect),
                   "no page moves before the level select has been shown");
    pages.Present(GameScreen::LevelSelect, 0, kLevels, 0);
    context.Expect(!pages.IsPointerOverAction(GameScreen::LevelSelect, {130.0f, 290.0f}, kLevels, false, true),
                   "the previous arrow on the first page is not an action");
    context.Expect(pages.IsPointerOverAction(GameScreen::LevelSelect, {1070.0f, 290.0f}, kLevels, false, true),
                   "the next arrow on the first page is an action");

    const std::optional<std::size_t> next =
        pages.ApplyLevelSelectNavigation({1070.0f, 290.0f}, true, 0, false, false, GameScreen::LevelSelect);
    context.Expect(next == std::size_t{10} && pages.LevelSelectPage() == 1,
                   "the next arrow moves a page and selects its first level");
    context.Expect(pages.HitTest(GameScreen::LevelSelect, {350.0f, 215.0f}, kLevels, false, true) == std::size_t{10},
                   "the tiles of the new page are hit");
    pages.Present(GameScreen::LevelSelect, 10, kLevels, 0);
    context.Expect(pages.LevelSelectPage() == 1, "the page stays where the arrow put it");

    context.Expect(!pages.ApplyLevelSelectNavigation({0.0f, 0.0f}, false, -60, false, false, GameScreen::LevelSelect),
                   "half a wheel notch moves nothing");
    context.Expect(
        pages.ApplyLevelSelectNavigation({0.0f, 0.0f}, false, -60, false, false, GameScreen::LevelSelect) ==
            std::size_t{20},
        "the second half notch down moves to the next page");
    context.Expect(!pages.ApplyLevelSelectNavigation({0.0f, 0.0f}, false, 60, false, false, GameScreen::LevelSelect),
                   "half a notch up moves nothing");
    context.Expect(!pages.ApplyLevelSelectNavigation({0.0f, 0.0f}, false, 0, true, false, GameScreen::LevelSelect),
                   "a key drops the wheel's remainder");
    context.Expect(!pages.ApplyLevelSelectNavigation({0.0f, 0.0f}, false, 60, false, false, GameScreen::LevelSelect),
                   "after a key, half a notch up again moves nothing");
    context.Expect(
        pages.ApplyLevelSelectNavigation({1070.0f, 290.0f}, true, -240, false, false, GameScreen::LevelSelect) ==
            std::nullopt,
        "an arrow on the last page takes the frame and moves nothing, the wheel too");
    context.Expect(pages.LevelSelectPage() == 2, "still the last page");

    MenuPages fresh;
    fresh.Present(GameScreen::LevelSelect, 0, kLevels, 0);
    fresh.Present(GameScreen::LevelSelect, 15, kLevels, 0);
    context.Expect(fresh.LevelSelectPage() == 1, "a selection moved by the keys brings its page");
    fresh.Present(GameScreen::MainMenu, 0, kLevels, 0);
    fresh.Present(GameScreen::LevelSelect, 22, kLevels, 0);
    context.Expect(fresh.LevelSelectPage() == 2, "entering the level select shows the selected level's page");
}

void TestFinalResultsPages(TestContext& context) {
    MenuPages pages;
    pages.Present(GameScreen::FinalResults, 0, kLevels, 25);
    context.Expect(pages.FinalResultsPage() == GetFinalResultsPageCount(25) - 1,
                   "the final results open on their last page");
    context.Expect(pages.IsPointerOverAction(GameScreen::FinalResults, {50.0f, 300.0f}, kLevels, false, true),
                   "the previous arrow is an action when there is a page before");
    context.Expect(!pages.IsPointerOverAction(GameScreen::FinalResults, {1180.0f, 300.0f}, kLevels, false, true),
                   "the next arrow on the last page is not an action");
    context.Expect(pages.ApplyFinalResultsNavigation({50.0f, 300.0f}, true, 0, false, false, GameScreen::FinalResults),
                   "the previous arrow changes the page");
    context.Expect(pages.FinalResultsPage() + 2 == GetFinalResultsPageCount(25), "one page back");
    context.Expect(!pages.ApplyFinalResultsNavigation({50.0f, 300.0f}, true, 0, false, true, GameScreen::FinalResults),
                   "an activation in the same frame keeps the page");
}

} // namespace

void RunMenuLayoutTests(TestContext& context) {
    TestMainMenuLayout(context);
    TestLevelSelectLayout(context);
    TestLevelSelectPages(context);
    TestFinalResultsPages(context);
}

} // namespace object_connect::tests
