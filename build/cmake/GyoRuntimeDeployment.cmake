include_guard(GLOBAL)

function(gyo_copy_runtime_dlls target)
    if(WIN32)
        get_target_property(configured ${target} GYO_RUNTIME_DLLS_CONFIGURED)
        if(configured)
            return()
        endif()
        set_property(TARGET ${target} PROPERTY GYO_RUNTIME_DLLS_CONFIGURED TRUE)
        # copy_if_different with no input DLLs fails for backend-neutral targets.
        # The generated script preserves each path, including spaces.
        set(script "${CMAKE_CURRENT_BINARY_DIR}/${target}-runtime-$<CONFIG>.cmake")
        file(GENERATE OUTPUT "${script}" CONTENT
            "set(files [==[$<TARGET_RUNTIME_DLLS:${target}>]==])\nset(destination [==[$<TARGET_FILE_DIR:${target}>]==])\nfile(MAKE_DIRECTORY \"\${destination}\")\nforeach(file IN LISTS files)\n  get_filename_component(name \"\${file}\" NAME)\n  file(COPY_FILE \"\${file}\" \"\${destination}/\${name}\" ONLY_IF_DIFFERENT)\nendforeach()\n")
        add_custom_target(${target}--runtime
            COMMAND "${CMAKE_COMMAND}" -P "${script}"
            DEPENDS "$<TARGET_RUNTIME_DLLS:${target}>"
            COMMAND_EXPAND_LISTS VERBATIM)
        add_dependencies(${target} ${target}--runtime)
    endif()
endfunction()

# A product owns its executable layout and native dependencies. Acceptance
# programs are independent targets and never register for installation.
function(gyo_install_product_target)
    cmake_parse_arguments(P "" "TARGET;PRODUCT;KIND;ROLE" "COMPONENTS" ${ARGN})
    if(NOT TARGET "${P_TARGET}" OR NOT P_PRODUCT MATCHES "^[a-z][a-z0-9_]*$" OR NOT P_KIND MATCHES "^(app|toolchain)$")
        message(FATAL_ERROR "Invalid product target declaration")
    endif()
    set(output "${GYO_OUTPUT_ROOT}/${P_PRODUCT}/bin")
    set_target_properties(${P_TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${output}")
    foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${P_TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${output}")
    endforeach()
    get_property(products GLOBAL PROPERTY GYO_PRODUCTS)
    if(NOT P_PRODUCT IN_LIST products)
        set_property(GLOBAL APPEND PROPERTY GYO_PRODUCTS "${P_PRODUCT}")
        set_property(GLOBAL PROPERTY GYO_PRODUCT_${P_PRODUCT}_KIND "${P_KIND}")
    endif()
    set_property(GLOBAL APPEND PROPERTY GYO_PRODUCT_${P_PRODUCT}_TARGETS "${P_TARGET}")
    set_property(GLOBAL APPEND PROPERTY GYO_PRODUCT_${P_PRODUCT}_COMPONENTS "${P_COMPONENTS}")
    set_property(TARGET ${P_TARGET} PROPERTY GYO_PRODUCT_ROLE "${P_ROLE}")
    install(TARGETS ${P_TARGET} RUNTIME DESTINATION bin COMPONENT "${P_PRODUCT}")
    gyo_copy_runtime_dlls(${P_TARGET})
    set(needs_sdl OFF)
    foreach(component IN LISTS P_COMPONENTS)
        if(component MATCHES "^SDL_" OR component STREQUAL IMGUI)
            set(needs_sdl ON)
        endif()
    endforeach()
    get_property(sdl_installed GLOBAL PROPERTY GYO_PRODUCT_${P_PRODUCT}_SDL_INSTALLED)
    if(needs_sdl AND TARGET SDL3-shared AND NOT sdl_installed)
        install(TARGETS SDL3-shared
            RUNTIME DESTINATION bin COMPONENT "${P_PRODUCT}"
            LIBRARY DESTINATION lib COMPONENT "${P_PRODUCT}")
        set_property(GLOBAL PROPERTY GYO_PRODUCT_${P_PRODUCT}_SDL_INSTALLED TRUE)
    endif()
    if(UNIX AND needs_sdl AND TARGET SDL3-shared)
        # A local product must resolve exactly the same relative library layout
        # as an installed product, without borrowing libraries from _build.
        set_target_properties(${P_TARGET} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
        set(script "${CMAKE_CURRENT_BINARY_DIR}/${P_TARGET}-native-$<CONFIG>.cmake")
        file(GENERATE OUTPUT "${script}" CONTENT
            "file(MAKE_DIRECTORY [==[${GYO_OUTPUT_ROOT}/${P_PRODUCT}/lib]==])\nfile(COPY [==[$<TARGET_SONAME_FILE:SDL3-shared>]==] DESTINATION [==[${GYO_OUTPUT_ROOT}/${P_PRODUCT}/lib]==] FOLLOW_SYMLINK_CHAIN)\n")
        add_custom_target(${P_TARGET}--native
            COMMAND "${CMAKE_COMMAND}" -P "${script}" DEPENDS SDL3-shared VERBATIM)
        add_dependencies(${P_TARGET} ${P_TARGET}--native)
    endif()
    if(MSVC)
        get_property(crt_installed GLOBAL PROPERTY GYO_PRODUCT_${P_PRODUCT}_CRT_INSTALLED)
        if(NOT crt_installed)
            include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GyoMsvcRuntime.cmake")
            gyo_install_msvc_runtime(COMPONENT "${P_PRODUCT}")
            set_property(GLOBAL PROPERTY GYO_PRODUCT_${P_PRODUCT}_CRT_INSTALLED TRUE)
        endif()
        get_property(crt GLOBAL PROPERTY GYO_MSVC_RUNTIME_LIBRARIES)
        set(script "${CMAKE_CURRENT_BINARY_DIR}/${P_TARGET}-crt-$<CONFIG>.cmake")
        file(GENERATE OUTPUT "${script}" CONTENT
            "if(NOT [==[$<CONFIG>]==] STREQUAL Debug)\n  file(MAKE_DIRECTORY [==[${output}]==])\n  set(files [==[${crt}]==])\n  foreach(file IN LISTS files)\n    get_filename_component(name \"\${file}\" NAME)\n    file(COPY_FILE \"\${file}\" \"${output}/\${name}\" ONLY_IF_DIFFERENT)\n  endforeach()\nendif()\n")
        add_custom_target(${P_TARGET}--crt
            COMMAND "${CMAKE_COMMAND}" -P "${script}" VERBATIM)
        add_dependencies(${P_TARGET} ${P_TARGET}--crt)
    endif()
endfunction()
