# The authority Module (docs/architecture/target/v2/authority-module-declaration.zh-Hant.md; batch 4,
# docs/architecture/target/v2/authority-rollback-declaration.zh-Hant.md section 4): the AuthorityStrategy socket
# the registrations of decision data, the official ServerAuthorityStrategy, the consistency fingerprint, the digest
# and the G-5 export.
add_library(gyo_authority STATIC
    src/AuthorityModule.cpp
    src/Consistency.cpp
    src/Registrations.cpp
    src/ServerAuthorityStrategy.cpp
)
add_library(GYO::Authority ALIAS gyo_authority)

target_include_directories(gyo_authority PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules
# come from config/modules.csv (O-6).
target_link_libraries(gyo_authority PUBLIC GYO::Base GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(authority)
target_compile_features(gyo_authority PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_authority PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_authority PRIVATE -Wall -Wextra -Wpedantic)
endif()
