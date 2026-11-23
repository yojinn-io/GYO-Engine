foreach(suite GameRegistry ToolRegistry UnitRegistry AcceptanceToken Component MsvcRuntime ProjectComposition ProductPackaging GoService ProcessCodePage)
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
# Two configures and about forty child processes: room for the Visual Studio generator.
set_tests_properties(cmake.UnitRegistry PROPERTIES TIMEOUT 300)
# The repository with nothing selected adds no Plugin (configure only, from the
# sources this build fetched).
add_test(NAME cmake.PluginSelection
    COMMAND "${CMAKE_COMMAND}"
        "-DGENERATOR=${CMAKE_GENERATOR}"
        "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
        "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DC_COMPILER=${CMAKE_C_COMPILER}"
        "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DPARENT_BINARY=${PROJECT_BINARY_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/PluginSelectionTests.cmake")
# A whole-repository configure: room for the Visual Studio generator, as cmake.ProjectComposition.
set_tests_properties(cmake.PluginSelection PROPERTIES LABELS "cpu;build" TIMEOUT 600)
