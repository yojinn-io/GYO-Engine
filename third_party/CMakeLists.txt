# Engine runtime asset catalogs use nlohmann/json.
add_subdirectory(nlohmann_json)

option(GYO_BUILD_UFBX_LOADER "Build the optional FBX model asset loader" OFF)
if(GYO_BUILD_UFBX_LOADER OR GYO_BUILD_OBJECT_FPS)
    add_subdirectory(ufbx)
endif()

option(
    GYO_BUILD_SDL_IMAGE_LOADER
    "Build the optional SDL_image-backed runtime texture loader"
    OFF
)

if(GYO_BUILD_RUNTIME OR GYO_BUILD_SANDBOX OR GYO_BUILD_OBJECT_FPS OR
   GYO_ENABLE_SDL_GPU OR GYO_BUILD_SDL_IMAGE_LOADER OR
   GYO_BUILD_SDL_TTF_ADAPTER OR GYO_BUILD_UI_EDITOR)
    add_subdirectory(sdl3)
endif()

if(GYO_BUILD_OBJECT_FPS OR GYO_BUILD_SDL_IMAGE_LOADER OR GYO_BUILD_UI_EDITOR)
    add_subdirectory(sdl3_image)
endif()

if(GYO_BUILD_OBJECT_FPS OR GYO_BUILD_SDL_TTF_ADAPTER OR GYO_BUILD_UI_EDITOR)
    add_subdirectory(sdl3_ttf)
endif()

if(GYO_BUILD_SANDBOX OR GYO_BUILD_UI_EDITOR)
    add_subdirectory(imgui)
endif()

if(BUILD_TESTING)
    add_subdirectory(doctest)
endif()
