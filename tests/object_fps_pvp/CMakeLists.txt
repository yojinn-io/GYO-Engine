gyo_get_app_target("${GYO_CURRENT_APP}" match_domain match_domain)
gyo_get_app_target("${GYO_CURRENT_APP}" runtime_host runtime_host)
gyo_get_app_target("${GYO_CURRENT_APP}" client_simulation client_simulation)
set(tests "gyo_${GYO_CURRENT_APP}_tests")
add_executable(${tests} TestMain.cpp PvpMatchTests.cpp PredictionTests.cpp PredictionElapsedTimeTests.cpp TimelineTests.cpp MovementRecoveryTests.cpp
    CombatTests.cpp CombatHostTests.cpp ShotQueryTests.cpp CrouchTests.cpp LifePredictionTests.cpp MathCharacterizationTests.cpp WireTests.cpp
    ArenaDigestTests.cpp ShotGateTests.cpp HitFeedbackTests.cpp SoundCuesTests.cpp ClientSimulationTests.cpp ClientSimulationRoleTests.cpp
    NetworkStatisticsTests.cpp MatchHostRoleTests.cpp IngressStatisticsTests.cpp MatchIngressLedgerTests.cpp
    DiagnosticFileTests.cpp SessionSummaryTests.cpp ConnectionLivenessTests.cpp AddressTextTests.cpp)
target_compile_features(${tests} PRIVATE cxx_std_20)
target_compile_definitions(${tests} PRIVATE PVP_DOMAIN_TEST_ARENA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/arena.json"
    PVP_INGRESS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/ingress_v1"
    PVP_HIT_FEEDBACK_PATH="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp/ui/hit_feedback.json"
    PVP_SOUND_SETTINGS_PATH="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp/audio/sfx.json"
    PVP_PRODUCT_ARENA_PATH="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp/pvp_arena.json"
    PVP_CONNECTION_TIMING_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/connection_timing.json")
target_link_libraries(${tests} PRIVATE ${match_domain} ${runtime_host} ${client_simulation} GYO::Net gyo_test_support nlohmann_json::nlohmann_json doctest::doctest
    asio::asio) # ConnectionLivenessTests: the socket error codes asio reports
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
add_executable(${ipc_tests} TestMain.cpp IpcHostTests.cpp ClockSyncTests.cpp)
target_compile_features(${ipc_tests} PRIVATE cxx_std_20)
target_link_libraries(${ipc_tests} PRIVATE ${ipc} asio::asio "gyo_${GYO_CURRENT_APP}_runtime_v9" nlohmann_json::nlohmann_json doctest::doctest)
target_compile_definitions(${ipc_tests} PRIVATE PVP_DROP_REASONS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/drop_reasons.json"
    PVP_LOG_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/log_v1")
if(WIN32)
    target_compile_definitions(${ipc_tests} PRIVATE NOMINMAX)
endif()
if(MSVC)
    target_compile_options(${ipc_tests} PRIVATE /utf-8)
endif()
add_test(NAME "${GYO_CURRENT_APP}.ipc" COMMAND ${ipc_tests})
# Every case that waits for Ready waits its full deadline when the Match never
# answers, so a broken IPC host still reports its failures within the timeout.
set_tests_properties("${GYO_CURRENT_APP}.ipc" PROPERTIES LABELS "cpu;pvp;network" TIMEOUT 120)

# The Match's command line for its ingress detail file (match-ingress.jsonl):
# name derivation and refusal, and exit 1 when the file cannot be opened.
gyo_get_app_target("${GYO_CURRENT_APP}" match match)
add_test(NAME "${GYO_CURRENT_APP}.match_ingress_cli"
    COMMAND "${CMAKE_COMMAND}" -DMATCH=$<TARGET_FILE:${match}>
        "-DARENA=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/arena.json"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/match_ingress_cli" -P "${CMAKE_CURRENT_SOURCE_DIR}/match_ingress_cli_check.cmake")
set_tests_properties("${GYO_CURRENT_APP}.match_ingress_cli" PROPERTIES LABELS "cpu;pvp" TIMEOUT 120)

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
    GYO::PlatformSDL nlohmann_json::nlohmann_json gyo_test_support doctest::doctest)
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
# v8-product-hitbox-survey (plans/v8/hitbox-plan.md): a report-only diagnostic of the
# production player against the 11 hurt regions, staged with the presentation tests.
# Its smoke run checks only that every region node is present once.
set(hitbox_survey "gyo_${GYO_CURRENT_APP}_hitbox_survey")
add_executable(${hitbox_survey} HitboxSurvey.cpp)
target_compile_features(${hitbox_survey} PRIVATE cxx_std_20)
target_link_libraries(${hitbox_survey} PRIVATE ${support} GYO::AssetUfbx GYO::AssetSdlImage GYO::PlatformSDL nlohmann_json::nlohmann_json)
gyo_use_app_config(${hitbox_survey} "${GYO_CURRENT_APP}")
set_target_properties(${hitbox_survey} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${presentation_stage}/bin")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(${hitbox_survey} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${presentation_stage}/bin")
endforeach()
if(WIN32)
    target_compile_definitions(${hitbox_survey} PRIVATE NOMINMAX)
endif()
if(MSVC)
    target_compile_options(${hitbox_survey} PRIVATE /utf-8)
endif()
gyo_copy_runtime_dlls(${hitbox_survey})
add_dependencies(${hitbox_survey} ${presentation_content})
add_test(NAME "${GYO_CURRENT_APP}.hitbox_survey" COMMAND ${hitbox_survey} --smoke)
set_tests_properties("${GYO_CURRENT_APP}.hitbox_survey" PROPERTIES LABELS "cpu;pvp;presentation" TIMEOUT 120
    ENVIRONMENT "SDL_VIDEODRIVER=dummy")

# v8-product-anim-bottlenecks: a report-only measurement of a Match-side pose
# (sampling time, load, bit hashes for cross-platform comparison). It fails
# only on a load or self-check failure; RUN_SERIAL keeps the timing free of
# concurrently running tests. The wrapper reads the load's peak memory.
set(anim_bottlenecks "gyo_${GYO_CURRENT_APP}_anim_bottlenecks")
add_executable(${anim_bottlenecks} AnimBottlenecks.cpp)
target_compile_features(${anim_bottlenecks} PRIVATE cxx_std_20)
target_link_libraries(${anim_bottlenecks} PRIVATE ${support} GYO::AssetUfbx GYO::AssetSdlImage GYO::PlatformSDL nlohmann_json::nlohmann_json)
gyo_use_app_config(${anim_bottlenecks} "${GYO_CURRENT_APP}")
# A macOS cross build names its target in CMAKE_OSX_ARCHITECTURES; the
# processor variable is the host's there.
set(anim_bottlenecks_processor "${CMAKE_SYSTEM_PROCESSOR}")
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    set(anim_bottlenecks_processor "${CMAKE_OSX_ARCHITECTURES}")
endif()
target_compile_definitions(${anim_bottlenecks} PRIVATE
    GYO_ANIM_BOTTLENECKS_PLATFORM="${CMAKE_SYSTEM_NAME}-${anim_bottlenecks_processor}")
set_target_properties(${anim_bottlenecks} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${presentation_stage}/bin")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(${anim_bottlenecks} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${presentation_stage}/bin")
endforeach()
if(WIN32)
    target_compile_definitions(${anim_bottlenecks} PRIVATE NOMINMAX)
endif()
if(MSVC)
    target_compile_options(${anim_bottlenecks} PRIVATE /utf-8)
endif()
gyo_copy_runtime_dlls(${anim_bottlenecks})
add_dependencies(${anim_bottlenecks} ${presentation_content})
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME "${GYO_CURRENT_APP}.anim_bottlenecks"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/anim_bottlenecks.py" $<TARGET_FILE:${anim_bottlenecks}>)
set_tests_properties("${GYO_CURRENT_APP}.anim_bottlenecks" PROPERTIES LABELS "cpu;pvp;presentation" TIMEOUT 300
    RUN_SERIAL TRUE ENVIRONMENT "SDL_VIDEODRIVER=dummy")

# The in-world pointer capture policy against its golden (batch 06's oracle,
# recorded at the policy's own boundary; sdl-isolation J1, v7.1 D168).
set(capture_tests "gyo_${GYO_CURRENT_APP}_pointer_capture_tests")
add_executable(${capture_tests} PointerCaptureCharacterizationTests.cpp)
target_compile_features(${capture_tests} PRIVATE cxx_std_20)
target_link_libraries(${capture_tests} PRIVATE ${support} GYO::Input doctest::doctest)
target_compile_definitions(${capture_tests} PRIVATE
    GYO_POINTER_CAPTURE_GOLDEN="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/pointer_capture_golden.tsv")
if(WIN32)
    target_compile_definitions(${capture_tests} PRIVATE NOMINMAX)
endif()
if(MSVC)
    target_compile_options(${capture_tests} PRIVATE /utf-8)
endif()
gyo_copy_runtime_dlls(${capture_tests})
add_test(NAME "${GYO_CURRENT_APP}.pointer_capture" COMMAND ${capture_tests})
set_tests_properties("${GYO_CURRENT_APP}.pointer_capture" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
# sdl-isolation completion guard (a): the product, its probes and its tests
# call no SDL directly (docs/object_fps_pvp/plans/v7.1/sdl-isolation-ip4-declaration.md section 4).
add_test(NAME "${GYO_CURRENT_APP}.sdl_symbol_guard"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/sdl_symbol_guard.py" --root "${GYO_REPOSITORY_ROOT}")
set_tests_properties("${GYO_CURRENT_APP}.sdl_symbol_guard" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60)
# Guard (b): no target of this product links SDL, SDL_image or an Engine *Native
# target itself (its own LINK_LIBRARIES; Engine backends' $<LINK_ONLY:> SDL is
# theirs). Checked once the whole project is configured.
function(gyo_pvp_collect_targets directory out)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        gyo_pvp_collect_targets("${child}" nested)
        list(APPEND targets ${nested})
    endforeach()
    set(${out} ${targets} PARENT_SCOPE)
endfunction()
function(gyo_pvp_expect_no_direct_sdl)
    set(all)
    foreach(directory IN ITEMS apps/object_fps_pvp build/acceptance/object_fps_pvp tests/object_fps_pvp)
        if(EXISTS "${GYO_REPOSITORY_ROOT}/${directory}/CMakeLists.txt")
            gyo_pvp_collect_targets("${GYO_REPOSITORY_ROOT}/${directory}" targets)
            list(APPEND all ${targets})
        endif()
    endforeach()
    foreach(target IN LISTS all)
        get_target_property(type ${target} TYPE)
        if(type STREQUAL "UTILITY")
            continue()
        endif()
        get_target_property(links ${target} LINK_LIBRARIES)
        foreach(entry IN LISTS links)
            if(entry MATCHES "^SDL3(_image)?(::|-)" OR entry MATCHES "(Native|_native)$")
                message(FATAL_ERROR "SDL link guard: ${target} links ${entry} itself; the product calls no SDL "
                                    "directly (sdl-isolation, docs/object_fps_pvp/plans/v7.1/sdl-isolation-ip4-declaration.md)")
            endif()
        endforeach()
    endforeach()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_pvp_expect_no_direct_sdl)
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
add_test(NAME "${GYO_CURRENT_APP}.drop_reasons"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_drop_reasons.py")
set_tests_properties("${GYO_CURRENT_APP}.drop_reasons" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60)
# pv8-seq-ceiling: each wire counter exhaustion site decides with WireCounter.hpp (protocol-v8 section 2a).
add_test(NAME "${GYO_CURRENT_APP}.wire_counter_sites"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_wire_counter_sites.py")
set_tests_properties("${GYO_CURRENT_APP}.wire_counter_sites" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60)
add_test(NAME "${GYO_CURRENT_APP}.diagnostic_isolation"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_diagnostic_isolation.py")
set_tests_properties("${GYO_CURRENT_APP}.diagnostic_isolation" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60)
# The log Data Contract v1 (docs/object_fps_pvp/log-contract.zh-Hant.md): writer pins, the frozen readers on
# real samples, and the reverse guards.
add_test(NAME "${GYO_CURRENT_APP}.log_contract"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_log_contract.py")
set_tests_properties("${GYO_CURRENT_APP}.log_contract" PROPERTIES LABELS "cpu;pvp" TIMEOUT 120)
# The lan15 judge's discrimination check (docs/object_fps_pvp/plans/v7.1/lan15-declaration.md section 7).
add_test(NAME "${GYO_CURRENT_APP}.lan15_judge"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_lan15_judge.py")
set_tests_properties("${GYO_CURRENT_APP}.lan15_judge" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60)
add_test(NAME "${GYO_CURRENT_APP}.max_players"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_max_players.py")
set_tests_properties("${GYO_CURRENT_APP}.max_players" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60)
add_test(NAME "${GYO_CURRENT_APP}.mutation_tool"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_run_mutations.py")
set_tests_properties("${GYO_CURRENT_APP}.mutation_tool" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60)
# The scaled two-tree comparison (docs/object_fps_pvp/plans/v8/v8-product-hit-rules-permille-declaration.md P1).
add_test(NAME "${GYO_CURRENT_APP}.compare_authority_trees"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_compare_authority_trees.py")
set_tests_properties("${GYO_CURRENT_APP}.compare_authority_trees" PROPERTIES LABELS "cpu;pvp" TIMEOUT 60)
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
# N-gui-black-map-intermittent: the record-only window-image classifier (never a verdict).
add_test(NAME "${GYO_CURRENT_APP}.black_frame"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_black_frame.py")
set_tests_properties("${GYO_CURRENT_APP}.black_frame" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
# The macOS window watch around a GUI command, driven by a fake system (no GUI is started).
add_test(NAME "${GYO_CURRENT_APP}.black_map_watch"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_black_map_watch.py")
set_tests_properties("${GYO_CURRENT_APP}.black_map_watch" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 60)

add_test(NAME "${GYO_CURRENT_APP}.command_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_command_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.command_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
# Analyzer v7: the command rules whose meaning changed with the simulation role.
add_test(NAME "${GYO_CURRENT_APP}.command_evidence_v7"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_command_evidence_v7.py")
set_tests_properties("${GYO_CURRENT_APP}.command_evidence_v7" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
# Batch 08c F3: the ingress v1 evidence (IG, I0, I1, I2, I3, T3, J5a, S1, S2, trace) on the golden lines.
add_test(NAME "${GYO_CURRENT_APP}.ingress_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_ingress_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.ingress_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 60)
# Batch 08c F3: a real Gateway (built with Go) and a real Match in one short clean run of the timing probe; the
# ingress evidence must hold IG, I0, I1, I2, T3, J5a, S1, S2 and the trace. It needs Go and POSIX signals
# (77 = skipped on Windows), so it stays outside the cpu preset like the worker test: run it with -R.
add_test(NAME "${GYO_CURRENT_APP}.ingress_cross"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ingress_cross_check.py"
        --match $<TARGET_FILE:${match}> --probe $<TARGET_FILE:gyo_${GYO_CURRENT_APP}_timing_probe>
        --arena "${GYO_REPOSITORY_ROOT}/assets/${GYO_CURRENT_APP}/pvp_corners.json"
        --output "${CMAKE_CURRENT_BINARY_DIR}/ingress_cross" --go "${GYO_GO_EXECUTABLE}")
set_tests_properties("${GYO_CURRENT_APP}.ingress_cross" PROPERTIES LABELS "network;go;pvp" TIMEOUT 300
    SKIP_RETURN_CODE 77)
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

# The reader of the Match's gw_drop report lines (log-contract; v7.1 contract-readers).
add_test(NAME "${GYO_CURRENT_APP}.compare_reports"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_compare_reports.py")
set_tests_properties("${GYO_CURRENT_APP}.compare_reports" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)

add_test(NAME "${GYO_CURRENT_APP}.native_window_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_native_window.py")
set_tests_properties("${GYO_CURRENT_APP}.native_window_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)

add_test(NAME "${GYO_CURRENT_APP}.timing_runner"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_run_timing.py")
set_tests_properties("${GYO_CURRENT_APP}.timing_runner" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)

add_test(NAME "${GYO_CURRENT_APP}.action_runner"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_run_action_short.py")
set_tests_properties("${GYO_CURRENT_APP}.action_runner" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)

# Batch 08c L2 preparation: the Client->Gateway pure-delay runner (fake clock, no sockets or services).
add_test(NAME "${GYO_CURRENT_APP}.client_delay_runner"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_run_client_delay.py")
set_tests_properties("${GYO_CURRENT_APP}.client_delay_runner" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
# observability-tools OT-2: late corrections counted as increments, the two sources cross-checked.
# v7.1 outage-resync resync-l2-l3: the outage injection relay and its evidence checks.
add_test(NAME "${GYO_CURRENT_APP}.outage_runner"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_run_outage.py")
set_tests_properties("${GYO_CURRENT_APP}.outage_runner" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
add_test(NAME "${GYO_CURRENT_APP}.late_count"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_late_count.py")
set_tests_properties("${GYO_CURRENT_APP}.late_count" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
# observability-tools OT-3: every non-respawn reset of a run_network.py run, classified.
add_test(NAME "${GYO_CURRENT_APP}.network_resets"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_network_resets.py")
set_tests_properties("${GYO_CURRENT_APP}.network_resets" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
# observability-tools OT-4 (E4): the network relays' loss seed, its default reproducing the former constants.
add_test(NAME "${GYO_CURRENT_APP}.loss_seed"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_loss_seed.py")
set_tests_properties("${GYO_CURRENT_APP}.loss_seed" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)

add_test(NAME "${GYO_CURRENT_APP}.gameplay_soak"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_gameplay_soak_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.gameplay_soak" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 10)

add_test(NAME "${GYO_CURRENT_APP}.combat_gui_evidence"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_combat_gui_evidence.py")
set_tests_properties("${GYO_CURRENT_APP}.combat_gui_evidence" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)
# Analyzer v7 (D83): the HUD rule whose meaning changed with the simulation role.
add_test(NAME "${GYO_CURRENT_APP}.combat_gui_evidence_v7"
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/build/acceptance/object_fps_pvp/test_combat_gui_evidence_v7.py")
set_tests_properties("${GYO_CURRENT_APP}.combat_gui_evidence_v7" PROPERTIES LABELS "cpu;pvp;acceptance" TIMEOUT 30)

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
target_link_libraries(${digest_o0} PRIVATE GYO::Engine GYO::Math GYO::Collision GYO::Threads GYO::Trace GYO::Os
    nlohmann_json::nlohmann_json)
# Appended after the configuration's flags, so the last optimisation flag wins.
target_compile_options(${digest_o0} PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/Od,-O0>)
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
