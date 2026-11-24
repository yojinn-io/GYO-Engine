# Tests of the ui Module (docs/architecture/target/v2/ui.zh-Hant.md, section 6.1). Data and arithmetic only: no
# render, no input, no SDL (Design Invariant UI-1).

# ui.contract: the gyo.ui Data Contract (Design Invariant UI-2). The documents under contract/ are master's (see
# UiContractSupport.hpp for where each one comes from) and the canonical forms the JSON Module's Writer gives them.
add_executable(gyo_ui_contract_tests test_main.cpp UiContractTests.cpp UiValidationRuleTests.cpp)
target_link_libraries(gyo_ui_contract_tests PRIVATE GYO::Ui GYO::Json gyo_test_support doctest::doctest)
target_compile_definitions(gyo_ui_contract_tests PRIVATE GYO_UI_CONTRACT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/contract")
if(MSVC)
    target_compile_options(gyo_ui_contract_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(gyo_ui_contract_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME ui.contract COMMAND gyo_ui_contract_tests "--test-case=ui_contract:*")
set_tests_properties(ui.contract PROPERTIES LABELS "cpu" TIMEOUT 60 FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
