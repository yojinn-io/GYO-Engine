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
# client: the Client's World around the game's (include/object_fps_pvp/client/ClientWorld.hpp): the controls, the
# first-person camera, the arena's model and the other players' stand-in bodies (render's 3D pipeline), the HUD
# (its 2D layer), the sounds, and the World that draws (extracts from it).
# main: the Client (src/client_main.cpp) with the sdl_window, sdl_audio and sdl_gpu Plugins.
# match: the Match (src/match_main.cpp), headless, listening on UDP.
gyo_game_project()

gyo_game_add_library(logic OUT_TARGET object_fps_pvp_logic
    PUBLIC_LIBRARIES GYO::Math
    PRIVATE_LIBRARIES GYO::Json
    SOURCES
    src/data/JsonFields.cpp
    src/data/VoxelMeshFile.cpp
    src/present/Controls.cpp
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

gyo_game_add_library(client OUT_TARGET object_fps_pvp_client
    PUBLIC_LIBRARIES ${object_fps_pvp_world} GYO::Asset GYO::Audio GYO::Camera GYO::Image GYO::Input GYO::Model GYO::Render
        GYO::Text GYO::Window GYO::WorldAsset
    PRIVATE_LIBRARIES GYO::IO
    SOURCES
    src/client/ClientWorld.cpp
    src/client/VoxelModelLoader.cpp)

gyo_game_add_executable(main MAIN OUT_TARGET object_fps_pvp_main
    LIBRARIES ${object_fps_pvp_client} GYO::OS GYO::Threads
    SOURCES
    src/client_main.cpp)
gyo_product_units(${object_fps_pvp_main} MODULES physics_3d render_world world_render camera_render sprite_render mesh_render
    PLUGINS sdl_window sdl_audio sdl_gpu)

gyo_game_add_executable(match OUT_TARGET object_fps_pvp_match
    LIBRARIES ${object_fps_pvp_world} GYO::OS GYO::Time
    SOURCES
    src/match_main.cpp)
gyo_product_units(${object_fps_pvp_match} MODULES physics_3d)
# The Match is the product's second program: installed with it, next to the Client, where the Client's target stages
# the content both read.
gyo_install_product_target(TARGET ${object_fps_pvp_match} PRODUCT object_fps_pvp KIND game OWNER object_fps_pvp ROLE match)
add_dependencies(${object_fps_pvp_match} gyo_object_fps_pvp--content)
