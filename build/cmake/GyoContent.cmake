include_guard(GLOBAL)
# CMake schedules compilation; the content manifest and assembler own assets.
function(gyo_attach_content target product)
    set(assets "${GYO_REPOSITORY_ROOT}/assets/${product}")
    # Only observe this selected product's source presence. A directory added
    # or removed after configure must update the graph on the next build.
    file(GLOB asset_presence CONFIGURE_DEPENDS LIST_DIRECTORIES TRUE
        "${assets}" "${assets}/content.json")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(content_args "")
    set(dependencies "")
    set(descriptor "${assets}/content.json")
    if(IS_DIRECTORY "${assets}")
        if(NOT EXISTS "${descriptor}")
            message(FATAL_ERROR "Asset root requires content.json: ${assets}")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${descriptor}")
        file(READ "${descriptor}" content)
        string(JSON version GET "${content}" version)
        if(NOT version EQUAL 1)
            message(FATAL_ERROR "Unsupported content manifest: ${descriptor}")
        endif()
        list(APPEND content_args --assets "${assets}")
        string(JSON count LENGTH "${content}" shader_bundles)
        if(count GREATER 0)
            # The shader pipeline (GyoShaders.cmake with engine/render) is not in
            # this tree, so declared shader bundles cannot be built.
            message(FATAL_ERROR "${descriptor} declares ${count} shader bundle(s), but this tree has no shader pipeline")
        endif()
        set_property(TARGET ${target} PROPERTY GYO_PRODUCT_REQUIRED_FILES "bin/assets/${product}/content.json")
    elseif(EXISTS "${assets}")
        message(FATAL_ERROR "Asset root must be a directory: ${assets}")
    endif()
    # An empty argument list is deliberate: the assembler removes obsolete
    # managed content. Keep this operation for local, installed and preview use.
    set_property(GLOBAL PROPERTY GYO_CONTENT_${product}_REGISTERED TRUE)
    set_property(GLOBAL PROPERTY GYO_CONTENT_${product}_ARGUMENTS "${content_args}")
    set_property(GLOBAL PROPERTY GYO_CONTENT_${product}_DEPENDENCIES "${dependencies}")
    set_property(GLOBAL PROPERTY GYO_CONTENT_PYTHON "${Python3_EXECUTABLE}")
    set(assembler "${GYO_REPOSITORY_ROOT}/build/assemble_runtime.py")
    add_custom_target(gyo_${product}--content
        COMMAND "${Python3_EXECUTABLE}" "${assembler}" --kind app --product "${product}"
            --stage "${GYO_OUTPUT_ROOT}/${product}" ${content_args}
        DEPENDS ${dependencies}
        COMMENT "Assembling ${product} runtime content" VERBATIM)
    add_dependencies(${target} gyo_${product}--content)
    set_property(TARGET ${target} PROPERTY GYO_APP_CONTENT_STAGE_TARGET gyo_${product}--content)
    set(arguments "")
    foreach(argument IN LISTS content_args)
        string(APPEND arguments " [==[${argument}]==]")
    endforeach()
    install(CODE "execute_process(COMMAND [==[${Python3_EXECUTABLE}]==] [==[${assembler}]==] --kind app --product [==[${product}]==] --stage \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\" ${arguments} COMMAND_ERROR_IS_FATAL ANY)" COMPONENT "${product}")
endfunction()

# Separate test and design programs consume the same assembled content rules.
function(gyo_stage_app_content target product stage)
    get_property(registered GLOBAL PROPERTY GYO_CONTENT_${product}_REGISTERED)
    get_property(content_args GLOBAL PROPERTY GYO_CONTENT_${product}_ARGUMENTS)
    get_property(dependencies GLOBAL PROPERTY GYO_CONTENT_${product}_DEPENDENCIES)
    get_property(python GLOBAL PROPERTY GYO_CONTENT_PYTHON)
    if(NOT registered)
        return()
    endif()
    add_custom_target(${target}--content
        COMMAND "${python}" "${GYO_REPOSITORY_ROOT}/build/assemble_runtime.py"
            --kind app --product "${product}" --stage "${stage}" ${content_args}
        DEPENDS ${dependencies} VERBATIM)
    add_dependencies(${target} ${target}--content)
endfunction()
