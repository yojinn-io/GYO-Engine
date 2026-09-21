include_guard(GLOBAL)
include(ExternalProject)
set(GYO_SHADER_TOOL_EXECUTABLE "" CACHE FILEPATH "Existing native gyo_shader_tool (required for cross compilation)")
set(GYO_SHADER_HOST_BUILD_DIR "${CMAKE_BINARY_DIR}/host-tools" CACHE PATH "Private native shader tool build directory")

function(_gyo_require_shader_tool out_command out_dependency)
    if(GYO_SHADER_TOOL_EXECUTABLE)
        if(NOT EXISTS "${GYO_SHADER_TOOL_EXECUTABLE}")
            message(FATAL_ERROR "GYO_SHADER_TOOL_EXECUTABLE does not exist: ${GYO_SHADER_TOOL_EXECUTABLE}")
        endif()
        set(${out_command} "${GYO_SHADER_TOOL_EXECUTABLE}" PARENT_SCOPE)
        set(${out_dependency} "${GYO_SHADER_TOOL_EXECUTABLE}" PARENT_SCOPE)
        return()
    endif()
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "Cross compiling requires GYO_SHADER_TOOL_EXECUTABLE built for the host")
    endif()
    if(CMAKE_HOST_WIN32)
        set(executable "${GYO_SHADER_HOST_BUILD_DIR}/bin/gyo_shader_tool.exe")
    else()
        set(executable "${GYO_SHADER_HOST_BUILD_DIR}/bin/gyo_shader_tool")
    endif()
    if(NOT TARGET gyo_shader_host_tools)
        set(host_args -DCMAKE_BUILD_TYPE=Release "-DBUILD_TESTING=${BUILD_TESTING}")
        # Native IDE profiles often select compilers and Ninja by absolute
        # path without exporting CC/CXX or adding every tool to PATH.
        # ExternalProject inherits the generator, but not these cache values.
        # Cross builds were rejected above, so these are native host tools;
        # never forward target toolchain files, sysroots or compile flags.
        foreach(host_tool CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_MAKE_PROGRAM)
            if(DEFINED ${host_tool} AND NOT "${${host_tool}}" STREQUAL "")
                list(APPEND host_args "-D${host_tool}:FILEPATH=${${host_tool}}")
            endif()
        endforeach()
        # Reuse explicitly provided, immutable source caches without sharing
        # target binaries or leaking the target toolchain into the host build.
        foreach(dependency SDL3 NLOHMANN_JSON GYO_SPIRVCROSS GYO_SHADERCROSS GYO_DXC GYO_DXC_SOURCE)
            if(FETCHCONTENT_SOURCE_DIR_${dependency})
                list(APPEND host_args "-DFETCHCONTENT_SOURCE_DIR_${dependency}=${FETCHCONTENT_SOURCE_DIR_${dependency}}")
            endif()
        endforeach()
        ExternalProject_Add(gyo_shader_host_tools
            SOURCE_DIR "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/shader_pipeline"
            BINARY_DIR "${GYO_SHADER_HOST_BUILD_DIR}"
            CMAKE_ARGS ${host_args}
            BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target gyo_shader_tool
            BUILD_BYPRODUCTS "${executable}"
            BUILD_ALWAYS TRUE
            INSTALL_COMMAND ""
            USES_TERMINAL_CONFIGURE TRUE USES_TERMINAL_BUILD TRUE)
    endif()
    set(${out_command} "${executable}" PARENT_SCOPE)
    set(${out_dependency} gyo_shader_host_tools PARENT_SCOPE)
endfunction()

# gyo_add_shader_bundle(target SPEC spec.json OUTPUT_DIRECTORY build/path
#                       [FORMATS spirv dxil metallib])
# Exports <target>_DIRECTORY and <target>_MANIFEST to the caller. Consumers add
# a dependency on target and deploy its entire directory (manifest + objects).
function(gyo_add_shader_bundle target)
    cmake_parse_arguments(BUNDLE "" "SPEC;OUTPUT_DIRECTORY" "FORMATS" ${ARGN})
    if(NOT BUNDLE_SPEC OR NOT BUNDLE_OUTPUT_DIRECTORY OR BUNDLE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "gyo_add_shader_bundle requires SPEC and OUTPUT_DIRECTORY")
    endif()
    get_filename_component(spec "${BUNDLE_SPEC}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(output "${BUNDLE_OUTPUT_DIRECTORY}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    # Generated binary files and manifests must never be written into sources.
    cmake_path(IS_PREFIX PROJECT_BINARY_DIR "${output}" NORMALIZE inside_build)
    if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR OR NOT inside_build)
        message(FATAL_ERROR "Shader bundle output must be in the build tree: ${output}")
    endif()
    set(formats ${BUNDLE_FORMATS})
    if(NOT formats)
        set(formats ${GYO_SHADER_FORMATS})
    endif()
    if(NOT formats OR formats STREQUAL "AUTO")
        if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
            set(formats dxil spirv)
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(formats spirv)
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set(formats metallib)
        else()
            message(FATAL_ERROR "No AUTO shader format policy for ${CMAKE_SYSTEM_NAME}")
        endif()
    endif()
    foreach(format IN LISTS formats)
        if(NOT format MATCHES "^(dxil|spirv|metallib)$")
            message(FATAL_ERROR "Unknown shader format '${format}'")
        endif()
    endforeach()
    set(metal_args)
    if("metallib" IN_LIST formats)
        if(NOT CMAKE_HOST_APPLE)
            message(FATAL_ERROR "Strict metallib builds require a native macOS host and Apple Metal tools")
        endif()
        if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
            message(FATAL_ERROR "Metallib requires CMAKE_OSX_DEPLOYMENT_TARGET; set it to the application's minimum macOS version")
        endif()
        set(deployment_target "${CMAKE_OSX_DEPLOYMENT_TARGET}")
        find_program(GYO_XCRUN_EXECUTABLE xcrun REQUIRED)
        foreach(apple_tool metal metallib)
            execute_process(COMMAND "${GYO_XCRUN_EXECUTABLE}" --sdk macosx --find ${apple_tool}
                RESULT_VARIABLE tool_result OUTPUT_QUIET ERROR_VARIABLE tool_error)
            if(NOT tool_result EQUAL 0)
                message(FATAL_ERROR "Missing Apple ${apple_tool} compiler: ${tool_error}")
            endif()
        endforeach()
        list(APPEND metal_args --xcrun "${GYO_XCRUN_EXECUTABLE}" --deployment-target "${deployment_target}")
    endif()
    _gyo_require_shader_tool(tool tool_dependency)
    string(JOIN "," format_argument ${formats})
    set(manifest "${output}/manifest.json")
    set(depfile "${output}/bundle.d")
    # A configuration stamp makes changing formats regenerate the manifest.
    set(stamp "${CMAKE_CURRENT_BINARY_DIR}/${target}-configuration.txt")
    file(CONFIGURE OUTPUT "${stamp}" CONTENT "${format_argument}\n${metal_args}\n${spec}\n" @ONLY)
    add_custom_command(OUTPUT "${manifest}"
        COMMAND "${tool}" --spec "${spec}" --output "${output}" --formats "${format_argument}"
                --depfile "${depfile}" ${metal_args}
        DEPENDS "${spec}" "${stamp}" ${tool_dependency} "${tool}"
        DEPFILE "${depfile}"
        COMMENT "Compiling and validating shader bundle ${target} (${format_argument})"
        VERBATIM)
    add_custom_target(${target} DEPENDS "${manifest}")
    set(${target}_DIRECTORY "${output}" PARENT_SCOPE)
    set(${target}_MANIFEST "${manifest}" PARENT_SCOPE)
endfunction()
