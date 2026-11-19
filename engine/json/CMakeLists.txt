# The JSON Module: reading and writing JSON behind the engine's own interface.
# It is the engine's only adapter for its two external components, yyjson (the
# parser and the number writer) and fast_float (decimal to double): their
# headers, types and calls appear only under src/ (Design Invariant J-1 of
# docs/architecture/target/v2/json-module-declaration.zh-Hant.md). Not part of
# any World (library_only in engine/config/modules.csv).
add_library(gyo_json STATIC
    src/JsonError.cpp
)
add_library(GYO::Json ALIAS gyo_json)

target_include_directories(gyo_json PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_json PUBLIC cxx_std_20)
# Engine-internal links come from engine/config/modules.csv (O-6); the external
# components are linked here, privately, so no user of GYO::Json sees them.
gyo_link_registered_dependencies(json)
target_link_libraries(gyo_json PRIVATE gyo_yyjson gyo_fast_float)
if(MSVC)
    target_compile_options(gyo_json PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_json PRIVATE -Wall -Wextra -Wpedantic)
endif()
