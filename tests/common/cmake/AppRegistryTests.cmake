cmake_minimum_required(VERSION 3.30)
get_filename_component(repository "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${repository}/build/cmake/GyoAppRegistry.cmake")

# Failing cases run in child processes so a required FATAL_ERROR is observable.
if(DEFINED GYO_APP_REGISTRY_TEST_CASE)
    if(GYO_APP_REGISTRY_TEST_CASE STREQUAL "read")
        gyo_read_app_registry("${TEST_CSV}" registry)
    elseif(GYO_APP_REGISTRY_TEST_CASE STREQUAL "select")
        gyo_read_app_registry("${TEST_CSV}" registry)
        gyo_select_apps("${registry}" "${TEST_SYSTEM}" "${TEST_SELECTION}" selected)
    elseif(GYO_APP_REGISTRY_TEST_CASE STREQUAL "sources")
        gyo_validate_app_sources("${TEST_ROOT}" ${TEST_SELECTION})
    else()
        message(FATAL_ERROR "Unknown registry test case")
    endif()
    return()
endif()

if(NOT DEFINED GYO_APP_REGISTRY_TEST_WORK)
    set(GYO_APP_REGISTRY_TEST_WORK "${CMAKE_CURRENT_BINARY_DIR}/app-registry-tests")
endif()
set(work "${GYO_APP_REGISTRY_TEST_WORK}")
file(MAKE_DIRECTORY "${work}")
set(header "name,description,version,enabled,windows,linux,macos")

function(expect_equal actual expected context)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "${context}: expected '${expected}', got '${actual}'")
    endif()
endfunction()

function(expect_failure name expected)
    execute_process(COMMAND "${CMAKE_COMMAND}" ${ARGN}
        -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        TIMEOUT 15)
    if(result EQUAL 0)
        message(FATAL_ERROR "Invalid registry case '${name}' was accepted")
    endif()
    string(FIND "${output}\n${error}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Registry case '${name}' did not report '${expected}':\n${output}\n${error}")
    endif()
endfunction()

function(expect_bad_csv name body expected)
    set(path "${work}/${name}.csv")
    file(WRITE "${path}" "${body}")
    expect_failure("${name}" "${expected}"
        -DGYO_APP_REGISTRY_TEST_CASE=read "-DTEST_CSV=${path}")
endfunction()

# One fixture exercises UTF-8, BOM, CRLF, embedded commas/quotes/newlines,
# semicolons, backslashes, empty metadata, boolean spellings and no final newline.
string(ASCII 239 187 191 bom)
set(csv "${work}/rich.csv")
file(WRITE "${csv}" "${bom}${header}\r\n\r\n"
    "game_a,\"資料, \"\"quoted\"\"; C:\\assets\r\nsecond line\",v;anything/1,true,1,0,TRUE\r\n"
    "disabled,Not active,,false,true,true,true\r\n"
    "linux_only,,,1,0,1,0\r\n"
    "empty_text,,,1,0,0,1")
gyo_read_app_registry("${csv}" registry)
string(JSON count LENGTH "${registry}" projects)
expect_equal("${count}" "4" "Project count")
string(JSON description GET "${registry}" projects 0 description)
expect_equal("${description}" "資料, \"quoted\"; C:\\assets\nsecond line" "Opaque description")
string(JSON version GET "${registry}" projects 0 version)
expect_equal("${version}" "v;anything/1" "Opaque version")
string(JSON empty_version GET "${registry}" projects 2 version)
expect_equal("${empty_version}" "" "Empty metadata field")
string(JSON boolean_type TYPE "${registry}" projects 0 enabled)
expect_equal("${boolean_type}" "BOOLEAN" "Typed boolean")

# Guard against CMake dynamic scope accidentally inheriting this variable.
set(selected inherited_value)
gyo_select_apps("${registry}" Windows AUTO windows_apps)
gyo_select_apps("${registry}" Linux AUTO linux_apps)
gyo_select_apps("${registry}" Darwin AUTO macos_apps)
gyo_select_apps("${registry}" Generic "" no_apps)
gyo_select_apps("${registry}" Darwin "empty_text;game_a" explicit_apps)
expect_equal("${windows_apps}" "game_a" "Windows selection")
expect_equal("${linux_apps}" "linux_only" "Linux selection")
expect_equal("${macos_apps}" "game_a;empty_text" "macOS selection")
expect_equal("${no_apps}" "" "Explicit core-only selection")
expect_equal("${explicit_apps}" "empty_text;game_a" "Explicit subset order")

gyo_export_app_matrix("${registry}" matrix)
string(JSON count LENGTH "${matrix}" include)
expect_equal("${count}" "4" "Matrix count")
set(expected_names game_a game_a linux_only empty_text)
set(expected_platforms windows-x64 macos-arm64 linux-x64 macos-arm64)
foreach(index RANGE 0 3)
    string(JSON app GET "${matrix}" include ${index} app)
    string(JSON platform GET "${matrix}" include ${index} platform)
    list(GET expected_names ${index} expected_app)
    list(GET expected_platforms ${index} expected_platform)
    expect_equal("${app}" "${expected_app}" "Matrix app ${index}")
    expect_equal("${platform}" "${expected_platform}" "Matrix platform ${index}")
endforeach()

# Descriptions and versions may change arbitrarily without changing selection.
file(WRITE "${work}/metadata.csv"
    "${header}\nfirst,\"\"\"; arbitrary metadata\",not-a-version,1,1,1,1\n")
gyo_read_app_registry("${work}/metadata.csv" metadata_registry)
gyo_select_apps("${metadata_registry}" Linux AUTO metadata_apps)
expect_equal("${metadata_apps}" "first" "Metadata independence")
file(WRITE "${work}/empty.csv" "${header}\n")
gyo_read_app_registry("${work}/empty.csv" empty_registry)
gyo_select_apps("${empty_registry}" Windows AUTO empty_apps)
gyo_export_app_matrix("${empty_registry}" empty_matrix)
expect_equal("${empty_apps}" "" "Header-only registry")
string(JSON empty_count LENGTH "${empty_matrix}" include)
expect_equal("${empty_count}" "0" "Empty matrix")

foreach(app IN ITEMS game_a empty_text)
    file(MAKE_DIRECTORY "${work}/sources/apps/${app}")
    file(WRITE "${work}/sources/apps/${app}/CMakeLists.txt" "# selected app fixture\n")
endforeach()
gyo_validate_app_sources("${work}/sources" game_a empty_text)
gyo_validate_app_sources("${work}/sources")

expect_bad_csv(empty_file "" "missing its header")
expect_bad_csv(short_header "name,description,version\n" "exactly 7 columns")
expect_bad_csv(wrong_header "id,description,version,enabled,windows,linux,macos\n" "must be 'name'")
expect_bad_csv(short_record "${header}\napp,,,true,true,true\n" "exactly 7 columns")
expect_bad_csv(long_record "${header}\napp,,,true,true,true,true,extra\n" "exactly 7 columns")
expect_bad_csv(empty_last_field "${header}\napp,,,true,true,true,\n" "Invalid boolean")
expect_bad_csv(bad_name "${header}\n../app,,,true,true,true,true\n" "Invalid app name")
expect_bad_csv(uppercase_name "${header}\nApp,,,true,true,true,true\n" "Invalid app name")
foreach(reserved common toolchain ui_editor)
    expect_bad_csv("reserved_${reserved}" "${header}\n${reserved},,,true,true,true,true\n" "Reserved app name")
    expect_bad_csv("reserved_disabled_${reserved}" "${header}\n${reserved},,,false,false,false,false\n" "Reserved app name")
endforeach()
expect_bad_csv(duplicate "${header}\napp,,,true,true,true,true\napp,,,false,false,false,false\n" "Duplicate app name")
expect_bad_csv(bad_boolean "${header}\napp,,,ON,true,true,true\n" "Invalid boolean")
expect_bad_csv(unterminated "${header}\napp,\"open" "unterminated quoted field")
expect_bad_csv(quote_in_plain "${header}\napp,bad\"quote,,1,1,1,1\n" "quote inside an unquoted field")
expect_bad_csv(after_quote "${header}\napp,\"closed\"extra,,1,1,1,1\n" "unexpected character after a quoted field")
expect_failure(missing_file "does not exist"
    -DGYO_APP_REGISTRY_TEST_CASE=read "-DTEST_CSV=${work}/not-present.csv")

expect_failure(unknown_app "is not registered"
    -DGYO_APP_REGISTRY_TEST_CASE=select "-DTEST_CSV=${csv}"
    -DTEST_SYSTEM=Windows -DTEST_SELECTION=unknown)
expect_failure(disabled_app "is disabled in the registry"
    -DGYO_APP_REGISTRY_TEST_CASE=select "-DTEST_CSV=${csv}"
    -DTEST_SYSTEM=Windows -DTEST_SELECTION=disabled)
expect_failure(unsupported_app "is not enabled for target Windows"
    -DGYO_APP_REGISTRY_TEST_CASE=select "-DTEST_CSV=${csv}"
    -DTEST_SYSTEM=Windows -DTEST_SELECTION=linux_only)
# Escape the argument list separator at this one function boundary; the child
# still receives a normal semicolon-delimited CMake selection value.
expect_failure(duplicate_selection "appears more than once"
    -DGYO_APP_REGISTRY_TEST_CASE=select "-DTEST_CSV=${csv}"
    -DTEST_SYSTEM=Windows "-DTEST_SELECTION=game_a\;game_a")
expect_failure(unsupported_system "Unsupported app target system"
    -DGYO_APP_REGISTRY_TEST_CASE=select "-DTEST_CSV=${csv}"
    -DTEST_SYSTEM=Generic -DTEST_SELECTION=AUTO)
expect_failure(missing_source "Selected app 'missing' requires"
    -DGYO_APP_REGISTRY_TEST_CASE=sources "-DTEST_ROOT=${work}/sources"
    -DTEST_SELECTION=missing)
expect_failure(unsafe_source "Invalid selected app name"
    -DGYO_APP_REGISTRY_TEST_CASE=sources "-DTEST_ROOT=${work}/sources"
    -DTEST_SELECTION=../escape)

# Standalone exporter needs no project(), compiler, Python or dependency fetch.
execute_process(COMMAND "${CMAKE_COMMAND}" "-DGYO_REGISTRY_FILE=${csv}"
    -P "${repository}/build/cmake/ExportAppRegistry.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 15)
if(NOT result EQUAL 0 OR NOT "${stderr}" STREQUAL "")
    message(FATAL_ERROR "Registry stdout export failed: ${stdout}\n${stderr}")
endif()
string(JSON exported_count LENGTH "${stdout}" include)
expect_equal("${exported_count}" "4" "Exporter stdout JSON")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DGYO_REGISTRY_FILE=${csv}"
    "-DGYO_OUTPUT=${work}/export/matrix.json"
    -P "${repository}/build/cmake/ExportAppRegistry.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 15)
if(NOT result EQUAL 0 OR NOT "${stdout}${stderr}" STREQUAL "")
    message(FATAL_ERROR "Registry file export failed: ${stdout}\n${stderr}")
endif()
file(READ "${work}/export/matrix.json" exported)
string(JSON exported_count LENGTH "${exported}" include)
expect_equal("${exported_count}" "4" "Exporter output file JSON")

# Configure a compiler-free fixture to exercise the public API in project mode.
# It declares the registry as a configure dependency, just like the real root.
set(fixture "${work}/project")
file(MAKE_DIRECTORY "${fixture}/apps/first")
file(WRITE "${fixture}/apps/first/CMakeLists.txt" "add_custom_target(first)\n")
file(WRITE "${fixture}/registry.csv" "${header}\nfirst,,,true,true,true,true\n")
file(WRITE "${fixture}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.30)
project(AppRegistryFixture NONE)
include("${REGISTRY_MODULE}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/registry.csv")
get_property(registry_inputs DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS)
if(NOT "${CMAKE_SOURCE_DIR}/registry.csv" IN_LIST registry_inputs)
    message(FATAL_ERROR "Registry is missing from configure dependencies")
endif()
gyo_read_app_registry("${CMAKE_SOURCE_DIR}/registry.csv" registry)
gyo_select_apps("${registry}" "${CMAKE_SYSTEM_NAME}" "${SELECTION}" selected)
gyo_validate_app_sources("${CMAKE_SOURCE_DIR}" ${selected})
foreach(app IN LISTS selected)
    add_subdirectory("apps/${app}")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "${selected}")
]=])
set(generator_args "")
if(DEFINED GENERATOR AND NOT "${GENERATOR}" STREQUAL "")
    list(APPEND generator_args -G "${GENERATOR}")
endif()
if(DEFINED MAKE_PROGRAM AND NOT "${MAKE_PROGRAM}" STREQUAL "")
    list(APPEND generator_args "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif()
if(DEFINED PLATFORM AND NOT "${PLATFORM}" STREQUAL "")
    list(APPEND generator_args -A "${PLATFORM}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${fixture}" -B "${work}/project-build"
    ${generator_args} "-DREGISTRY_MODULE=${repository}/build/cmake/GyoAppRegistry.cmake" -DSELECTION=AUTO
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 30)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Registry configure fixture failed: ${stdout}\n${stderr}")
endif()
file(READ "${work}/project-build/selected.txt" fixture_selection)
expect_equal("${fixture_selection}" "first" "Project integration selection")
file(WRITE "${fixture}/registry.csv" "${header}\nfirst,,,false,true,true,true\n")
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${work}/project-build"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 30)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Registry automatic reconfigure failed (${result}): ${stdout}\n${stderr}")
endif()
file(READ "${work}/project-build/selected.txt" fixture_selection)
expect_equal("${fixture_selection}" "" "Registry change triggered automatic reconfigure")

message(STATUS "App registry: CSV, selection, source validation, export and reconfigure checks passed")
