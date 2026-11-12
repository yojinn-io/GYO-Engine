# Zone-C source scan (spec J-17; docs/architecture/plans/kernel/j17-scanner.zh-Hant.md). A text
# guard in Python, so it gives the same result on every platform.
#   zone_scan.selftest  the scanner on synthetic owner trees (the judgement's positive and negative cases)
# The product lists and their scans live with each owner (AGENTS.md section 7; spec J.4): object_fps_pvp
# registers them in tests/object_fps_pvp/CMakeLists.txt (v8-product-kernel-prep-zones). run_baseline.py
# stays for an ad-hoc run over any list files.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME zone_scan.selftest
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_*.py" -v)
set_tests_properties(zone_scan.selftest PROPERTIES LABELS "cpu" TIMEOUT 120)
