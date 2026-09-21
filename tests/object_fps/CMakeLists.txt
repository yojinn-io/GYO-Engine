# Game tests consume completed product libraries. No test mutates the product.
gyo_get_app_target("${GYO_CURRENT_APP}" main app)
gyo_get_app_target("${GYO_CURRENT_APP}" app_support support)
include(sources.cmake)
set(test_stage "${PROJECT_BINARY_DIR}/test-products/${GYO_CURRENT_APP}")
set(test_content "gyo_${GYO_CURRENT_APP}_test_runtime")
add_custom_target(${test_content})
gyo_stage_app_content(${test_content} "${GYO_CURRENT_APP}" "${test_stage}")

set(headless "gyo_${GYO_CURRENT_APP}_headless_tests")
set(model "gyo_${GYO_CURRENT_APP}_model_tests")
add_executable(${headless} ${APP_HEADLESS_SOURCES})
target_link_libraries(${headless} PRIVATE ${support} GYO::AssetUfbx GYO::AssetSdlImage
    GYO::PlatformSDL GYO::UiRenderer GYO::Text nlohmann_json::nlohmann_json)
target_include_directories(${headless} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/common/support")

add_executable(${model} Model/Mark23ModelTests.cpp)
target_link_libraries(${model} PRIVATE ${support} gyo_ufbx GYO::AssetUfbx GYO::PlatformSDL doctest::doctest)

foreach(target ${headless} ${model})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE "${GYO_APP_BINARY_DIR}/generated")
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${test_stage}/bin")
    foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${test_stage}/bin")
    endforeach()
    if(WIN32)
        target_compile_definitions(${target} PRIVATE NOMINMAX)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()
    gyo_copy_runtime_dlls(${target})
    add_dependencies(${target} ${test_content})
endforeach()
add_test(NAME "${GYO_CURRENT_APP}.headless" COMMAND ${headless})
add_test(NAME "${GYO_CURRENT_APP}.model_assets" COMMAND ${model})
set_tests_properties("${GYO_CURRENT_APP}.headless" "${GYO_CURRENT_APP}.model_assets"
    PROPERTIES LABELS "cpu;gameplay" TIMEOUT 180)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME "${GYO_CURRENT_APP}.package_tools"
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/package_tools" -v)
set_tests_properties("${GYO_CURRENT_APP}.package_tools" PROPERTIES LABELS cpu TIMEOUT 90)
