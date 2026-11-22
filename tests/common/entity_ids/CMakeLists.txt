# Tests of the default entity allotter (docs/architecture/target/v2/world-gen-declaration.zh-Hant.md,
# sections 10 and 11; Design Invariants EI-1, EI-2 in docs/architecture/target/v2/entity-ids.zh-Hant.md).
add_executable(gyo_entity_ids_tests TestMain.cpp EntityIdsTests.cpp)
target_link_libraries(gyo_entity_ids_tests PRIVATE GYO::EntityIds gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_entity_ids_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_entity_ids_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_entity_ids_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS allotter module)
    add_test(NAME entity_ids.${group} COMMAND gyo_entity_ids_tests "--test-case=${group}:*")
    set_tests_properties(entity_ids.${group} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()

# C16: the table gyo_product_units generates from the real register files (no REGISTERS: config/).
add_executable(gyo_entity_ids_default_units_tests TestMain.cpp DefaultUnitsTests.cpp)
gyo_product_units(gyo_entity_ids_default_units_tests)
target_link_libraries(gyo_entity_ids_default_units_tests PRIVATE gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_entity_ids_default_units_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_entity_ids_default_units_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_entity_ids_default_units_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME entity_ids.default COMMAND gyo_entity_ids_default_units_tests "--test-case=default:*")
set_tests_properties(entity_ids.default PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Design Invariant EI-3: entity_ids links the orchestration layer, the kernel and Base, nothing else (no rollback,
# authority or time). Stood up after scenario segment 6 passed (AGENTS §3; world-gen declaration section 9, step 4).
function(gyo_expect_entity_ids_links)
    gyo_expect_links(gyo_entity_ids GYO::Base GYO::Kernel GYO::Orchestration)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_entity_ids_links)
