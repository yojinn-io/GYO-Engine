add_executable(gyo_map_markers_tests MapMarkersTests.cpp)
target_link_libraries(gyo_map_markers_tests PRIVATE GYO::MapMarkers doctest::doctest)
add_test(NAME gyo_map_markers_tests COMMAND gyo_map_markers_tests)

# Design Invariant 1: the module, its tests and its contract name no gameplay
# concept (docs/architecture/map-markers.md).
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME gyo_map_markers.neutral_names
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/neutral_names_guard.py"
        --root "${GYO_REPOSITORY_ROOT}")
set_tests_properties(gyo_map_markers.neutral_names PROPERTIES LABELS cpu TIMEOUT 60)

# Design Invariant 2: its links are exactly Base, Math and, privately, the JSON
# parser (gyo_expect_links is defined with the time tests). Checked once the
# whole project is configured.
function(gyo_expect_map_markers_links)
    gyo_expect_links(gyo_map_markers GYO::Base GYO::Math nlohmann_json::nlohmann_json)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_map_markers_links)
