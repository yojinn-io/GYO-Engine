# Object_FPS_PVP: the networked FPS ported from master's apps/object_fps_pvp.
# The porting inventory is docs/porting-inventory.zh-Hant.md in this directory.
#
# logic: the game's rules that need no window, input, renderer, audio device,
# physics or network: the planar movement input, the hit feedback and the
# client view, the sound cues and the lobby's address filter. It links no
# World-facing unit yet: the World layer, the Match and the Client come with
# the physics, model, window, input and render batches.
gyo_game_project()

gyo_game_add_library(logic OUT_TARGET object_fps_pvp_logic
    PUBLIC_LIBRARIES GYO::Math
    PRIVATE_LIBRARIES GYO::Json
    SOURCES
    src/data/JsonFields.cpp
    src/present/HitFeedback.cpp
    src/present/SoundCues.cpp
    src/sim/PlanarMovement.cpp)
target_include_directories(${object_fps_pvp_logic} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
