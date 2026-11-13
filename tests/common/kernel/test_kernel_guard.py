"""Self-test of kernel_guard.py on synthetic trees (no repository file is read)."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kernel_guard as guard  # noqa: E402

PROJECTS = "name,description,version,enabled,windows,linux,macos\nobject_fps_pvp,fps mvp,v3,1,1,1,1\n"
TOOLS = ("name,description,version,enabled,default,release,windows,linux,macos\n"
         "voxel_map_editor,GYO Voxel Map Editor,1,true,true,false,true,true,true\n")

# Words a kernel legitimately uses: none may be reported.
CLEAN_HEADER = """#pragma once

#include "engine/base/Result.hpp"
#include "engine/kernel/World.hpp"

#include <algorithm>
#include <vector>

namespace GYO::Kernel {
// Spawn and Despawn queue structural changes; ExtractRes reads a resource; zone is C or B.
// The timezone of a host is not read here; a matcher matches ids; rematch is a word too.
// A comment may say static int x; and std::unordered_map without being code.
struct Despawn {};
struct ExtractRes {};
enum class Zone { C, B };
struct Matcher {
    [[nodiscard]] bool matches(int nearest, int farther) const noexcept { return nearest < farther; }
};
struct TypeInfo {
    static constexpr int kLimit = 4;
    static int Count(const TypeInfo& info) noexcept;
    static void Reset();
};
template <typename T>
bool Check(const T& value) {
    static_assert(sizeof(T) > 0);
    const auto narrow = static_cast<int>(value.id);
    const char* text = "std::chrono printf( static int x; far near";
    return narrow < TypeInfo::kLimit && text != nullptr;
}
inline int rematch = 0;
} // namespace GYO::Kernel
"""


class GuardTree(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.root = Path(self._directory.name)
        self.write("engine/config/projects.csv", PROJECTS)
        self.write("engine/config/tools.csv", TOOLS)
        self.write("engine/kernel/include/engine/kernel/Clean.hpp", CLEAN_HEADER)
        self.write("engine/kernel/CMakeLists.txt", "add_library(gyo_kernel INTERFACE)\n")

    def tearDown(self):
        self._directory.cleanup()

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def header(self, body, name="Added.hpp"):
        self.write("engine/kernel/include/engine/kernel/" + name,
                   "#pragma once\nnamespace GYO::Kernel {\n" + body + "\n}\n")

    def assertReported(self, rule, needle):
        problems = guard.check(self.root)
        hits = [p for p in problems if f": {rule} " in p and needle in p]
        self.assertTrue(hits, f"expected {rule} {needle!r} among {problems}")

    def assertClean(self):
        self.assertEqual(guard.check(self.root), [])

    # Counter-examples -------------------------------------------------------------------------------------------

    def test_clean_tree_passes(self):
        self.assertClean()

    # K1 ---------------------------------------------------------------------------------------------------------

    def test_k1_registered_product_id_is_reported(self):
        self.header("// object_fps_pvp drives this")
        self.assertReported("K1", "object_fps_pvp")

    def test_k1_registered_id_in_camel_case_is_reported(self):
        self.header("struct ObjectFpsPvpState {};")
        self.assertReported("K1", "ObjectFpsPvp")

    def test_k1_registered_tool_id_is_reported(self):
        self.header("struct VoxelMapEditorHook {};")
        self.assertReported("K1", "VoxelMapEditor")

    def test_k1_a_newly_registered_product_is_reported_without_changing_the_guard(self):
        self.write("engine/config/projects.csv", PROJECTS + "space_race,new,v1,1,1,1,1\n")
        self.header("struct SpaceRaceState {};")
        self.assertReported("K1", "SpaceRace")

    def test_k1_product_alias_is_reported(self):
        self.header("// see RetroFPS/Sim")
        self.assertReported("K1", "RetroFPS")

    def test_k1_role_word_is_reported(self):
        self.header("struct MatchClock {}; // per Match")
        self.assertReported("K1", "Match")

    def test_k1_role_word_player_is_reported(self):
        self.header("struct Player {};")
        self.assertReported("K1", "Player")

    def test_k1_name_in_cmakelists_is_reported(self):
        self.write("engine/kernel/CMakeLists.txt", "add_library(gyo_kernel INTERFACE) # for Gateway\n")
        self.assertReported("K1", "Gateway")

    def test_k1_missing_registry_is_reported(self):
        (self.root / "engine/config/tools.csv").unlink()
        self.assertReported("K1", "registry missing")

    def test_k1_registry_without_name_column_is_reported(self):
        self.write("engine/config/projects.csv", "id,description\nx,y\n")
        self.assertReported("K1", "no name column")

    # K2 ---------------------------------------------------------------------------------------------------------

    def test_k2_unordered_container_is_reported(self):
        self.header("std::unordered_map<int, int> index;")
        self.assertReported("K2", "unordered_map")

    def test_k2_std_hash_is_reported(self):
        self.header("inline auto h = std::hash<int>{}(1);")
        self.assertReported("K2", "std::hash")

    # K3 ---------------------------------------------------------------------------------------------------------

    def test_k3_clock_header_is_reported(self):
        self.write("engine/kernel/src/Clock.cpp", "#include <chrono>\n")
        self.assertReported("K3", "<chrono>")

    def test_k3_random_call_is_reported(self):
        self.header("inline int Roll() { return rand(); }")
        self.assertReported("K3", "rand")

    def test_k3_thread_name_is_reported(self):
        self.header("inline void Run() { std::thread worker; }")
        self.assertReported("K3", "std::thread")

    def test_k3_atomic_name_is_reported(self):
        self.header("inline std::atomic<int> counter;")
        self.assertReported("K3", "std::atomic")

    # K4 ---------------------------------------------------------------------------------------------------------

    def test_k4_function_local_static_cache_is_reported(self):
        self.header("inline int Cached() { static int cache = 0; return cache; }")
        self.assertReported("K4", "static cache")

    def test_k4_thread_local_variable_is_reported(self):
        self.header("inline thread_local int depth = 0;")
        self.assertReported("K4", "thread_local depth")

    def test_k4_direct_initialized_static_is_reported(self):
        self.header("inline int Cached() { static int cache(0); return cache; }")
        self.assertReported("K4", "static")

    def test_k4_allowlisted_thread_local_passes(self):
        self.header("struct Context {};\ninline thread_local Context* tlCurrent = nullptr;")
        self.assertClean()

    def test_k4_allowlisted_static_thread_local_passes(self):
        self.header("struct Context {};\nstatic thread_local Context* tlCurrent = nullptr;")
        self.assertClean()

    def test_k4_allowlisted_type_key_char_passes(self):
        self.header("template <typename T>\nstruct TypeKey { static inline char typeKeyTag; };")
        self.assertClean()

    def test_k4_allowlisted_name_with_the_wrong_type_is_reported(self):
        self.header("template <typename T>\nstruct TypeKey { static inline int typeKeyTag; };")
        self.assertReported("K4", "typeKeyTag")

    def test_k4_tlcurrent_without_thread_local_is_reported(self):
        self.header("struct Context {};\nstatic Context* tlCurrent = nullptr;")
        self.assertReported("K4", "tlCurrent")

    def test_k4_allowlist_is_exactly_two_symbols(self):
        # spec J-9: adding a third symbol to the allowlist must fail this self-test.
        self.assertEqual(set(guard.ALLOWED_STATICS), {"tlCurrent", "typeKeyTag"})

    # K5 ---------------------------------------------------------------------------------------------------------

    def test_k5_capability_module_include_is_reported(self):
        self.write("engine/kernel/src/Uses.cpp", '#include "engine/render/RenderDevice.hpp"\n')
        self.assertReported("K5", "engine/render/RenderDevice.hpp")

    def test_k5_platform_header_is_reported(self):
        self.write("engine/kernel/src/Uses.cpp", "#include <windows.h>\n")
        self.assertReported("K5", "windows.h")

    # K6 ---------------------------------------------------------------------------------------------------------

    def test_k6_log_call_is_reported(self):
        self.header('inline void Fail() { Engine::Base::Log(Engine::Base::LogLevel::Error, "kernel", "x"); }')
        self.assertReported("K6", "Log")

    def test_k6_log_header_is_reported(self):
        self.write("engine/kernel/src/World.cpp", '#include "engine/base/Log.hpp"\n')
        self.assertReported("K6", "engine/base/Log.hpp")

    def test_k6_stream_output_is_reported(self):
        self.write("engine/kernel/src/World.cpp", "#include <iostream>\nvoid F() { std::cerr << 1; }\n")
        self.assertReported("K6", "std::cerr")
        self.assertReported("K6", "iostream")

    def test_k6_printf_is_reported(self):
        self.header('inline void Fail() { std::printf("x"); }')
        self.assertReported("K6", "printf")

    def test_k6_executor_file_is_exempt(self):
        self.write("engine/kernel/src/Executor.cpp",
                   '#include "engine/base/Log.hpp"\nnamespace GYO::Kernel {\n'
                   'void Report() { Engine::Base::Log(Engine::Base::LogLevel::Error, "kernel", "x"); }\n}\n')
        self.assertClean()

    # K7 ---------------------------------------------------------------------------------------------------------

    def test_k7_far_identifier_is_reported(self):
        self.header("inline float Clip(float far) { return far; }")
        self.assertReported("K7", "far")

    def test_k7_near_identifier_is_reported(self):
        self.header("struct Plane { float near; };")
        self.assertReported("K7", "near")

    # Root -------------------------------------------------------------------------------------------------------

    def test_missing_scan_root_is_reported(self):
        for path in sorted((self.root / "engine/kernel").rglob("*"), reverse=True):
            path.unlink() if path.is_file() else path.rmdir()
        (self.root / "engine/kernel").rmdir()
        self.assertEqual(guard.check(self.root), ["engine/kernel: no source to scan (moved or renamed? update "
                                                  "this guard)"])

    def test_main_returns_one_on_finding(self):
        self.header("struct Player {};")
        self.assertEqual(guard.main(["--root", str(self.root)]), 1)

    def test_main_returns_zero_when_clean(self):
        self.assertEqual(guard.main(["--root", str(self.root)]), 0)


if __name__ == "__main__":
    unittest.main()
