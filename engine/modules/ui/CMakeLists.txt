# The ui Module (docs/architecture/target/v2/ui.zh-Hant.md): the gyo.ui Data Contract (codec, validation, the
# asset loader), layout with text measured by the text Module, the runtime (focus, controls, the text field) and the
# draw list. No render, no input, no SDL (Design Invariant UI-1). Not part of any World (library_only in
# config/modules.csv); world_ui and ui_render are its two ends.
add_library(gyo_ui STATIC
    src/UiColor.cpp
    src/UiDocumentCodec.cpp
    src/UiSerialization.cpp
    src/UiValidation.cpp
)
add_library(GYO::Ui ALIAS gyo_ui)

target_include_directories(gyo_ui
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(gyo_ui PUBLIC cxx_std_20)
# Links to core, ecs and orchestration are written here; links to other Modules come from config/modules.csv (O-6).
target_link_libraries(gyo_ui PUBLIC GYO::Base GYO::Math)
gyo_link_registered_dependencies(ui)
if(MSVC)
    target_compile_options(gyo_ui PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(gyo_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()
