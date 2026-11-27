# The ink Plugin (config/plugins.csv, library_only): Ink stories through inkcpp
# (docs/architecture/target/v2/ink.zh-Hant.md). The build layer adds this
# directory only when a product carries the Plugin or GYO_PLUGINS names it, so
# inkcpp is neither downloaded nor built otherwise (as SDL-2 asks of SDL).
#
# inkcpp is linked PRIVATE and only src/adapters/inkcpp/ includes it; the public
# header speaks the game's words (lines, choices, commands).
include(FetchContent)

# inkcpp pinned to a commit, not a tag, so the fetched content cannot move. Its
# own CMake is not used: the top level adds compile options to the whole
# directory and the unreal and command line projects, and the library lists
# copy files into ${CMAKE_BINARY_DIR}/unreal. The sources are listed here
# instead. The submodules (pybind11, the ink-proof suite) are not fetched.
FetchContent_Declare(inkcpp
    GIT_REPOSITORY https://github.com/JBenda/inkcpp.git
    GIT_TAG d38608dffe50de8b6c954419647a5185044fbeee
    GIT_SUBMODULES ""
    SOURCE_SUBDIR gyo-no-upstream-cmake
)
FetchContent_MakeAvailable(inkcpp)

# The runtime and its shared headers, with exceptions and the STL interface
# (inkcpp's defaults): the adapter turns every ink::ink_exception into a Result.
set(gyo_inkcpp_runtime_sources
    array.h choice.cpp functional.cpp functions.h functions.cpp globals_impl.h
    globals_impl.cpp output.h output.cpp platform.h runner_impl.h runner_impl.cpp
    simple_restorable_stack.h stack.h stack.cpp story_impl.h story_impl.cpp
    snapshot_impl.h snapshot_impl.cpp snapshot_interface.h story_ptr.cpp system.cpp
    value.h value.cpp tuple.hpp string_table.h string_table.cpp list_table.h
    list_table.cpp list_impl.h list_impl.cpp operations.h operation_bases.h
    list_operations.h list_operations.cpp container_operations.h
    container_operations.cpp numeric_operations.h numeric_operations.cpp
    string_operations.h string_operations.cpp hungarian_solver.h
    hungarian_solver.cpp casting.h executioner.h string_utils.h header.cpp random.h
    collections/restorable.h collections/restorable.cpp)
list(TRANSFORM gyo_inkcpp_runtime_sources PREPEND "${inkcpp_SOURCE_DIR}/inkcpp/")
add_library(gyo_inkcpp STATIC ${gyo_inkcpp_runtime_sources})
target_include_directories(gyo_inkcpp SYSTEM PUBLIC
    ${inkcpp_SOURCE_DIR}/inkcpp/include
    ${inkcpp_SOURCE_DIR}/shared/public
    ${inkcpp_SOURCE_DIR}/shared/private)
# Warnings in inkcpp's own sources are not GYO's to fix.
target_compile_options(gyo_inkcpp PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>)
if(MSVC)
    target_compile_definitions(gyo_inkcpp PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_library(gyo_ink STATIC src/adapters/inkcpp/Dialogue.cpp)
add_library(GYO::Ink ALIAS gyo_ink)
target_include_directories(gyo_ink PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(gyo_ink PUBLIC cxx_std_20)
target_link_libraries(gyo_ink PUBLIC GYO::Base PRIVATE gyo_inkcpp)
gyo_link_registered_dependencies(ink)
if(MSVC)
    target_compile_options(gyo_ink PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_ink PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    # inkcpp's compiler (ink.json to the runtime's binary), for the tests only: a
    # product reads stories compiled ahead of time and links no compiler.
    set(gyo_inkcpp_compiler_sources
        compiler.cpp binary_stream.cpp json_compiler.cpp emitter.cpp reporter.cpp
        binary_emitter.cpp list_data.cpp command.cpp)
    list(TRANSFORM gyo_inkcpp_compiler_sources PREPEND "${inkcpp_SOURCE_DIR}/inkcpp_compiler/")
    add_library(gyo_inkcpp_compiler STATIC ${gyo_inkcpp_compiler_sources})
    target_include_directories(gyo_inkcpp_compiler SYSTEM PUBLIC ${inkcpp_SOURCE_DIR}/inkcpp_compiler/include)
    target_compile_definitions(gyo_inkcpp_compiler PRIVATE INK_EXPOSE_JSON)
    target_link_libraries(gyo_inkcpp_compiler PUBLIC gyo_inkcpp)
    target_compile_options(gyo_inkcpp_compiler PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>)
    if(MSVC)
        target_compile_definitions(gyo_inkcpp_compiler PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    add_subdirectory(tests)
endif()
