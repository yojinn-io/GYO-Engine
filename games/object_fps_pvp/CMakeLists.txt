# Object_FPS_PVP: the networked FPS ported from master's apps/object_fps_pvp.
# The porting inventory is docs/porting-inventory.zh-Hant.md in this directory;
# the port's declaration is docs/port-declaration.zh-Hant.md.
#
# logic: the game's rules that need no World, window, input, renderer, audio
# device or network: the arena files and the baked voxel geometry, the
# movement, stance and combat rules, the planar movement input, the hit
# feedback and the client view, the sound cues and the lobby's address filter.
# world: the game in a GYO World, the same at the Match and every Client
# (include/object_fps_pvp/world/PvpWorld.hpp): the players' components and
# input, the fixed-step Systems on physics_3d, the server authority, the
# replication and the rollback the Client predicts with.
# main: the Match (src/match_main.cpp), headless, listening on UDP.
gyo_game_project()

gyo_game_add_library(logic OUT_TARGET object_fps_pvp_logic
    PUBLIC_LIBRARIES GYO::Math
    PRIVATE_LIBRARIES GYO::Json
    SOURCES
    src/data/JsonFields.cpp
    src/data/VoxelMeshFile.cpp
    src/present/HitFeedback.cpp
    src/present/SoundCues.cpp
    src/sim/Arena.cpp
    src/sim/CombatRules.cpp
    src/sim/MovementRules.cpp
    src/sim/PlanarMovement.cpp)
target_include_directories(${object_fps_pvp_logic} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

gyo_game_add_library(world OUT_TARGET object_fps_pvp_world
    PUBLIC_LIBRARIES ${object_fps_pvp_logic} GYO::Kernel GYO::Orchestration GYO::WorldTime GYO::Rollback
        GYO::Authority GYO::EntityIds GYO::Net GYO::Transform GYO::Physics3d
    PRIVATE_LIBRARIES GYO::IO
    SOURCES
    src/world/PvpWorld.cpp)

gyo_game_add_executable(main MAIN OUT_TARGET object_fps_pvp_main
    LIBRARIES ${object_fps_pvp_world} GYO::OS GYO::Time
    SOURCES
    src/match_main.cpp)
gyo_product_units(${object_fps_pvp_main} MODULES physics_3d)
