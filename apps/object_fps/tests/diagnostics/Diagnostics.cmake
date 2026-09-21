# Shared by the optional test and release adapters, including package-only builds.
# No unit-test framework is needed to exercise the real executable's diagnostics.
function(object_fps_enable_diagnostics)
    gyo_app_get_target(main OUT_TARGET executable)
    get_target_property(enabled ${executable} OBJECT_FPS_DIAGNOSTICS_ENABLED)
    if(enabled)
        return()
    endif()
    set_property(TARGET ${executable} PROPERTY OBJECT_FPS_DIAGNOSTICS_ENABLED TRUE)
    target_sources(${executable} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/HeadlessSmoke.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/MuzzleProbe.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/PackageProbes.cpp")
    target_include_directories(${executable} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    target_compile_definitions(${executable} PRIVATE OBJECT_FPS_WITH_DIAGNOSTICS=1)
endfunction()
