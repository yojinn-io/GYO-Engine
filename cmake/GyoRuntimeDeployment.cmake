include_guard(GLOBAL)

function(gyo_copy_runtime_dlls target)
    if(WIN32)
        # copy_if_different with no input DLLs fails for backend-neutral targets.
        # The generated script preserves each path, including spaces.
        set(script "${CMAKE_CURRENT_BINARY_DIR}/${target}-runtime-$<CONFIG>.cmake")
        file(GENERATE OUTPUT "${script}" CONTENT
            "set(files [==[$<TARGET_RUNTIME_DLLS:${target}>]==])\nset(destination [==[$<TARGET_FILE_DIR:${target}>]==])\nforeach(file IN LISTS files)\n  get_filename_component(name \"\${file}\" NAME)\n  file(COPY_FILE \"\${file}\" \"\${destination}/\${name}\" ONLY_IF_DIFFERENT)\nendforeach()\n")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -P "${script}" VERBATIM)
    endif()
endfunction()
