if(BUILD_TESTING)
    add_test(NAME shader.host_pipeline COMMAND gyo_shader_tool --self-test
        "${GYO_REPOSITORY_ROOT}" "${CMAKE_BINARY_DIR}/test-output")
    set_tests_properties(shader.host_pipeline PROPERTIES LABELS "shader;host" TIMEOUT 120)
    add_test(NAME shader.cmake_dependencies COMMAND ${CMAKE_COMMAND}
        "-DROOT=${GYO_REPOSITORY_ROOT}"
        "-DWORK=${CMAKE_BINARY_DIR}/cmake-test-output"
        "-DTOOL=$<TARGET_FILE:gyo_shader_tool>"
        "-DGENERATOR=${CMAKE_GENERATOR}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        -P "${CMAKE_CURRENT_LIST_DIR}/CMakeIntegration.cmake")
    set_tests_properties(shader.cmake_dependencies PROPERTIES LABELS "shader;host;cmake" TIMEOUT 180)
    find_program(GYO_SHADER_TEST_NINJA ninja)
    if(GYO_SHADER_TEST_NINJA)
        add_test(NAME shader.host_toolchain COMMAND ${CMAKE_COMMAND}
            "-DROOT=${GYO_REPOSITORY_ROOT}"
            "-DWORK=${CMAKE_BINARY_DIR}/host-toolchain-test-output"
            "-DC_COMPILER=${CMAKE_C_COMPILER}" "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DNINJA=${GYO_SHADER_TEST_NINJA}" "-DUSES_MSVC=${MSVC}"
            -P "${CMAKE_CURRENT_LIST_DIR}/HostToolchain.cmake")
        set_tests_properties(shader.host_toolchain PROPERTIES LABELS "shader;host;cmake" TIMEOUT 120
            SKIP_REGULAR_EXPRESSION "SKIP: native Ninja compiler probe requires")
    endif()
endif()
