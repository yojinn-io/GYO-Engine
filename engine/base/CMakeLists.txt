# The lowest engine layer: the failure-handling mechanisms (Assert for
# Programmer Errors, Result and Error for Runtime Errors) and other
# domain-free helpers. Header-only and dependent on the C++ standard library
# alone, so every module, Math included, may link it without new transitive
# edges. See docs/architecture/error-handling.md.
add_library(gyo_base INTERFACE)
add_library(GYO::Base ALIAS gyo_base)

target_include_directories(gyo_base INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_base INTERFACE cxx_std_20)
