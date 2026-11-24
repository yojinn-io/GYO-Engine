#include "object_connect/runtime/RuntimeWorld.hpp"

#include "object_connect/data/PuzzleCatalogLoader.hpp"
#include "object_connect/world/GameScreenState.hpp"
#include "object_connect/world/Messages.hpp"
#include "object_connect/world/ObjectConnectWorld.hpp"
#include "object_connect/world/Resources.hpp"

#include "engine/base/Log.hpp"
#include "engine/base/Result.hpp"
#include "engine/input/Input.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/orchestration/OrchestrationError.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/WorldGeneration.hpp"
#include "engine/window/Window.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/AssetView.hpp"

#include <cmath>
#include <limits>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace object_connect::runtime {
namespace {

namespace A = GYO::Engine::Asset;
namespace I = GYO::Engine::Input;
namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace W = GYO::Engine::Window;
namespace WA = GYO::Engine::WorldAsset;

[[nodiscard]] K::SystemResult Failed(std::string message) {
    return GYO::Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, std::move(message)));
}

void LogWarning(const std::string& text) {
    GYO::Engine::Base::Log(GYO::Engine::Base::LogLevel::Warning, "object_connect", text);
}

[[nodiscard]] std::string Describe(const std::vector<WA::AssetReadyFailure>& failures) {
    std::string text;
    for (const WA::AssetReadyFailure& failure : failures) {
        if (!text.empty()) text += "; ";
        text += failure.id.Empty() ? failure.writtenPath : failure.id.Text();
        if (!failure.parent.Empty()) text += " (named by " + failure.parent.Text() + ")";
        text += ": ";
        text += A::ToString(failure.code);
    }
    return text;
}

// Requests the content on the first generating frame and holds generation
// until it is Complete; then builds the catalog (the original Game::Initialize).
K::SystemResult LoadGameData(K::Res<WA::AssetRequests> requests, K::Res<WA::AssetView> view,
                             K::ResMut<GameContent> content, K::ResMut<UiText> text,
                             K::ResMut<world::Catalog> catalog, K::ResMut<O::WorldGeneration> generation) {
    if (content.Read().loaded) {
        return {};
    }
    if (content.Read().levels.Id().Empty()) {
        GameContent& c = content.Get();
        c.levels = requests->Load<LevelTable>(A::AssetId(kLevelsId));
        c.nodes = requests->Load<NodeTable>(A::AssetId(kNodesId));
        c.uiFont = requests->Load<GYO::Engine::Text::FontData>(A::AssetId(kUiFontId));
        for (std::size_t index = 0; index < kCursorIds.size(); ++index) {
            c.cursors[index] = requests->Load<GYO::Engine::Image::ImageData>(A::AssetId(kCursorIds[index]));
        }
    }
    const GameContent& c = content.Read();
    const WA::AssetReadiness ready =
        view->Ready({c.levels, c.nodes, c.uiFont, c.cursors[0], c.cursors[1], c.cursors[2]});
    if (!ready.failures.empty()) {
        return Failed("the game's content did not load: " + Describe(ready.failures));
    }
    // Every loader is registered in the product's configure, before any request: a wait for one never ends.
    if (!ready.waitingForLoader.empty() || !ready.skipped.empty()) {
        std::string ids;
        for (const auto* list : {&ready.waitingForLoader, &ready.skipped}) {
            for (const A::AssetId& id : *list) ids += (ids.empty() ? "" : ", ") + id.Text();
        }
        return Failed("the game's content has no loader for: " + ids);
    }
    if (!ready.complete) {
        generation->Hold();
        return {};
    }

    const LevelTable& levels = *view->Get(c.levels);
    const NodeTable& nodes = *view->Get(c.nodes);
    std::vector<PuzzleMapCsvSource> maps;
    maps.reserve(levels.maps.size());
    for (const NamedFile<A::Text>& map : levels.maps) {
        const A::Text* contents = view->Get(map.asset);
        if (contents == nullptr) {
            return Failed("the map '" + map.path + "' is not loaded with its level table");
        }
        maps.push_back({map.path, contents->Bytes()});
    }
    world::Catalog parsed;
    std::string error;
    std::vector<std::string> warnings;
    if (!PuzzleCatalogLoader::Parse({levels.csv, nodes.csv, maps}, parsed.puzzles, error, &warnings)) {
        return Failed("the level data is not valid: " + error);
    }
    for (const std::string& warning : warnings) {
        LogWarning(warning);
    }
    if (parsed.puzzles.Empty()) {
        return Failed("the level data must contain at least one level");
    }
    auto font = text->context.AddFont(*view->Get(c.uiFont), 0);
    if (!font) {
        return Failed("the UI font did not open: " + GYO::Engine::Base::Describe(font.error()));
    }
    text->font = *font;
    catalog.Get() = std::move(parsed);
    content->loaded = true;
    return {};
}

// Keyboard state of one frame as the original InputSystem gave it.
struct Keys final {
    bool previous = false;
    bool next = false;
    bool enter = false;
    bool escape = false;
};

[[nodiscard]] Keys ReadKeys(const I::ButtonInput<I::KeyCode>& keys) {
    return {
        keys.JustPressed(I::KeyCode::W) || keys.JustPressed(I::KeyCode::ArrowUp),
        keys.JustPressed(I::KeyCode::S) || keys.JustPressed(I::KeyCode::ArrowDown),
        keys.JustPressed(I::KeyCode::Enter) || keys.JustPressed(I::KeyCode::NumpadEnter),
        keys.JustPressed(I::KeyCode::Escape),
    };
}

// The window's input as the game's FrameInput: the original InputSystem::Sample
// and the start of Game::Impl::Update up to GameFlow.
K::SystemResult ReadPlayerInput(K::Res<I::ButtonInput<I::KeyCode>> keys, K::Res<I::ButtonInput<I::MouseButton>> mouse,
                                K::Res<I::CursorPosition> cursor, K::Res<I::AccumulatedMouseMotion> motion,
                                K::Res<I::AccumulatedMouseScroll> scroll, K::Res<W::WindowState> window,
                                K::Res<world::ScreenState> screen, K::Res<world::Catalog> catalog,
                                K::Res<world::Session> session, K::Res<world::Playfield> playfield,
                                K::ResMut<PointerState> pointer, K::ResMut<MenuPagesState> pages,
                                K::ResMut<world::FrameInput> frame, K::MessageWriter<W::WarpCursor> warp) {
    PointerState& state = pointer.Get();
    const GameScreen screenBeforeInput = screen->Current().value_or(GameScreen::MainMenu);
    const bool wrapPointer = session->board && screenBeforeInput == GameScreen::Playing &&
                             session->board->GetDefinition().wrapEdges && session->board->IsDragging();

    const AxisAlignedBox& bounds = playfield->bounds;
    const Vec2 clientSize = window->known
                                ? Vec2{window->LogicalSize().x, window->LogicalSize().y}
                                : Vec2{bounds.maximum.x - bounds.minimum.x, bounds.maximum.y - bounds.minimum.y};
    state.focused = window->focused;
    if (cursor->position) {
        state.lastClient = {cursor->position->x, cursor->position->y};
    }
    // Inside the client as the original's PtInRect: a held button keeps reporting places outside the window.
    state.insideClient = cursor->position.has_value() && state.lastClient.x >= 0.0f &&
                         state.lastClient.y >= 0.0f && state.lastClient.x < clientSize.x &&
                         state.lastClient.y < clientSize.y;
    const bool focusLost = state.previousFocused && !state.focused;
    state.previousFocused = state.focused;

    // Unfocused, the original read nothing but the focus (InputSystem::Sample),
    // and the frame still went through the UI with that (Game::Impl::Update).
    const Keys pressed = state.focused ? ReadKeys(*keys) : Keys{};
    const bool mapped = input::CanMapPointer(bounds, clientSize);
    Vec2 position = mapped ? input::ClientToPlayfield(state.lastClient, bounds, clientSize) : state.lastClient;
    std::optional<Vec2> unwrapped;

    bool leftHeld = false;
    bool leftPressed = false;
    bool leftReleased = false;
    int wheelDelta = 0;
    if (state.focused) {
        leftHeld = mouse->Pressed(I::MouseButton::Left);
        leftPressed = mouse->JustPressed(I::MouseButton::Left) || (leftHeld && !state.previousLeftHeld);
        leftReleased = mouse->JustReleased(I::MouseButton::Left) || (!leftHeld && state.previousLeftHeld);
        wheelDelta = static_cast<int>(std::lround(scroll->delta.y * static_cast<float>(ui::kWheelDeltaPerPage)));
    }
    state.previousLeftHeld = leftHeld;

    if (state.focused && wrapPointer && mapped) {
        const input::PointerWrapResult wrapped = state.wrap.Sample(
            {state.lastClient, clientSize, Vec2{motion->delta.x, motion->delta.y}}, bounds);
        position = wrapped.position;
        unwrapped = wrapped.unwrappedPosition;
        if (wrapped.warpTo) {
            warp.Send(W::WarpCursor{{wrapped.warpTo->x, wrapped.warpTo->y}});
        }
    } else {
        state.wrap.Reset();
    }

    // The UI before GameFlow (Game::Impl::Update): the hit test, then the page arrows.
    const std::size_t puzzleCount = catalog->puzzles.GetPuzzles().size();
    const bool hasNext = world::HasNextPuzzle(*catalog, *session);
    const bool solvedMenuReady = session->solvedElapsedSeconds >= world::kSolvedMenuDelaySeconds;
    // The menus see the cursor only over the window (the original's mouse place followed the cursor outside it).
    const ui::UiPoint point = state.focused && state.insideClient
                                  ? ui::UiPoint{position.x, position.y}
                                  : ui::UiPoint{std::numeric_limits<float>::quiet_NaN(),
                                                std::numeric_limits<float>::quiet_NaN()};
    ui::MenuPages& menuPages = pages->pages;
    const std::optional<std::size_t> hoveredItem =
        menuPages.HitTest(screenBeforeInput, point, puzzleCount, hasNext, solvedMenuReady);
    const bool keyboardNavigated = pressed.previous || pressed.next;
    const bool activationRequested =
        pressed.enter || pressed.escape || focusLost || (leftPressed && hoveredItem.has_value());
    const std::optional<std::size_t> pageSelection = menuPages.ApplyLevelSelectNavigation(
        point, leftPressed, wheelDelta, keyboardNavigated, activationRequested, screenBeforeInput);
    const bool finalResultsPageChanged = menuPages.ApplyFinalResultsNavigation(
        point, leftPressed, wheelDelta, keyboardNavigated, activationRequested, screenBeforeInput);
    const bool pageNavigationHandled = pageSelection.has_value() || finalResultsPageChanged;

    world::FrameInput input;
    input.menu.previousPressed = pressed.previous && !pageNavigationHandled;
    input.menu.nextPressed = pressed.next && !pageNavigationHandled;
    input.menu.confirmPressed = pressed.enter && !pageNavigationHandled;
    input.menu.escapePressed = pressed.escape;
    input.menu.focusLost = focusLost;
    input.menu.mousePrimaryPressed = leftPressed && !pageNavigationHandled;
    input.menu.hoveredItem = pageSelection ? pageSelection : hoveredItem;

    input.pointer.position = position;
    input.pointer.unwrappedPosition = unwrapped;
    input.pointer.leftPressed = leftPressed;
    input.pointer.leftHeld = leftHeld;
    input.pointer.leftReleased = leftReleased;
    frame.Get() = input;
    return {};
}

// The end of the frame: the cursor (Game::Impl::FinishFrame), the pages as
// shown (the paging half of GameUiRenderer::Draw) and quitting.
K::SystemResult PresentMenus(K::Res<world::ScreenState> screen, K::Res<world::ScreenRequest> request,
                             K::Res<world::MenuSelection> selection, K::Res<world::Catalog> catalog,
                             K::Res<world::Session> session, K::Res<world::RunResults> results,
                             K::Res<world::FrameInput> frame, K::ResMut<PointerState> pointer,
                             K::ResMut<MenuPagesState> pages, K::ResMut<CursorAppearance> cursor,
                             K::ResMut<W::WindowRequest> window, K::MessageReader<world::QuitChosen> quit) {
    const GameScreen shown = world::EffectiveScreen(*screen, *request);
    const std::size_t puzzleCount = catalog->puzzles.GetPuzzles().size();
    const bool hasNext = world::HasNextPuzzle(*catalog, *session);
    const bool solvedMenuReady = session->solvedElapsedSeconds >= world::kSolvedMenuDelaySeconds;
    const bool dragging = session->board && session->board->IsDragging();

    if (!session->board || shown != GameScreen::Playing || !dragging) {
        pointer->wrap.Reset();
    }

    const ui::UiPoint point{frame->pointer.position.x, frame->pointer.position.y};
    const bool overAction = pointer.Read().insideClient &&
                            pages.Read().pages.IsPointerOverAction(shown, point, puzzleCount, hasNext, solvedMenuReady);
    CursorAppearance next{ResolveMouseCursorIcon(shown, dragging, overAction),
                          ShouldUseCustomMouseCursor(pointer.Read().focused, pointer.Read().insideClient)};
    if (cursor.Read().icon != next.icon || cursor.Read().custom != next.custom) {
        cursor.Get() = next;
    }

    pages->pages.Present(shown, selection->item, puzzleCount, results->summary.stages.size());

    if (!quit.Read().empty() && !window.Read().close) {
        window->close = true;
    }
    return {};
}

K::SystemOptions InUpdate(std::string name, std::vector<std::string> before, std::vector<std::string> after,
                          std::vector<K::ConditionRef> conditions) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = std::string(O::DefaultSchedules::frame);
    mount.stage = std::string(O::DefaultSchedules::update);
    mount.before = std::move(before);
    mount.after = std::move(after);
    mount.conditions = std::move(conditions);
    options.mounts.push_back(std::move(mount));
    return options;
}

} // namespace

void BuildRuntime(O::WorldSetup& setup) {
    world::BuildWorld(setup, world::WorldOptions{{O::AfterGenerating()}});
    K::Registrar& r = setup.Registrar();
    r.Resource<GameContent>({.name = "object_connect.GameContent"});
    r.Resource<UiText>({.name = "object_connect.UiText"});
    r.Resource<PointerState>({.name = "object_connect.PointerState"});
    r.Resource<MenuPagesState>({.name = "object_connect.MenuPages"});
    r.Resource<CursorAppearance>({.name = "object_connect.CursorAppearance"});
    r.AddSystem<&LoadGameData>(InUpdate("object_connect.LoadGameData", {}, {}, {O::WhileGenerating()}));
    r.AddSystem<&ReadPlayerInput>(InUpdate("object_connect.ReadPlayerInput", {"object_connect.CancelDragOnLeave"},
                                           {}, {O::AfterGenerating()}));
    r.AddSystem<&PresentMenus>(
        InUpdate("object_connect.PresentMenus", {}, {"object_connect.TickSolvedTimer"}, {O::AfterGenerating()}));
    setup.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(GameContent{});
        world.SetResource(UiText{});
        world.SetResource(PointerState{});
        world.SetResource(MenuPagesState{});
        world.SetResource(CursorAppearance{});
        return {};
    });
}

} // namespace object_connect::runtime
