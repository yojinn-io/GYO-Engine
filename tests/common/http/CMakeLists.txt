# Tests of the http Module (the PvP Gateway port's declaration D8): the message on its own, and a call against a loopback
# listener (tests/common/os/LoopbackListener.hpp keeps the native socket code in tests/common/os).
add_executable(gyo_http_tests TestMain.cpp HttpTests.cpp)
target_include_directories(gyo_http_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../os")
target_link_libraries(gyo_http_tests PRIVATE GYO::Http doctest::doctest)
if(WIN32)
    # The listener's own Winsock calls.
    target_link_libraries(gyo_http_tests PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(gyo_http_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_http_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_http_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME http.client COMMAND gyo_http_tests "--test-case=http:*")
set_tests_properties(http.client PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
