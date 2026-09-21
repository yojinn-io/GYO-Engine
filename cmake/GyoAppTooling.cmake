include_guard(GLOBAL)

# Included only by an explicitly requested quality/engineering configuration.
# Each adapter receives the finished product context in an isolated scope.
function(_gyo_configure_app_quality)
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GyoAppTesting.cmake")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/Tests.cmake")
        include("${CMAKE_CURRENT_SOURCE_DIR}/tests/Tests.cmake")
    endif()
endfunction()

function(_gyo_configure_app_release)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Package.cmake")
        include("${CMAKE_CURRENT_SOURCE_DIR}/packaging/Package.cmake")
    endif()
endfunction()

function(_gyo_configure_optional_app_tools)
    if(BUILD_TESTING)
        _gyo_configure_app_quality()
    endif()
    if(GYO_ENABLE_PACKAGING)
        _gyo_configure_app_release()
    endif()
endfunction()
