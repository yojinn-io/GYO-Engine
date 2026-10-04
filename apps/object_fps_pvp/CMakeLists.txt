gyo_app_project()
include(sources.cmake)

# Owner-demand dependencies: selecting no networking product does not fetch them.
if(NOT TARGET protobuf::libprotobuf)
    add_subdirectory("${GYO_REPOSITORY_ROOT}/third_party/protobuf" "${PROJECT_BINARY_DIR}/third_party/protobuf")
endif()
if(NOT TARGET asio::asio)
    add_subdirectory("${GYO_REPOSITORY_ROOT}/third_party/asio" "${PROJECT_BINARY_DIR}/third_party/asio")
endif()
if(NOT TARGET httplib::httplib)
    add_subdirectory("${GYO_REPOSITORY_ROOT}/third_party/httplib" "${PROJECT_BINARY_DIR}/third_party/httplib")
endif()
set(protocol_output "${CMAKE_CURRENT_BINARY_DIR}/protocol")
file(MAKE_DIRECTORY "${protocol_output}")
foreach(contract client_v5 runtime_v5)
    add_custom_command(OUTPUT "${protocol_output}/${contract}.pb.cc" "${protocol_output}/${contract}.pb.h"
        COMMAND $<TARGET_FILE:protobuf::protoc> "--cpp_out=${protocol_output}"
            "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}/protocol" "${CMAKE_CURRENT_SOURCE_DIR}/protocol/${contract}.proto"
        DEPENDS protobuf::protoc "${CMAKE_CURRENT_SOURCE_DIR}/protocol/${contract}.proto" VERBATIM)
    set(protocol_target "gyo_${GYO_APP_ID}_${contract}")
    add_library(${protocol_target} STATIC "${protocol_output}/${contract}.pb.cc")
    target_include_directories(${protocol_target} PUBLIC "${protocol_output}")
    target_link_libraries(${protocol_target} PUBLIC protobuf::libprotobuf)
endforeach()

gyo_app_add_library(match_domain OUT_TARGET domain SOURCES ${PVP_DOMAIN_SOURCES}
    PUBLIC_LIBRARIES GYO::Engine GYO::Math GYO::Collision
    PRIVATE_LIBRARIES nlohmann_json::nlohmann_json)
# Apple libc++ hides std::jthread/std::stop_token behind its experimental gate.
# Every product target reaches the domain library, so the flag stays product-local.
target_compile_options(${domain} PUBLIC $<$<CXX_COMPILER_ID:AppleClang>:-fexperimental-library>)
gyo_app_add_library(runtime_host OUT_TARGET host SOURCES src/Pvp/MatchRuntimeHost.cpp
    PUBLIC_LIBRARIES ${domain})
gyo_app_add_library(ipc OUT_TARGET ipc SOURCES src/Pvp/IpcHost.cpp
    PUBLIC_LIBRARIES ${host} GYO::Net
    PRIVATE_LIBRARIES asio::asio "gyo_${GYO_APP_ID}_runtime_v5")
gyo_app_add_library(client_network OUT_TARGET network SOURCES src/Pvp/ClientConnection.cpp
    PUBLIC_LIBRARIES ${domain} GYO::Net
    PRIVATE_LIBRARIES asio::asio httplib::httplib nlohmann_json::nlohmann_json "gyo_${GYO_APP_ID}_client_v5")
gyo_app_add_library(app_support OUT_TARGET support SOURCES src/Pvp/PvpApplication.cpp
        src/Pvp/PlayerPresentation.cpp src/App/CharacterPresentationDefinition.cpp
        src/App/WeaponViewModel.cpp src/App/WeaponPresentationDefinition.cpp src/App/AnimationSetDefinition.cpp
    PUBLIC_LIBRARIES ${domain} ${network} GYO::Render GYO::Model GYO::ModelRenderer
    PRIVATE_LIBRARIES GYO::Input GYO::Text GYO::UiRenderer nlohmann_json::nlohmann_json
    PRIVATE_COMPONENTS SDL_IMAGE UFBX SDL_INPUT SDL_PLATFORM SDL_GPU SDL_TTF)
gyo_app_add_executable(main MAIN OUT_TARGET app SOURCES main.cpp LIBRARIES ${support}
    COMPONENTS SDL_IMAGE UFBX SDL_INPUT SDL_PLATFORM SDL_GPU SDL_TTF)
target_compile_definitions(${app} PRIVATE GYO_DEFAULT_GPU_DRIVER="${GYO_DEFAULT_GPU_DRIVER}")
gyo_app_add_executable(match OUT_TARGET match SOURCES match_main.cpp LIBRARIES ${ipc})
gyo_install_product_target(TARGET ${match} PRODUCT "${GYO_APP_ID}" KIND app OWNER "${GYO_APP_ID}" ROLE match)
add_dependencies(${match} "gyo_${GYO_APP_ID}--content")

# The Go gateway is this game's service: Go stays optional for native builds,
# the host build is staged outside native archives and CI cross-builds the
# declared platforms (no macOS gateway).
gyo_app_add_go_service(gateway PACKAGE ./gateway/cmd PLATFORMS linux-x64 windows-x64)
