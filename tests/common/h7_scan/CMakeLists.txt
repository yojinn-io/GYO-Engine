# H-7 host scan (spec H-7, target/kernel DI-KS 15; docs/architecture/plans/kernel/h7-scanner-declaration.zh-Hant.md).
# A text guard in Python, so it gives the same result on every platform.
#   h7_scan.selftest  the scanner on synthetic owner trees (positive and negative cases, contract, removability)
#   h7_scan.empty     with no owner list the common layer scans nothing and passes (AGENTS.md section 7)
# Each owner registers its own host list and scan in tests/<owner>/ (spec J.4); nothing here names a product.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME h7_scan.selftest
    COMMAND "${Python3_EXECUTABLE}" -I -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_*.py" -v)
add_test(NAME h7_scan.empty
    COMMAND "${Python3_EXECUTABLE}" -I -B "${CMAKE_CURRENT_SOURCE_DIR}/h7_scan.py")
set_tests_properties(h7_scan.selftest h7_scan.empty PROPERTIES LABELS "cpu" TIMEOUT 120)
set_tests_properties(h7_scan.empty PROPERTIES PASS_REGULAR_EXPRESSION "^h7_scan: nothing to scan")
