# Engine threads (docs/architecture/threads.md): RoleThread, one named,
# long-lived thread per role with a cooperative stop through the role's
# Time::Waiter and a per-platform priority hint; WorkerPool, a fixed number of
# role threads taking jobs first in, first out, owned by whoever creates it. No
# scheduler, job dependencies or global pool; loops, handoffs and stop order
# belong to the consumer. Depends on GYO::Time, and privately on GYO::OS for
# the thread name and priority calls (os-threads-backend, D69).
add_library(gyo_threads STATIC
    src/RoleThread.cpp
    src/WorkerPool.cpp
)
add_library(GYO::Threads ALIAS gyo_threads)

target_include_directories(gyo_threads PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_compile_features(gyo_threads PUBLIC cxx_std_20)
# Engine-internal links are written here: core, ecs and orchestration are not
# Modules and are not registered (O-6 covers the Modules and Plugins only).
target_link_libraries(gyo_threads PUBLIC GYO::Time PRIVATE GYO::OS)
target_link_libraries(gyo_threads PUBLIC Threads::Threads)
