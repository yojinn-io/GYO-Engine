if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(ObjectFPSDomain LANGUAGES CXX)
    include(CTest)
endif()

set(OBJECT_FPS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
get_filename_component(
    OBJECT_FPS_ASSET_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/object_fps"
    ABSOLUTE
)

add_library(object_fps_domain STATIC
    src/App/ObjectFpsUi.cpp
    src/Collision/CombatCollision.cpp
    src/Collision/GridCollision.cpp
    src/Data/Csv.cpp
    src/Data/GameData.cpp
    src/Game/CampaignRunState.cpp
    src/Game/CampaignContent.cpp
    src/Game/GameFlow.cpp
    src/Game/GameSession.cpp
    src/Gameplay/Combat/ProjectileSystem.cpp
    src/Gameplay/Enemy/EnemySpawnDirector.cpp
    src/Gameplay/Enemy/EnemySystem.cpp
    src/Gameplay/Player/PlanarMovement.cpp
    src/Gameplay/Player/Player.cpp
    src/Gameplay/Player/PlayerCombatState.cpp
    src/Gameplay/Player/PlayerController.cpp
    src/Gameplay/Player/PlayerSettings.cpp
    src/Gameplay/Weapon/WeaponController.cpp
    src/Rendering/MapGeometryGenerator.cpp
    src/World/GridMap.cpp
    src/World/GridMapLoader.cpp
    src/World/World.cpp
)
add_library(ObjectFPS::Domain ALIAS object_fps_domain)
target_include_directories(object_fps_domain PUBLIC "${OBJECT_FPS_INCLUDE_DIR}")
if(TARGET GYO::Engine)
    target_link_libraries(object_fps_domain PUBLIC GYO::Engine GYO::Ui)
else()
    target_include_directories(
        object_fps_domain
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/include"
    )
endif()

if(TARGET GYO::Engine)
    add_library(object_fps_app_support STATIC
        src/App/CampaignContentLoader.cpp
        src/App/ObjectFpsPresentation.cpp
        src/App/ObjectFpsRuntimeClient.cpp
    )
    add_library(ObjectFPS::AppSupport ALIAS object_fps_app_support)
    target_include_directories(
        object_fps_app_support
        PUBLIC "${OBJECT_FPS_INCLUDE_DIR}"
    )
    target_link_libraries(
        object_fps_app_support
        PUBLIC ObjectFPS::Domain GYO::Engine
        PRIVATE
            GYO::Input
            GYO::InputBackendSDL
            GYO::PlatformSDL
            GYO::Render
            GYO::Text
            GYO::UiRenderer
    )
    target_compile_features(object_fps_app_support PUBLIC cxx_std_20)
    set_target_properties(object_fps_app_support PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(object_fps_app_support PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(object_fps_app_support PRIVATE NOMINMAX)
    else()
        target_compile_options(object_fps_app_support PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(TARGET GYO::AssetSdlImage AND
       TARGET GYO::RenderBackendSDLGPU AND
       TARGET GYO::TextBackendSDLTTF)
        add_executable(gyo_object_fps main.cpp)
        target_link_libraries(
            gyo_object_fps
            PRIVATE
                ObjectFPS::AppSupport
                GYO::AssetSdlImage
                GYO::InputBackendSDL
                GYO::PlatformSDL
                GYO::RenderBackendSDLGPU
                GYO::TextBackendSDLTTF
        )
        target_compile_features(gyo_object_fps PRIVATE cxx_std_20)
        target_compile_definitions(
            gyo_object_fps
            PRIVATE
                OBJECT_FPS_ASSET_ROOT="${OBJECT_FPS_ASSET_ROOT}"
                NOMINMAX
        )
        set_target_properties(gyo_object_fps PROPERTIES CXX_EXTENSIONS OFF)

        add_custom_command(TARGET gyo_object_fps POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:gyo_object_fps>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${OBJECT_FPS_ASSET_ROOT}"
                "$<TARGET_FILE_DIR:gyo_object_fps>/assets/object_fps"
        )
        gyo_copy_runtime_dlls(gyo_object_fps)

        if(MSVC)
            target_compile_options(gyo_object_fps PRIVATE /W4 /permissive- /utf-8)
        else()
            target_compile_options(gyo_object_fps PRIVATE -Wall -Wextra -Wpedantic)
        endif()

        if(BUILD_TESTING)
            add_test(NAME object_fps.smoke COMMAND gyo_object_fps --smoke-test)
            set_tests_properties(object_fps.smoke PROPERTIES TIMEOUT 30)
            add_test(
                NAME object_fps.menu_smoke
                COMMAND gyo_object_fps --menu-smoke-test
            )
            set_tests_properties(object_fps.menu_smoke PROPERTIES TIMEOUT 30)
        endif()
    endif()
endif()
target_compile_features(object_fps_domain PUBLIC cxx_std_20)
set_target_properties(object_fps_domain PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(object_fps_domain PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(object_fps_domain PRIVATE NOMINMAX)
else()
    target_compile_options(object_fps_domain PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(object_fps_headless_tests
        tests/TestMain.cpp
        tests/App/ObjectFpsPresentationTests.cpp
        tests/App/ObjectFpsUiTests.cpp
        tests/Collision/CollisionTests.cpp
        tests/Collision/CombatCollisionTests.cpp
        tests/Data/GameDataCatalogTests.cpp
        tests/Game/CampaignRunStateTests.cpp
        tests/Game/GameFlowTests.cpp
        tests/Game/GameSessionTests.cpp
        tests/Gameplay/EnemySpawnDirectorTests.cpp
        tests/Gameplay/EnemySystemTests.cpp
        tests/Gameplay/PlayerCombatStateTests.cpp
        tests/Gameplay/PlayerControllerTests.cpp
        tests/Gameplay/ProjectileSystemTests.cpp
        tests/Gameplay/WeaponControllerTests.cpp
        tests/Rendering/EnemyBillboardTests.cpp
        tests/Rendering/MapGeometryTests.cpp
        tests/World/WorldTests.cpp
    )
    target_link_libraries(object_fps_headless_tests PRIVATE ObjectFPS::Domain)
    target_sources(object_fps_headless_tests PRIVATE
        src/App/ObjectFpsPresentation.cpp
    )
    target_link_libraries(object_fps_headless_tests PRIVATE
        GYO::Render
        GYO::Text
        GYO::UiRenderer
    )
    target_include_directories(object_fps_headless_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(object_fps_headless_tests PRIVATE cxx_std_20)
    target_compile_definitions(
        object_fps_headless_tests
        PRIVATE RETROFPS_TEST_RESOURCE_ROOT="${OBJECT_FPS_ASSET_ROOT}"
    )
    set_target_properties(object_fps_headless_tests PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(object_fps_headless_tests PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(object_fps_headless_tests PRIVATE NOMINMAX)
    else()
        target_compile_options(object_fps_headless_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME object_fps.headless COMMAND object_fps_headless_tests)
endif()
