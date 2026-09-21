add_library(gyo_ui STATIC
    src/UiColor.cpp
    src/UiDocumentCodec.cpp
    src/UiSerialization.cpp
    src/UiValidation.cpp
    src/UiRuntime.cpp
)
add_library(GYO::Ui ALIAS gyo_ui)

target_include_directories(gyo_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(gyo_ui
    PUBLIC GYO::Engine
    PRIVATE nlohmann_json::nlohmann_json
)
target_compile_features(gyo_ui PUBLIC cxx_std_20)

add_library(gyo_ui_renderer STATIC
    src/UiRenderer.cpp
)
add_library(GYO::UiRenderer ALIAS gyo_ui_renderer)

target_include_directories(gyo_ui_renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(gyo_ui_renderer PUBLIC
    GYO::Ui
    GYO::Render
    GYO::Text
)
target_compile_features(gyo_ui_renderer PUBLIC cxx_std_20)


