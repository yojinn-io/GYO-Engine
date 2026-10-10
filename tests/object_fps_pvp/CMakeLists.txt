gyo_get_app_target("${GYO_CURRENT_APP}" match_domain match_domain)
gyo_get_app_target("${GYO_CURRENT_APP}" runtime_host runtime_host)
gyo_get_app_target("${GYO_CURRENT_APP}" client_simulation client_simulation)
set(tests "gyo_${GYO_CURRENT_APP}_tests")
add_executable(${tests} TestMain.cpp PvpMatchTests.cpp PredictionTests.cpp PredictionElapsedTimeTests.cpp TimelineTests.cpp MovementRecoveryTests.cpp
    CombatTests.cpp CombatHostTests.cpp ShotQueryTests.cpp LifePredictionTests.cpp MathCharacterizationTests.cpp WireTests.cpp
    ArenaDigestTests.cpp FireGateTests.cpp HitFeedbackTests.cpp ClientSimulationTests.cpp ClientSimulationRoleTests.cpp
    NetworkStatisticsTests.cpp MatchHostRoleTests.cpp IngressStatisticsTests.cpp MatchIngressLedgerTests.cpp)
target_compile_features(${tests} PRIVATE cxx_std_20)
target_compile_definitions(${tests} PRIVATE PVP_DOMAIN_TEST_ARENA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/arena.json"
    PVP_INGRESS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/ingress_v1"
    PVP_HIT_FEEDBACK_PATH="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp/ui/hit_feedback.json"
    PVP_PRODUCT_ARENA_PATH="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp/pvp_arena.json")
target_link_libraries(${tests} PRIVATE ${match_domain} ${runtime_host} ${client_simulation} GYO::Net gyo_test_support nlohmann_json::nlohmann_json doctest::doctest)
if(WIN32)
    target_compile_definitions(${tests} PRIVATE NOMINMAX)
endif()
if(MSVC)
    target_compile_options(${tests} PRIVATE /utf-8)
endif()
add_test(NAME "${GYO_CURRENT_APP}.cpu" COMMAND ${tests})
# The movement recovery matrix runs the v6 and the product command paths.
set_tests_properties("${GYO_CURRENT_APP}.cpu" PROPERTIES LABELS "cpu;gameplay;pvp" TIMEOUT 60)

# The Match IPC over real loopback TCP (asio and the runtime contract), kept
# apart so the domain test target gains no transport dependency.
gyo_get_app_target("${GYO_CURRENT_APP}" ipc ipc)
set(ipc_tests "gyo_${GYO_CURRENT_APP}_ipc_tests")
add_executable(${ipc_tests} TestMain.cpp IpcHostTests.cpp)
target_compile_features(${ipc_tests} PRIVATE cxx_std_20)
target_link_libraries(${ipc_tests} PRIVATE ${ipc} asio::asio "gyo_${GYO_CURRENT_APP}_runtime_v6" doctest::doctest)
if(WIN32)
    target_compile_definitions(${ipc_tests} PRIVATE NOMINMAX)
endif()
if(MSVC)
    target_compile_options(${ipc_tests} PRIVATE /utf-8)
endif()
add_test(NAME "${GYO_CURRENT_APP}.ipc" COMMAND ${ipc_tests})
set_tests_properties("${GYO_CURRENT_APP}.ipc" PROPERTIES LABELS "cpu;pvp;network" TIMEOUT 60)

# The acceptance probes' header-only start-phase recorder, driven by the real
# prediction, and the GUI probes' room (gui_room.hpp). Kept apart so the
# domain test target gains no JSON dependency.
set(record_tests "gyo_${GYO_CURRENT_APP}_start_phase_record_tests")
add_executable(${record_tests} TestMain.cpp StartPhaseRecordTests.cpp GuiRoomTests.cpp)
target_compile_features(${record_tests} PRIVATE cxx_std_20)
target_include_directories(${record_tests} PRIVATE "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp")
target_link_libraries(${record_tests} PRIVATE ${match_domain} nlohmann_json::nlohmann_json doctest::doctest)
if(WIN32)
    target_compile_definitions(${record_tests} PRIVATE NOMINMAX)
endif()
if(MSVC)
    target_compile_options(${record_tests} PRIVATE /utf-8)
endif()
add_test(NAME "${GYO_CURRENT_APP}.start_phase_record" COMMAND ${record_tests})
set_tests_properties("${GYO_CURRENT_APP}.start_phase_record" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)

# Client presentation has its own CPU asset/pose tests. Keep Model/SDL and
# staged character content out of the authority-domain test target.
gyo_get_app_target("${GYO_CURRENT_APP}" app_support support)
set(presentation_tests "gyo_${GYO_CURRENT_APP}_presentation_tests")
set(presentation_stage "${PROJECT_BINARY_DIR}/test-products/${GYO_CURRENT_APP}")
set(presentation_content "gyo_${GYO_CURRENT_APP}_presentation_test_runtime")
add_custom_target(${presentation_content})
gyo_stage_app_content(${presentation_content} "${GYO_CURRENT_APP}" "${presentation_stage}")
add_executable(${presentation_tests} TestMain.cpp PlayerPresentationTests.cpp PresentationMathCharacterizationTests.cpp)
target_compile_features(${presentation_tests} PRIVATE cxx_std_20)
target_link_libraries(${presentation_tests} PRIVATE ${support} GYO::AssetUfbx GYO::AssetSdlImage
    SDL3::SDL3 nlohmann_json::nlohmann_json gyo_test_support doctest::doctest)
gyo_use_app_config(${presentation_tests} "${GYO_CURRENT_APP}")
set_target_properties(${presentation_tests} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${presentation_stage}/bin")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(${presentation_tests} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${presentation_stage}/bin")
endforeach()
if(WIN32)
    target_compile_definitions(${presentation_tests} PRIVATE NOMINMAX)
endif()
if(MSVC)
    target_compile_options(${presentation_tests} PRIVATE /utf-8)
endif()
gyo_copy_runtime_dlls(${presentation_tests})
add_dependencies(${presentation_tests} ${presentation_content})
add_test(NAME "${GYO_CURRENT_APP}.presentation_cpu" COMMAND ${presentation_tests})
set_tests_properties("${GYO_CURRENT_APP}.presentation_cpu" PROPERTIES LABELS "cpu;pvp;presentation" TIMEOUT 120
    ENVIRONMENT "SDL_VIDEODRIVER=dummy")

# Batch 06: the in-world pointer capture policy against the pre-batch native
# SDL handling, through Engine SDL input on the dummy video driver.
set(capture_tests "gyo_${GYO_CURRENT_APP}_pointer_capture_tests")
add_executable(${capture_tests} PointerCaptureCharacterizationTests.cpp)
target_compile_features(${capture_tests} PRIVATE cxx_std_20)
target_link_libraries(${capture_tests} PRIVATE ${support} GYO::InputBackendSDL doctest::doctest)
if(WIN32)
    target_compile_definitions(${capture_tests} PRIVATE NOMINMAX)
endif()
if(MSVC)
    target_compile_options(${capture_tests} PRIVATE /utf-8)
endif()
gyo_copy_runtime_dlls(${capture_tests})
add_test(NAME "${GYO_CURRENT_APP}.pointer_capture" COMMAND ${capture_tests})
set_tests_properties("${GYO_CURRENT_APP}.pointer_capture" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
# Batch 08: catalog entries exist, asset files are listed, and every id is used.
add_test(NAME "${GYO_CURRENT_APP}.asset_catalog"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_asset_catalog.py")
set_tests_properties("${GYO_CURRENT_APP}.asset_catalog" PROPERTIES LABELS "cpu;pvp;content" TIMEOUT 60)
add_test(NAME "${GYO_CURRENT_APP}.probe_command_path"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_probe_command_path.py")
set_tests_properties("${GYO_CURRENT_APP}.probe_command_path" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
add_test(NAME "${GYO_CURRENT_APP}.protocol_version"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_protocol_version.py")
set_tests_properties("${GYO_CURRENT_APP}.protocol_version" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60)
add_test(NAME "${GYO_CURRENT_APP}.max_players"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_max_players.py")
set_tests_properties("${GYO_CURRENT_APP}.max_players" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60)
add_test(NAME "${GYO_CURRENT_APP}.mutation_tool"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_run_mutations.py")
set_tests_properties("${GYO_CURRENT_APP}.mutation_tool" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60)
add_test(NAME "${GYO_CURRENT_APP}.gameplay_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_gameplay_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.gameplay_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)
add_test(NAME "${GYO_CURRENT_APP}.player_presentation_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_player_presentation_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.player_presentation_evidence" PROPERTIES LABELS "cpu;pvp;presentation;acceptance" TIMEOUT 10)
add_test(NAME "${GYO_CURRENT_APP}.action_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_action_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.action_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)
add_test(NAME "${GYO_CURRENT_APP}.presentation_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_presentation_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.presentation_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 60)

add_test(NAME "${GYO_CURRENT_APP}.command_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_command_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.command_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
# Analyzer v7: the command rules whose meaning changed with the simulation role.
add_test(NAME "${GYO_CURRENT_APP}.command_evidence_v7"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_command_evidence_v7.py")
set_tests_properties("${GYO_CURRENT_APP}.command_evidence_v7" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
# P2-log: the network path's 10-second statistics lines (Gateway, Match, Client worker).
add_test(NAME "${GYO_CURRENT_APP}.network_statistics"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_network_statistics.py")
set_tests_properties("${GYO_CURRENT_APP}.network_statistics" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
# Analyzer v7 (batch 08): action latency measures that the probe frame does not quantize.
add_test(NAME "${GYO_CURRENT_APP}.action_latency_v7"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_action_latency_v7.py")
set_tests_properties("${GYO_CURRENT_APP}.action_latency_v7" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
# Room-capacity evidence, cross-validated value by value against command_evidence.
add_test(NAME "${GYO_CURRENT_APP}.quad_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_quad_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.quad_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 60)

add_test(NAME "${GYO_CURRENT_APP}.recovery_relay"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_recovery_probe.py")
set_tests_properties("${GYO_CURRENT_APP}.recovery_relay" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)

add_test(NAME "${GYO_CURRENT_APP}.backpressure_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_backpressure_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.backpressure_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)

add_test(NAME "${GYO_CURRENT_APP}.native_window_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_native_window.py")
set_tests_properties("${GYO_CURRENT_APP}.native_window_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)

add_test(NAME "${GYO_CURRENT_APP}.timing_runner"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_run_timing.py")
set_tests_properties("${GYO_CURRENT_APP}.timing_runner" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)

add_test(NAME "${GYO_CURRENT_APP}.action_runner"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_run_action_short.py")
set_tests_properties("${GYO_CURRENT_APP}.action_runner" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)

add_test(NAME "${GYO_CURRENT_APP}.gameplay_soak"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_gameplay_soak_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.gameplay_soak" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)

add_test(NAME "${GYO_CURRENT_APP}.combat_gui_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_combat_gui_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.combat_gui_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)

add_test(NAME "${GYO_CURRENT_APP}.steady_clock"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_steady_clock.py")
set_tests_properties("${GYO_CURRENT_APP}.steady_clock" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)

add_test(NAME "${GYO_CURRENT_APP}.service_startup"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_service_startup.py")
set_tests_properties("${GYO_CURRENT_APP}.service_startup" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)

# Authority digest gate (v6 batch 03). The runner drives the real authority and
# prediction through predeclared scenarios and chains each Tick's state with
# SHA-256. A second runner recompiles the product's domain sources at -O0
# (Engine Collision and Math stay as built); the test requires both runners to
# agree on the zero-angle scenarios and those to match their golden file. Other
# scenarios may legitimately differ with optimisation (libm call form), so their
# differences are listed for information only (authority_digest_check.cmake).
# Formal "authority unchanged" proof is the same-host two-tree comparison
# (compare_authority_trees.py), not this test.
set(digest_arena_definitions
    PVP_DIGEST_CONTRACT_ARENA="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/arena.json"
    PVP_DIGEST_PRODUCT_ARENA="${GYO_REPOSITORY_ROOT}/assets/${GYO_CURRENT_APP}/pvp_arena.json")
set(digest "gyo_${GYO_CURRENT_APP}_authority_digest")
add_executable(${digest} AuthorityDigest.cpp)
target_compile_features(${digest} PRIVATE cxx_std_20)
target_compile_definitions(${digest} PRIVATE ${digest_arena_definitions})
target_link_libraries(${digest} PRIVATE ${match_domain} ${runtime_host})

set(digest_app_root "${GYO_REPOSITORY_ROOT}/apps/${GYO_CURRENT_APP}")
include("${digest_app_root}/sources.cmake")
set(digest_domain_sources ${PVP_DOMAIN_SOURCES} src/Pvp/MatchRuntimeHost.cpp)
list(TRANSFORM digest_domain_sources PREPEND "${digest_app_root}/")
set(digest_o0 "gyo_${GYO_CURRENT_APP}_authority_digest_o0")
add_executable(${digest_o0} AuthorityDigest.cpp ${digest_domain_sources})
target_compile_features(${digest_o0} PRIVATE cxx_std_20)
set_target_properties(${digest_o0} PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(${digest_o0} PRIVATE "${digest_app_root}/include")
gyo_use_app_config(${digest_o0} "${GYO_CURRENT_APP}")
target_compile_definitions(${digest_o0} PRIVATE ${digest_arena_definitions})
target_link_libraries(${digest_o0} PRIVATE GYO::Engine GYO::Math GYO::Collision GYO::Threads nlohmann_json::nlohmann_json)
# Appended after the configuration's flags, so the last optimisation flag wins.
target_compile_options(${digest_o0} PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/Od,-O0>
    $<$<CXX_COMPILER_ID:AppleClang>:-fexperimental-library>)
foreach(target ${digest} ${digest_o0})
    if(WIN32)
        target_compile_definitions(${target} PRIVATE NOMINMAX)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()
endforeach()
add_test(NAME "${GYO_CURRENT_APP}.authority_digest"
    COMMAND "${CMAKE_COMMAND}" -DOPTIMIZED=$<TARGET_FILE:${digest}> -DUNOPTIMIZED=$<TARGET_FILE:${digest_o0}>
        "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/authority_golden.txt"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/authority_digest" -P "${CMAKE_CURRENT_SOURCE_DIR}/authority_digest_check.cmake")
set_tests_properties("${GYO_CURRENT_APP}.authority_digest" PROPERTIES LABELS "cpu;gameplay;pvp" TIMEOUT 300)
