# The voxel map editor's headless part: the tool's own grid format gyo.voxel_grid, the bake of a grid into the chunked
# map (gyo.voxel_map and gyo.voxel_region), the box and Goxel imports and the synthetic maps, behind one command line.
# The map's only validator is the voxel_map Module (GYO::VoxelMap): the tool writes, the Module reads back. Files go
# through engine/io, JSON through the json Module, PNG through the image Module; no window, no ImGui, no game.
add_library(gyo_voxel_map_editor_core STATIC
    src/BoxImport.cpp
    src/FileAccess.cpp
    src/GoxImport.cpp
    src/GridDocument.cpp
    src/MapBuild.cpp
    src/MergeBoxes.cpp
    src/Orientation.cpp
    src/Synthetic.cpp
)
target_include_directories(gyo_voxel_map_editor_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(gyo_voxel_map_editor_core PUBLIC cxx_std_20)
target_link_libraries(gyo_voxel_map_editor_core
    PUBLIC GYO::Base GYO::Math GYO::VoxelMap
    PRIVATE GYO::IO GYO::Json GYO::Image GYO::Asset
)

add_executable(gyo_voxel_map_editor src/Main.cpp)
target_compile_features(gyo_voxel_map_editor PRIVATE cxx_std_20)
# The UTF-8 command line and console come from the OS layer.
target_link_libraries(gyo_voxel_map_editor PRIVATE gyo_voxel_map_editor_core GYO::OS GYO::Json)
# Local-only tools go to build/target/_tools/<owner>/bin.
gyo_tool_install_target(TARGET gyo_voxel_map_editor ROLE main)

foreach(_target IN ITEMS gyo_voxel_map_editor_core gyo_voxel_map_editor)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(${_target} PRIVATE NOMINMAX)
    else()
        target_compile_options(${_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
