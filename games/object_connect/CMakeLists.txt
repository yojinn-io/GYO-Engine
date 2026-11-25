# Object_Connect: a 2D puzzle game ported from ~/Code/GitHome/Object_Connect (ca4482c).
# The porting inventory is docs/porting-inventory.zh-Hant.md in this directory.
#
# logic: the game's rules and simulation, with no window, input, renderer or audio
# device (the original object_connect_core). It links no Engine unit.
# world: the game in a GYO World (the screen state, Resources, Messages and the
# Systems of one frame; include/object_connect/world/ObjectConnectWorld.hpp).
# runtime: the game with a window: the content through asset, the player's
# input through input and window, the menus' pages, the cursor, the sound and
# the picture (render's 2D pass)
# (include/object_connect/runtime/RuntimeWorld.hpp).
# main: the program (src/main.cpp) with render_world and the sdl_window,
# sdl_audio and sdl_gpu Plugins.
gyo_game_project()

#
# fast_float (third_party/) is this product's own dependency: it replaces the
# floating-point std::from_chars the macOS 13.3 target lacks; its header appears
# only in src/adapters/fast_float/.
gyo_game_add_library(logic OUT_TARGET object_connect_logic
    PRIVATE_LIBRARIES gyo_fast_float
    SOURCES
    src/adapters/fast_float/DecimalToFloat.cpp
    src/data/Csv.cpp
    src/data/PuzzleCatalogLoader.cpp
    src/game/FinalResults.cpp
    src/game/GameFlow.cpp
    src/geometry/Geometry2D.cpp
    src/input/PointerWrap.cpp
    src/puzzle/PuzzleBoard.cpp
    src/tentacle/BloodTentacle.cpp
    src/tentacle/RibbonStrip.cpp
    src/ui/MenuLayout.cpp)

gyo_game_add_library(world OUT_TARGET object_connect_world
    PUBLIC_LIBRARIES ${object_connect_logic} GYO::Kernel GYO::Orchestration GYO::StateMachine GYO::WorldTime
    SOURCES
    src/world/ObjectConnectWorld.cpp)

gyo_game_add_library(runtime OUT_TARGET object_connect_runtime
    PUBLIC_LIBRARIES ${object_connect_world} GYO::Asset GYO::Audio GYO::Camera GYO::Image GYO::Input GYO::Render GYO::RenderWorld
        GYO::Text GYO::Window GYO::WorldAsset
    SOURCES
    src/runtime/Drawing.cpp
    src/runtime/GameData.cpp
    src/runtime/GameSound.cpp
    src/runtime/PlayfieldFit.cpp
    src/runtime/PuzzleDrawing.cpp
    src/runtime/UiDrawing.cpp
    src/runtime/RuntimeWorld.cpp)

gyo_game_add_executable(main MAIN OUT_TARGET object_connect_main
    LIBRARIES ${object_connect_runtime} GYO::OS GYO::Threads
    SOURCES
    src/main.cpp)
gyo_product_units(${object_connect_main} MODULES render_world PLUGINS sdl_window sdl_audio sdl_gpu)
