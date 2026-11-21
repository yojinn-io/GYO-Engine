# The rollback Module (docs/architecture/target/v2/rollback-module-declaration.zh-Hant.md; batch 4,
# docs/architecture/target/v2/authority-rollback-declaration.zh-Hant.md section 5): the public registrations,
# the RollbackStrategy interface and, from step 4, the official strategy.
add_library(gyo_rollback STATIC
    src/Registrations.cpp
    src/RollbackModule.cpp
    src/StateRollbackStrategy.cpp
)
add_library(GYO::Rollback ALIAS gyo_rollback)

target_include_directories(gyo_rollback PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Engine-internal links come from config/modules.csv (O-6).
gyo_link_registered_dependencies(rollback)
target_compile_features(gyo_rollback PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_rollback PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_rollback PRIVATE -Wall -Wextra -Wpedantic)
endif()
