# Header-only helpers shared by engine tests. gyo_test_support carries the
# GYO_ASSERT helpers (AssertTestSupport.hpp); product tests may link it too.
add_library(gyo_test_support INTERFACE)
target_include_directories(gyo_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gyo_test_support INTERFACE GYO::Base doctest::doctest)
