cmake_minimum_required(VERSION 3.30)
# The Module and Plugin registers (declaration
# docs/architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md,
# section 3.2; Design Invariant O-6): reading and validating the two files,
# the units a product carries, and the table gyo_product_units generates.
get_filename_component(repository "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${repository}/build/cmake/GyoUnitRegistry.cmake")

# Failing cases run in child processes so a required FATAL_ERROR is observable.
if(DEFINED GYO_UNIT_REGISTRY_TEST_CASE)
    if(GYO_UNIT_REGISTRY_TEST_CASE STREQUAL "load")
        gyo_load_unit_registers(T "${TEST_MODULES}" "${TEST_PLUGINS}")
    elseif(GYO_UNIT_REGISTRY_TEST_CASE STREQUAL "select")
        gyo_load_unit_registers(T "${TEST_MODULES}" "${TEST_PLUGINS}")
        gyo_unit_selection(T selected DISABLE_MODULES ${TEST_DISABLE} MODULES ${TEST_MODULE_LIST} PLUGINS ${TEST_PLUGIN_LIST})
    else()
        message(FATAL_ERROR "Unknown unit register test case")
    endif()
    return()
endif()

if(NOT DEFINED GYO_UNIT_REGISTRY_TEST_WORK)
    set(GYO_UNIT_REGISTRY_TEST_WORK "${CMAKE_CURRENT_BINARY_DIR}/unit-registry-tests")
endif()
# Absolute: the probe project below reads these files from its own directory.
get_filename_component(work "${GYO_UNIT_REGISTRY_TEST_WORK}" ABSOLUTE)
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}")
set(header "name,target,description,library_only,default,depends,private_depends,unit,header")

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
        message(FATAL_ERROR "Invalid unit register case '${name}' was accepted")
    endif()
    # CMake wraps a long message across lines; compare with runs of white space as one space.
    string(REGEX REPLACE "[ \t\r\n]+" " " text "${output}\n${error}")
    string(FIND "${text}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Unit register case '${name}' did not report '${expected}':\n${output}\n${error}")
    endif()
endfunction()

# Writes <name>.modules.csv and <name>.plugins.csv (bodies without the header
# unless the body starts with "!", which replaces the header too).
function(write_registers name modules_body plugins_body)
    foreach(kind IN ITEMS modules plugins)
        set(body "${${kind}_body}")
        if(body MATCHES "^!")
            string(SUBSTRING "${body}" 1 -1 body)
            file(WRITE "${work}/${name}.${kind}.csv" "${body}")
        else()
            file(WRITE "${work}/${name}.${kind}.csv" "${header}\n${body}")
        endif()
    endforeach()
endfunction()

function(expect_bad_registers name modules_body plugins_body expected)
    write_registers("${name}" "${modules_body}" "${plugins_body}")
    expect_failure("${name}" "${expected}" -DGYO_UNIT_REGISTRY_TEST_CASE=load
        "-DTEST_MODULES=${work}/${name}.modules.csv" "-DTEST_PLUGINS=${work}/${name}.plugins.csv")
endfunction()

# --- The repository's own registers -----------------------------------------
gyo_load_unit_registers(REPO "${repository}/engine/config/modules.csv" "${repository}/engine/config/plugins.csv")
get_property(repo_order GLOBAL PROPERTY REPO_ORDER)
# Every engine directory with a CMakeLists.txt has a row, and every row has one
# (the register is where engine/CMakeLists.txt takes its directories from).
file(GLOB engine_dirs RELATIVE "${repository}/engine" "${repository}/engine/*/CMakeLists.txt")
set(engine_modules "")
foreach(entry IN LISTS engine_dirs)
    get_filename_component(dir "${entry}" DIRECTORY)
    list(APPEND engine_modules "${dir}")
endforeach()
foreach(dir IN LISTS engine_modules)
    if(NOT dir IN_LIST repo_order)
        message(FATAL_ERROR "engine/${dir} has a CMakeLists.txt but no row in engine/config/modules.csv")
    endif()
endforeach()
foreach(name IN LISTS repo_order)
    get_property(kind GLOBAL PROPERTY REPO_${name}_KIND)
    if(kind STREQUAL "Module" AND NOT EXISTS "${repository}/engine/${name}/CMakeLists.txt")
        message(FATAL_ERROR "modules.csv row '${name}' has no engine/${name}/CMakeLists.txt")
    endif()
endforeach()

# --- A valid pair: reading and the stored row --------------------------------
write_registers(good
    "core,GYO::Core,\"The core, with a comma\",yes,no,,,,\nwalk,T::Walk,Walking,no,yes,core,,Test::Walk,test/Walk.hpp\nextra,T::Extra,Extra,no,no,walk,core,Test::Extra,test/Extra.hpp\noff,T::Off,Off by default,no,YES,,,Test::Off,test/Off.hpp\n"
    "tool,T::Tool,A third-party tool,no,no,walk,,Test::Tool,test/Tool.hpp\nspare,T::Spare,Not carried,no,no,,,Test::Spare,test/Spare.hpp\nlib,T::Lib,A library-only plugin,yes,no,core,,,\n")
gyo_load_unit_registers(G "${work}/good.modules.csv" "${work}/good.plugins.csv")
get_property(order GLOBAL PROPERTY G_ORDER)
expect_equal("${order}" "core;walk;extra;off;tool;spare;lib" "register order")
get_property(value GLOBAL PROPERTY G_extra_DEPENDS)
expect_equal("${value}" "walk" "extra depends")
get_property(value GLOBAL PROPERTY G_extra_PRIVATE)
expect_equal("${value}" "core" "extra private_depends")
get_property(value GLOBAL PROPERTY G_tool_KIND)
expect_equal("${value}" "Plugin" "tool kind")
get_property(value GLOBAL PROPERTY G_off_DEFAULT)
expect_equal("${value}" "TRUE" "default YES spelled in capitals")
get_property(value GLOBAL PROPERTY G_core_LIBRARY_ONLY)
expect_equal("${value}" "TRUE" "core library_only")

# --- Selection --------------------------------------------------------------
gyo_unit_selection(G selected)
expect_equal("${selected}" "walk;off" "default selection: in-World Modules with default yes")
gyo_unit_selection(G selected DISABLE_MODULES off MODULES extra PLUGINS tool)
expect_equal("${selected}" "walk;extra;tool" "disable, add a default-no Module and a Plugin")
gyo_unit_selection(G selected PLUGINS tool MODULES extra)
expect_equal("${selected}" "walk;extra;off;tool" "register order, not argument order")

# --- Invalid files ----------------------------------------------------------
expect_bad_registers(header_name "!name,target,description,library_only,default,depends,private,unit,header\n" "" "must be 'private_depends'")
expect_bad_registers(header_count "!name,target\n" "" "must have 9 columns")
expect_bad_registers(row_count "a,T::A,A,yes,no,,,\n" "" "row 1: 8 columns")
expect_bad_registers(empty_name ",T::A,A,yes,no,,,,\n" "" "empty name")
expect_bad_registers(no_target "a,,A,yes,no,,,,\n" "" "'a' has no target")
expect_bad_registers(duplicate "a,T::A,A,yes,no,,,,\na,T::B,B,yes,no,,,,\n" "" "'a' is already in")
expect_bad_registers(duplicate_across "a,T::A,A,yes,no,,,,\n" "a,T::B,B,yes,no,,,,\n" "'a' is already in")
expect_bad_registers(missing_dep "a,T::A,A,yes,no,ghost,,,\n" "" "depends on 'ghost', which is in no register")
expect_bad_registers(missing_private_dep "a,T::A,A,yes,no,,ghost,,\n" "" "depends on 'ghost', which is in no register")
expect_bad_registers(later_dep "a,T::A,A,yes,no,b,,,\nb,T::B,B,yes,no,,,,\n" "" "depends on 'b', which comes later")
expect_bad_registers(self_dep "a,T::A,A,yes,no,a,,,\n" "" "depends on 'a', which comes later")
expect_bad_registers(module_on_plugin "a,T::A,A,yes,no,p,,,\n" "p,T::P,P,yes,no,,,,\n" "Module 'a' depends on Plugin 'p'; a Module does not depend on a Plugin")
expect_bad_registers(module_on_plugin_private "a,T::A,A,yes,no,,p,,\n" "p,T::P,P,yes,no,,,,\n" "Module 'a' depends on Plugin 'p'")
expect_bad_registers(plugin_later_plugin "" "p,T::P,P,yes,no,q,,,\nq,T::Q,Q,yes,no,,,,\n" "depends on 'q', which comes later")
expect_bad_registers(library_with_unit "a,T::A,A,yes,no,,,X::A,\n" "" "'a' is library_only but names a unit or header")
expect_bad_registers(library_with_header "a,T::A,A,yes,no,,,,a/A.hpp\n" "" "'a' is library_only but names a unit or header")
expect_bad_registers(unit_without_header "a,T::A,A,no,no,,,X::A,\n" "" "'a' goes into a World but has no unit or header")
expect_bad_registers(default_library "a,T::A,A,yes,yes,,,,\n" "" "'a' has default yes, but only an in-World Module may")
expect_bad_registers(default_plugin "" "p,T::P,P,no,yes,,,X::P,p/P.hpp\n" "'p' has default yes, but only an in-World Module may")
expect_bad_registers(bad_yes_no "a,T::A,A,maybe,no,,,,\n" "" "column 'library_only' must be yes or no")
expect_failure(missing_file "Registry does not exist" -DGYO_UNIT_REGISTRY_TEST_CASE=load
    "-DTEST_MODULES=${work}/absent.csv" "-DTEST_PLUGINS=${work}/absent.csv")

# --- Invalid selections -----------------------------------------------------
function(expect_bad_selection name expected)
    expect_failure("${name}" "${expected}" -DGYO_UNIT_REGISTRY_TEST_CASE=select
        "-DTEST_MODULES=${work}/good.modules.csv" "-DTEST_PLUGINS=${work}/good.plugins.csv" ${ARGN})
endfunction()
expect_bad_selection(select_unknown "MODULES names 'ghost', which is no in-World Module" "-DTEST_MODULE_LIST=ghost")
expect_bad_selection(select_library "MODULES names 'core', which is no in-World Module" "-DTEST_MODULE_LIST=core")
expect_bad_selection(select_plugin_as_module "MODULES names 'tool', which is no in-World Module" "-DTEST_MODULE_LIST=tool")
expect_bad_selection(select_module_as_plugin "PLUGINS names 'walk', which is no in-World Plugin" "-DTEST_PLUGIN_LIST=walk")
expect_bad_selection(disable_unknown "DISABLE_MODULES names 'ghost', which is no in-World Module" "-DTEST_DISABLE=ghost")
expect_bad_selection(disable_needed "'tool' depends on 'walk', which the product does not carry" "-DTEST_DISABLE=walk" "-DTEST_PLUGIN_LIST=tool")
expect_bad_selection(needed_not_listed "'extra' depends on 'walk', which the product does not carry" "-DTEST_DISABLE=walk" "-DTEST_MODULE_LIST=extra")

# --- gyo_product_units in a configured project --------------------------------
# PROBE_ARGS separates with commas (a semicolon would split the -D argument).
# Configure only: the generated table and the configure-time refusals. The
# build and the run of a generated table are orchestration.units (C++).
set(project_dir "${work}/project")
file(MAKE_DIRECTORY "${project_dir}")
string(CONCAT project_text
    "cmake_minimum_required(VERSION 3.30)\n"
    "project(UnitRegistryProbe LANGUAGES CXX)\n"
    "include(\"${repository}/build/cmake/GyoUnitRegistry.cmake\")\n"
    "foreach(name IN ITEMS Orchestration Core Walk Extra Off Tool Spare Lib)\n"
    "    add_library(probe_\${name} INTERFACE)\n"
    "    add_library(GYO::\${name} ALIAS probe_\${name})\n"
    "endforeach()\n"
    "foreach(name IN ITEMS Walk Extra Off Tool Spare Lib)\n"
    "    add_library(T::\${name} ALIAS probe_\${name})\n"
    "endforeach()\n"
    "add_library(product STATIC)\n"
    "string(REPLACE \",\" \";\" probe_args \"\${PROBE_ARGS}\")\n"
    "gyo_product_units(product \${probe_args} REGISTERS \"${work}/good.modules.csv\" \"${work}/good.plugins.csv\")\n"
    "get_target_property(links product LINK_LIBRARIES)\n"
    "message(STATUS \"PROBE_LINKS=\${links}\")\n"
    "gyo_load_unit_registers(GYO_UNITS \"${work}/good.modules.csv\" \"${work}/good.plugins.csv\")\n"
    "file(WRITE \"\${CMAKE_BINARY_DIR}/empty.cpp\" \"\")\n"
    "add_library(gyo_extra STATIC \"\${CMAKE_BINARY_DIR}/empty.cpp\")\n"
    "add_library(gyo_walk INTERFACE)\n"
    "gyo_link_registered_dependencies(extra)\n"
    "gyo_link_registered_dependencies(walk)\n"
    "foreach(probe IN ITEMS gyo_extra gyo_walk)\n"
    "    get_target_property(own \${probe} LINK_LIBRARIES)\n"
    "    get_target_property(interface \${probe} INTERFACE_LINK_LIBRARIES)\n"
    "    message(STATUS \"PROBE_\${probe}=\${own}|\${interface}\")\n"
    "endforeach()\n")
file(WRITE "${project_dir}/CMakeLists.txt" "${project_text}")

set(generator_args "")
if(DEFINED GENERATOR AND NOT GENERATOR STREQUAL "")
    list(APPEND generator_args -G "${GENERATOR}")
endif()
if(DEFINED MAKE_PROGRAM AND NOT MAKE_PROGRAM STREQUAL "")
    list(APPEND generator_args "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif()
if(DEFINED PLATFORM AND NOT PLATFORM STREQUAL "")
    list(APPEND generator_args -A "${PLATFORM}")
endif()
if(DEFINED CXX_COMPILER AND NOT CXX_COMPILER STREQUAL "")
    list(APPEND generator_args "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
endif()

function(configure_probe name result_var output_var)
    set(binary "${work}/probe-${name}")
    file(REMOVE_RECURSE "${binary}")
    execute_process(COMMAND "${CMAKE_COMMAND}" ${generator_args} -S "${project_dir}" -B "${binary}" ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 120)
    set(${result_var} "${result}" PARENT_SCOPE)
    set(${output_var} "${output}\n${error}" PARENT_SCOPE)
endfunction()

configure_probe(selected result output "-DPROBE_ARGS=DISABLE_MODULES,off,MODULES,extra,PLUGINS,tool")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "The probe project did not configure:\n${output}")
endif()
file(READ "${work}/probe-selected/product_units.cpp" generated)
foreach(expected IN ITEMS
        "#include \"engine/orchestration/RegisteredUnits.hpp\""
        "#include \"test/Walk.hpp\"" "#include \"test/Extra.hpp\"" "#include \"test/Tool.hpp\""
        "RegisteredUnits MakeRegisteredUnits()"
        "units.rows.push_back(RegisterRow{\"core\", UnitKind::Module, true, false, {}, {}})"
        "units.rows.push_back(RegisterRow{\"walk\", UnitKind::Module, false, true, {\"core\", }, {}})"
        "units.rows.push_back(RegisterRow{\"extra\", UnitKind::Module, false, false, {\"walk\", }, {\"core\", }})"
        "units.rows.push_back(RegisterRow{\"off\", UnitKind::Module, false, true, {}, {}})"
        "units.rows.push_back(RegisterRow{\"tool\", UnitKind::Plugin, false, false, {\"walk\", }, {}})"
        "units.rows.push_back(RegisterRow{\"spare\", UnitKind::Plugin, false, false, {}, {}})"
        "units.rows.push_back(RegisterRow{\"lib\", UnitKind::Plugin, true, false, {\"core\", }, {}})"
        "units.modules.push_back(UnitEntry<Module>{\"walk\", std::make_unique<Test::Walk>()})"
        "units.modules.push_back(UnitEntry<Module>{\"extra\", std::make_unique<Test::Extra>()})"
        "units.plugins.push_back(UnitEntry<Plugin>{\"tool\", std::make_unique<Test::Tool>()})")
    string(FIND "${generated}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "The generated table lacks '${expected}':\n${generated}")
    endif()
endforeach()
foreach(absent IN ITEMS "Test::Off" "Test::Spare" "test/Off.hpp" "test/Spare.hpp")
    string(FIND "${generated}" "${absent}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "The generated table carries '${absent}', which is not selected:\n${generated}")
    endif()
endforeach()
# Rows in register order, instances in register order (walk before extra).
string(FIND "${generated}" "RegisterRow{\"core\"" core_at)
string(FIND "${generated}" "RegisterRow{\"lib\"" lib_at)
string(FIND "${generated}" "UnitEntry<Module>{\"walk\"" walk_at)
string(FIND "${generated}" "UnitEntry<Module>{\"extra\"" extra_at)
if(NOT core_at LESS lib_at OR NOT walk_at LESS extra_at)
    message(FATAL_ERROR "The generated table is not in register order:\n${generated}")
endif()
# The product links the orchestration layer and exactly the carried units' targets.
string(REGEX MATCH "PROBE_LINKS=[^\n]*" links "${output}")
expect_equal("${links}" "PROBE_LINKS=GYO::Orchestration;T::Walk;T::Extra;T::Tool" "the product's links")

# gyo_link_registered_dependencies: depends PUBLIC, private_depends PRIVATE
# (seen as $<LINK_ONLY:...> in the interface), INTERFACE for an interface library.
string(REGEX MATCH "PROBE_gyo_extra=[^\n]*" links "${output}")
expect_equal("${links}" "PROBE_gyo_extra=T::Walk;GYO::Core|T::Walk;$<LINK_ONLY:GYO::Core>" "extra's registered links")
string(REGEX MATCH "PROBE_gyo_walk=[^\n]*" links "${output}")
expect_equal("${links}" "PROBE_gyo_walk=own-NOTFOUND|GYO::Core" "walk's registered links (an interface library)")

configure_probe(refused result output "-DPROBE_ARGS=DISABLE_MODULES,walk,PLUGINS,tool")
if(result EQUAL 0)
    message(FATAL_ERROR "A product that disables a carried unit's dependency was configured")
endif()
string(REGEX REPLACE "[ \t\r\n]+" " " text "${output}")
string(FIND "${text}" "'tool' depends on 'walk', which the product does not carry" found)
if(found EQUAL -1)
    message(FATAL_ERROR "The refused probe did not name the dependency:\n${output}")
endif()

message(STATUS "Unit register tests passed")
