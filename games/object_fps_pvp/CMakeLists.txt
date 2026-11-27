# Object_FPS_PVP: the networked FPS ported from master's apps/object_fps_pvp.
# The porting inventory is docs/porting-inventory.zh-Hant.md in this directory;
# the port's declaration is docs/port-declaration.zh-Hant.md.
#
# logic: the game's rules that need no World, window, input, renderer, audio
# device or network: the arena files and the baked voxel geometry, the
# movement, stance and combat rules, the planar movement input, the hit
# feedback and the client view, the sound cues and the Match's admission token
# (include/object_fps_pvp/match/AdmissionToken.hpp, the services' token/token.go in C++).
# character: the player's character for the Match and the Client: the body's skeleton, its clips retargeted from
# the animation library, the pose of a moment (the game's layering over the model library's sampling), the hit
# shapes on its bones and the third-person weapon's mount (include/object_fps_pvp/character/CharacterRig.hpp).
# world: the game in a GYO World, the same at the Match and every Client
# (include/object_fps_pvp/world/PvpWorld.hpp): the players' components and
# input, the fixed-step Systems on physics_3d, the server authority, the
# replication and the rollback the Client predicts with; the Match's saves through the save service
# (include/object_fps_pvp/world/MatchSaves.hpp, GYO::Http).
# client: the Client's World around the game's (include/object_fps_pvp/client/ClientWorld.hpp): the controls, the
# first-person camera, the arena's model and the other players' stand-in bodies (render's 3D pipeline), the HUD
# (its 2D layer), the sounds, and the World that draws (extracts from it).
# The client library also has the scenes (include/object_fps_pvp/client/Scenes.hpp: one Product, its Worlds replaced
# when the scene changes, the window open throughout; docs/game-flow-declaration.zh-Hant.md): the menu, the lobby
# and the room of the Gateway (its HTTP protocol v1: include/object_fps_pvp/client/Gateway.hpp; ui, world_ui,
# ui_render), the single player's room, the game.
# main: the Client (src/client_main.cpp) with the sdl_window, sdl_audio and sdl_gpu Plugins: the menu first; the
# scenes change between two frames of sdl_window's runner.
# match: the Match (src/match_main.cpp), headless, listening on UDP.
# services: the Gateway and the services behind it (docs/gateway-declaration.zh-Hant.md), a Go module of its own
# (go.mod here, the packages in services/): one executable, a subcommand per service. Native builds never need Go.
gyo_game_project()

gyo_game_add_library(logic OUT_TARGET object_fps_pvp_logic
    PUBLIC_LIBRARIES GYO::Math
    PRIVATE_LIBRARIES GYO::Json
    SOURCES
    src/data/JsonFields.cpp
    src/match/AdmissionToken.cpp
    src/match/LoadThreads.cpp
    src/match/PlayerSave.cpp
    src/match/SaveSync.cpp
    src/present/Controls.cpp
    src/present/HitFeedback.cpp
    src/present/Interpolation.cpp
    src/present/SoundCues.cpp
    src/sim/Arena.cpp
    src/sim/CombatRules.cpp
    src/sim/MovementRules.cpp
    src/sim/PlanarMovement.cpp
    src/sim/PlayerAnimation.cpp)
target_include_directories(${object_fps_pvp_logic} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

gyo_game_add_library(character OUT_TARGET object_fps_pvp_character
    PUBLIC_LIBRARIES ${object_fps_pvp_logic} GYO::Model
    PRIVATE_LIBRARIES GYO::Asset GYO::IO GYO::Json
    SOURCES
    src/character/CharacterRig.cpp)
target_include_directories(${object_fps_pvp_character} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

gyo_game_add_library(world OUT_TARGET object_fps_pvp_world
    PUBLIC_LIBRARIES ${object_fps_pvp_character} GYO::Kernel GYO::Orchestration GYO::WorldTime GYO::Rollback
        GYO::Authority GYO::EntityIds GYO::Net GYO::Transform GYO::Physics3d GYO::VoxelMap GYO::Asset GYO::WorldAsset GYO::Json
    PRIVATE_LIBRARIES GYO::IO GYO::Http
    SOURCES
    src/data/ArenaMap.cpp
    src/world/ArenaLoad.cpp
    src/world/GameplaySets.cpp
    src/world/MatchSaves.cpp
    src/world/Modes.cpp
    src/world/PvpWorld.cpp)
# The mode files are read with the game's JSON helpers (src/data/JsonFields.hpp, in the logic library).
target_include_directories(${object_fps_pvp_world} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

gyo_game_add_library(client OUT_TARGET object_fps_pvp_client
    PUBLIC_LIBRARIES ${object_fps_pvp_world} GYO::Asset GYO::Audio GYO::Camera GYO::Image GYO::Input GYO::Model GYO::Render
        GYO::Text GYO::Ui GYO::Window GYO::WorldAsset GYO::OS GYO::Http GYO::IO GYO::StateMachine
    PRIVATE_LIBRARIES GYO::Json GYO::RenderWorld GYO::MeshRender
    SOURCES
    src/client/ClientWorld.cpp
    src/client/Gateway.cpp
    src/client/GatewayScene.cpp
    src/client/LobbyScene.cpp
    src/client/LocalFiles.cpp
    src/client/RoomScene.cpp
    src/client/Scenes.cpp
    src/client/Settings.cpp
    src/client/SoloScene.cpp)
target_include_directories(${object_fps_pvp_client} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

gyo_game_add_executable(main MAIN OUT_TARGET object_fps_pvp_main
    LIBRARIES ${object_fps_pvp_client} GYO::OS GYO::Threads GYO::MeshRender
    SOURCES
    src/client_main.cpp)
gyo_product_units(${object_fps_pvp_main} MODULES physics_3d render_world world_render camera_render sprite_render mesh_render visibility
    world_ui ui_render
    PLUGINS sdl_window sdl_audio sdl_gpu)

gyo_game_add_executable(match OUT_TARGET object_fps_pvp_match
    LIBRARIES ${object_fps_pvp_world} GYO::OS GYO::Time GYO::Threads GYO::WorldAsset
    SOURCES
    src/match_main.cpp)
gyo_product_units(${object_fps_pvp_match} MODULES physics_3d)
# The Match is the product's second program: installed with it, next to the Client, where the Client's target stages
# the content both read.
gyo_install_product_target(TARGET ${object_fps_pvp_match} PRODUCT object_fps_pvp KIND game OWNER object_fps_pvp ROLE match)
add_dependencies(${object_fps_pvp_match} gyo_object_fps_pvp--content)

gyo_game_add_go_service(services PACKAGE ./services/cmd/services
    PLATFORMS windows-x64 linux-x64 macos-arm64 macos-x64)
