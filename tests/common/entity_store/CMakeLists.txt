# Rule tests of the Engine entity store (docs/architecture/entity-store.md,
# Guard). The executable links only GYO::EntityStore and gyo_test_support
# (GYO::Base and doctest), so a store header that includes anything but the
# standard library and engine/base does not compile here (Design Invariant 9).
add_executable(gyo_entity_store_tests EntityStoreTests.cpp)
target_link_libraries(gyo_entity_store_tests PRIVATE GYO::EntityStore gyo_test_support doctest::doctest)
# Same warning level as the math tests so header code that would warn in a
# consumer fails here first.
if(MSVC)
    target_compile_options(gyo_entity_store_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_entity_store_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME gyo_entity_store_tests COMMAND gyo_entity_store_tests)
set_tests_properties(gyo_entity_store_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

# Design Invariants 8 and 9 as text: no product name, no unordered container,
# hash, clock, random source, thread or atomic, no static variable, and no
# include beyond the standard library and engine/base. The guard scans the
# sources as text, so it gives the same result on every platform; its
# self-test runs it on synthetic trees.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME entity_store.word_guard
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/entity_store_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME entity_store.word_guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_*.py" -v)
set_tests_properties(entity_store.word_guard entity_store.word_guard_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)

# Design Invariant 9: the store is a leaf directly above GYO::Base; its links,
# private and interface alike, are exactly GYO::Base. gyo_expect_links is
# defined with the time tests, which configure first. Checked once the whole
# project is configured, so an edge added later from another directory is
# seen too.
function(gyo_expect_entity_store_links)
    gyo_expect_links(gyo_entity_store GYO::Base)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_entity_store_links)
