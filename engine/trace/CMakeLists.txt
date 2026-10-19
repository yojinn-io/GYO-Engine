# Engine trace transport: a bounded multi-producer queue of pre-serialized
# lines per file, written by one role thread woken by notify (never by a
# timer), with exact drop counts and a consumer-supplied end line. Record
# kinds, content and schemas belong to the consumer. Depends on GYO::Threads
# and the standard library only; no SDL.
add_library(gyo_trace STATIC
    src/TraceSink.cpp
)
add_library(GYO::Trace ALIAS gyo_trace)

target_include_directories(gyo_trace PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_trace PUBLIC cxx_std_20)
target_link_libraries(gyo_trace PUBLIC GYO::Threads)
