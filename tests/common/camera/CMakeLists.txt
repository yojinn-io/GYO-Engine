# Tests of the camera Module (docs/architecture/target/v2/transform-camera-declaration.zh-Hant.md, section 6.1;
# Design Invariants CM-1 to CM-5 in docs/architecture/target/v2/camera.zh-Hant.md). Synthetic units only.
add_executable(gyo_camera_tests TestMain.cpp CameraTests.cpp)
target_link_libraries(gyo_camera_tests PRIVATE GYO::Camera GYO::Transform GYO::EntityIds gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_camera_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_camera_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_camera_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME camera.module COMMAND gyo_camera_tests "--test-case=camera:*")
set_tests_properties(camera.module PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Design Invariant CM-2: camera links the orchestration layer, the kernel, Base, Math and transform, nothing else
# (no window, input or render).
function(gyo_expect_camera_links)
    gyo_expect_links(gyo_camera GYO::Base GYO::Kernel GYO::Math GYO::Orchestration GYO::Transform)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_camera_links)
