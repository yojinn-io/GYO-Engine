# Engine entity store (docs/architecture/entity-store.md): the container part of
# an ECS data layout. Typed columns kept in strictly ascending order of a key
# the caller gives, read through EntityKey<T>. Header-only and dependent only on
# the C++ standard library and GYO::Base (for GYO_ASSERT), so it is a leaf like
# GYO::Math (Design Invariant 9).
add_library(gyo_entity_store INTERFACE)
add_library(GYO::EntityStore ALIAS gyo_entity_store)

target_include_directories(gyo_entity_store INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gyo_entity_store INTERFACE GYO::Base)
target_compile_features(gyo_entity_store INTERFACE cxx_std_20)
