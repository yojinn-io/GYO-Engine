# The engine time base: the monotonic clock every engine module uses, the
# deadline-or-notify Waiter and late-wake statistics. A leaf above GYO::Base
# that depends on the operating system only (kqueue on macOS, waitable timers
# on Windows, a condition variable elsewhere); no SDL, so roles without a
# window may use it.
add_library(gyo_time STATIC
    src/LateWakeStats.cpp
    src/Waiter.cpp
)
add_library(GYO::Time ALIAS gyo_time)

target_include_directories(gyo_time PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_compile_features(gyo_time PUBLIC cxx_std_20)
# Engine-internal links are written here: core, ecs and orchestration are not
# Modules and are not registered (O-6 covers the Modules and Plugins only).
target_link_libraries(gyo_time PUBLIC GYO::Base)
target_link_libraries(gyo_time PRIVATE Threads::Threads)
