# The trace Module (docs/architecture/target/v2/trace-declaration.zh-Hant.md; Design Invariants in
# docs/architecture/target/v2/trace.zh-Hant.md): the receiver of the Base trace facade. It keeps one span stack
# per thread, times a span only when some sink takes its record, and writes spans and paths as logging records
# (declaration D5: the responsibility is trace's, the transport is logging's). No external component.
add_library(gyo_trace STATIC
    src/Trace.cpp
)
add_library(GYO::Trace ALIAS gyo_trace)

target_include_directories(gyo_trace PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_trace PUBLIC cxx_std_20)
# Engine-internal links come from config/modules.csv (O-6): GYO::Base publicly; GYO::Logging and GYO::Time
# privately.
gyo_link_registered_dependencies(trace)
if(MSVC)
    target_compile_options(gyo_trace PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_trace PRIVATE -Wall -Wextra -Wpedantic)
endif()
