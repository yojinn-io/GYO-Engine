# The net Module (docs/architecture/target/v2/net-module-declaration.zh-Hant.md; Design Invariants draft in
# docs/architecture/target/v2/net.zh-Hant.md). Step 1a holds the part that knows no World: the Link interface
# with its UDP and in-memory implementations, the packet header and the Connection, a pure state machine for
# acks, the two channels, fragments and the drop counts (declaration D-2, D-3, D-12). The registration unit,
# the Peer entity and the apply points come in step 1b; until then the row in config/modules.csv is
# library_only.
add_library(gyo_net STATIC
    src/Connection.cpp
    src/MemoryLink.cpp
    src/NetFailure.cpp
    src/PacketHeader.cpp
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
target_link_libraries(gyo_net PUBLIC GYO::Base GYO::OS)
gyo_link_registered_dependencies(net)
target_compile_features(gyo_net PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_net PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_net PRIVATE -Wall -Wextra -Wpedantic)
endif()
