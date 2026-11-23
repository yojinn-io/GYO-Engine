# The input Module (docs/architecture/target/v2/input.zh-Hant.md; Design Invariants IN-1 to IN-5): the input Messages
# a backend writes, the state kept from them and the text input request. No SDL, no backend interface (SDL
# declaration, B').
add_library(gyo_input STATIC
    src/InputModule.cpp
)
add_library(GYO::Input ALIAS gyo_input)

target_include_directories(gyo_input PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules come from config/modules.csv (O-6).
# It depends on no other Module (IN-1): not window, not any Plugin.
target_link_libraries(gyo_input PUBLIC GYO::Base GYO::Kernel GYO::Orchestration GYO::Math GYO::Time)
gyo_link_registered_dependencies(input)
target_compile_features(gyo_input PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_input PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_input PRIVATE -Wall -Wextra -Wpedantic)
endif()
