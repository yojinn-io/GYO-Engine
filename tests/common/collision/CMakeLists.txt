add_executable(gyo_collision_tests CollisionTests.cpp)
target_link_libraries(gyo_collision_tests PRIVATE GYO::Collision gyo_test_support doctest::doctest)
add_test(NAME gyo_collision_tests COMMAND gyo_collision_tests)
