# Byte transport only: the GYOP datagram header codec. Message kinds, payloads
# and protocol versions belong to the products that use it.
add_library(gyo_net STATIC
    src/GyopDatagram.cpp
)
add_library(GYO::Net ALIAS gyo_net)

target_include_directories(gyo_net PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_net PUBLIC cxx_std_20)
target_link_libraries(gyo_net PUBLIC GYO::Base)
