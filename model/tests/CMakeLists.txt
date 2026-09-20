add_executable(gyo_model_tests ModelTests.cpp)
target_link_libraries(gyo_model_tests PRIVATE GYO::Model doctest::doctest)
add_test(NAME gyo_model_tests COMMAND gyo_model_tests)

if(TARGET GYO::AssetUfbx)
    add_executable(gyo_ufbx_model_tests UfbxModelTests.cpp)
    target_link_libraries(gyo_ufbx_model_tests PRIVATE GYO::AssetUfbx gyo_ufbx doctest::doctest)
    target_compile_definitions(gyo_ufbx_model_tests PRIVATE
        GYO_MARK23_FBX="${PROJECT_SOURCE_DIR}/assets/object_fps/weapons/mark23/viewmodel/Mark23.fbx")
    add_test(NAME gyo_ufbx_model_tests COMMAND gyo_ufbx_model_tests)
endif()
