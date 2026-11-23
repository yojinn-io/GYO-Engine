# The model Module's tests (docs/architecture/target/v2/model.zh-Hant.md):
# the model data rules, skeletal animation's first layer and retargeting on
# node lists written in the test (gyo_model_animation_tests), and the
# "model"/"fbx" Data Contract on contract/skinned_bar.fbx, written by Blender
# from contract/make_skinned_bar.py (gyo_model_fbx_tests). Offline: no device,
# no SDL, no product's assets.
foreach(suite IN ITEMS animation fbx)
    if(suite STREQUAL "animation")
        set(source AnimationTests.cpp)
    else()
        set(source FbxLoaderTests.cpp)
    endif()
    add_executable(gyo_model_${suite}_tests ${source})
    target_link_libraries(gyo_model_${suite}_tests PRIVATE GYO::Model gyo_test_support doctest::doctest)
    target_compile_definitions(gyo_model_${suite}_tests PRIVATE
        GYO_MODEL_CONTRACT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/contract")
    if(MSVC)
        target_compile_options(gyo_model_${suite}_tests PRIVATE /W4 /permissive-)
        target_compile_definitions(gyo_model_${suite}_tests PRIVATE NOMINMAX)
    else()
        target_compile_options(gyo_model_${suite}_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME gyo_model_${suite}_tests COMMAND gyo_model_${suite}_tests)
    set_tests_properties(gyo_model_${suite}_tests PROPERTIES LABELS "cpu" TIMEOUT 60)
endforeach()

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Design Invariants MD-1 and MD-5: the Module's links are exactly GYO::Base,
# GYO::Math, the asset loader interface, JSON (the settings) and gyo_ufbx, so
# no SDL, renderer or GPU appears, and it hands ufbx to none of its users.
# Checked once the whole project is configured, so an edge added later from
# another directory is seen too.
function(gyo_expect_model_links)
    gyo_expect_links(gyo_model GYO::Base GYO::Math GYO::Asset GYO::Json gyo_ufbx)
    get_target_property(interface gyo_model INTERFACE_LINK_LIBRARIES)
    if(gyo_ufbx IN_LIST interface)
        message(FATAL_ERROR "gyo_model exposes gyo_ufbx to its users; link it PRIVATE")
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_model_links)
