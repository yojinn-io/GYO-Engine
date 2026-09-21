# The quality layer consumes product targets; the product never includes this file.
gyo_app_get_target(main OUT_TARGET app)
gyo_app_get_target(domain OUT_TARGET domain)
get_target_property(content_stage ${app} GYO_APP_CONTENT_STAGE_TARGET)
include("${CMAKE_CURRENT_LIST_DIR}/sources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/diagnostics/Diagnostics.cmake")
object_fps_enable_diagnostics()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    gyo_app_add_test(NAME package_tools TIMEOUT 90
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/package/tests" -v)
endif()
gyo_app_add_test(NAME startup_smoke COMMAND ${app} --startup-smoke-test
    LABELS cpu smoke startup TIMEOUT 30 WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
gyo_app_add_test(NAME headless_smoke COMMAND ${app} --headless-smoke-test
    LABELS cpu smoke gameplay TIMEOUT 30 WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
gyo_app_add_test(NAME package COMMAND ${app} --validate-package
    LABELS cpu shader TIMEOUT 60 WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")

foreach(probe smoke menu_smoke viewmodel_smoke reload_smoke muzzle_smoke shader_smoke)
    string(REPLACE "_" "-" flag "${probe}")
    gyo_app_add_test(NAME "${probe}" COMMAND ${app} "--${flag}-test"
        LABELS gpu TIMEOUT 60 RUN_SERIAL)
endforeach()
gyo_app_add_test(NAME muzzle_smoke_4x3 COMMAND ${app} --muzzle-smoke-test --preview-4x3
    LABELS gpu TIMEOUT 60 RUN_SERIAL)
gyo_app_add_test(NAME muzzle_smoke_21x9 COMMAND ${app} --muzzle-smoke-test --preview-21x9
    LABELS gpu TIMEOUT 60 RUN_SERIAL)

gyo_app_add_executable(model_tests OUT_TARGET model_tests
    SOURCES tests/Model/Mark23ModelTests.cpp
    LIBRARIES gyo_ufbx doctest::doctest COMPONENTS UFBX SDL_PLATFORM)
add_dependencies(${model_tests} ${content_stage})
gyo_app_add_test(NAME model_assets COMMAND ${model_tests}
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")

gyo_app_add_executable(headless_tests OUT_TARGET headless_tests
    SOURCES ${APP_HEADLESS_SOURCES} ${APP_HEADLESS_SUPPORT_SOURCES}
    LIBRARIES ${domain} GYO::Render GYO::Text GYO::UiRenderer
        GYO::Model nlohmann_json::nlohmann_json
    COMPONENTS UFBX SDL_IMAGE SDL_PLATFORM)
target_include_directories(${headless_tests} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${PROJECT_SOURCE_DIR}/tests/support")
target_compile_definitions(${headless_tests} PRIVATE OBJECT_FPS_TEST_SDL_IMAGE=1)
add_dependencies(${headless_tests} ${content_stage})
gyo_app_add_test(NAME headless COMMAND ${headless_tests}
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
