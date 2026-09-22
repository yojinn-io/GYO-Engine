foreach(suite AppRegistry ToolRegistry AcceptanceToken Component RenderOptions MsvcRuntime ProjectComposition ProductPackaging)
    add_test(NAME "cmake.${suite}"
        COMMAND "${CMAKE_COMMAND}"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/${suite}Tests.cmake")
    set_tests_properties("cmake.${suite}" PROPERTIES LABELS "cpu;build" TIMEOUT 180)
endforeach()
set_tests_properties(cmake.ProjectComposition PROPERTIES TIMEOUT 600)
