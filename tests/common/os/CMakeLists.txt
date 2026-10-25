# Tests of the OS layer (docs/architecture/os.md). This is the one common test
# directory allowed to assert per OS; the boundary guard does not scan it.
add_executable(gyo_os_tests OsErrorTests.cpp)
target_link_libraries(gyo_os_tests PRIVATE GYO::Os gyo_test_support doctest::doctest)
add_test(NAME gyo_os_tests COMMAND gyo_os_tests)
set_tests_properties(gyo_os_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

# The boundary guard scans the sources as text, so it gives the same result on
# every platform; its self-test runs it on synthetic trees.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME os.boundary_guard
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/os_boundary_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME os.boundary_guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_*.py" -v)
set_tests_properties(os.boundary_guard os.boundary_guard_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)

# The OS layer is the lowest layer above GYO::Base: its links, private and
# interface alike, are exactly GYO::Base (gyo_expect_links is defined with the
# time tests, which configure first). Checked once the whole project is
# configured, so an edge added later from another directory is seen too.
function(gyo_expect_os_links)
    gyo_expect_links(gyo_os GYO::Base)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_os_links)
