# A small HTTP/1.1 client (the PvP Gateway port's declaration D8, games/object_fps_pvp/docs/gateway-declaration.zh-Hant.md):
# the request's bytes, an answer read as its bytes arrive, and one call on an engine/core/os StreamSocket polled by its
# caller. A library (config/modules.csv library_only=yes): no registration unit.
add_library(gyo_http STATIC
    src/Http.cpp
)
add_library(GYO::Http ALIAS gyo_http)

target_include_directories(gyo_http PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core are written here; links to other Modules come from config/modules.csv (O-6). It depends on no other
# Module. The socket is engine/core/os's (Design Invariant 1 of os.md: OS knowledge lives only there).
target_link_libraries(gyo_http PUBLIC GYO::Base GYO::OS)
gyo_link_registered_dependencies(http)
target_compile_features(gyo_http PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_http PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_http PRIVATE -Wall -Wextra -Wpedantic)
endif()
