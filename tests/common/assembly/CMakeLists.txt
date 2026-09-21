find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME build.assembly COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -v)
set_tests_properties(build.assembly PROPERTIES LABELS "cpu;build" TIMEOUT 180)
