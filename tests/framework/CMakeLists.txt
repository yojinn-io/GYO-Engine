# The framework layer (docs/framework/architecture.md); added with it, when
# a tool is selected or a game declares the component FRAMEWORK.
add_executable(gyo_framework_tests AbsolutePathTests.cpp CatalogStructureTests.cpp EventSoundsTests.cpp
                                   HeadlessShellTests.cpp UiTextInputTests.cpp)
target_compile_features(gyo_framework_tests PRIVATE cxx_std_20)
target_link_libraries(gyo_framework_tests PRIVATE GYO::ToolFramework GYO::ToolFrameworkJson GYO::Os
                                                  nlohmann_json::nlohmann_json doctest::doctest)
target_compile_definitions(gyo_framework_tests PRIVATE
    GYO_ASSET_CONTRACT_FIXTURE="${GYO_REPOSITORY_ROOT}/tests/common/fixtures/asset_contract/cases.json")
add_test(NAME gyo_framework.core COMMAND gyo_framework_tests)
set_tests_properties(gyo_framework.core PROPERTIES LABELS "cpu;editor" TIMEOUT 60)

# The JSON binding is built with the core (Design Invariant 2).
if(NOT TARGET GYO::ToolFrameworkJson)
    message(FATAL_ERROR "framework: the core is built but no GYO::ToolFrameworkJson")
endif()

# The shells are built wherever their Engine targets are (Design Invariant 6):
# a binding or shell that is silently not built would take its tests with it.
if(GYO_ENABLE_SDL_PLATFORM AND NOT TARGET GYO::ToolFrameworkWindow)
    message(FATAL_ERROR "framework: the build has Engine's SDL platform but no GYO::ToolFrameworkWindow")
endif()
if(GYO_ENABLE_SDL_RENDERER AND GYO_ENABLE_SDL_INPUT AND GYO_ENABLE_IMGUI AND NOT TARGET GYO::ToolFrameworkImGui)
    message(FATAL_ERROR "framework: the build has the SDL renderer, SDL input and ImGui but no GYO::ToolFrameworkImGui")
endif()
if(GYO_ENABLE_SDL_GPU AND GYO_ENABLE_SDL_RENDERER AND NOT TARGET GYO::ToolFrameworkPreview)
    message(FATAL_ERROR "framework: the build has Engine's SDL GPU backend and SDL renderer but no GYO::ToolFrameworkPreview")
endif()

# The windowed shell, on SDL's dummy video driver.
if(TARGET GYO::ToolFrameworkWindow)
    add_executable(gyo_framework_window_tests WindowShellTests.cpp)
    target_compile_features(gyo_framework_window_tests PRIVATE cxx_std_20)
    target_link_libraries(gyo_framework_window_tests PRIVATE GYO::ToolFrameworkWindow GYO::PlatformSDLNative
                                                             doctest::doctest)
    gyo_copy_runtime_dlls(gyo_framework_window_tests)
    add_test(NAME gyo_framework.window_shell COMMAND gyo_framework_window_tests)
    set_tests_properties(gyo_framework.window_shell PROPERTIES
        LABELS "editor" ENVIRONMENT "SDL_VIDEODRIVER=dummy" TIMEOUT 30)
    # Q1 and Q2 of v8-framework-close-request, in the cpu set as well (L1).
    add_test(NAME gyo_framework.window_close COMMAND gyo_framework_window_tests "--test-case=Q*")
    set_tests_properties(gyo_framework.window_close PROPERTIES
        LABELS "cpu;editor" ENVIRONMENT "SDL_VIDEODRIVER=dummy" TIMEOUT 30)
endif()

# The ImGui binding, on the dummy video driver and the software renderer, in a
# directory of its own so that a side file would be seen.
if(TARGET GYO::ToolFrameworkImGui)
    add_executable(gyo_framework_imgui_tests ImGuiShellTests.cpp)
    target_compile_features(gyo_framework_imgui_tests PRIVATE cxx_std_20)
    target_link_libraries(gyo_framework_imgui_tests PRIVATE GYO::ToolFrameworkImGui GYO::PlatformSDLNative
                                                            GYO::RenderBackendSDLNative imgui::core doctest::doctest)
    gyo_copy_runtime_dlls(gyo_framework_imgui_tests)
    set(_gyo_imgui_directory "${CMAKE_CURRENT_BINARY_DIR}/imgui_shell")
    file(MAKE_DIRECTORY "${_gyo_imgui_directory}")
    add_test(NAME gyo_framework.imgui_shell COMMAND gyo_framework_imgui_tests
        WORKING_DIRECTORY "${_gyo_imgui_directory}")
    set_tests_properties(gyo_framework.imgui_shell PROPERTIES
        LABELS "editor" ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 60)
    # A renderer that cannot start: SDL fails when the driver it is told to use
    # does not exist.
    add_test(NAME gyo_framework.imgui_shell_no_renderer
        COMMAND gyo_framework_imgui_tests --no-skip "--test-case=I8:*"
        WORKING_DIRECTORY "${_gyo_imgui_directory}")
    set_tests_properties(gyo_framework.imgui_shell_no_renderer PROPERTIES
        LABELS "editor" ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=gyo-no-such-driver" TIMEOUT 30)
    # Q3 of v8-framework-close-request, in the cpu set as well (L1).
    add_test(NAME gyo_framework.imgui_close
        COMMAND gyo_framework_imgui_tests "--test-case=Q3:*"
        WORKING_DIRECTORY "${_gyo_imgui_directory}")
    set_tests_properties(gyo_framework.imgui_close PROPERTIES
        LABELS "cpu;editor" ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30)
    # T2 of v8-engine-sdl-renderer-texture, in the cpu set as well (L1).
    add_test(NAME gyo_framework.imgui_texture
        COMMAND gyo_framework_imgui_tests "--test-case=T2:*"
        WORKING_DIRECTORY "${_gyo_imgui_directory}")
    set_tests_properties(gyo_framework.imgui_texture PROPERTIES
        LABELS "cpu;editor" ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30)
    unset(_gyo_imgui_directory)

    # The public headers need neither ImGui's nor SDL's include paths.
    add_executable(gyo_framework_shell_header_tests ShellHeaderTests.cpp)
    target_compile_features(gyo_framework_shell_header_tests PRIVATE cxx_std_20)
    target_link_libraries(gyo_framework_shell_header_tests PRIVATE GYO::ToolFrameworkImGui doctest::doctest)
    gyo_copy_runtime_dlls(gyo_framework_shell_header_tests)
    add_test(NAME gyo_framework.shell_headers COMMAND gyo_framework_shell_header_tests)
    set_tests_properties(gyo_framework.shell_headers PROPERTIES LABELS "cpu;editor" TIMEOUT 30)
endif()

# The offscreen preview (docs/framework/preview.md). PV1 to PV4 draw on a GPU
# (your Mac; CI's GPU step runs Engine's render.* tests only); PV5 and PV6 run
# on the dummy video driver and the software renderer, in the cpu set (L1).
if(TARGET GYO::ToolFrameworkPreview)
    add_executable(gyo_framework_preview_tests PreviewTests.cpp)
    target_compile_features(gyo_framework_preview_tests PRIVATE cxx_std_20)
    target_link_libraries(gyo_framework_preview_tests PRIVATE GYO::ToolFrameworkPreview GYO::RenderBackendSDLNative
                                                              doctest::doctest)
    target_compile_definitions(gyo_framework_preview_tests PRIVATE NOMINMAX
        GYO_TEST_SHADER_BUNDLE="${PROJECT_BINARY_DIR}/shaders/builtin")
    add_dependencies(gyo_framework_preview_tests gyo_builtin_shaders)
    gyo_copy_runtime_dlls(gyo_framework_preview_tests)
    add_test(NAME gyo_framework.preview COMMAND gyo_framework_preview_tests "--test-case=PV1:*,PV2:*,PV3:*,PV4:*")
    set_tests_properties(gyo_framework.preview PROPERTIES LABELS "gpu;editor" TIMEOUT 60)
    add_test(NAME gyo_framework.preview_cpu COMMAND gyo_framework_preview_tests "--test-case=PV5:*,PV6:*")
    set_tests_properties(gyo_framework.preview_cpu PROPERTIES
        LABELS "cpu;editor" ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30)
endif()

# Design Invariants 1-3: not a product, one dependency direction, neutral.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME gyo_framework.dependency_guard
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/framework_dependency_guard.py"
        --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME gyo_framework.dependency_guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}"
        -p "test_framework_dependency_guard.py" -v)
set_tests_properties(gyo_framework.dependency_guard gyo_framework.dependency_guard_selftest
    PROPERTIES LABELS "cpu;editor" TIMEOUT 60)

# Design Invariant 2, at configure time: what each framework target links. The
# core (every target outside framework/bindings/) reaches no SDL3 (SDL3_image,
# SDL3_ttf and -lSDL3 included), ImGui, nlohmann or *Native target, and no
# binding; the ImGui binding names no SDL3, nlohmann or other binding itself
# (ImGui's SDL backend brings SDL3 on); the JSON binding reaches no SDL3, ImGui,
# *Native target or other binding; any other binding fails. A target's own
# links are all checked, $<LINK_ONLY:> and other generator expressions
# included (by the names inside them); an INTERFACE library's own links are its
# interface. Through another target's interface, a $<LINK_ONLY:> entry (that
# target's PRIVATE link) is not followed, and neither is a forbidden target.
# There are no exceptions: the allowlist of the core's old links emptied and
# was removed in v8-framework-invariants.
# Limit: an IMPORTED target that is not GLOBAL and is defined in another
# directory cannot be followed. Checked once the whole project is configured.
function(_gyo_framework_forbidden entry real kind out)
    set(b "(^|[^A-Za-z0-9_])")
    set(e "($|[^A-Za-z0-9_])")
    set(sdl "${b}(-l)?SDL3([_:-][A-Za-z0-9_:-]*)?${e}")
    set(imgui "${b}imgui([_:][A-Za-z0-9_:]*)?${e}")
    set(json "${b}nlohmann_json")
    set(native "(Native|_native)${e}")
    set(hit "")
    foreach(candidate IN ITEMS "${entry}" "${real}")
        if(candidate MATCHES "${sdl}" OR candidate MATCHES "${json}")
            if(NOT kind STREQUAL "json" OR candidate MATCHES "${sdl}")
                set(hit "${entry}")
            endif()
        endif()
        if((kind STREQUAL "core" OR kind STREQUAL "json") AND (candidate MATCHES "${imgui}" OR candidate MATCHES "${native}"))
            set(hit "${entry}")
        endif()
    endforeach()
    set(${out} "${hit}" PARENT_SCOPE)
endfunction()

function(_gyo_framework_binding_of target out)
    set(binding "")
    if(TARGET "${target}")
        get_target_property(aliased "${target}" ALIASED_TARGET)
        if(aliased)
            set(target "${aliased}")
        endif()
        get_target_property(imported "${target}" IMPORTED)
        if(NOT imported)
            get_target_property(source "${target}" SOURCE_DIR)
            file(RELATIVE_PATH relative "${GYO_REPOSITORY_ROOT}/framework" "${source}")
            if(relative MATCHES "^bindings/([^/]+)")
                set(binding "${CMAKE_MATCH_1}")
            endif()
        endif()
    endif()
    set(${out} "${binding}" PARENT_SCOPE)
endfunction()

function(_gyo_framework_link_findings target kind out)
    get_target_property(type ${target} TYPE)
    if(type STREQUAL "INTERFACE_LIBRARY")
        get_target_property(own ${target} INTERFACE_LINK_LIBRARIES)
    else()
        get_target_property(own ${target} LINK_LIBRARIES)
    endif()
    if(NOT own)
        set(own "")
    endif()
    # Each queue item is "own|<entry>" or "via|<entry>".
    set(queue "")
    foreach(entry IN LISTS own)
        list(APPEND queue "own|${entry}")
    endforeach()
    set(seen "")
    set(found "")
    list(LENGTH queue remaining)
    while(remaining GREATER 0)
        list(POP_FRONT queue item)
        list(LENGTH queue remaining)
        string(SUBSTRING "${item}" 0 3 origin)
        string(SUBSTRING "${item}" 4 -1 entry)
        if(origin STREQUAL "via" AND entry MATCHES "^\\$<LINK_ONLY:")
            continue()
        endif()
        if(entry IN_LIST seen)
            continue()
        endif()
        list(APPEND seen "${entry}")
        set(real "${entry}")
        if(TARGET "${entry}")
            get_target_property(aliased "${entry}" ALIASED_TARGET)
            if(aliased)
                set(real "${aliased}")
            endif()
        endif()
        if(kind STREQUAL "imgui" AND origin STREQUAL "via")
            continue()  # the ImGui binding is checked on its own links only
        endif()
        _gyo_framework_forbidden("${entry}" "${real}" "${kind}" hit)
        _gyo_framework_binding_of("${entry}" binding)
        if(binding AND NOT binding STREQUAL kind)
            set(hit "${entry}")
        endif()
        if(hit)
            list(APPEND found "${hit}")
            continue()
        endif()
        if(TARGET "${real}" AND NOT entry MATCHES "\\$<")
            get_target_property(interface "${real}" INTERFACE_LINK_LIBRARIES)
            if(interface)
                foreach(next IN LISTS interface)
                    list(APPEND queue "via|${next}")
                endforeach()
                list(LENGTH queue remaining)
            endif()
        endif()
    endwhile()
    list(REMOVE_DUPLICATES found)
    list(SORT found)
    set(${out} "${found}" PARENT_SCOPE)
endfunction()

# The offscreen preview is the one core library on Engine's render targets
# (docs/framework/preview.md, Design Invariant 1): what the core and the
# windowed shell link, their own links (PRIVATE, PUBLIC and INTERFACE) and what
# those bring through their interfaces, holds no GYO::Render* or gyo_render*
# target, however it is spelled (an alias, a variable, a generator expression,
# a target named inside one, such as the preview's own target). Through
# another target's interface, a $<LINK_ONLY:> entry (its PRIVATE link) is not
# followed.
function(_gyo_framework_render_findings target out)
    get_target_property(own ${target} LINK_LIBRARIES)
    get_target_property(own_interface ${target} INTERFACE_LINK_LIBRARIES)
    if(NOT own)
        set(own "")
    endif()
    if(own_interface)
        list(APPEND own ${own_interface})
    endif()
    set(queue "")
    foreach(entry IN LISTS own)
        list(APPEND queue "own|${entry}")
    endforeach()
    # Two lists: the link entries already read, and the targets whose
    # interfaces were already followed (an entry may be a target's own name).
    set(read_entries "")
    set(expanded "")
    set(found "")
    list(LENGTH queue remaining)
    while(remaining GREATER 0)
        list(POP_FRONT queue item)
        list(LENGTH queue remaining)
        string(SUBSTRING "${item}" 0 3 origin)
        string(SUBSTRING "${item}" 4 -1 entry)
        if(origin STREQUAL "via" AND entry MATCHES "^\\$<LINK_ONLY:")
            continue()
        endif()
        if(entry IN_LIST read_entries)
            continue()
        endif()
        list(APPEND read_entries "${entry}")
        string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_]*(::[A-Za-z_][A-Za-z0-9_]*)?" names "${entry}")
        foreach(name IN LISTS names)
            set(real "${name}")
            if(TARGET "${name}")
                get_target_property(aliased "${name}" ALIASED_TARGET)
                if(aliased)
                    set(real "${aliased}")
                endif()
            endif()
            if(name MATCHES "^(GYO::Render|gyo_render)" OR real MATCHES "^(GYO::Render|gyo_render)")
                list(APPEND found "${name}")
            elseif(TARGET "${real}" AND NOT real IN_LIST expanded)
                list(APPEND expanded "${real}")
                get_target_property(interface "${real}" INTERFACE_LINK_LIBRARIES)
                if(interface)
                    foreach(next IN LISTS interface)
                        list(APPEND queue "via|${next}")
                    endforeach()
                    list(LENGTH queue remaining)
                endif()
            endif()
        endforeach()
    endwhile()
    list(REMOVE_DUPLICATES found)
    list(SORT found)
    set(${out} "${found}" PARENT_SCOPE)
endfunction()

function(_gyo_framework_targets_in directory out)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        _gyo_framework_targets_in("${child}" more)
        list(APPEND targets ${more})
    endforeach()
    set(${out} "${targets}" PARENT_SCOPE)
endfunction()

function(gyo_expect_framework_links)
    set(root "${GYO_REPOSITORY_ROOT}/framework")
    _gyo_framework_targets_in("${root}" targets)
    set(problems "")
    foreach(target IN LISTS targets)
        _gyo_framework_binding_of(${target} kind)
        if(kind)
            if(NOT kind STREQUAL "imgui" AND NOT kind STREQUAL "json")
                list(APPEND problems "${target}: binding '${kind}' has no link rule")
                continue()
            endif()
        else()
            set(kind "core")
        endif()
        _gyo_framework_link_findings(${target} ${kind} found)
        if(found)
            list(JOIN found "," found_text)
            list(LENGTH found count)
            list(APPEND problems "${target} (${kind}) links ${count} forbidden targets [${found_text}]")
        endif()
    endforeach()
    foreach(target IN ITEMS gyo_tool_framework gyo_tool_framework_window)
        if(TARGET ${target})
            _gyo_framework_render_findings(${target} found)
            if(found)
                list(JOIN found "," found_text)
                list(APPEND problems "${target} (core) reaches a render target [${found_text}]; only the preview may (docs/framework/preview.md)")
            endif()
        endif()
    endforeach()
    if(problems)
        list(JOIN problems "\n  " text)
        message(FATAL_ERROR "framework link check (docs/framework/architecture.md, Design Invariant 2):\n  ${text}")
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_framework_links)
