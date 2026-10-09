add_executable(gyo_threads_tests RoleThreadTests.cpp)
target_link_libraries(gyo_threads_tests PRIVATE GYO::Threads gyo_test_support doctest::doctest)
add_test(NAME gyo_threads_tests COMMAND gyo_threads_tests)
set_tests_properties(gyo_threads_tests PROPERTIES LABELS "cpu" TIMEOUT 60)
