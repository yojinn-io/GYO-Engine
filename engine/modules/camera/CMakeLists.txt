# The camera Module (docs/architecture/target/v2/transform-camera-declaration.zh-Hant.md, section 4.2; Design
# Invariants CM-1 to CM-5 in docs/architecture/target/v2/camera.zh-Hant.md): cameras, projections, the target sizes
# and the registration unit CameraModule that keeps each camera's derived values.
add_library(gyo_camera STATIC
    src/Camera.cpp
    src/CameraModule.cpp
)
add_library(GYO::Camera ALIAS gyo_camera)

target_include_directories(gyo_camera PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules (transform)
# come from config/modules.csv (O-6). It depends on no window, input or render (CM-2).
target_link_libraries(gyo_camera PUBLIC GYO::Base GYO::Kernel GYO::Orchestration GYO::Math)
gyo_link_registered_dependencies(camera)
target_compile_features(gyo_camera PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_camera PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_camera PRIVATE -Wall -Wextra -Wpedantic)
endif()
