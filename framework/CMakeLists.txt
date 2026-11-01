# The layer games and tools share (docs/framework/architecture.md). It is not a
# product: GyoBuild adds it once, after the engine, and only while something
# selected uses it; so far only tools do (Design Invariant 1).

# The warnings every framework target builds with, the bindings' included.
function(gyo_framework_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(${target} PRIVATE NOMINMAX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(gyo_tool_framework STATIC
    src/CatalogRegistration.cpp
    src/FileService.cpp
    src/HeadlessShell.cpp
    src/PathDialog.cpp
    src/ReadOnlyAssetCatalog.cpp
)
add_library(GYO::ToolFramework ALIAS gyo_tool_framework)
target_include_directories(gyo_tool_framework PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(gyo_tool_framework PUBLIC cxx_std_20)
# The catalog mount names engine/asset types; paths go to UTF-8 and the
# command line comes as UTF-8 through GYO::Os.
target_link_libraries(gyo_tool_framework
    PUBLIC GYO::Engine
    PRIVATE GYO::Os
)
set(_gyo_framework_targets gyo_tool_framework)

# The windowed shell, where the build has Engine's SDL platform. It reaches SDL
# only through that neutral target and creates no render device.
if(TARGET GYO::PlatformSDL)
    add_library(gyo_tool_framework_window STATIC src/WindowShell.cpp)
    add_library(GYO::ToolFrameworkWindow ALIAS gyo_tool_framework_window)
    target_link_libraries(gyo_tool_framework_window PUBLIC gyo_tool_framework GYO::PlatformSDL)
    list(APPEND _gyo_framework_targets gyo_tool_framework_window)
endif()

# What the core still links or builds that its Design Invariant 2 forbids: the
# catalog registration's JSON parser and the old window host and path dialogs,
# until the JSON binding, the shell, the ImGui binding and Engine's path
# dialogs replace them (removed in v8-framework-invariants,
# docs/architecture/plans/framework-rework). Kept in its own file so that this
# one has no exception; the sites are frozen in
# tests/framework/framework_guard_allowlist.txt.
include("${CMAKE_CURRENT_SOURCE_DIR}/Legacy.cmake")

foreach(_target IN LISTS _gyo_framework_targets)
    gyo_framework_warnings(${_target})
endforeach()
unset(_gyo_framework_targets)

# Every binding directory that has a CMakeLists.txt; the core names none of
# them (Design Invariant 2). Each decides from the targets it needs whether it
# builds; tests/framework checks that the expected ones exist.
file(GLOB _gyo_bindings LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/bindings/*")
list(SORT _gyo_bindings)
foreach(_binding IN LISTS _gyo_bindings)
    if(EXISTS "${_binding}/CMakeLists.txt")
        add_subdirectory("${_binding}")
    endif()
endforeach()
unset(_gyo_bindings)
