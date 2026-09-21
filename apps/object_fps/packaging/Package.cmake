# Release acceptance is an optional engineering contract, not a build prerequisite.
include("${CMAKE_CURRENT_LIST_DIR}/../tests/diagnostics/Diagnostics.cmake")
object_fps_enable_diagnostics()
gyo_register_app_package(STARTUP_ARGS --startup-smoke-test
    REQUIRED_FILES package_info.py manual_gpu_smoke.py validate_content.py)
gyo_add_app_package_check(NAME gameplay PROFILES release TIMEOUT 90
    COMMAND "@EXECUTABLE@" --headless-smoke-test)
gyo_add_app_package_check(NAME content PROFILES release TIMEOUT 420
    COMMAND "@PYTHON@" "@PACKAGE_ROOT@/validate_content.py"
        --stage "@PACKAGE_ROOT@" --logs "@LOG_ROOT@/content")
gyo_add_app_package_check(NAME rendering GPU PLATFORMS linux-x64 TIMEOUT 1100
    COMMAND "@PYTHON@" "@PACKAGE_ROOT@/manual_gpu_smoke.py"
        --package "@PACKAGE_ROOT@" --driver "@DRIVER@" --suite "@GPU_SUITE@"
        --timeout 120 --output "@LOG_ROOT@/rendering")
install(PROGRAMS tests/package/package_info.py tests/package/manual_gpu_smoke.py tests/package/validate_content.py DESTINATION .)
install(DIRECTORY docs/ DESTINATION docs OPTIONAL)
