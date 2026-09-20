# Engine runtime asset catalogs use nlohmann/json.
add_subdirectory(nlohmann_json)

if(GYO_ENABLE_UFBX)
    add_subdirectory(ufbx)
endif()

if(GYO_ENABLE_SDL3)
    add_subdirectory(sdl3)
endif()

if(GYO_ENABLE_SDL_IMAGE)
    add_subdirectory(sdl3_image)
endif()

if(GYO_ENABLE_SDL_TTF)
    add_subdirectory(sdl3_ttf)
endif()

if(GYO_ENABLE_IMGUI)
    add_subdirectory(imgui)
endif()

if(BUILD_TESTING)
    add_subdirectory(doctest)
endif()
