# The logging Module (docs/architecture/target/v2/logging-declaration.zh-Hant.md; Design Invariants
# LG-1 to LG-11 in docs/architecture/target/v2/logging.zh-Hant.md): records, levels and fields, the one
# Logger of the process, the sinks, the JSON lines and text formats, writing one record directly, spans
# and the fatal path's last write. It knows nothing of ECS: what reads a World lives in
# engine/world_logging, as engine/time and engine/world_time are split. No external component.
add_library(gyo_logging STATIC
    src/ConsoleSink.cpp
    src/JsonLines.cpp
    src/JsonLinesFileSink.cpp
    src/Logger.cpp
    src/MemorySink.cpp
    src/RecordBuilder.cpp
    src/Sink.cpp
    src/TextFormat.cpp
    src/WriteFatalRecord.cpp
)
add_library(GYO::Logging ALIAS gyo_logging)

target_include_directories(gyo_logging PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_logging PUBLIC cxx_std_20)
# Engine-internal links come from engine/config/modules.csv (O-6): GYO::Base publicly; GYO::Json,
# GYO::Os, GYO::Threads and GYO::Time privately, so no user of GYO::Logging sees their headers.
gyo_link_registered_dependencies(logging)
if(MSVC)
    target_compile_options(gyo_logging PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_logging PRIVATE -Wall -Wextra -Wpedantic)
endif()
