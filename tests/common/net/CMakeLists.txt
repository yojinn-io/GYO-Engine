# Tests of the net Module, step 1a (docs/architecture/target/v2/net-module-declaration.zh-Hant.md, section 8;
# Design Invariants draft in docs/architecture/target/v2/net.zh-Hant.md). "transport:" is judgement G-2 as
# declared; "header:", "connection:" and "link:" are the unit cases of the header, the Connection and the Links
# ("link:" also runs G-7 over UDP loopback); "handshake:" is G-3, "liveness:" G-4 (step 1b); "module:" is G-1
# and the net Module in a Product with a MemoryLink.
add_executable(gyo_net_tests TestMain.cpp HeaderTests.cpp ConnectionTests.cpp TransportTests.cpp LinkTests.cpp
    SessionTests.cpp ModuleTests.cpp ReplicationTests.cpp ScenarioTests.cpp InputTests.cpp ReconnectTests.cpp ClockTests.cpp
    ResourceTests.cpp RollbackClientTests.cpp)
# GYO::StateMachine: the "statesync:" cases join the state machine and the net Module as a product does (neither
# Module depends on the other).
target_link_libraries(gyo_net_tests PRIVATE GYO::Net GYO::StateMachine doctest::doctest)
if(MSVC)
    target_compile_options(gyo_net_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_net_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_net_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS header connection transport link handshake liveness module replication scenario input reconnect clock
    resource statesync k3)
    add_test(NAME net.${group} COMMAND gyo_net_tests "--test-case=${group}:*")
    set_tests_properties(net.${group} PROPERTIES LABELS "cpu" TIMEOUT 120
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()

# The CPU measurement of declaration section 6 (step 3b): run by hand on one host, not a ctest.
add_executable(gyo_net_budget NetBudget.cpp)
target_link_libraries(gyo_net_budget PRIVATE GYO::Net doctest::doctest)
if(MSVC)
    target_compile_options(gyo_net_budget PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_net_budget PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_net_budget PRIVATE -Wall -Wextra -Wpedantic)
endif()

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# NT-7: the net Module links the core libraries it uses and the Modules of its register row (world_time, rollback,
# authority, entity_ids), nothing else; no Module links it (the state machine's own link set is pinned by its tests).
# Set up once net.scenario passed (AGENTS §3, the time to set a guard; design review before PR-1, small thing 6).
function(gyo_expect_net_links)
    gyo_expect_links(gyo_net GYO::Base GYO::OS GYO::Time GYO::Kernel GYO::Orchestration GYO::WorldTime GYO::Rollback
        GYO::Authority GYO::EntityIds)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_net_links)
