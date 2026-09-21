add_library(gyo_text_backend_sdl_ttf STATIC
    src/SdlTtfTextRasterizer.cpp
)

add_library(GYO::TextBackendSDLTTF ALIAS gyo_text_backend_sdl_ttf)

target_include_directories(gyo_text_backend_sdl_ttf PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gyo_text_backend_sdl_ttf
    PUBLIC
        GYO::Text
    PRIVATE
        SDL3_ttf::SDL3_ttf
        SDL3::SDL3
)

target_compile_features(gyo_text_backend_sdl_ttf PUBLIC cxx_std_20)
