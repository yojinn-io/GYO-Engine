# Engine role threads: one named, long-lived thread per role with a
# cooperative stop through the role's Time::Waiter and a per-platform priority
# hint. No thread pool, scheduler or work queue; loops, handoffs and stop order
# belong to the consumer. Depends on GYO::Time and the operating system only.
add_library(gyo_threads STATIC
    src/RoleThread.cpp
)
add_library(GYO::Threads ALIAS gyo_threads)

target_include_directories(gyo_threads PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_compile_features(gyo_threads PUBLIC cxx_std_20)
target_link_libraries(gyo_threads PUBLIC GYO::Time Threads::Threads)
