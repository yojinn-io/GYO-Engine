# Zone-C source scan (spec J-17; docs/architecture/plans/kernel/j17-scanner.zh-Hant.md). A text
# guard in Python, so it gives the same result on every platform.
#   zone_scan.selftest  the scanner on synthetic owner trees (the judgement's positive and negative cases)
#   zone_scan.baseline  every list in baseline/ scanned with --no-fail: a report, not a gate (the
#                       products are not zone-C compliant yet); it fails only on a contract error,
#                       such as a listed file that no longer exists.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME zone_scan.selftest
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_*.py" -v)
add_test(NAME zone_scan.baseline
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_baseline.py" --root "${GYO_REPOSITORY_ROOT}"
            --out "${CMAKE_BINARY_DIR}/zone_scan")
set_tests_properties(zone_scan.selftest zone_scan.baseline PROPERTIES LABELS "cpu" TIMEOUT 120)
