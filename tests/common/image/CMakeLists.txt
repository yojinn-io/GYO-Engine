# The image Module's tests (docs/architecture/target/v2/image.zh-Hant.md): the
# "image"/"png" Data Contract, rule by rule, on PNG files written byte by byte
# in the test, and contract/gradient_rgba_16x8.png from a real encoder
# (contract/make_example.py). Offline: no device, no SDL.
add_executable(gyo_image_tests PngLoaderTests.cpp)
target_link_libraries(gyo_image_tests PRIVATE GYO::Image gyo_test_support doctest::doctest)
target_compile_definitions(gyo_image_tests PRIVATE GYO_IMAGE_CONTRACT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/contract")
if(MSVC)
    target_compile_options(gyo_image_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_image_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_image_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME gyo_image_tests COMMAND gyo_image_tests)
set_tests_properties(gyo_image_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Design Invariants IM-1 and IM-3: the Module's links are exactly GYO::Base,
# the asset loader interface, JSON (the settings) and gyo_stb_image, so no SDL
# or renderer appears, and it hands stb_image to none of its users. Checked once
# the whole project is configured, so an edge added later from another
# directory is seen too.
function(gyo_expect_image_links)
    gyo_expect_links(gyo_image GYO::Base GYO::Asset GYO::Json gyo_stb_image)
    get_target_property(interface gyo_image INTERFACE_LINK_LIBRARIES)
    if(gyo_stb_image IN_LIST interface)
        message(FATAL_ERROR "gyo_image exposes gyo_stb_image to its users; link it PRIVATE")
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_image_links)
