include(FetchContent)

# Decimal-to-double conversion for the JSON Module's raw-number path
# (decision v8-arch-json-fast-float: the floating-point std::from_chars is not
# available for the macOS 13.3 deployment target). Header-only; only gyo_json
# links it, privately (Design Invariant J-1). Used under its MIT licence
# (LICENSE-MIT; the project is also offered under Apache-2.0 and BSL-1.0).
# Pinned to the commit of the v8.3.3 release.
FetchContent_Declare(fast_float
    GIT_REPOSITORY https://github.com/fastfloat/fast_float.git
    GIT_TAG dbeabeeb3e6791cb96994c2a7a084c36e62ccfb2 # v8.3.3
    GIT_SUBMODULES ""
    SOURCE_SUBDIR gyo-no-upstream-cmake
)
FetchContent_MakeAvailable(fast_float)

add_library(gyo_fast_float INTERFACE)
target_include_directories(gyo_fast_float SYSTEM INTERFACE ${fast_float_SOURCE_DIR}/include)
target_compile_features(gyo_fast_float INTERFACE cxx_std_20)
