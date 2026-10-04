add_executable(gyo_net_tests GyopDatagramTests.cpp)
target_link_libraries(gyo_net_tests PRIVATE GYO::Net doctest::doctest nlohmann_json::nlohmann_json)
target_compile_definitions(gyo_net_tests PRIVATE
    GYO_TEST_GYOP_VECTORS="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/gyop_datagram_vectors.json")
add_test(NAME gyo_net_tests COMMAND gyo_net_tests)
set_tests_properties(gyo_net_tests PROPERTIES LABELS "cpu")
