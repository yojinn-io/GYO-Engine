# The operating-system layer: the only Engine code that includes OS headers or
# branches on the OS (docs/architecture/os.md). A leaf directly above
# GYO::Base: it links nothing else, no other Engine module, no SDL and no
# product, so every module above it, GYO::Engine included, may use it.
# Sources that do not depend on the OS live in src/common; one source per OS
# family lives under src/<os> and is selected here (the build layer may branch
# on the OS).
add_library(gyo_os STATIC
    src/common/OsError.cpp
    src/common/Paths.cpp
)
if(WIN32)
    target_sources(gyo_os PRIVATE src/windows/Files.cpp)
else()
    target_sources(gyo_os PRIVATE src/posix/Files.cpp)
endif()
add_library(GYO::Os ALIAS gyo_os)

target_include_directories(gyo_os PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_os PUBLIC cxx_std_20)
target_link_libraries(gyo_os PUBLIC GYO::Base)
