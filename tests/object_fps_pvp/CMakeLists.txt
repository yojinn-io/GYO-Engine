gyo_get_app_target("${GYO_CURRENT_APP}" match_domain match_domain)
gyo_get_app_target("${GYO_CURRENT_APP}" runtime_host runtime_host)
set(tests "gyo_${GYO_CURRENT_APP}_tests")
add_executable(${tests} TestMain.cpp PvpMatchTests.cpp PredictionTests.cpp PredictionElapsedTimeTests.cpp TimelineTests.cpp MovementRecoveryTests.cpp)
target_compile_features(${tests} PRIVATE cxx_std_20)
target_compile_definitions(${tests} PRIVATE PVP_DOMAIN_TEST_ARENA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/arena.json")
target_link_libraries(${tests} PRIVATE ${match_domain} ${runtime_host} doctest::doctest)
if(WIN32)
    target_compile_definitions(${tests} PRIVATE NOMINMAX)
endif()
if(MSVC)
    target_compile_options(${tests} PRIVATE /utf-8)
endif()
add_test(NAME "${GYO_CURRENT_APP}.cpu" COMMAND ${tests})
set_tests_properties("${GYO_CURRENT_APP}.cpu" PROPERTIES LABELS "cpu;gameplay;pvp" TIMEOUT 30)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME "${GYO_CURRENT_APP}.presentation_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_presentation_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.presentation_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)

add_test(NAME "${GYO_CURRENT_APP}.command_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_command_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.command_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)

add_test(NAME "${GYO_CURRENT_APP}.recovery_relay"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_recovery_probe.py")
set_tests_properties("${GYO_CURRENT_APP}.recovery_relay" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)

add_test(NAME "${GYO_CURRENT_APP}.backpressure_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_backpressure_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.backpressure_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
