if(NOT COMMAND gyo_app_requirements)
    message(FATAL_ERROR "Configure this application from the GYO repository root with GYO_APPS")
endif()
gyo_app_requirements(COMPONENTS SDL_GPU SDL_INPUT SDL_IMAGE SDL_TTF UFBX)
if(GYO_APP_DISCOVERY)
    return()
endif()
if(APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "13.3")
    message(FATAL_ERROR "This application requires macOS 13.3 or newer")
endif()

gyo_app_project()
include(sources.cmake)

gyo_app_add_library(domain OUT_TARGET domain SOURCES ${APP_DOMAIN_SOURCES}
    PUBLIC_LIBRARIES GYO::Engine GYO::Ui GYO::Collision)
gyo_app_add_library(app_support OUT_TARGET support SOURCES ${APP_SUPPORT_SOURCES}
    PUBLIC_LIBRARIES ${domain} GYO::Engine GYO::Model GYO::Render
    PRIVATE_LIBRARIES GYO::Input GYO::Text GYO::UiRenderer nlohmann_json::nlohmann_json
    PRIVATE_COMPONENTS SDL_INPUT SDL_PLATFORM)
gyo_app_add_executable(main MAIN OUT_TARGET app
    SOURCES main.cpp
    LIBRARIES ${support}
    COMPONENTS SDL_IMAGE UFBX SDL_INPUT SDL_PLATFORM SDL_GPU SDL_TTF)
target_compile_definitions(${app} PRIVATE GYO_DEFAULT_GPU_DRIVER="${GYO_DEFAULT_GPU_DRIVER}")

gyo_app_deploy_content(TARGET ${app} ASSETS COMMON_ASSETS
    SHADER_SPEC shaders/bundle.json BUILTIN_SHADERS)
