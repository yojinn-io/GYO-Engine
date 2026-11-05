# Judgment E7 of v8-tool-voxel-map-editor-core (docs/tools/plans/v8/voxel-map-editor.md):
# the command line's success and failure paths, run with cmake -P.
# Inputs: EDITOR (the executable), WORK (a scratch folder), FIXTURES.

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(last_output "")
macro(check expected name)
    execute_process(COMMAND "${EDITOR}" ${ARGN} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT code STREQUAL "${expected}")
        message(FATAL_ERROR "${name}: exit ${code}, expected ${expected}\n${out}${err}")
    endif()
    set(last_output "${out}${err}")
endmacro()

check(0 help --help)
check(2 no_command)
check(2 unknown nonsense)
check(2 build_arguments build "${FIXTURES}/sample.voxel_grid.json")

# build writes what the golden file holds, byte for byte.
check(0 build build "${FIXTURES}/sample.voxel_grid.json" "${WORK}/out/sample.voxel_mesh.json")
file(SHA256 "${WORK}/out/sample.voxel_mesh.json" built)
file(SHA256 "${FIXTURES}/sample.voxel_mesh.json" golden)
if(NOT built STREQUAL golden)
    message(FATAL_ERROR "build: the baked file differs from the golden file")
endif()
if(NOT last_output MATCHES "11 cells, 84 quads, 12 colliders")
    message(FATAL_ERROR "build: the summary is wrong\n${last_output}")
endif()

# verify: the same file passes; one changed byte fails.
check(0 verify_same verify "${FIXTURES}/sample.voxel_grid.json" "${WORK}/out/sample.voxel_mesh.json")
file(READ "${WORK}/out/sample.voxel_mesh.json" mesh)
string(REPLACE "\"cell_size\": 0.5" "\"cell_size\": 0.6" changed "${mesh}")
file(CONFIGURE OUTPUT "${WORK}/out/changed.voxel_mesh.json" CONTENT "${changed}" @ONLY NEWLINE_STYLE LF)
check(4 verify_different verify "${FIXTURES}/sample.voxel_grid.json" "${WORK}/out/changed.voxel_mesh.json")
check(3 verify_missing verify "${FIXTURES}/sample.voxel_grid.json" "${WORK}/nowhere.json")

# An invalid grid writes nothing.
file(CONFIGURE OUTPUT "${WORK}/bad.voxel_grid.json" CONTENT "{\"schema\": \"gyo.voxel_grid\"}\n" @ONLY NEWLINE_STYLE LF)
check(1 build_invalid build "${WORK}/bad.voxel_grid.json" "${WORK}/bad.voxel_mesh.json")
if(EXISTS "${WORK}/bad.voxel_mesh.json")
    message(FATAL_ERROR "build_invalid: a file was written")
endif()
# A grid Engine refuses to have baked (no blocks placed, so no quads) writes nothing.
file(READ "${FIXTURES}/sample.voxel_grid.json" grid)
string(REGEX REPLACE "\"chunks\": \\[.*\\]\n}" "\"chunks\": []\n}" emptied "${grid}")
file(CONFIGURE OUTPUT "${WORK}/empty.voxel_grid.json" CONTENT "${emptied}" @ONLY NEWLINE_STYLE LF)
check(1 build_refused build "${WORK}/empty.voxel_grid.json" "${WORK}/empty.voxel_mesh.json")
if(NOT last_output MATCHES "BuildRefused")
    message(FATAL_ERROR "build_refused: not refused by Engine\n${last_output}")
endif()
if(EXISTS "${WORK}/empty.voxel_mesh.json")
    message(FATAL_ERROR "build_refused: a file was written")
endif()

# inspect knows both formats.
check(0 inspect_grid inspect "${FIXTURES}/sample.voxel_grid.json")
if(NOT last_output MATCHES "gyo.voxel_grid: 3 shapes, 3 blocks, 2 chunks, 11 cells")
    message(FATAL_ERROR "inspect_grid\n${last_output}")
endif()
check(0 inspect_mesh inspect "${FIXTURES}/sample.voxel_mesh.json")
if(NOT last_output MATCHES "84 quads, 12 colliders")
    message(FATAL_ERROR "inspect_mesh\n${last_output}")
endif()
check(1 inspect_invalid inspect "${WORK}/bad.voxel_grid.json")

# import-boxes takes boxes from wherever the caller points.
file(CONFIGURE OUTPUT "${WORK}/boxes.json" CONTENT "{\"level\": {\"solid\": [{\"min\": [-1, 0, -1], \"max\": [0, 3, 2]}, {\"min\": [2, 0, 2], \"max\": [4, 2, 4]}]}}\n" @ONLY NEWLINE_STYLE LF)
check(0 import import-boxes "${WORK}/boxes.json" /level/solid "${WORK}/imported.voxel_grid.json")
check(0 import_build build "${WORK}/imported.voxel_grid.json" "${WORK}/imported.voxel_mesh.json")
if(NOT last_output MATCHES "17 cells, .* quads, 2 colliders")
    message(FATAL_ERROR "import_build\n${last_output}")
endif()
check(1 import_missing_pointer import-boxes "${WORK}/boxes.json" /walls "${WORK}/nothing.voxel_grid.json")
if(EXISTS "${WORK}/nothing.voxel_grid.json")
    message(FATAL_ERROR "import_missing_pointer: a file was written")
endif()
