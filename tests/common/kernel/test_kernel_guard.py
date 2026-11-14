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
inline constexpr int rematch = 0;
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

    def test_k1_registered_id_matches_as_a_word_not_inside_other_words(self):
        # Architect 2026-11-13 (review A8): an id like arena must not turn every word containing it red.
        self.write("engine/config/projects.csv", PROJECTS + "arena,new,v1,1,1,1,1\n")
        for text in ("struct Marenal {};", "constexpr int parenage = 0;"):
            with self.subTest(text=text):
                self.header(text)
                self.assertClean()

    def test_k1_registered_id_as_a_camel_case_word_is_reported(self):
        self.write("engine/config/projects.csv", PROJECTS + "arena,new,v1,1,1,1,1\n")
        for text, needle in (("struct FrameArena {};", "Arena (registered id arena"),
                             ("struct ArenaHost {};", "Arena (registered id arena")):
            with self.subTest(text=text):
                self.header(text)
                self.assertReported("K1", needle)

    def test_k1_registered_id_message_names_both_readings(self):
        self.header("struct ObjectFpsPvpHost {};")
        line = next(p for p in guard.check(self.root) if ": K1 ObjectFpsPvp" in p)
        self.assertIn("change the kernel", line)
        self.assertIn("change that product's registered id", line)

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
        self.header("template <typename T>\nstruct TypeKey { static inline char typeKeyAnchor; };")
        self.assertClean()

    def test_k4_allowlisted_name_with_the_wrong_type_is_reported(self):
        self.header("template <typename T>\nstruct TypeKey { static inline int typeKeyAnchor; };")
        self.assertReported("K4", "typeKeyAnchor")

    def test_k4_tlcurrent_without_thread_local_is_reported(self):
        self.header("struct Context {};\nstatic Context* tlCurrent = nullptr;")
        self.assertReported("K4", "tlCurrent")

    def test_k4_inline_variable_without_static_is_reported(self):
        # Review B8: not seen by the static rule, and not in the library when no kernel source odr-uses it.
        for text, name in (("inline int headerCounter = 0;", "headerCounter"),
                           ("struct Map {};\ninline Map cache;", "cache"),
                           ("inline int counter{0};", "counter"),
                           ("inline const char* label = nullptr;", "label"),
                           ("namespace Detail {\ninline int nested = 1;\n}", "nested")):
            with self.subTest(text=text):
                self.header(text)
                self.assertReported("K4", "inline " + name)

    def test_k4_inline_constants_functions_and_namespaces_pass(self):
        for text in ("inline constexpr int kLimit = 16;",
                     "constexpr inline int kOther = 1;",
                     "inline const int kConst = 1;",
                     "inline char* const kFixed = nullptr;",
                     "inline int Twice(int x) { return x * 2; }",
                     "inline void Reset() {}",
                     "inline namespace v1 {}"):
            with self.subTest(text=text):
                self.header(text)
                self.assertClean()

    def test_k4_allowlist_is_exactly_two_symbols(self):
        # spec J-9: adding a third symbol to the allowlist must fail this self-test.
        self.assertEqual(set(guard.ALLOWED_STATICS), {"tlCurrent", "typeKeyAnchor"})

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

    # Scanning survives comments, literals and file types (adversarial check B1, B20, A1, A3) ---------------------

    def test_findings_after_a_line_comment_are_reported(self):
        self.header("// a comment line first\ninline int Cached() { static int cache = 0; return cache; }")
        self.assertReported("K4", "static cache")

    def test_findings_after_a_block_comment_raw_string_and_digit_separator_are_reported(self):
        self.header('/* block static int z;\n comment */ inline constexpr const char* raw = R"x(static int y;)x";\n'
                    "inline constexpr int big = 1'000;\nstd::unordered_map<int, int> index;")
        problems = guard.check(self.root)
        self.assertTrue(any(": K2 unordered_map" in p for p in problems), problems)
        self.assertFalse(any(": K4 " in p for p in problems), problems)

    def test_finding_after_an_unterminated_string_is_reported(self):
        self.header('inline const char* broken = "no end\nstd::unordered_map<int, int> index;')
        self.assertReported("K2", "unordered_map")

    def test_unscanned_file_type_is_reported(self):
        self.write("engine/kernel/include/engine/kernel/World.txt", "static int g;\n")
        self.assertReported("K0", "World.txt")

    def test_template_implementation_file_is_scanned(self):
        for suffix in (".ipp", ".tpp", ".inc", ".ixx", ".inl", ".h", ".hh", ".hxx", ".c", ".cc", ".cxx"):
            with self.subTest(suffix=suffix):
                self.write("engine/kernel/include/engine/kernel/World" + suffix, "#include <thread>\n")
                self.assertReported("K3", "<thread>")
                (self.root / ("engine/kernel/include/engine/kernel/World" + suffix)).unlink()

    def test_include_after_a_comment_on_the_same_line_is_read(self):
        self.write("engine/kernel/src/Uses.cpp", "/* x */ #include <windows.h>\n")
        self.assertReported("K5", "windows.h")

    def test_macro_include_is_reported(self):
        self.write("engine/kernel/src/Uses.cpp", "#define HEADER <thread>\n#include HEADER\n")
        self.assertReported("K5", "unreadable include target")

    def test_import_and_digraph_include_are_read(self):
        self.write("engine/kernel/src/Uses.cpp", "#import <windows.h>\n%:include <unistd.h>\n")
        self.assertReported("K5", "windows.h")
        self.assertReported("K5", "unistd.h")

    def test_include_on_the_first_line_after_a_bom_is_read(self):
        path = self.root / "engine/kernel/src/Uses.cpp"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes("\ufeff#include <windows.h>\n".encode("utf-8"))
        self.assertReported("K5", "windows.h")

    def test_form_feed_in_a_comment_does_not_shift_lines(self):
        self.write("engine/kernel/src/Uses.cpp", "// page\x0cbreak\n#include <thread>\n")
        self.assertReported("K3", "Uses.cpp:2: K3 <thread>")

    # Executor exemption covers K6 only (B2) ---------------------------------------------------------------------

    def test_executor_file_is_checked_by_every_other_rule(self):
        self.write("engine/kernel/src/Executor.cpp",
                   "#include <thread>\n#include <windows.h>\nnamespace GYO::Kernel {\n"
                   "std::unordered_map<int, int> index;\nint Cached() { static int cache = 0; return cache; }\n"
                   "struct Player {};\nfloat Clip(float far) { return far; }\n}\n")
        for rule, needle in (("K1", "Player"), ("K2", "unordered_map"), ("K3", "<thread>"), ("K4", "cache"),
                             ("K5", "windows.h"), ("K7", "far")):
            with self.subTest(rule=rule):
                self.assertReported(rule, needle)

    # K1 coverage (B9-B13) ---------------------------------------------------------------------------------------

    def test_k1_registered_id_ignores_case(self):
        self.header("#define OBJECT_FPS_PVP_ENABLED 1")
        self.assertReported("K1", "OBJECT_FPS_PVP")

    def test_k1_every_product_alias_is_reported(self):
        for text, needle in (("namespace fps::pvp {}", "fps::pvp"), ("struct PvpState {};", "Pvp"),
                             ("struct pvpState {};", "pvp"), ("namespace Gyo::Tools {}", "Gyo::Tools")):
            with self.subTest(text=text):
                self.header(text)
                self.assertReported("K1", needle)

    def test_k1_every_role_word_is_reported(self):
        for text, needle in (("struct player {};", "player"), ("struct Gateway {};", "Gateway")):
            with self.subTest(text=text):
                self.header(text)
                self.assertReported("K1", needle)

    # K2-K3 coverage (A7, B14-B16) -------------------------------------------------------------------------------

    def test_k2_hash_under_using_namespace_std_is_reported(self):
        self.header("inline auto h = hash<int>{}(1);")
        self.assertReported("K2", "hash")

    def test_k3_every_banned_header_is_reported(self):
        # A fixed list, not guard.K3_HEADERS: shrinking the guard's tuple must fail here.
        for header in ("chrono", "random", "thread", "atomic", "mutex", "condition_variable", "future",
                       "shared_mutex", "stop_token", "semaphore", "latch", "barrier", "ctime", "time.h", "execution"):
            with self.subTest(header=header):
                self.write("engine/kernel/src/Clock.cpp", f"#include <{header}>\n")
                self.assertReported("K3", f"<{header}>")

    def test_k3_every_banned_name_is_reported(self):
        for name in ("std::chrono", "std::random_device", "std::mt19937_64", "std::rand", "std::srand",
                     "std::thread", "std::jthread", "std::this_thread", "std::atomic_flag", "std::mutex",
                     "std::execution", "std::clock", "std::time"):
            with self.subTest(name=name):
                self.header(f"inline void Use() {{ (void){name}; }}")
                self.assertReported("K3", name)

    def test_k3_every_banned_call_is_reported(self):
        for call, needle in (("srand(1)", "srand"), ("::rand()", "::rand"), ("time(nullptr)", "time"),
                             ("clock()", "clock")):
            with self.subTest(call=call):
                self.header(f"inline void Use() {{ (void){call}; }}")
                self.assertReported("K3", needle)

    # K4 coverage: legal function shapes pass (A5), tightened allowlist (A6) -------------------------------------

    def test_k4_legal_function_and_constant_shapes_pass(self):
        self.header("""struct SystemContext;
template <typename T> struct Box {};
struct Shapes {
    constexpr static int kOrder = 4;
    static int Fetch(SystemContext&);
    static void Destroy(void*);
    template <typename... Args> static void Make(Args&&... args);
    static void Fill(Box<int> box);
    static decltype(auto) Get();
    static void Visit(void (*callback)(int));
    static void Note([[maybe_unused]] int value);
    static Shapes Create(const SystemContext& context, int count);
};""")
        self.assertClean()

    def test_k4_direct_initialized_from_a_name_is_reported(self):
        self.header("inline int seed = 1;\ninline int Cached() { static int cache(seed); return cache; }")
        self.assertReported("K4", "static")

    def test_k4_allowlisted_names_with_another_shape_are_reported(self):
        for text in ("template <typename T>\nstruct TypeKey { static inline int typeKeyAnchor; };",
                     "struct Map {};\ntemplate <typename T>\nstruct TypeKey { static inline Map typeKeyAnchor; };",
                     "struct Map {};\nthread_local Map tlCurrent;",
                     "static thread_local int evil, tlCurrent;"):
            with self.subTest(text=text):
                self.header(text)
                self.assertTrue(any(": K4 " in p for p in guard.check(self.root)), text)

    # K5 coverage: the allowlists are pinned (B4, B5, B19, A2) ---------------------------------------------------

    def test_k5_project_allowlist_is_exactly_base_entity_store_and_kernel(self):
        # Delta-3 (world-exec, user 2026-11-13) added engine/entity_store/; nothing else.
        self.assertEqual(guard.ALLOWED_PROJECT_INCLUDES, ("engine/base/", "engine/entity_store/", "engine/kernel/"))

    def test_k5_entity_store_include_passes(self):
        self.write("engine/kernel/src/Uses.cpp", '#include "engine/entity_store/EntityColumns.hpp"\n')
        self.assertClean()

    def test_k5_other_engine_modules_and_products_are_reported(self):
        for target in ("engine/os/Thread.hpp", "engine/entity_store_extra/Columns.hpp", "engine/math/Vec3.hpp",
                       "engine/render/RenderDevice.hpp", "apps/demo/Demo.hpp"):
            with self.subTest(target=target):
                self.write("engine/kernel/src/Uses.cpp", f'#include "{target}"\n')
                self.assertReported("K5", target)

    def test_k5_platform_and_third_party_headers_are_reported(self):
        for target in ("pthread.h", "unistd.h", "SDL3/SDL.h", "stdio.h"):
            with self.subTest(target=target):
                self.write("engine/kernel/src/Uses.cpp", f"#include <{target}>\n")
                self.assertReported("K5", target)

    def test_k5_standard_header_in_quotes_is_reported(self):
        self.write("engine/kernel/src/Uses.cpp", '#include "vector"\n')
        self.assertReported("K5", "vector")

    def test_k5_parent_directory_in_a_project_include_is_reported(self):
        for target in ("engine/kernel/../render/RenderDevice.hpp", "engine/base/../os/Thread.hpp",
                       "engine/entity_store/../os/Thread.hpp", "engine/entity_store/..\\os\\Thread.hpp",
                       "engine/base/..\\os\\Thread.hpp"):
            with self.subTest(target=target):
                self.write("engine/kernel/src/Uses.cpp", f'#include "{target}"\n')
                self.assertReported("K5", target)

    # K6-K7 coverage (A7, B17, B18) ------------------------------------------------------------------------------

    def test_k6_every_log_header_is_reported(self):
        for target in ("<iostream>", "<cstdio>", "<print>", '"engine/base/Log.hpp"'):
            with self.subTest(target=target):
                self.write("engine/kernel/src/World.cpp", f"#include {target}\n")
                self.assertReported("K6", target.strip('<>"'))

    def test_k6_every_log_call_is_reported(self):
        for call, needle in (("Engine::Base::SetLogHandler(nullptr)", "SetLogHandler"),
                             ("std::cout << 1", "std::cout"), ("std::clog << 1", "std::clog"),
                             ('std::print("x")', "std::print"), ('std::println("x")', "std::println"),
                             ('SDL_Log("x")', "SDL_Log"), ('fprintf(nullptr, "x")', "fprintf"),
                             ('vprintf("x", nullptr)', "vprintf"), ('puts("x")', "puts"),
                             ('fputs("x", nullptr)', "fputs"), ("putchar(1)", "putchar"), ('perror("x")', "perror"),
                             ("fwrite(nullptr, 1, 1, nullptr)", "fwrite")):
            with self.subTest(call=call):
                self.header(f"inline void Fail() {{ {call}; }}")
                self.assertReported("K6", needle)

    def test_k7_uppercase_macros_are_reported(self):
        self.header("struct Plane { float FAR; float NEAR; };")
        self.assertReported("K7", "FAR")
        self.assertReported("K7", "NEAR")

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
