cmake_minimum_required(VERSION 3.30)
get_filename_component(repository "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(work "${CMAKE_CURRENT_BINARY_DIR}/app-project-tests")
set(source "${work}/source")
set(binary "${work}/build")
set(stage "${work}/stage")
file(MAKE_DIRECTORY "${source}/config/engine" "${source}/assets/common"
    "${source}/assets/orbit_demo" "${source}/assets/off"
    "${source}/content/puzzle assets" "${source}/no-catalog")
set(header "name,description,version,enabled,windows,linux,macos\n")
file(WRITE "${source}/config/engine/projects.csv"
    "${header}orbit_demo,First arbitrary app,,1,1,1,1\npuzzle_lab,Second arbitrary app,,1,1,1,1\n")
file(WRITE "${source}/assets/common/asset_catalog.json" "{}\n")
file(WRITE "${source}/assets/common/shared.txt" "shared\n")
file(WRITE "${source}/assets/orbit_demo/asset_catalog.json" "{}\n")
file(WRITE "${source}/assets/orbit_demo/marker.txt" "orbit_demo:original\n")
file(WRITE "${source}/assets/off/asset_catalog.json" "{}\n")
file(WRITE "${source}/assets/off/marker.txt" "off:original\n")
file(WRITE "${source}/content/puzzle assets/asset_catalog.json" "{}\n")
file(WRITE "${source}/content/puzzle assets/marker.txt" "puzzle_lab:original\n")
file(WRITE "${source}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.30)
project(AppProjectFixture LANGUAGES CXX)
include(CTest)
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif()
set(GYO_BUILD_UI_EDITOR OFF)
include("${GYO_TEST_REPOSITORY}/cmake/GyoApps.cmake")
include("${GYO_TEST_REPOSITORY}/cmake/GyoAppProject.cmake")
include("${GYO_TEST_REPOSITORY}/cmake/GyoAppTesting.cmake")
include("${GYO_TEST_REPOSITORY}/cmake/GyoAppPackaging.cmake")

# Supply only the component interface needed by this fixture. A compiler error
# proves the app helpers failed to link it; no engine dependencies are downloaded.
if(NOT CASE STREQUAL missing_component)
    add_library(fixture_ufbx INTERFACE)
    target_compile_definitions(fixture_ufbx INTERFACE FIXTURE_UFBX_LINKED=1)
    add_library(GYO::AssetUfbx ALIAS fixture_ufbx)
endif()
add_library(fixture_public INTERFACE)
target_compile_definitions(fixture_public INTERFACE FIXTURE_PUBLIC_LINKED=1)
add_library(fixture_private INTERFACE)
target_compile_definitions(fixture_private INTERFACE FIXTURE_PRIVATE_LINKED=1)
if(CASE STREQUAL false_id OR CASE STREQUAL duplicate_context)
    file(WRITE "${PROJECT_BINARY_DIR}/mock_sdl.cpp" "int mock_sdl() { return 0; }\n")
    add_library(SDL3-shared SHARED "${PROJECT_BINARY_DIR}/mock_sdl.cpp")
    set_target_properties(SDL3-shared PROPERTIES OUTPUT_NAME FalseLikeSDL WINDOWS_EXPORT_ALL_SYMBOLS ON)
    add_library(GYO::PlatformSDL ALIAS SDL3-shared)
    file(GENERATE OUTPUT "${PROJECT_BINARY_DIR}/sdl-$<CONFIG>.txt" CONTENT "$<TARGET_FILE_NAME:SDL3-shared>")
endif()

# Exercise real deployment and dependency wiring with tiny prebuilt shader
# bundles, leaving shader compiler correctness to its own test suite.
function(gyo_add_shader_bundle target)
    cmake_parse_arguments(BUNDLE "" "SPEC;OUTPUT_DIRECTORY" "" ${ARGN})
    if(NOT EXISTS "${BUNDLE_SPEC}")
        message(FATAL_ERROR "Fixture shader spec was not resolved relative to the app")
    endif()
    add_custom_command(OUTPUT "${BUNDLE_OUTPUT_DIRECTORY}/manifest.json"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${BUNDLE_OUTPUT_DIRECTORY}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${BUNDLE_SPEC}" "${BUNDLE_OUTPUT_DIRECTORY}/manifest.json"
        DEPENDS "${BUNDLE_SPEC}" VERBATIM)
    add_custom_target(${target} DEPENDS "${BUNDLE_OUTPUT_DIRECTORY}/manifest.json")
    set(${target}_DIRECTORY "${BUNDLE_OUTPUT_DIRECTORY}" PARENT_SCOPE)
    set(${target}_MANIFEST "${BUNDLE_OUTPUT_DIRECTORY}/manifest.json" PARENT_SCOPE)
endfunction()
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/shaders/builtin")
file(WRITE "${PROJECT_BINARY_DIR}/shaders/builtin/manifest.json" "{}\n")
add_custom_target(gyo_builtin_shaders)
gyo_configure_apps()
gyo_resolve_components(${GYO_REQUESTED_COMPONENTS})
foreach(GYO_CURRENT_APP IN LISTS GYO_ACTIVE_APPS)
    add_subdirectory("apps/${GYO_CURRENT_APP}")
endforeach()
gyo_finalize_app_installation()
gyo_finalize_app_packages()
file(WRITE "${CMAKE_BINARY_DIR}/selection.txt" "${GYO_ACTIVE_APPS}")
]=])

set(app_cmake [=[
if(CASE STREQUAL undeclared_component)
    gyo_app_requirements()
elseif(CASE STREQUAL false_id OR CASE STREQUAL duplicate_context)
    gyo_app_requirements(COMPONENTS UFBX SDL_PLATFORM)
else()
    gyo_app_requirements(COMPONENTS UFBX)
endif()
if(GYO_APP_DISCOVERY)
    return()
endif()
set(project_options DISPLAY_NAME "Fixture ${GYO_CURRENT_APP}")
if(CASE STREQUAL context_mismatch)
    set(GYO_CURRENT_APP mismatched)
elseif(CASE STREQUAL missing_assets OR CASE STREQUAL no_content)
    list(APPEND project_options ASSET_ROOT "missing-assets")
elseif(CASE STREQUAL missing_catalog)
    list(APPEND project_options ASSET_ROOT "../../no-catalog")
elseif(GYO_CURRENT_APP STREQUAL puzzle_lab)
    list(APPEND project_options ASSET_ROOT "../../content/puzzle assets")
endif()
gyo_app_project(${project_options})
if(CASE STREQUAL duplicate_context)
    gyo_app_project()
endif()
if(NOT GYO_APP_ID STREQUAL GYO_CURRENT_APP OR NOT IS_DIRECTORY "${GYO_APP_GENERATED_INCLUDE_DIR}")
    message(FATAL_ERROR "App identity/generated include directory not exported")
endif()
if(GYO_APP_ID STREQUAL puzzle_lab AND NOT CASE MATCHES "^(missing_assets|missing_catalog|no_content)$")
    cmake_path(NORMAL_PATH GYO_APP_ASSET_ROOT OUTPUT_VARIABLE actual_assets)
    cmake_path(SET expected_assets NORMALIZE "${PROJECT_SOURCE_DIR}/content/puzzle assets")
    if(NOT actual_assets STREQUAL expected_assets)
        message(FATAL_ERROR "Relative asset override did not use the app directory")
    endif()
endif()
if(CASE STREQUAL invalid_role)
    gyo_app_add_library(bad-role OUT_TARGET domain SOURCES domain.cpp)
endif()
gyo_app_add_library(domain OUT_TARGET domain SOURCES domain.cpp
    PUBLIC_LIBRARIES fixture_public PRIVATE_LIBRARIES fixture_private
    PRIVATE_COMPONENTS UFBX)
if(NOT domain STREQUAL "gyo_${GYO_APP_ID}-domain")
    message(FATAL_ERROR "Library target does not derive from app identity: ${domain}")
endif()
get_target_property(public_includes ${domain} INTERFACE_INCLUDE_DIRECTORIES)
string(FIND "${public_includes}" "${GYO_APP_GENERATED_INCLUDE_DIR}" leaked_include)
if(NOT leaked_include EQUAL -1)
    message(FATAL_ERROR "AppConfig.hpp leaked through the library's public interface")
endif()
if(CASE STREQUAL duplicate_role)
    gyo_app_add_library(domain OUT_TARGET duplicate SOURCES domain.cpp)
endif()
# A caller may naturally store its executable in 'app', as Object_FPS does.
# Package field names must not resolve through this caller-scope variable.
gyo_app_add_executable(game MAIN OUT_TARGET app SOURCES main.cpp LIBRARIES ${domain} COMPONENTS UFBX)
set(main "${app}")
if(CASE STREQUAL false_id)
    gyo_app_link_components(${main} PRIVATE SDL_PLATFORM)
    target_compile_definitions(${main} PRIVATE FIXTURE_REQUIRE_SDL=1)
endif()
gyo_app_add_library(public_component OUT_TARGET public_component SOURCES interface.cpp PUBLIC_COMPONENTS UFBX)
gyo_app_add_executable(probe OUT_TARGET probe SOURCES probe.cpp LIBRARIES ${domain} ${public_component})
if(NOT main STREQUAL "gyo_${GYO_APP_ID}" OR NOT probe STREQUAL "gyo_${GYO_APP_ID}-probe")
    message(FATAL_ERROR "Executable targets do not derive from app identity")
endif()
if(CASE STREQUAL unknown_component)
    gyo_app_link_components(${main} PRIVATE NOT_A_COMPONENT)
endif()
set(staging stale_output)
if(CASE STREQUAL no_content)
    gyo_app_deploy_content(TARGET ${main} OUT_REQUIRED_FILES required OUT_STAGE_TARGET staging)
    if(NOT required STREQUAL "" OR NOT staging STREQUAL "")
        message(FATAL_ERROR "A content-free app acquired required files or a staging target")
    endif()
    set(startup --identity)
    gyo_register_app_package(TARGET ${main} STARTUP_ARGS ${startup})
else()
    gyo_app_deploy_content(TARGET ${main} ASSETS COMMON_ASSETS
        SHADER_SPEC shaders/bundle.json BUILTIN_SHADERS
        OUT_REQUIRED_FILES required OUT_STAGE_TARGET staging)
    if(NOT TARGET "${staging}")
        message(FATAL_ERROR "Content deployment did not export its staging target")
    endif()
    add_dependencies(${probe} ${staging})
    set(startup --startup)
    gyo_register_app_package(TARGET ${main} STARTUP_ARGS ${startup})
endif()
if(CASE STREQUAL duplicate_destination)
    gyo_app_deploy_content(TARGET ${main} ASSETS OUT_REQUIRED_FILES duplicate_required)
endif()
gyo_app_add_test(NAME startup COMMAND ${main} ${startup}
    LABELS fixture cpu TIMEOUT 20 WORKING_DIRECTORY "$<TARGET_FILE_DIR:${main}>")
gyo_app_add_test(NAME argv COMMAND ${main} --argv "a;b" "" "space here" ""
    ENVIRONMENT "GYO_FIXTURE=present" RUN_SERIAL LABELS fixture TIMEOUT 20)
if(CASE STREQUAL duplicate_test)
    gyo_app_add_test(NAME startup COMMAND ${main} --startup)
endif()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/${GYO_APP_ID}-$<CONFIG>.txt" CONTENT "$<TARGET_FILE:${main}>")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/${GYO_APP_ID}-probe-$<CONFIG>.txt" CONTENT "$<TARGET_FILE:${probe}>")
]=])
set(domain_cpp [=[
#include <domain.hpp>
#include <gyo/AppConfig.hpp>
#ifndef FIXTURE_UFBX_LINKED
#error App component interface is missing
#endif
#ifndef FIXTURE_PUBLIC_LINKED
#error Public library interface is missing
#endif
#ifndef FIXTURE_PRIVATE_LINKED
#error Private library interface is missing
#endif
consteval int identity_version() { return 20; }
const char* domain_identity() { static_assert(identity_version() == 20); return Gyo::AppConfig::Id; }
]=])
set(main_cpp [=[
#include <domain.hpp>
#include <gyo/AppConfig.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#ifndef FIXTURE_UFBX_LINKED
#error Executable component interface is missing
#endif
#ifndef FIXTURE_PUBLIC_LINKED
#error Library public interface did not reach its consumer
#endif
#ifdef FIXTURE_PRIVATE_LINKED
#error Library private interface leaked into its consumer
#endif
#ifdef FIXTURE_REQUIRE_SDL
extern int mock_sdl();
#endif
int main(int argc, char** argv) {
    using namespace std::literals;
    using namespace Gyo::AppConfig;
#ifdef FIXTURE_REQUIRE_SDL
    if (mock_sdl() != 0) return 8;
#endif
    if (argc > 1 && argv[1] == "--argv"sv) {
        return argc == 6 && argv[2] == "a;b"sv && argv[3] == ""sv &&
            argv[4] == "space here"sv && argv[5] == ""sv && std::getenv("GYO_FIXTURE") ? 0 : 3;
    }
    if (Id != "@app@"sv || DisplayName != "Fixture @app@"sv || domain_identity() != "@app@"sv ||
        Assets != "assets/@app@"sv || Shaders != "shaders/@app@"sv ||
        CommonAssets != "assets/common"sv || BuiltinShaders != "shaders/builtin"sv) return 4;
    if (argc > 1 && argv[1] == "--identity"sv) return 0;
    const auto root = std::filesystem::absolute(argv[0]).parent_path();
    for (const auto& file : {std::string(Assets) + "/asset_catalog.json", std::string(Shaders) + "/manifest.json",
             std::string(CommonAssets) + "/asset_catalog.json", std::string(BuiltinShaders) + "/manifest.json"}) {
        if (!std::filesystem::is_regular_file(root / file)) { std::cerr << file << " missing\n"; return 5; }
    }
    std::ifstream marker(root / Assets / "marker.txt");
    std::string text;
    std::getline(marker, text);
    if (!text.starts_with("@app@:")) return 6;
    if (argc > 2 && argv[1] == "--marker"sv && text != argv[2]) return 7;
    return 0;
}
]=])
foreach(app orbit_demo puzzle_lab off)
    file(MAKE_DIRECTORY "${source}/apps/${app}/include" "${source}/apps/${app}/shaders")
    file(WRITE "${source}/apps/${app}/CMakeLists.txt" "${app_cmake}")
    file(WRITE "${source}/apps/${app}/include/domain.hpp" "#pragma once\nconst char* domain_identity();\n")
    file(WRITE "${source}/apps/${app}/domain.cpp" "${domain_cpp}")
    file(WRITE "${source}/apps/${app}/interface.cpp" "int interface_fixture() { return 0; }\n")
    string(CONFIGURE "${main_cpp}" configured_main @ONLY)
    file(WRITE "${source}/apps/${app}/main.cpp" "${configured_main}")
    file(WRITE "${source}/apps/${app}/probe.cpp"
        "#include <domain.hpp>\n#include <gyo/AppConfig.hpp>\n#include <string_view>\n#ifndef FIXTURE_UFBX_LINKED\n#error Public component did not reach consumer\n#endif\nint main() { return std::string_view(domain_identity()) == Gyo::AppConfig::Id ? 0 : 1; }\n")
    file(WRITE "${source}/apps/${app}/shaders/bundle.json" "{\"app\":\"${app}\"}\n")
endforeach()

set(generator_args -DBUILD_TESTING=ON -DGYO_ENABLE_PACKAGING=ON)
if(GENERATOR)
    list(APPEND generator_args -G "${GENERATOR}")
endif()
if(MAKE_PROGRAM)
    list(APPEND generator_args "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif()
if(PLATFORM)
    list(APPEND generator_args -A "${PLATFORM}")
endif()
if(CXX_COMPILER)
    list(APPEND generator_args "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
endif()
function(run expected fragment)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 90)
    if(expected AND NOT result EQUAL 0)
        message(FATAL_ERROR "Command failed (${result}): ${ARGN}\n${stdout}\n${stderr}")
    elseif(NOT expected AND result EQUAL 0)
        message(FATAL_ERROR "Invalid app project was accepted: ${ARGN}")
    endif()
    if(NOT fragment STREQUAL "" AND NOT "${stdout}\n${stderr}" MATCHES "${fragment}")
        message(FATAL_ERROR "Expected '${fragment}':\n${stdout}\n${stderr}")
    endif()
endfunction()

function(check_tests directory)
    execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${directory}" -C Debug --show-only=json-v1
        RESULT_VARIABLE result OUTPUT_VARIABLE listing ERROR_VARIABLE stderr TIMEOUT 30)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Could not inspect app tests: ${stderr}")
    endif()
    set(expected "")
    foreach(app IN LISTS ARGN)
        list(APPEND expected "${app}.argv" "${app}.startup")
    endforeach()
    list(SORT expected)
    set(actual "")
    string(JSON count LENGTH "${listing}" tests)
    if(count GREATER 0)
        math(EXPR last "${count}-1")
        foreach(index RANGE 0 ${last})
            string(JSON name GET "${listing}" tests ${index} name)
            list(APPEND actual "${name}")
            set(has_timeout FALSE)
            set(has_label FALSE)
            set(has_serial FALSE)
            string(JSON property_count LENGTH "${listing}" tests ${index} properties)
            math(EXPR property_last "${property_count}-1")
            foreach(property_index RANGE 0 ${property_last})
                string(JSON property GET "${listing}" tests ${index} properties ${property_index} name)
                string(JSON value GET "${listing}" tests ${index} properties ${property_index} value)
                if(property STREQUAL TIMEOUT AND value EQUAL 20)
                    set(has_timeout TRUE)
                elseif(property STREQUAL LABELS AND value MATCHES "fixture")
                    set(has_label TRUE)
                elseif(property STREQUAL RUN_SERIAL AND value)
                    set(has_serial TRUE)
                endif()
            endforeach()
            if(NOT has_timeout OR NOT has_label OR (name MATCHES "\\.argv$" AND NOT has_serial))
                message(FATAL_ERROR "App test properties did not propagate: ${name}: ${listing}")
            endif()
        endforeach()
    endif()
    list(SORT actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected app tests '${expected}', got '${actual}'")
    endif()
endfunction()

run(TRUE "" "${CMAKE_COMMAND}" -S "${source}" -B "${binary}" ${generator_args}
    "-DGYO_TEST_REPOSITORY=${repository}" -DGYO_APPS=AUTO -DGYO_RENDER_DEVICE=NONE -DCMAKE_BUILD_TYPE=Debug -DCASE=)

# Tests consume deployed content without linking or building the main program.
# Remove only these fixture outputs so reused build trees prove the dependency
# as strictly as a fresh build (including the asset-root override with spaces).
foreach(app orbit_demo puzzle_lab)
    file(READ "${binary}/${app}-Debug.txt" executable)
    file(READ "${binary}/${app}-probe-Debug.txt" probe)
    cmake_path(IS_PREFIX work "${executable}" NORMALIZE inside_fixture)
    if(NOT inside_fixture)
        message(FATAL_ERROR "Refusing to remove an executable outside the fixture: ${executable}")
    endif()
    get_filename_component(executable_directory "${executable}" DIRECTORY)
    set(marker "${executable_directory}/assets/${app}/marker.txt")
    file(REMOVE "${executable}" "${marker}")
    run(TRUE "" "${CMAKE_COMMAND}" --build "${binary}" --config Debug --target "gyo_${app}-probe" --parallel 2)
    if(EXISTS "${executable}" OR NOT EXISTS "${marker}")
        message(FATAL_ERROR "Building ${app}'s probe did not stage content independently of its main executable")
    endif()
    file(READ "${marker}" marker_content)
    if(NOT marker_content STREQUAL "${app}:original\n")
        message(FATAL_ERROR "Building ${app}'s probe staged incorrect content: ${marker_content}")
    endif()
    run(TRUE "" "${probe}")
endforeach()
run(TRUE "" "${CMAKE_COMMAND}" --build "${binary}" --config Debug --parallel 2)
check_tests("${binary}" orbit_demo puzzle_lab)
run(TRUE "" "${CMAKE_CTEST_COMMAND}" --test-dir "${binary}" -C Debug --output-on-failure --no-tests=error)
run(TRUE "" "${CMAKE_COMMAND}" --install "${binary}" --config Debug --prefix "${stage}")
foreach(app orbit_demo puzzle_lab)
    file(READ "${binary}/${app}-Debug.txt" executable)
    file(READ "${binary}/${app}-probe-Debug.txt" probe)
    run(TRUE "" "${executable}" --startup)
    run(TRUE "" "${probe}")
    file(READ "${stage}/share/gyo/apps/${app}/manifest.json" manifest)
    string(JSON identity GET "${manifest}" app)
    string(JSON package_executable GET "${manifest}" executable)
    if(NOT identity STREQUAL app)
        message(FATAL_ERROR "Installed app identity mismatch: ${identity}")
    endif()
    string(JSON required_count LENGTH "${manifest}" required_files)
    if(NOT required_count EQUAL 5)
        message(FATAL_ERROR "Expected executable and four required content manifests: ${manifest}")
    endif()
    foreach(index RANGE 0 4)
        string(JSON required GET "${manifest}" required_files ${index})
        if(NOT EXISTS "${stage}/${required}")
            message(FATAL_ERROR "Declared package file does not exist: ${required}")
        endif()
    endforeach()
    if(NOT EXISTS "${binary}/apps/${app}/shaders/manifest.json")
        message(FATAL_ERROR "App shader output was not isolated in the app binary directory")
    endif()
    file(READ "${stage}/bin/shaders/${app}/manifest.json" shader_manifest)
    string(JSON shader_identity GET "${shader_manifest}" app)
    if(NOT shader_identity STREQUAL app)
        message(FATAL_ERROR "Installed shader bundle belongs to a different app: ${shader_identity}")
    endif()
    run(TRUE "" "${stage}/${package_executable}" --startup)
endforeach()

# Content-only changes must stage even when no source needs recompilation.
file(WRITE "${source}/assets/orbit_demo/marker.txt" "orbit_demo:updated\n")
run(TRUE "" "${CMAKE_COMMAND}" --build "${binary}" --config Debug --target gyo_orbit_demo)
file(READ "${binary}/orbit_demo-Debug.txt" executable)
run(TRUE "" "${executable}" --marker orbit_demo:updated)

# The same role names are valid in different apps; CSV alone removes one graph.
file(WRITE "${source}/config/engine/projects.csv" "${header}orbit_demo,,,0,1,1,1\npuzzle_lab,,,1,1,1,1\n")
run(TRUE "" "${CMAKE_COMMAND}" --build "${binary}" --config Debug)
file(READ "${binary}/selection.txt" selected)
if(NOT selected STREQUAL puzzle_lab)
    message(FATAL_ERROR "CSV regeneration retained the removed app: ${selected}")
endif()
run(FALSE "" "${CMAKE_COMMAND}" --build "${binary}" --config Debug --target gyo_orbit_demo)
check_tests("${binary}" puzzle_lab)
run(TRUE "" "${CMAKE_CTEST_COMMAND}" --test-dir "${binary}" -C Debug --output-on-failure --no-tests=error)

# Reuse the compiler detection cache, but create every declaration anew on each
# configure so earlier successful declarations cannot conceal a contract error.
set(missing_component_error "Required component UFBX target .* is unavailable")
set(undeclared_component_error "did not declare component UFBX")
set(unknown_component_error "Unknown GYO component 'NOT_A_COMPONENT'")
set(missing_assets_error "content directory is missing")
set(missing_catalog_error "required content is missing")
set(invalid_role_error "Invalid app target role")
set(duplicate_role_error "Duplicate app target role")
set(duplicate_test_error "Duplicate app test")
set(duplicate_destination_error "Duplicate app deployment destination")
set(context_mismatch_error "requires the CSV-selected app directory")
foreach(case missing_component undeclared_component unknown_component missing_assets missing_catalog
        invalid_role duplicate_role duplicate_test duplicate_destination context_mismatch)
    run(FALSE "${${case}_error}" "${CMAKE_COMMAND}" -S "${source}" -B "${binary}" -DGYO_APPS=puzzle_lab "-DCASE=${case}")
endforeach()

# Content is opt-in: a nonexistent asset root is valid if nothing deploys it.
set(bare_binary "${work}/without-content")
set(bare_stage "${work}/without-content-stage")
run(TRUE "" "${CMAKE_COMMAND}" -S "${source}" -B "${bare_binary}" ${generator_args}
    "-DGYO_TEST_REPOSITORY=${repository}" -DGYO_APPS=puzzle_lab -DGYO_RENDER_DEVICE=NONE -DCMAKE_BUILD_TYPE=Debug -DCASE=no_content)
run(TRUE "" "${CMAKE_COMMAND}" --build "${bare_binary}" --config Debug --parallel 2)
check_tests("${bare_binary}" puzzle_lab)
run(TRUE "" "${CMAKE_CTEST_COMMAND}" --test-dir "${bare_binary}" -C Debug --output-on-failure --no-tests=error)
run(TRUE "" "${CMAKE_COMMAND}" --install "${bare_binary}" --config Debug --prefix "${bare_stage}")
file(READ "${bare_stage}/share/gyo/apps/puzzle_lab/manifest.json" bare_manifest)
string(JSON bare_required_count LENGTH "${bare_manifest}" required_files)
string(JSON bare_executable GET "${bare_manifest}" executable)
if(NOT bare_required_count EQUAL 1 OR EXISTS "${bare_stage}/bin/assets" OR EXISTS "${bare_stage}/bin/shaders")
    message(FATAL_ERROR "Content-free app acquired installed content: ${bare_manifest}")
endif()
run(TRUE "" "${bare_stage}/${bare_executable}" --identity)

# App IDs are strings, including names that CMake interprets as false booleans.
# A sole selected 'off' app must retain its project context and runtime install.
file(WRITE "${source}/config/engine/projects.csv" "${header}off,,,1,1,1,1\n")
set(false_binary "${work}/false-id")
set(false_stage "${work}/false-id-stage")
run(TRUE "" "${CMAKE_COMMAND}" -S "${source}" -B "${false_binary}" ${generator_args}
    "-DGYO_TEST_REPOSITORY=${repository}" -DGYO_APPS=AUTO -DGYO_RENDER_DEVICE=NONE -DCMAKE_BUILD_TYPE=Debug -DCASE=false_id)
run(TRUE "" "${CMAKE_COMMAND}" --build "${false_binary}" --config Debug --parallel 2)
check_tests("${false_binary}" off)
run(TRUE "" "${CMAKE_CTEST_COMMAND}" --test-dir "${false_binary}" -C Debug --output-on-failure --no-tests=error)
file(READ "${false_binary}/sdl-Debug.txt" sdl_filename)
# Reusing a fixture build must still prove this install provides the runtime.
file(REMOVE "${false_stage}/bin/${sdl_filename}" "${false_stage}/lib/${sdl_filename}")
run(TRUE "" "${CMAKE_COMMAND}" --install "${false_binary}" --config Debug --prefix "${false_stage}")
file(READ "${false_stage}/share/gyo/apps/off/manifest.json" false_manifest)
string(JSON false_identity GET "${false_manifest}" app)
string(JSON false_executable GET "${false_manifest}" executable)
string(JSON false_dependency GET "${false_manifest}" runtime_dependencies 0)
if(NOT false_identity STREQUAL "off" OR NOT false_dependency STREQUAL SDL3)
    message(FATAL_ERROR "False-like app identity or runtime dependencies were lost: ${false_manifest}")
endif()
if(NOT EXISTS "${false_stage}/bin/${sdl_filename}" AND NOT EXISTS "${false_stage}/lib/${sdl_filename}")
    message(FATAL_ERROR "A sole false-like app lost its installed SDL runtime: ${sdl_filename}")
endif()
run(TRUE "" "${false_stage}/${false_executable}" --startup)
run(FALSE "already has a project context" "${CMAKE_COMMAND}" -S "${source}" -B "${false_binary}" -DCASE=duplicate_context)
message(STATUS "App projects: two app builds/install/startup, independent test staging, private config identity, components, optional content, content updates, CSV regeneration, false-like IDs and 11 invalid contracts passed")
