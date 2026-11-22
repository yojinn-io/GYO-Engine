# The net Module (docs/architecture/target/v2/net-module-declaration.zh-Hant.md; Design Invariants draft in
# docs/architecture/target/v2/net.zh-Hant.md). Step 1a holds the part that knows no World: the Link interface
# with its UDP and in-memory implementations, the packet header and the Connection, a pure state machine for
# acks, the two channels, fragments and the drop counts (declaration D-2, D-3, D-12). Step 1b adds the
# connection layer (ClientSession, ServerHost: the handshake and liveness, D-6, D-8), the Product's NetService
# and the registration unit NetModule (the Peer entity, the apply points; D-3, D-10).
add_library(gyo_net STATIC
    src/ClientSession.cpp
    src/Connection.cpp
    src/Handshake.cpp
    src/MemoryLink.cpp
    src/NetFailure.cpp
    src/NetModule.cpp
    src/NetService.cpp
    src/PacketHeader.cpp
    src/ServerHost.cpp
    src/UdpLink.cpp
)
add_library(GYO::Net ALIAS gyo_net)

target_include_directories(gyo_net PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules
# come from config/modules.csv (O-6). The socket is reached only through
# engine/core/os (declaration D-1; AGENTS section 6: one adapter per external
# component, and the OS is that adapter's owner).
# GYO::Time: the monotonic clock of the IO layer (liveness and RTT only, D-9).
target_link_libraries(gyo_net PUBLIC GYO::Base GYO::OS GYO::Time GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(net)
target_compile_features(gyo_net PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_net PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_net PRIVATE -Wall -Wextra -Wpedantic)
endif()
