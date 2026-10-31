add_executable(gyo_model_tests ModelTests.cpp AnimationEventsTests.cpp)
target_link_libraries(gyo_model_tests PRIVATE GYO::Model doctest::doctest)
add_test(NAME gyo_model_tests COMMAND gyo_model_tests)

# Model stays below audio and products: its links are exactly the engine, math
# and, privately, the JSON parser for gyo.anim_events
# (docs/architecture/animation-events.md, Design Invariant 1; gyo_expect_links
# is defined with the time tests). Checked once the whole project is configured.
function(gyo_expect_model_links)
    gyo_expect_links(gyo_model GYO::Engine GYO::Math nlohmann_json::nlohmann_json)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_model_links)

add_executable(gyo_model_renderer_tests ModelRendererTests.cpp)
target_include_directories(gyo_model_renderer_tests PRIVATE ../support)
target_link_libraries(gyo_model_renderer_tests PRIVATE GYO::ModelRenderer doctest::doctest)
add_test(NAME gyo_model_renderer_tests COMMAND gyo_model_renderer_tests)

if(TARGET GYO::AssetUfbx)
    add_executable(gyo_ufbx_model_tests UfbxModelTests.cpp)
    target_link_libraries(gyo_ufbx_model_tests PRIVATE GYO::AssetUfbx doctest::doctest)
    add_test(NAME gyo_ufbx_model_tests COMMAND gyo_ufbx_model_tests)
endif()
