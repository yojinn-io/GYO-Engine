set(GYO_TEST_PROFILE full CACHE STRING "Engine verification capabilities: core or full")
set_property(CACHE GYO_TEST_PROFILE PROPERTY STRINGS core full)
if(GYO_TEST_PROFILE STREQUAL full)
    # The common tests that needed SDL, ufbx or GPU backends were removed with
    # their modules; no carried common test requests a component.
    set(GYO_COMMON_TEST_COMPONENTS "")
elseif(GYO_TEST_PROFILE STREQUAL core)
    set(GYO_COMMON_TEST_COMPONENTS "")
else()
    message(FATAL_ERROR "GYO_TEST_PROFILE must be core or full")
endif()
