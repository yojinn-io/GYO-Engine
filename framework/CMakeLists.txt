# The layer games and tools share (docs/framework/architecture.md). It is not a
# product: GyoBuild adds it once, after the engine, and only while something
# selected uses it; so far only tools do (Design Invariant 1).

add_library(gyo_tool_framework STATIC
    src/CatalogRegistration.cpp
    src/FileService.cpp
    src/PathDialog.cpp
    src/ReadOnlyAssetCatalog.cpp
)
add_library(GYO::ToolFramework ALIAS gyo_tool_framework)
target_include_directories(gyo_tool_framework PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(gyo_tool_framework PUBLIC cxx_std_20)
# The catalog mount names engine/asset types; paths go to UTF-8 through GYO::Os.
target_link_libraries(gyo_tool_framework
    PUBLIC GYO::Engine
    PRIVATE GYO::Os
)
set(_gyo_framework_targets gyo_tool_framework)

# What the core still links or builds that its Design Invariant 2 forbids: the
# catalog registration's JSON parser and the old window host and path dialogs,
# until the JSON binding, the shell, the ImGui binding and Engine's path
# dialogs replace them (removed in v8-framework-invariants,
# docs/architecture/plans/framework-rework). Kept in its own file so that this
# one has no exception; the sites are frozen in
# tests/framework/framework_guard_allowlist.txt.
include("${CMAKE_CURRENT_SOURCE_DIR}/Legacy.cmake")

foreach(_target IN LISTS _gyo_framework_targets)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(${_target} PRIVATE NOMINMAX)
    else()
        target_compile_options(${_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
unset(_gyo_framework_targets)
