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
    cmake_parse_arguments(P "" "TARGET;PRODUCT;KIND;OWNER;ROLE" "COMPONENTS" ${ARGN})
    list(REMOVE_ITEM P_KEYWORDS_MISSING_VALUES COMPONENTS)
    if(P_UNPARSED_ARGUMENTS OR P_KEYWORDS_MISSING_VALUES OR NOT TARGET "${P_TARGET}" OR
        NOT P_PRODUCT MATCHES "^[a-z][a-z0-9_]*$" OR NOT P_OWNER MATCHES "^[a-z][a-z0-9_]*$" OR
        NOT P_ROLE MATCHES "^[a-z][a-z0-9_]*$" OR NOT P_KIND MATCHES "^(app|toolchain)$")
        message(FATAL_ERROR "Invalid product target declaration")
    endif()
    get_target_property(type ${P_TARGET} TYPE)
    if(NOT type STREQUAL EXECUTABLE)
        message(FATAL_ERROR "Product roles require executable targets: ${P_TARGET}")
    endif()
    get_property(registered TARGET ${P_TARGET} PROPERTY GYO_PRODUCT_OWNER)
    get_property(role_target GLOBAL PROPERTY GYO_PRODUCT_${P_PRODUCT}_${P_OWNER}.${P_ROLE}_TARGET)
    get_property(kind GLOBAL PROPERTY GYO_PRODUCT_${P_PRODUCT}_KIND)
    if(registered OR role_target OR (kind AND NOT kind STREQUAL P_KIND))
        message(FATAL_ERROR "Duplicate product target/role or conflicting kind: ${P_PRODUCT}/${P_OWNER}.${P_ROLE}")
    endif()
    if((P_KIND STREQUAL app AND NOT P_OWNER STREQUAL P_PRODUCT) OR
        (P_KIND STREQUAL toolchain AND NOT P_PRODUCT STREQUAL toolchain))
        message(FATAL_ERROR "Product identity does not match its owner/kind")
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
    set_property(GLOBAL APPEND PROPERTY GYO_PRODUCT_${P_PRODUCT}_OWNERS "${P_OWNER}")
    set_property(GLOBAL PROPERTY GYO_PRODUCT_${P_PRODUCT}_${P_OWNER}.${P_ROLE}_TARGET "${P_TARGET}")
    set_property(TARGET ${P_TARGET} PROPERTY GYO_PRODUCT_OWNER "${P_OWNER}")
    set_property(TARGET ${P_TARGET} PROPERTY GYO_PRODUCT_ROLE "${P_ROLE}")
    set_property(TARGET ${P_TARGET} PROPERTY GYO_PRODUCT_COMPONENTS "${P_COMPONENTS}")
    if(GYO_ENABLE_PACKAGING)
        install(SCRIPT "${PROJECT_BINARY_DIR}/packages/${P_PRODUCT}/$<IF:$<BOOL:$<CONFIG>>,$<CONFIG>,Unspecified>/validate-layout.cmake" COMPONENT "${P_PRODUCT}")
    endif()
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
        if(WIN32)
            set_property(GLOBAL APPEND PROPERTY GYO_PRODUCT_${P_PRODUCT}_NATIVE_FILES
                "bin/$<TARGET_FILE_NAME:SDL3-shared>")
        else()
            set_property(GLOBAL APPEND PROPERTY GYO_PRODUCT_${P_PRODUCT}_NATIVE_FILES
                "lib/$<TARGET_FILE_NAME:SDL3-shared>" "lib/$<TARGET_SONAME_FILE_NAME:SDL3-shared>"
                "lib/$<TARGET_LINKER_FILE_NAME:SDL3-shared>")
        endif()
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
        if(NOT crt_installed)
            foreach(file IN LISTS crt)
                get_filename_component(name "${file}" NAME)
                set_property(GLOBAL APPEND PROPERTY GYO_PRODUCT_${P_PRODUCT}_NATIVE_FILES "$<$<NOT:$<CONFIG:Debug>>:bin/${name}>")
            endforeach()
        endif()
        set(script "${CMAKE_CURRENT_BINARY_DIR}/${P_TARGET}-crt-$<CONFIG>.cmake")
        file(GENERATE OUTPUT "${script}" CONTENT
            "if(NOT [==[$<CONFIG>]==] STREQUAL Debug)\n  file(MAKE_DIRECTORY [==[${output}]==])\n  set(files [==[${crt}]==])\n  foreach(file IN LISTS files)\n    get_filename_component(name \"\${file}\" NAME)\n    file(COPY_FILE \"\${file}\" \"${output}/\${name}\" ONLY_IF_DIFFERENT)\n  endforeach()\nendif()\n")
        add_custom_target(${P_TARGET}--crt
            COMMAND "${CMAKE_COMMAND}" -P "${script}" VERBATIM)
        add_dependencies(${P_TARGET} ${P_TARGET}--crt)
    endif()
endfunction()
