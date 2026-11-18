# The engine Modules, in the order of engine/config/modules.csv: the build
# order, what a Module depends on first (Base lowest). The register is the one
# place that names the Modules and their dependencies (Design Invariant O-6 of
# docs/architecture/target/v2/orchestration.zh-Hant.md); each Module links its
# registered dependencies with gyo_link_registered_dependencies. GyoBuild.cmake
# loads the registers before this directory.
get_property(_gyo_units GLOBAL PROPERTY GYO_UNITS_ORDER)
foreach(_gyo_unit IN LISTS _gyo_units)
    get_property(_gyo_kind GLOBAL PROPERTY GYO_UNITS_${_gyo_unit}_KIND)
    if(_gyo_kind STREQUAL "Module")
        add_subdirectory(${_gyo_unit})
    endif()
endforeach()
