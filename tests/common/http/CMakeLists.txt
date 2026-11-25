# Tests of the http Module (docs/architecture/target/v2/http.zh-Hant.md): the message on its own, and a call against the
# test library gyo_test_loopback_listener (tests/common/os keeps the native socket code).
add_executable(gyo_http_tests TestMain.cpp HttpTests.cpp)
target_link_libraries(gyo_http_tests PRIVATE GYO::Http gyo_test_loopback_listener doctest::doctest)
if(MSVC)
    target_compile_options(gyo_http_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_http_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_http_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME http.client COMMAND gyo_http_tests "--test-case=http:*")
set_tests_properties(http.client PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
