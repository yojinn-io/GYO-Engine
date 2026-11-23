# The window Module (docs/architecture/target/v2/window.zh-Hant.md; Design Invariants WN-1 to WN-4): the window settings,
# the window Messages, the state kept from them and the requests. No SDL, no backend interface (SDL
# declaration, B').
add_library(gyo_window STATIC
    src/WindowModule.cpp
)
add_library(GYO::Window ALIAS gyo_window)

target_include_directories(gyo_window PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules come from config/modules.csv (O-6).
# It depends on no other Module (WN-1): not input, not camera, not any Plugin.
target_link_libraries(gyo_window PUBLIC GYO::Base GYO::Kernel GYO::Orchestration GYO::Math GYO::Time)
gyo_link_registered_dependencies(window)
target_compile_features(gyo_window PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_window PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_window PRIVATE -Wall -Wextra -Wpedantic)
endif()
