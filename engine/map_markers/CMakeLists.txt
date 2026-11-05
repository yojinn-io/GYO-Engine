# gyo.map_markers: named points, boxes and paths placed on a map
# (docs/architecture/map-markers.md). Its links are exactly Base, Math and,
# privately, the JSON parser (Design Invariant 2): it knows no geometry format,
# collision, render or product.
add_library(gyo_map_markers STATIC src/MapMarkers.cpp)
add_library(GYO::MapMarkers ALIAS gyo_map_markers)
target_include_directories(gyo_map_markers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gyo_map_markers PUBLIC GYO::Base GYO::Math PRIVATE nlohmann_json::nlohmann_json)
target_compile_features(gyo_map_markers PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_map_markers PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_map_markers PRIVATE -Wall -Wextra -Wpedantic)
endif()
