cmake_minimum_required(VERSION 3.30)
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${root}/build/cmake/GyoMsvcRuntime.cmake")
set(work "${CMAKE_CURRENT_BINARY_DIR}/msvc-runtime-tests")
set(vc "${work}/VS/VC")
set(compiler "${vc}/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe")
file(MAKE_DIRECTORY "${vc}/Tools/MSVC/14.51.36231/bin/Hostx64/x64")
foreach(version 14.44.35112 14.51.36231 14.52.10000)
    foreach(architecture x64 arm64)
        set(directory "${vc}/Redist/MSVC/${version}/${architecture}/Microsoft.VC145.CRT")
        file(MAKE_DIRECTORY "${directory}")
        foreach(name msvcp140 vcruntime140 vcruntime140_1 concrt140)
            file(WRITE "${directory}/${name}.dll" "test ${version} ${architecture}")
        endforeach()
    endforeach()
endforeach()
gyo_find_msvc_runtime(files COMPILER "${compiler}" ARCHITECTURE x64)
list(LENGTH files count)
if(NOT count EQUAL 4 OR NOT "${files}" MATCHES "14.52.10000/x64/")
    message(FATAL_ERROR "Compiler-relative newest runtime discovery failed: ${files}")
endif()
gyo_find_msvc_runtime(files COMPILER "${compiler}" ARCHITECTURE arm64
    REDIST_ROOT "${vc}/Redist/MSVC/14.51.36231")
if(NOT "${files}" MATCHES "14.51.36231/arm64/")
    message(FATAL_ERROR "Explicit runtime root or target architecture was ignored")
endif()
gyo_find_msvc_runtime(files COMPILER "${compiler}" ARCHITECTURE x64
    REDIST_ROOT "${vc}/Redist/MSVC/14.44.35112")
if(files)
    message(FATAL_ERROR "Accepted runtime older than compiler toolset")
endif()
string(TOLOWER "${compiler}" lower_case_compiler)
gyo_find_msvc_runtime(files COMPILER "${lower_case_compiler}" ARCHITECTURE x64
    REDIST_ROOT "${vc}/Redist/MSVC/14.44.35112")
if(files)
    message(FATAL_ERROR "Compiler path casing bypassed the minimum runtime version")
endif()
gyo_find_msvc_runtime(files COMPILER "${compiler}" ARCHITECTURE x86
    REDIST_ROOT "${vc}/Redist/MSVC")
if(files)
    message(FATAL_ERROR "Accepted runtime for another target architecture")
endif()

# A missing redistributable must allow project generation but fail a release
# install. Exercise the generated install script, without compiling a program.
if(NOT GENERATOR)
    message(FATAL_ERROR "Test requires GENERATOR and MAKE_PROGRAM from its parent build")
endif()
set(project "${work}/missing-runtime-project")
file(MAKE_DIRECTORY "${project}")
file(WRITE "${project}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.30)\nproject(RuntimeInstallTest LANGUAGES NONE)\n"
    "include(\"${root}/build/cmake/GyoMsvcRuntime.cmake\")\n"
    "set(CMAKE_CXX_COMPILER \"${compiler}\")\nset(MSVC_CXX_ARCHITECTURE_ID x64)\n"
    "set(GYO_MSVC_REDIST_DIR \"${work}/missing\" CACHE PATH \"\" FORCE)\n"
    "gyo_install_msvc_runtime()\n")
set(generator_arguments -G "${GENERATOR}" "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
if(PLATFORM)
    list(APPEND generator_arguments -A "${PLATFORM}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${project}" -B "${project}/build"
    ${generator_arguments} RESULT_VARIABLE configured OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT configured EQUAL 0)
    message(FATAL_ERROR "Missing CRT blocked development configure: ${output}\n${error}")
endif()
foreach(config Release release rElWiThDeBiNfO MinSizeRel)
    execute_process(COMMAND "${CMAKE_COMMAND}" --install "${project}/build" --config "${config}"
        --prefix "${work}/stage" RESULT_VARIABLE installed OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(installed EQUAL 0 OR NOT "${output}${error}" MATCHES "MSVC runtime is required for a release package")
        message(FATAL_ERROR "${config} install did not reject missing CRT: ${output}\n${error}")
    endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${project}/build" --config Debug
    --prefix "${work}/stage" RESULT_VARIABLE installed OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT installed EQUAL 0)
    message(FATAL_ERROR "Debug install attempted to redistribute the release CRT: ${output}\n${error}")
endif()
message(STATUS "MSVC runtime discovery, architecture, version and install-boundary checks passed")
