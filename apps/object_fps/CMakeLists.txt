if(COMMAND gyo_app_requirements)
    gyo_app_requirements(COMPONENTS SDL_GPU SDL_INPUT SDL_IMAGE SDL_TTF UFBX)
    if(GYO_APP_DISCOVERY)
        return()
    endif()
endif()
if(APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "13.3")
    message(FATAL_ERROR "Object_FPS uses macOS 13.3 as its minimum supported deployment target")
endif()

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
    target_link_libraries(object_fps_domain PUBLIC GYO::Engine GYO::Ui GYO::Collision)
else()
    target_include_directories(
        object_fps_domain
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/include"
    )
endif()

if(TARGET GYO::Engine)
    add_library(object_fps_app_support STATIC
        src/App/AnimationSetDefinition.cpp
        src/App/CharacterPresentationDefinition.cpp
        src/App/CampaignContentLoader.cpp
        src/App/ObjectFpsPresentation.cpp
        src/App/WeaponViewModel.cpp
        src/App/WeaponPresentationDefinition.cpp
        src/App/ObjectFpsRuntimeClient.cpp
    )
    add_library(ObjectFPS::AppSupport ALIAS object_fps_app_support)
    target_include_directories(
        object_fps_app_support
        PUBLIC "${OBJECT_FPS_INCLUDE_DIR}"
    )
    target_link_libraries(
        object_fps_app_support
        PUBLIC ObjectFPS::Domain GYO::Engine GYO::Model GYO::Render
        PRIVATE
            GYO::Input
            GYO::InputBackendSDL
            GYO::PlatformSDL
            GYO::Text
            GYO::UiRenderer
            nlohmann_json::nlohmann_json
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
        add_executable(gyo_object_fps main.cpp diagnostics/MuzzleProbe.cpp diagnostics/HeadlessSmoke.cpp)
        target_link_libraries(
            gyo_object_fps
            PRIVATE
                ObjectFPS::AppSupport
                GYO::AssetSdlImage
                GYO::AssetUfbx
                GYO::InputBackendSDL
                GYO::PlatformSDL
                GYO::RenderBackendSDLGPU
                GYO::TextBackendSDLTTF
        )
        target_compile_features(gyo_object_fps PRIVATE cxx_std_20)
        target_compile_definitions(
            gyo_object_fps
            PRIVATE
                GYO_DEFAULT_GPU_DRIVER="${GYO_DEFAULT_GPU_DRIVER}"
                NOMINMAX
        )
        set_target_properties(gyo_object_fps PROPERTIES CXX_EXTENSIONS OFF)

        include("${PROJECT_SOURCE_DIR}/cmake/GyoShaders.cmake")
        gyo_add_shader_bundle(gyo_object_fps_shaders
            SPEC "${CMAKE_CURRENT_SOURCE_DIR}/shaders/bundle.json"
            OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/shaders/object_fps")

        # Stage content on every game build, including JSON/texture-only edits.
        # CMP0112 NEW keeps TARGET_FILE_DIR from creating the inverse target
        # dependency; gyo_object_fps can safely depend on this staging target.
        cmake_policy(PUSH)
        cmake_policy(SET CMP0112 NEW)
        add_custom_target(object_fps_assets
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:gyo_object_fps>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${OBJECT_FPS_ASSET_ROOT}"
                "$<TARGET_FILE_DIR:gyo_object_fps>/assets/object_fps"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${OBJECT_FPS_ASSET_ROOT}/../common"
                "$<TARGET_FILE_DIR:gyo_object_fps>/assets/common"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PROJECT_BINARY_DIR}/shaders/builtin"
                "$<TARGET_FILE_DIR:gyo_object_fps>/shaders/builtin"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${gyo_object_fps_shaders_DIRECTORY}"
                "$<TARGET_FILE_DIR:gyo_object_fps>/shaders/object_fps"
            DEPENDS gyo_builtin_shaders gyo_object_fps_shaders
            COMMENT "Staging Object_FPS, common assets and compiled shader bundles"
            VERBATIM
        )
        cmake_policy(POP)
        add_dependencies(gyo_object_fps object_fps_assets)
        gyo_register_app_package(TARGET gyo_object_fps STARTUP_ARGS --startup-smoke-test
            REQUIRED_FILES
                bin/assets/common/asset_catalog.json
                bin/assets/object_fps/asset_catalog.json
                bin/shaders/builtin/manifest.json
                bin/shaders/object_fps/manifest.json
                manual_gpu_smoke.py validate_content.py)
        gyo_add_app_package_check(NAME gameplay PROFILES release TIMEOUT 90
            COMMAND "@EXECUTABLE@" --headless-smoke-test)
        gyo_add_app_package_check(NAME content PROFILES release TIMEOUT 420
            COMMAND "@PYTHON@" "@PACKAGE_ROOT@/validate_content.py"
                --stage "@PACKAGE_ROOT@" --logs "@LOG_ROOT@/content")
        gyo_add_app_package_check(NAME rendering GPU PLATFORMS linux-x64 TIMEOUT 1100
            COMMAND "@PYTHON@" "@PACKAGE_ROOT@/manual_gpu_smoke.py"
                --package "@PACKAGE_ROOT@" --driver "@DRIVER@" --suite "@GPU_SUITE@"
                --timeout 120 --output "@LOG_ROOT@/rendering")
        install(PROGRAMS ci/manual_gpu_smoke.py ci/validate_content.py DESTINATION .)
        install(DIRECTORY docs/ DESTINATION docs OPTIONAL)
        install(DIRECTORY "${OBJECT_FPS_ASSET_ROOT}" "${OBJECT_FPS_ASSET_ROOT}/../common"
            DESTINATION bin/assets)
        install(DIRECTORY "${PROJECT_BINARY_DIR}/shaders/builtin/" DESTINATION bin/shaders/builtin)
        install(DIRECTORY "${gyo_object_fps_shaders_DIRECTORY}/" DESTINATION bin/shaders/object_fps)

        if(MSVC)
            target_compile_options(gyo_object_fps PRIVATE /W4 /permissive- /utf-8)
        else()
            target_compile_options(gyo_object_fps PRIVATE -Wall -Wextra -Wpedantic)
        endif()

        if(BUILD_TESTING)
            find_package(Python3 COMPONENTS Interpreter QUIET)
            if(Python3_Interpreter_FOUND)
                add_test(NAME object_fps.package_tools
                    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
                        -s "${CMAKE_CURRENT_SOURCE_DIR}/ci/tests" -v)
                set_tests_properties(object_fps.package_tools PROPERTIES LABELS cpu TIMEOUT 90)
            endif()
            add_test(NAME object_fps.startup_smoke COMMAND gyo_object_fps --startup-smoke-test)
            set_tests_properties(object_fps.startup_smoke PROPERTIES LABELS "cpu;smoke;startup" TIMEOUT 30
                WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
            add_test(NAME object_fps.headless_smoke COMMAND gyo_object_fps --headless-smoke-test)
            set_tests_properties(object_fps.headless_smoke PROPERTIES LABELS "cpu;smoke;gameplay" TIMEOUT 30
                WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
            add_test(NAME object_fps.smoke COMMAND gyo_object_fps --smoke-test)
            set_tests_properties(object_fps.smoke PROPERTIES TIMEOUT 30)
            add_test(
                NAME object_fps.menu_smoke
                COMMAND gyo_object_fps --menu-smoke-test
            )
            set_tests_properties(object_fps.menu_smoke PROPERTIES TIMEOUT 30)
            add_test(NAME object_fps.viewmodel_smoke COMMAND gyo_object_fps --viewmodel-smoke-test)
            set_tests_properties(object_fps.viewmodel_smoke PROPERTIES TIMEOUT 30)
            add_test(NAME object_fps.reload_smoke COMMAND gyo_object_fps --reload-smoke-test)
            set_tests_properties(object_fps.reload_smoke PROPERTIES TIMEOUT 60)
            add_test(NAME object_fps.muzzle_smoke COMMAND gyo_object_fps --muzzle-smoke-test)
            add_test(NAME object_fps.muzzle_smoke_4x3 COMMAND gyo_object_fps --muzzle-smoke-test --preview-4x3)
            add_test(NAME object_fps.muzzle_smoke_21x9 COMMAND gyo_object_fps --muzzle-smoke-test --preview-21x9)
            set_tests_properties(object_fps.muzzle_smoke object_fps.muzzle_smoke_4x3
                object_fps.muzzle_smoke_21x9 PROPERTIES TIMEOUT 60)
            add_test(NAME object_fps.shader_smoke COMMAND gyo_object_fps --shader-smoke-test)
            set_tests_properties(object_fps.smoke object_fps.menu_smoke
                object_fps.viewmodel_smoke object_fps.reload_smoke object_fps.muzzle_smoke
                object_fps.muzzle_smoke_4x3 object_fps.muzzle_smoke_21x9 object_fps.shader_smoke
                PROPERTIES LABELS gpu RUN_SERIAL TRUE)
            add_test(NAME object_fps.package COMMAND gyo_object_fps --validate-package)
            set_tests_properties(object_fps.package PROPERTIES LABELS "cpu;shader" TIMEOUT 60
                WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
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
    add_executable(object_fps_model_tests tests/Model/Mark23ModelTests.cpp)
    target_link_libraries(object_fps_model_tests PRIVATE GYO::AssetUfbx gyo_ufbx doctest::doctest)
    target_compile_definitions(object_fps_model_tests PRIVATE
        GYO_MARK23_FBX="${OBJECT_FPS_ASSET_ROOT}/weapons/mark23/viewmodel/Mark23.fbx")
    add_test(NAME object_fps.model_assets COMMAND object_fps_model_tests)

    add_executable(object_fps_headless_tests
        tests/TestMain.cpp
        tests/App/AssetPresentationDefinitionTests.cpp
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
        src/App/AnimationSetDefinition.cpp
        src/App/CharacterPresentationDefinition.cpp
        src/App/ObjectFpsPresentation.cpp
        src/App/WeaponViewModel.cpp
        src/App/WeaponPresentationDefinition.cpp
        src/App/CampaignContentLoader.cpp
    )
    target_link_libraries(object_fps_headless_tests PRIVATE
        GYO::Render
        GYO::Text
        GYO::UiRenderer
        GYO::Model
        GYO::AssetUfbx
        nlohmann_json::nlohmann_json
    )
    if(TARGET GYO::AssetSdlImage)
        target_link_libraries(object_fps_headless_tests PRIVATE GYO::AssetSdlImage)
        target_compile_definitions(object_fps_headless_tests PRIVATE OBJECT_FPS_TEST_SDL_IMAGE=1)
        gyo_copy_runtime_dlls(object_fps_headless_tests)
    endif()
    target_include_directories(object_fps_headless_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_include_directories(object_fps_headless_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests/support")
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
