add_executable(gyo_model_tests ModelTests.cpp)
target_link_libraries(gyo_model_tests PRIVATE GYO::Model doctest::doctest)
add_test(NAME gyo_model_tests COMMAND gyo_model_tests)

add_executable(gyo_model_renderer_tests ModelRendererTests.cpp)
target_include_directories(gyo_model_renderer_tests PRIVATE ../support)
target_link_libraries(gyo_model_renderer_tests PRIVATE GYO::ModelRenderer doctest::doctest)
add_test(NAME gyo_model_renderer_tests COMMAND gyo_model_renderer_tests)

if(TARGET GYO::AssetUfbx)
    add_executable(gyo_ufbx_model_tests UfbxModelTests.cpp)
    target_link_libraries(gyo_ufbx_model_tests PRIVATE GYO::AssetUfbx doctest::doctest)
    add_test(NAME gyo_ufbx_model_tests COMMAND gyo_ufbx_model_tests)
endif()
