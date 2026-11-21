# The engine: core/ (the internal capabilities), ecs/, orchestration/ and the
# Modules under modules/. core, ecs and orchestration are not Modules: every
# product has them, nothing selects them, and they are added here directly and
# link each other in their own CMakeLists.txt. The Modules follow in the order of
# config/modules.csv, each from the directory its row names (dir: modules/<name>);
# the register is the one place for the dependencies between Modules and Plugins
# (Design Invariant O-6 of docs/architecture/target/v2/orchestration.zh-Hant.md),
# linked with gyo_link_registered_dependencies. GyoBuild.cmake loads the
# registers before this directory.
add_subdirectory(core/base)
add_subdirectory(core/math)
add_subdirectory(core/os)
add_subdirectory(core/time)
add_subdirectory(core/threads)
add_subdirectory(core/io)
add_subdirectory(ecs/entity_store)
add_subdirectory(ecs/kernel)
add_subdirectory(orchestration)

get_property(_gyo_units GLOBAL PROPERTY GYO_UNITS_ORDER)
foreach(_gyo_unit IN LISTS _gyo_units)
    get_property(_gyo_kind GLOBAL PROPERTY GYO_UNITS_${_gyo_unit}_KIND)
    if(_gyo_kind STREQUAL "Module")
        get_property(_gyo_dir GLOBAL PROPERTY GYO_UNITS_${_gyo_unit}_DIR)
        add_subdirectory(${_gyo_dir})
    endif()
endforeach()
