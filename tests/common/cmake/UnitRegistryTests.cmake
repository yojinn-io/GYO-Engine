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
        gyo_unit_selection(T selected ${TEST_EXTRA} DISABLE_MODULES ${TEST_DISABLE} MODULES ${TEST_MODULE_LIST} PLUGINS ${TEST_PLUGIN_LIST})
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
# Cells with two names (extra, aux, gadget), an in-World private dependency (aux on
# extra), a Plugin on a Plugin (gadget on tool) and two carried Plugins.
write_registers(good
    "core,GYO::Core,\"The core, with a comma\",yes,no,,,,\nutil,GYO::Util,A second library row,yes,no,core,,,\nwalk,T::Walk,Walking,no,yes,core,,Test::Walk,test/Walk.hpp\nextra,T::Extra,Extra,no,no,walk,core;util,Test::Extra,test/Extra.hpp\naux,T::Aux,Two dependencies and an in-World private one,no,no,core;walk,extra,Test::Aux,test/Aux.hpp\noff,T::Off,Off by default,no,YES,,,Test::Off,test/Off.hpp\n"
    "tool,T::Tool,A third-party tool,no,no,walk,,Test::Tool,test/Tool.hpp\ngadget,T::Gadget,A Plugin on a Plugin,no,no,tool;walk,,Test::Gadget,test/Gadget.hpp\nspare,T::Spare,Not carried,no,no,,,Test::Spare,test/Spare.hpp\nlib,T::Lib,A library-only plugin,yes,no,core,,,\n")
gyo_load_unit_registers(G "${work}/good.modules.csv" "${work}/good.plugins.csv")
get_property(order GLOBAL PROPERTY G_ORDER)
expect_equal("${order}" "core;util;walk;extra;aux;off;tool;gadget;spare;lib" "register order")
get_property(value GLOBAL PROPERTY G_extra_DEPENDS)
expect_equal("${value}" "walk" "extra depends")
get_property(value GLOBAL PROPERTY G_extra_PRIVATE)
expect_equal("${value}" "core;util" "extra private_depends (two names)")
get_property(value GLOBAL PROPERTY G_aux_DEPENDS)
expect_equal("${value}" "core;walk" "aux depends (two names)")
get_property(value GLOBAL PROPERTY G_tool_KIND)
expect_equal("${value}" "Plugin" "tool kind")
get_property(value GLOBAL PROPERTY G_off_DEFAULT)
expect_equal("${value}" "TRUE" "default YES spelled in capitals")
get_property(value GLOBAL PROPERTY G_core_LIBRARY_ONLY)
expect_equal("${value}" "TRUE" "core library_only")
# A change to a register re-runs configure.
get_property(value DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS)
foreach(file IN ITEMS good.modules.csv good.plugins.csv)
    if(NOT "${work}/${file}" IN_LIST value)
        message(FATAL_ERROR "The register ${file} is not a configure dependency: ${value}")
    endif()
endforeach()

# --- Selection --------------------------------------------------------------
gyo_unit_selection(G selected)
expect_equal("${selected}" "walk;off" "default selection: in-World Modules with default yes")
gyo_unit_selection(G selected DISABLE_MODULES off MODULES extra PLUGINS tool)
expect_equal("${selected}" "walk;extra;tool" "disable, add a default-no Module and a Plugin")
gyo_unit_selection(G selected PLUGINS gadget tool MODULES aux extra)
expect_equal("${selected}" "walk;extra;aux;off;tool;gadget" "register order, not argument order")

# --- Invalid files ----------------------------------------------------------
expect_bad_registers(header_name "!name,target,description,library_only,default,depends,private,unit,header\n" "" "must be 'private_depends'")
expect_bad_registers(header_count "!name,target\n" "" "must have 9 columns")
expect_bad_registers(header_excess "!name,target,description,library_only,default,depends,private_depends,unit,header,extra\n" "" "must have 9 columns")
expect_bad_registers(empty_file "!" "" "is missing its header")
# Before row_count: a row with too many columns would otherwise be read silently.
expect_bad_registers(row_excess "a,T::A,A,yes,no,,,,,x\n" "" "row_excess.modules.csv row 1: 10 columns, expected 9")
expect_bad_registers(row_count "a,T::A,A,yes,no,,,\n" "" "row_count.modules.csv row 1: 8 columns")
expect_bad_registers(empty_name ",T::A,A,yes,no,,,,\n" "" "empty name")
expect_bad_registers(bad_name "Bad,T::A,A,yes,no,,,,\n" "" "name 'Bad' is not a directory name")
expect_bad_registers(no_target "a,,A,yes,no,,,,\n" "" "'a' has no target")
expect_bad_registers(duplicate "a,T::A,A,yes,no,,,,\na,T::B,B,yes,no,,,,\n" "" "duplicate.modules.csv row 2: 'a' is already in ${work}/duplicate.modules.csv")
expect_bad_registers(duplicate_across "a,T::A,A,yes,no,,,,\n" "a,T::B,B,yes,no,,,,\n" "duplicate_across.plugins.csv row 1: 'a' is already in ${work}/duplicate_across.modules.csv")
expect_bad_registers(missing_dep "a,T::A,A,yes,no,ghost,,,\n" "" "missing_dep.modules.csv row 1: 'a' depends on 'ghost', which is in no register")
expect_bad_registers(missing_private_dep "b,T::B,B,yes,no,,,,\na,T::A,A,yes,no,,ghost,,\n" "" "missing_private_dep.modules.csv row 2: 'a' depends on 'ghost', which is in no register")
expect_bad_registers(later_dep "a,T::A,A,yes,no,b,,,\nb,T::B,B,yes,no,,,,\n" "" "later_dep.modules.csv row 1: 'a' depends on 'b', which comes later")
expect_bad_registers(self_dep "a,T::A,A,yes,no,a,,,\n" "" "depends on 'a', which comes later")
expect_bad_registers(module_on_plugin "a,T::A,A,yes,no,p,,,\n" "p,T::P,P,yes,no,,,,\n" "module_on_plugin.modules.csv row 1: Module 'a' depends on Plugin 'p'; a Module does not depend on a Plugin")
expect_bad_registers(module_on_plugin_private "a,T::A,A,yes,no,,p,,\n" "p,T::P,P,yes,no,,,,\n" "Module 'a' depends on Plugin 'p'")
expect_bad_registers(plugin_later_plugin "" "p,T::P,P,yes,no,q,,,\nq,T::Q,Q,yes,no,,,,\n" "plugin_later_plugin.plugins.csv row 1: 'p' depends on 'q', which comes later")
expect_bad_registers(library_with_unit "a,T::A,A,yes,no,,,X::A,\n" "" "'a' is library_only but names a unit or header")
expect_bad_registers(library_with_header "a,T::A,A,yes,no,,,,a/A.hpp\n" "" "'a' is library_only but names a unit or header")
expect_bad_registers(unit_without_header "a,T::A,A,no,no,,,X::A,\n" "" "'a' goes into a World but has no unit or header")
expect_bad_registers(header_without_unit "a,T::A,A,no,no,,,,a/A.hpp\n" "" "'a' goes into a World but has no unit or header")
expect_bad_registers(bad_unit "a,T::A,A,no,no,,,X A,a/A.hpp\n" "" "'a' unit 'X A' is not a C++ class name")
expect_bad_registers(bad_header "a,T::A,A,no,no,,,X::A,a A.hpp\n" "" "'a' header 'a A.hpp' is not a header path")
expect_bad_registers(default_library "a,T::A,A,yes,yes,,,,\n" "" "'a' has default yes, but only an in-World Module may")
expect_bad_registers(default_plugin "" "p,T::P,P,no,yes,,,X::P,p/P.hpp\n" "'p' has default yes, but only an in-World Module may")
expect_bad_registers(default_needs_listed "w,T::W,W,no,no,,,X::W,w/W.hpp\nd,T::D,D,no,yes,w,,X::D,d/D.hpp\n" "" "'d' is carried by default but depends on 'w', which is not")
# Read as no, each would be a valid row: only the strict reading refuses them.
expect_bad_registers(bad_yes_no "a,T::A,A,maybe,no,,,X::A,a/A.hpp\n" "" "column 'library_only' must be yes or no")
expect_bad_registers(bad_default "a,T::A,A,no,perhaps,,,X::A,a/A.hpp\n" "" "column 'default' must be yes or no")
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
expect_bad_selection(disable_not_default "DISABLE_MODULES names 'extra', which is not carried by default" "-DTEST_DISABLE=extra")
expect_bad_selection(disable_and_list "'off' is in both DISABLE_MODULES and MODULES" "-DTEST_DISABLE=off" "-DTEST_MODULE_LIST=off")
expect_bad_selection(disable_needed "'tool' depends on 'walk', which the product does not carry" "-DTEST_DISABLE=walk" "-DTEST_PLUGIN_LIST=tool")
expect_bad_selection(private_needed_not_listed "'aux' depends on 'extra', which the product does not carry" "-DTEST_MODULE_LIST=aux")
expect_bad_selection(plugin_needed_not_listed "'gadget' depends on 'tool', which the product does not carry" "-DTEST_PLUGIN_LIST=gadget")
expect_bad_selection(unknown_argument "unknown arguments 'DISABLE_MODULE'" "-DTEST_EXTRA=DISABLE_MODULE")

# --- gyo_product_units in a configured project --------------------------------
# PROBE_ARGS separates with commas (a semicolon would split the -D argument).
# Configure only: the generated tables and the links. The build and the run of
# a generated table are orchestration.units (C++).
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

set(probe_prelude
    "cmake_minimum_required(VERSION 3.30)\n"
    "include(\"${repository}/build/cmake/GyoUnitRegistry.cmake\")\n"
    "foreach(name IN ITEMS Orchestration Core Util Walk Extra Aux Off Tool Gadget Spare Lib)\n"
    "    add_library(probe_\${name} INTERFACE)\n"
    "    add_library(GYO::\${name} ALIAS probe_\${name})\n"
    "    add_library(T::\${name} ALIAS probe_\${name})\n"
    "endforeach()\n")
string(CONCAT probe_prelude ${probe_prelude})

set(project_dir "${work}/project")
file(MAKE_DIRECTORY "${project_dir}")
string(CONCAT project_text
    "cmake_minimum_required(VERSION 3.30)\n"
    "project(UnitRegistryProbe LANGUAGES CXX)\n"
    "${probe_prelude}"
    "add_library(product STATIC)\n"
    "string(REPLACE \",\" \";\" probe_args \"\${PROBE_ARGS}\")\n"
    "gyo_product_units(product \${probe_args} REGISTERS \"${work}/good.modules.csv\" \"${work}/good.plugins.csv\")\n"
    "get_target_property(links product LINK_LIBRARIES)\n"
    "message(STATUS \"PROBE_LINKS=\${links}\")\n"
    "get_property(before GLOBAL PROPERTY GYO_UNITS_ORDER)\n"
    "message(STATUS \"PROBE_UNITS_BEFORE=[\${before}]\")\n"
    "gyo_load_unit_registers(GYO_UNITS \"${work}/good.modules.csv\" \"${work}/good.plugins.csv\")\n"
    "add_library(product_default STATIC)\n"
    "gyo_product_units(product_default)\n"
    "get_target_property(links product_default LINK_LIBRARIES)\n"
    "message(STATUS \"PROBE_DEFAULT_LINKS=\${links}\")\n"
    "file(WRITE \"\${CMAKE_BINARY_DIR}/empty.cpp\" \"\")\n"
    "add_library(gyo_extra STATIC \"\${CMAKE_BINARY_DIR}/empty.cpp\")\n"
    "add_library(gyo_aux STATIC \"\${CMAKE_BINARY_DIR}/empty.cpp\")\n"
    "add_library(gyo_walk INTERFACE)\n"
    "gyo_link_registered_dependencies(extra)\n"
    "gyo_link_registered_dependencies(aux)\n"
    "gyo_link_registered_dependencies(walk)\n"
    "foreach(probe IN ITEMS gyo_extra gyo_aux gyo_walk)\n"
    "    get_target_property(own \${probe} LINK_LIBRARIES)\n"
    "    get_target_property(interface \${probe} INTERFACE_LINK_LIBRARIES)\n"
    "    message(STATUS \"PROBE_\${probe}=\${own}|\${interface}\")\n"
    "endforeach()\n")
file(WRITE "${project_dir}/CMakeLists.txt" "${project_text}")

function(configure_probe source name result_var output_var)
    set(binary "${work}/probe-${name}")
    file(REMOVE_RECURSE "${binary}")
    execute_process(COMMAND "${CMAKE_COMMAND}" ${generator_args} -S "${source}" -B "${binary}" ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 120)
    set(${result_var} "${result}" PARENT_SCOPE)
    set(${output_var} "${output}\n${error}" PARENT_SCOPE)
endfunction()

# One line of the probe's output, without a carriage return.
function(probe_line output key result_var)
    string(REGEX MATCH "${key}=[^\r\n]*" line "${output}")
    set(${result_var} "${line}" PARENT_SCOPE)
endfunction()

configure_probe("${project_dir}" selected result output "-DPROBE_ARGS=DISABLE_MODULES,off,MODULES,extra,aux,PLUGINS,tool")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "The probe project did not configure:\n${output}")
endif()
file(READ "${work}/probe-selected/product_units.cpp" generated)
foreach(expected IN ITEMS
        "#include \"engine/orchestration/RegisteredUnits.hpp\""
        "#include \"test/Walk.hpp\"" "#include \"test/Extra.hpp\"" "#include \"test/Aux.hpp\"" "#include \"test/Tool.hpp\""
        "RegisteredUnits MakeRegisteredUnits()"
        "units.rows.push_back(RegisterRow{\"core\", UnitKind::Module, true, false, {}, {}})"
        "units.rows.push_back(RegisterRow{\"walk\", UnitKind::Module, false, true, {\"core\", }, {}})"
        "units.rows.push_back(RegisterRow{\"util\", UnitKind::Module, true, false, {\"core\", }, {}})"
        "units.rows.push_back(RegisterRow{\"extra\", UnitKind::Module, false, false, {\"walk\", }, {\"core\", \"util\", }})"
        "units.rows.push_back(RegisterRow{\"aux\", UnitKind::Module, false, false, {\"core\", \"walk\", }, {\"extra\", }})"
        "units.rows.push_back(RegisterRow{\"off\", UnitKind::Module, false, true, {}, {}})"
        "units.rows.push_back(RegisterRow{\"tool\", UnitKind::Plugin, false, false, {\"walk\", }, {}})"
        "units.rows.push_back(RegisterRow{\"gadget\", UnitKind::Plugin, false, false, {\"tool\", \"walk\", }, {}})"
        "units.rows.push_back(RegisterRow{\"spare\", UnitKind::Plugin, false, false, {}, {}})"
        "units.rows.push_back(RegisterRow{\"lib\", UnitKind::Plugin, true, false, {\"core\", }, {}})"
        "units.modules.push_back(UnitEntry<Module>{\"walk\", std::make_unique<Test::Walk>()})"
        "units.modules.push_back(UnitEntry<Module>{\"extra\", std::make_unique<Test::Extra>()})"
        "units.modules.push_back(UnitEntry<Module>{\"aux\", std::make_unique<Test::Aux>()})"
        "units.plugins.push_back(UnitEntry<Plugin>{\"tool\", std::make_unique<Test::Tool>()})")
    string(FIND "${generated}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "The generated table lacks '${expected}':\n${generated}")
    endif()
endforeach()
foreach(absent IN ITEMS "Test::Off" "Test::Spare" "Test::Gadget" "test/Off.hpp" "test/Spare.hpp")
    string(FIND "${generated}" "${absent}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "The generated table carries '${absent}', which is not selected:\n${generated}")
    endif()
endforeach()
# Rows in register order, instances in register order (walk before extra before aux).
string(FIND "${generated}" "RegisterRow{\"core\"" core_at)
string(FIND "${generated}" "RegisterRow{\"lib\"" lib_at)
string(FIND "${generated}" "UnitEntry<Module>{\"walk\"" walk_at)
string(FIND "${generated}" "UnitEntry<Module>{\"extra\"" extra_at)
string(FIND "${generated}" "UnitEntry<Module>{\"aux\"" aux_at)
if(NOT core_at LESS lib_at OR NOT walk_at LESS extra_at OR NOT extra_at LESS aux_at)
    message(FATAL_ERROR "The generated table is not in register order:\n${generated}")
endif()
# The product links the orchestration layer and exactly the carried units' targets.
probe_line("${output}" PROBE_LINKS links)
expect_equal("${links}" "PROBE_LINKS=GYO::Orchestration;T::Walk;T::Extra;T::Aux;T::Tool" "the product's links")
# REGISTERS reads the synthetic registers under the target's own prefix.
probe_line("${output}" PROBE_UNITS_BEFORE before)
expect_equal("${before}" "PROBE_UNITS_BEFORE=[]" "REGISTERS leaves the loaded registers alone")
# Without REGISTERS (what a product writes): the loaded registers, the default selection.
file(READ "${work}/probe-selected/product_default_units.cpp" generated_default)
foreach(expected IN ITEMS "UnitEntry<Module>{\"walk\"" "UnitEntry<Module>{\"off\"" "RegisterRow{\"gadget\"")
    string(FIND "${generated_default}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "The default table lacks '${expected}':\n${generated_default}")
    endif()
endforeach()
probe_line("${output}" PROBE_DEFAULT_LINKS links)
expect_equal("${links}" "PROBE_DEFAULT_LINKS=GYO::Orchestration;T::Walk;T::Off" "the default product's links")

# gyo_link_registered_dependencies: depends PUBLIC, private_depends PRIVATE
# (seen as $<LINK_ONLY:...> in the interface), INTERFACE for an interface library.
probe_line("${output}" PROBE_gyo_extra links)
expect_equal("${links}" "PROBE_gyo_extra=T::Walk;GYO::Core;GYO::Util|T::Walk;$<LINK_ONLY:GYO::Core>;$<LINK_ONLY:GYO::Util>" "extra's registered links")
probe_line("${output}" PROBE_gyo_aux links)
expect_equal("${links}" "PROBE_gyo_aux=GYO::Core;T::Walk;T::Extra|GYO::Core;T::Walk;$<LINK_ONLY:T::Extra>" "aux's registered links")
probe_line("${output}" PROBE_gyo_walk links)
expect_equal("${links}" "PROBE_gyo_walk=own-NOTFOUND|GYO::Core" "walk's registered links (an interface library)")

# --- Refusals at configure (no compiler: LANGUAGES NONE) ---------------------
set(errors_dir "${work}/errors")
file(MAKE_DIRECTORY "${errors_dir}")
string(CONCAT errors_text
    "cmake_minimum_required(VERSION 3.30)\n"
    "project(UnitRegistryRefusals LANGUAGES NONE)\n"
    "include(\"${repository}/build/cmake/GyoUnitRegistry.cmake\")\n"
    "if(NOT PROBE_CASE STREQUAL \"dep_not_defined\")\n"
    "${probe_prelude}"
    "endif()\n"
    "gyo_load_unit_registers(GYO_UNITS \"${work}/good.modules.csv\" \"${work}/good.plugins.csv\")\n"
    "add_library(product INTERFACE)\n"
    "if(PROBE_CASE STREQUAL \"link_unregistered\")\n"
    "    add_library(gyo_ghost INTERFACE)\n"
    "    gyo_link_registered_dependencies(ghost)\n"
    "elseif(PROBE_CASE STREQUAL \"link_no_target\")\n"
    "    gyo_link_registered_dependencies(extra)\n"
    "elseif(PROBE_CASE STREQUAL \"link_interface_private\")\n"
    "    add_library(gyo_extra INTERFACE)\n"
    "    gyo_link_registered_dependencies(extra)\n"
    "elseif(PROBE_CASE STREQUAL \"dep_not_defined\")\n"
    "    add_library(gyo_walk INTERFACE)\n"
    "    gyo_link_registered_dependencies(walk)\n"
    "elseif(PROBE_CASE STREQUAL \"registers_three\")\n"
    "    gyo_product_units(product REGISTERS a.csv b.csv c.csv)\n"
    "elseif(PROBE_CASE STREQUAL \"registers_none\")\n"
    "    gyo_product_units(product REGISTERS)\n"
    "elseif(PROBE_CASE STREQUAL \"unknown_argument\")\n"
    "    gyo_product_units(product DISABLE_MODULE off)\n"
    "endif()\n")
file(WRITE "${errors_dir}/CMakeLists.txt" "${errors_text}")

function(expect_refused_probe name expected)
    configure_probe("${errors_dir}" "refused-${name}" result output "-DPROBE_CASE=${name}")
    if(result EQUAL 0)
        message(FATAL_ERROR "Refused probe '${name}' was configured")
    endif()
    string(REGEX REPLACE "[ \t\r\n]+" " " text "${output}")
    string(FIND "${text}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Refused probe '${name}' did not report '${expected}':\n${output}")
    endif()
endfunction()
expect_refused_probe(link_unregistered "'ghost' is in no unit register")
expect_refused_probe(link_no_target "no target gyo_extra for register row 'extra'")
expect_refused_probe(link_interface_private "interface library gyo_extra has private_depends")
expect_refused_probe(dep_not_defined "'walk' depends on 'core' (GYO::Core), which is not defined yet")
expect_refused_probe(registers_three "REGISTERS takes <modules.csv> <plugins.csv>")
expect_refused_probe(registers_none "REGISTERS takes <modules.csv> <plugins.csv>")
expect_refused_probe(unknown_argument "unknown arguments 'DISABLE_MODULE")

message(STATUS "Unit register tests passed")
