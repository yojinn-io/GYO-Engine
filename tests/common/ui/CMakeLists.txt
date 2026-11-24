# Tests of ui, world_ui and ui_render (docs/architecture/target/v2/ui.zh-Hant.md, section 6.1). Data and arithmetic
# only: no backend, no GPU (render_world's RenderCommands are read and compared).

# ui.contract: the gyo.ui Data Contract (Design Invariant UI-2). The documents under contract/ are master's (see
# UiContractSupport.hpp for where each one comes from) and the canonical forms the JSON Module's Writer gives them.
add_executable(gyo_ui_contract_tests TestMain.cpp UiContractTests.cpp UiValidationRuleTests.cpp)
target_link_libraries(gyo_ui_contract_tests PRIVATE GYO::Ui GYO::Json gyo_test_support doctest::doctest)
target_compile_definitions(gyo_ui_contract_tests PRIVATE GYO_UI_CONTRACT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/contract")
if(MSVC)
    target_compile_options(gyo_ui_contract_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(gyo_ui_contract_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME ui.contract COMMAND gyo_ui_contract_tests "--test-case=ui_contract:*")
set_tests_properties(ui.contract PROPERTIES LABELS "cpu" TIMEOUT 60 FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

# The runtime, text, world_ui, ui_render and the lobby (ui.runtime ... ui.lobby).
add_executable(gyo_ui_tests TestMain.cpp RuntimeTests.cpp TextFieldTests.cpp TextPlacementTests.cpp UiPassTests.cpp WorldUiTests.cpp)
target_link_libraries(gyo_ui_tests PRIVATE GYO::Ui GYO::WorldUi GYO::UiRender GYO::Input GYO::Window GYO::WindowCamera GYO::Camera
    GYO::Transform GYO::EntityIds GYO::WorldAsset GYO::WorldRender GYO::RenderWorld GYO::CameraRender
    gyo_test_support doctest::doctest)
target_include_directories(gyo_ui_tests PRIVATE "${GYO_REPOSITORY_ROOT}/tests/common/asset" "${GYO_REPOSITORY_ROOT}/tests/common/world_asset")
target_compile_definitions(gyo_ui_tests PRIVATE GYO_ASSET_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
    GYO_UI_TEST_FONT="${GYO_REPOSITORY_ROOT}/tests/common/text/fonts/BIZUDPGothic-Subset.ttf"
    GYO_UI_TEST_LOBBY="${GYO_REPOSITORY_ROOT}/tests/common/ui/contract/pvp_lobby.json"
    GYO_UI_TEST_CONTRACT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/contract")
if(MSVC)
    target_compile_options(gyo_ui_tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(gyo_ui_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_ui_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME ui.runtime COMMAND gyo_ui_tests "--test-case=ui_runtime:*")
add_test(NAME ui.layout COMMAND gyo_ui_tests "--test-case=ui_layout:*")
add_test(NAME ui.text_field COMMAND gyo_ui_tests "--test-case=ui_text_field:*")
add_test(NAME ui.world COMMAND gyo_ui_tests "--test-case=ui_world:*")
add_test(NAME ui.render COMMAND gyo_ui_tests "--test-case=ui_render:*")
add_test(NAME ui.lobby COMMAND gyo_ui_tests "--test-case=ui_lobby:*")
set_tests_properties(ui.runtime ui.layout ui.text_field ui.world ui.render ui.lobby PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# UI-1: ui links Base, Math, the kernel (for EntityId and HashValue) and the Modules whose data it reads (json, text,
# asset, image); no render, no input, no window, no Plugin, no SDL. world_ui links ui and the source World's Modules it
# reads; ui_render links ui and the render World's units, not input or window.
function(gyo_expect_ui_links)
    gyo_expect_links(gyo_ui GYO::Asset GYO::Base GYO::Image GYO::Json GYO::Kernel GYO::Math GYO::Text)
    gyo_expect_links(gyo_world_ui GYO::Base GYO::Camera GYO::Input GYO::Kernel GYO::Orchestration GYO::Ui GYO::Window GYO::WorldAsset)
    gyo_expect_links(gyo_ui_render GYO::Base GYO::CameraRender GYO::Kernel GYO::Orchestration GYO::Render GYO::RenderWorld GYO::Ui)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_ui_links)
