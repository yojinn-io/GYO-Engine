# The naming guard (docs/architecture/target/v2/rebuild-naming.zh-Hant.md N17, target/naming Design Invariant 1):
# every Engine unit's namespace is the one derived from its directory, and the roots under GYO are a closed table.
# It reads engine/, plugins/, games/, tools/, tests/ and examples/ as text, so it owns no unit and sits apart from
# the unit tests (v8-arch-gyo2-naming-cleanup; it was kernel.naming_guard). Its mutants are
# docs/architecture/target/v2/scripts/naming_mutations.json. -B: no bytecode is written into the source tree.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME naming.guard
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/naming_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME naming.guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_naming_guard.py" -v)
set_tests_properties(naming.guard naming.guard_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)
