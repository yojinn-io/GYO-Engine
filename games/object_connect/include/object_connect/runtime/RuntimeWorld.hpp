#pragma once

#include "object_connect/game/MouseCursorPolicy.hpp"
#include "object_connect/input/PointerWrap.hpp"
#include "object_connect/math/Vec2.hpp"
#include "object_connect/runtime/GameData.hpp"
#include "object_connect/ui/MenuLayout.hpp"

#include "engine/asset/Handle.hpp"
#include "engine/image/ImageData.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/text/FontData.hpp"
#include "engine/text/TextContext.hpp"
#include "engine/text/TextStyle.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The game with a window: what the original Game::Impl and InputSystem did
// around the game's own Systems (object_connect/world/ObjectConnectWorld.hpp),
// on the Engine's input, window and asset Modules.
//
//   generating   LoadGameData       requests the content, holds generation until
//                                   it is Complete, then fills the Catalog and
//                                   the text context (a failure ends the World)
//   Main/Update  ReadPlayerInput    input and window state -> FrameInput (the
//                                   original InputSystem::Sample, the UI hit test
//                                   and the page arrows); a wrap asks WarpCursor
//                (the game's Systems, after generation)
//                PresentMenus       the pages and the cursor as the frame ends;
//                                   QuitChosen closes the window
//
// The window's settings (1280 x 720, not resizable) are the product's, in
// main.cpp; the drawing comes with the render batches and reads the Resources
// here and the game's.

namespace object_connect::runtime {

// The catalog ids of the content (assets/object_connect/asset_catalog.json).
inline constexpr std::string_view kLevelsId = "object_connect.levels";
inline constexpr std::string_view kNodesId = "object_connect.nodes";
inline constexpr std::string_view kUiFontId = "object_connect.font.ui";
inline constexpr std::array<std::string_view, 3> kCursorIds{
    "object_connect.cursor.normal", "object_connect.cursor.interactive", "object_connect.cursor.holding"};

// The content the game holds for its whole run, once generation has ended.
struct GameContent final {
    GYO::Engine::Asset::Handle<LevelTable> levels;
    GYO::Engine::Asset::Handle<NodeTable> nodes;
    GYO::Engine::Asset::Handle<GYO::Engine::Text::FontData> uiFont;
    // In MouseCursorIcon order.
    std::array<GYO::Engine::Asset::Handle<GYO::Engine::Image::ImageData>, 3> cursors;
    bool loaded = false;
};

// The fonts of the game's text, with the UI font in it once loaded.
struct UiText final {
    GYO::Engine::Text::TextContext context;
    std::optional<GYO::Engine::Text::FontId> font;
};

// What ReadPlayerInput keeps from frame to frame (the original InputSystem's members).
struct PointerState final {
    input::PointerWrap wrap;
    bool previousFocused = false;
    bool previousLeftHeld = false;
    // The last place the cursor had in the window (kept while it is outside).
    Vec2 lastClient{};
    bool focused = false;
    bool insideClient = false;
};

struct MenuPagesState final {
    ui::MenuPages pages;
};

// The cursor the game shows this frame (the original MouseCursorRenderer's
// input). Drawn by the render batch; until then the system cursor stays.
struct CursorAppearance final {
    MouseCursorIcon icon = MouseCursorIcon::Normal;
    // Focused and inside the window: the game's cursor replaces the system one.
    bool custom = false;
};

// Registers the Resources and Systems above, and the game's own World
// (BuildWorld) with AfterGenerating() on its Systems.
void BuildRuntime(GYO::Engine::Orchestration::WorldSetup& setup);

inline void HashValue(GYO::Kernel::Hasher&, const GameContent&) {}
inline void HashValue(GYO::Kernel::Hasher&, const UiText&) {}
inline void HashValue(GYO::Kernel::Hasher&, const PointerState&) {}
inline void HashValue(GYO::Kernel::Hasher&, const MenuPagesState&) {}
inline void HashValue(GYO::Kernel::Hasher&, const CursorAppearance&) {}

} // namespace object_connect::runtime
