# Engine-wide math foundation: value types, pure geometry and coordinate
# conventions. Header-only and dependent on the C++ standard library alone, so
# every engine module and product may link it without new transitive edges.
add_library(gyo_math INTERFACE)
add_library(GYO::Math ALIAS gyo_math)

target_include_directories(gyo_math INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_math INTERFACE cxx_std_20)
