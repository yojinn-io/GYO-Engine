# The engine time base: the monotonic clock every engine module uses, the
# deadline-or-notify Waiter and late-wake statistics. Above GYO::Base and,
# privately, GYO::Os, whose wake primitive the Waiter blocks on (kqueue on
# macOS, waitable timers on Windows, a condition variable elsewhere;
# os-wait-backend, D71): what links GYO::Time gets no engine/os headers from
# it. No SDL, so roles without a window may use it.
add_library(gyo_time STATIC
    src/LateWakeStats.cpp
    src/Waiter.cpp
)
add_library(GYO::Time ALIAS gyo_time)

target_include_directories(gyo_time PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_time PUBLIC cxx_std_20)
target_link_libraries(gyo_time PUBLIC GYO::Base PRIVATE GYO::Os)
