# Tests of the net Module, step 1a (docs/architecture/target/v2/net-module-declaration.zh-Hant.md, section 8;
# Design Invariants draft in docs/architecture/target/v2/net.zh-Hant.md). "transport:" is judgement G-2 as
# declared; "header:", "connection:" and "link:" are the unit cases of the header, the Connection and the Links.
add_executable(gyo_net_tests TestMain.cpp HeaderTests.cpp ConnectionTests.cpp TransportTests.cpp LinkTests.cpp)
target_link_libraries(gyo_net_tests PRIVATE GYO::Net doctest::doctest)
if(MSVC)
    target_compile_options(gyo_net_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_net_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_net_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS header connection transport link)
    add_test(NAME net.${group} COMMAND gyo_net_tests "--test-case=${group}:*")
    set_tests_properties(net.${group} PROPERTIES LABELS "cpu" TIMEOUT 120
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()
