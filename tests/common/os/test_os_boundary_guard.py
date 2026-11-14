"""Self-test of os_boundary_guard.py on synthetic trees (no repository file is read)."""
import contextlib
import io
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import os_boundary_guard as guard  # noqa: E402


class GuardTree(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.root = Path(self._directory.name)
        # Every root needs at least one scanned source.
        self.write("engine/base/src/Base.cpp", "int Base() { return 0; }\n")
        self.write("tests/common/base/BaseTests.cpp", "int main() { return 0; }\n")

    def tearDown(self):
        self._directory.cleanup()

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def problems(self, roots=guard.ROOTS):
        return guard.check(self.root, roots)

    def assertReported(self, needle, roots=guard.ROOTS):
        problems = self.problems(roots)
        self.assertTrue(any(needle in line for line in problems), f"{needle!r} not in {problems}")

    def assertClean(self, roots=guard.ROOTS):
        self.assertEqual(self.problems(roots), [])


class Rules(GuardTree):
    def test_clean_tree_passes(self):
        self.assertClean()

    def test_os_header_outside_engine_os_is_reported(self):
        self.write("engine/trace/src/Sink.cpp", "#include <unistd.h>\n#include <sys/stat.h>\n")
        self.assertReported("engine/trace/src/Sink.cpp:1: R1 unistd.h")
        self.assertReported("engine/trace/src/Sink.cpp:2: R1 sys/stat.h")

    def test_os_header_in_engine_os_sources_is_allowed(self):
        self.write("engine/os/src/windows/Files.cpp", "#include <windows.h>\n#ifdef _WIN32\n#endif\n")
        self.assertClean()

    def test_os_header_in_engine_os_public_header_is_reported(self):
        self.write("engine/os/include/engine/os/Files.hpp", "#pragma once\n#include <windows.h>\n")
        self.assertReported("engine/os/include/engine/os/Files.hpp:2: R1 windows.h")

    def test_linux_branch_is_reported(self):
        self.write("engine/time/src/Wait.cpp", "#if defined(__linux__)\n#endif\n")
        self.assertReported("engine/time/src/Wait.cpp:1: R2 __linux__")

    def test_every_listed_os_macro_is_reported(self):
        macros = ["_WIN32", "__APPLE__", "__linux__", "__unix__", "__ANDROID__", "__MINGW32__", "__CYGWIN__",
                  "TARGET_OS_IPHONE", "SDL_PLATFORM_WINDOWS", "_POSIX_VERSION"]
        self.write("engine/a/A.cpp", "".join(f"#ifdef {m}\n#endif\n" for m in macros))
        for macro in macros:
            self.assertReported(f"R2 {macro}")

    def test_every_conditional_directive_is_checked(self):
        self.write("engine/a/A.cpp", "#if 1\n#elif defined(_WIN32)\n#endif\n#ifndef __APPLE__\n#endif\n#if 0\n"
                                     "#elifdef __linux__\n#elifndef __unix__\n#endif\n"
                                     "  #  if defined(__ANDROID__)\n#endif\n")
        problems = self.problems()
        for line, macro in ((2, "_WIN32"), (4, "__APPLE__"), (7, "__linux__"), (8, "__unix__"), (10, "__ANDROID__")):
            with self.subTest(line=line):
                self.assertIn(f"engine/a/A.cpp:{line}: R2 {macro}", problems)

    def test_has_include_of_an_os_header_is_reported(self):
        self.write("engine/a/A.cpp", "#if __has_include(<unistd.h>)\n#endif\n")
        self.assertReported("engine/a/A.cpp:1: R2 __has_include(<unistd.h>)")

    def test_compiler_and_architecture_macros_are_not_os_branches(self):
        self.write("engine/a/A.cpp", "#if defined(_MSC_VER) && defined(__aarch64__)\n#endif\n"
                                     "#ifdef __clang__\n#endif\n#if defined(_M_X64)\n#endif\n")
        self.assertClean()

    def test_comments_and_strings_are_not_branches(self):
        self.write("engine/a/A.cpp", '// #ifdef _WIN32\n/* __linux__ */\nconst char* s = "_WIN32 localtime_r(";\n')
        self.assertClean()

    def test_os_api_call_is_reported(self):
        self.write("engine/a/A.cpp", "void F() { localtime_r(nullptr, nullptr); std::signal(2, nullptr); }\n")
        self.assertReported("engine/a/A.cpp:1: R3 localtime_r")
        self.assertReported("engine/a/A.cpp:1: R3 std::signal")

    def test_member_named_like_an_api_is_not_reported(self):
        self.write("engine/a/A.cpp", "void F(W& w) { w.signal(); w->kqueue(); }\n")
        self.assertClean()

    def test_direct_file_access_in_runtime_code_is_reported(self):
        self.write("engine/asset/src/Read.cpp",
                   "#include <fstream>\nvoid F() { std::ifstream in(\"x\"); fs::exists(p); "
                   "std::filesystem::weakly_canonical(p); FILE* f = fopen(\"x\", \"r\"); }\n")
        for match in ("R4 std::ifstream", "R4 fs::exists", "R4 std::filesystem::weakly_canonical", "R4 fopen"):
            self.assertReported(f"engine/asset/src/Read.cpp:2: {match}")
        self.assertFalse(any("Read.cpp:1:" in line for line in self.problems()), "an #include line is not a use")

    def test_path_type_and_pure_path_operations_are_allowed(self):
        self.write("engine/a/A.cpp", "std::filesystem::path p = std::filesystem::path(\"a\") / \"b\";\n"
                                     "auto q = p.parent_path().filename();\n")
        self.assertClean()

    def test_direct_file_access_in_tests_and_in_engine_os_is_allowed(self):
        self.write("tests/common/a/ATests.cpp", "void F() { std::ofstream out(\"x\"); fs::remove(p); }\n")
        self.write("tests/sample/SampleTests.cpp", "void F() { std::ofstream out(\"x\"); }\n")
        self.write("build/acceptance/sample/main.cpp", "void F() { std::ifstream in(\"x\"); }\n")
        self.write("engine/os/src/posix/Files.cpp", "void F() { std::ifstream in(\"x\"); }\n")
        self.write("engine/os/include/engine/os/Files.hpp", "inline void F() { std::ifstream in(\"x\"); }\n")
        self.assertClean()

    def test_direct_file_access_in_products_and_tools_is_reported(self):
        self.write("apps/sample/src/Load.cpp", "void F() { std::ifstream in(\"x\"); }\n")
        self.write("tools/sample/src/Save.cpp", "void F() { std::filesystem::rename(a, b); }\n")
        self.assertReported("apps/sample/src/Load.cpp:1: R4 std::ifstream")
        self.assertReported("tools/sample/src/Save.cpp:1: R4 std::filesystem::rename")

    def test_os_knowledge_in_product_tests_and_acceptance_programs_is_reported(self):
        self.write("tests/sample/SampleTests.cpp", "#include <windows.h>\n")
        self.write("build/acceptance/sample/main.cpp", "#ifdef _WIN32\n#endif\n")
        self.assertReported("tests/sample/SampleTests.cpp:1: R1 windows.h")
        self.assertReported("build/acceptance/sample/main.cpp:1: R2 _WIN32")

    def test_engine_os_includes_only_base_and_itself(self):
        self.write("engine/os/src/common/A.cpp", '#include "engine/os/OsError.hpp"\n#include "engine/base/Result.hpp"\n'
                                                 '#include "Local.hpp"\n#include <vector>\n#include <sys/event.h>\n')
        self.assertClean()
        self.write("engine/os/src/common/B.cpp", '#include "engine/io/IoError.hpp"\n#include <SDL3/SDL.h>\n'
                                                 '#include <asio.hpp>\n')
        self.assertReported("engine/os/src/common/B.cpp:1: R5 engine/io/IoError.hpp")
        self.assertReported("engine/os/src/common/B.cpp:2: R5 SDL3/SDL.h")
        self.assertReported("engine/os/src/common/B.cpp:3: R5 asio.hpp")

    def test_engine_os_relative_includes_must_stay_inside_engine_os(self):
        self.write("engine/os/src/posix/Files.cpp", '#include "../common/Paths.hpp"\n#include "./Local.hpp"\n')
        self.assertClean()
        self.write("engine/os/src/posix/Up.cpp", '#include "../../../io/include/engine/io/IoError.hpp"\n')
        self.assertReported("engine/os/src/posix/Up.cpp:1: R5 ../../../io/include/engine/io/IoError.hpp")

    def test_ansi_code_page_in_engine_os_is_reported(self):
        for constant in ("CP_ACP", "CP_THREAD_ACP", "CP_OEMCP", "CP_MACCP"):
            with self.subTest(constant=constant):
                self.write("engine/os/src/windows/Files.cpp", f"unsigned Page() {{ return {constant}; }}\n")
                self.assertReported(f"engine/os/src/windows/Files.cpp:1: R6 {constant}")

    def test_ansi_entry_point_in_engine_os_is_reported(self):
        self.write("engine/os/src/windows/Process.cpp", "void Path() { GetModuleFileNameA(nullptr, buffer, 260); }\n")
        self.assertReported("engine/os/src/windows/Process.cpp:1: R6 GetModuleFileNameA")
        self.write("engine/os/include/engine/os/Files.hpp", "inline void Open() { CreateFileA(name); }\n")
        self.assertReported("engine/os/include/engine/os/Files.hpp:1: R6 CreateFileA")
        # engine/os calls Win32 through the global "::".
        self.write("engine/os/src/windows/Wide.cpp", "void Get() { ::GetEnvironmentVariableA(name, out, 8); }\n")
        self.assertReported("engine/os/src/windows/Wide.cpp:1: R6 GetEnvironmentVariableA")

    def test_utf8_conversion_and_wide_entry_points_in_engine_os_are_allowed(self):
        self.write("engine/os/src/windows/Files.cpp",
                   "int Convert() { return WideCharToMultiByte(CP_UTF8, 0, w, n, nullptr, 0, nullptr, nullptr); }\n"
                   "void Open() { CreateFileW(name); obj.SetDataA(1); Ns::MakeA(2); }\n"
                   "// CP_ACP and CreateFileA in a comment\n"
                   "const char* text = \"CP_ACP CreateFileA(\";\n")
        self.assertClean()

    def test_ansi_names_outside_engine_os_are_left_to_r3(self):
        self.write("tools/editor/src/Strings.cpp", "unsigned Page() { return CP_ACP; }\n")
        self.assertClean()

    def test_an_empty_root_fails(self):
        (self.root / "engine/base/src/Base.cpp").unlink()
        (self.root / "engine/base/src").mkdir(exist_ok=True)
        self.assertReported("engine: no C/C++ source scanned")

    def test_an_empty_tests_root_fails(self):
        (self.root / "tests/common/base/BaseTests.cpp").unlink()
        self.assertReported("tests: no C/C++ source scanned")

    def test_a_missing_required_root_fails(self):
        # A moved root or a wrong --root must not read as "nothing to report".
        shutil.rmtree(self.root / "tests")
        self.assertReported("tests: no C/C++ source scanned (does not exist)")
        shutil.rmtree(self.root / "engine")
        skipped = []
        problems = guard.check(self.root, skipped=skipped)
        self.assertEqual(problems, ["engine: no C/C++ source scanned (does not exist); is the root right?",
                                    "tests: no C/C++ source scanned (does not exist); is the root right?"])
        self.assertEqual(skipped, ["apps (does not exist)", "tools (does not exist)",
                                   "build/acceptance (does not exist)"])

    def test_product_roots_without_sources_are_skipped_and_named(self):
        # No product or tool is registered: apps and tools are absent, build/acceptance holds only scripts.
        self.write("build/acceptance/common/check.py", "print('x')\n")
        skipped = []
        self.assertEqual(guard.check(self.root, skipped=skipped), [])
        self.assertEqual(skipped, ["apps (does not exist)", "tools (does not exist)",
                                   "build/acceptance (holds no C/C++ source)"])
        self.write("apps/sample/project.json", "{}\n")
        skipped = []
        self.assertEqual(guard.check(self.root, skipped=skipped), [])
        self.assertIn("apps (holds no C/C++ source)", skipped)

    def test_a_product_root_is_scanned_again_once_it_holds_sources(self):
        self.write("apps/sample/src/a.cpp", "#ifdef __linux__\n#endif\n")
        skipped = []
        problems = guard.check(self.root, skipped=skipped)
        self.assertIn("apps/sample/src/a.cpp:1: R2 __linux__", problems)
        self.assertFalse(any(entry.startswith("apps ") for entry in skipped), skipped)

    def test_render_shader_tool_is_runtime_code_for_r4(self):
        # The host shader tool's exclusion left with engine/render; runtime code under engine/ is covered again.
        self.write("engine/render/shaders/pipeline/src/main.cpp", "void F() { std::ifstream in(\"x\"); }\n")
        self.assertReported("engine/render/shaders/pipeline/src/main.cpp:1: R4 std::ifstream")

    def test_roots_name_no_product(self):
        # Removing or adding a product must leave the scanned roots unchanged (AGENTS.md sections 7 and 9).
        self.assertEqual(guard.ROOTS, ("engine", "apps", "tools", "build/acceptance", "tests"))
        self.assertEqual(guard.RUNTIME_ROOTS, ("engine", "apps", "tools"))

    def test_exclusions_and_required_roots_are_pinned(self):
        # Each of these narrows the guard when it grows (an exclusion, an allowlist scope) or shrinks (a required
        # root): changing one changes this test, so it is a visible change of the guard's judgment.
        self.assertEqual(guard.EXCLUDED, ("tests/common/os/", "third_party/", "build/target/"))
        self.assertEqual(guard.R4_EXCLUDED, ("engine/os/",))
        self.assertEqual(guard.REQUIRED_ROOTS, ("engine", "tests"))
        self.assertEqual(guard.ENGINE_ALLOWLIST_SCOPE, ("engine/", "tests/common/"))

    def test_os_layer_tests_may_assert_per_os(self):
        self.write("tests/common/os/FilesTests.cpp", "#include <windows.h>\n#ifdef _WIN32\n#endif\n"
                                                     "void F() { GetLastError(); }\n")
        self.assertClean()

    def test_every_source_suffix_is_scanned(self):
        suffixes = (".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".mm")
        for suffix in suffixes:
            self.write(f"engine/a/File{suffix}", "#ifdef _WIN32\n#endif\n")
        problems = self.problems()
        for suffix in suffixes:
            with self.subTest(suffix=suffix):
                self.assertIn(f"engine/a/File{suffix}:1: R2 _WIN32", problems)


class RuleLists(GuardTree):
    """The rule lists are the wording of the invariant: an entry may be added, none may silently go."""

    @staticmethod
    def sample(entry):
        # One name that a list entry (a literal or a small pattern) matches.
        for pattern, text in (("2?", "2"), ('[^>\\"]+', "x.h"), ("\\w+", "X"), ("\\.", ".")):
            entry = entry.replace(pattern, text)
        return entry

    def test_rule_lists_do_not_shrink(self):
        lists = (("R1", guard.OS_HEADERS, 45, "#include <{}>"), ("R2", guard.OS_MACROS, 20, "#ifdef {}\n#endif"),
                 ("R3", guard.OS_APIS, 32, "void F() {{ {}(); }}"),
                 ("R4", guard.FILESYSTEM_OPERATIONS, 40, "void F() {{ std::filesystem::{}(p); }}"))
        for rule, entries, least, template in lists:
            self.assertGreaterEqual(len(entries), least, f"{rule}: an entry was removed from the rule list")
            self.assertEqual(len(set(entries)), len(entries), rule)
            names = [self.sample(entry) for entry in entries]
            lines = [line for name in names for line in template.format(name).split("\n")]
            self.write(f"engine/a/{rule}.cpp", "\n".join(lines) + "\n")
        problems = self.problems()
        for rule, entries, _, template in lists:
            step = template.count("\n") + 1
            prefix = "std::filesystem::" if rule == "R4" else ""
            for index, entry in enumerate(entries):
                with self.subTest(rule=rule, entry=entry):
                    line = index * step + 1
                    self.assertIn(f"engine/a/{rule}.cpp:{line}: {rule} {prefix}{self.sample(entry)}", problems)


class CommandLine(GuardTree):
    def run_main(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            status = guard.main(["--root", str(self.root)])
        return status, output.getvalue()

    def test_main_reports_findings_with_exit_status_1(self):
        self.write("engine/a/A.cpp", "#ifdef _WIN32\n#endif\n")
        status, output = self.run_main()
        self.assertEqual(status, 1)
        self.assertIn("OS boundary guard: engine/a/A.cpp:1: R2 _WIN32\n", output)
        self.assertIn("OS boundary guard: 1 problems", output)

    def test_main_names_skipped_roots_and_passes(self):
        status, output = self.run_main()
        self.assertEqual(status, 0)
        for root in ("apps", "tools", "build/acceptance"):
            self.assertIn(f"OS boundary guard: skipped root {root} (does not exist); it is scanned again", output)
        self.assertNotIn("skipped root engine", output)
        self.assertNotIn("skipped root tests", output)

    def test_main_fails_on_a_directory_that_is_not_the_repository(self):
        with tempfile.TemporaryDirectory() as empty:
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                status = guard.main(["--root", empty])
        self.assertEqual(status, 1)
        self.assertIn("engine: no C/C++ source scanned (does not exist)", output.getvalue())


class Allowlists(GuardTree):
    SITE = "#ifdef _WIN32\n#endif\n#ifdef __APPLE__\n#endif\n"

    def test_exact_count_passes(self):
        self.write("engine/time/src/Wait.cpp", self.SITE)
        self.write("tests/common/os/os_boundary_allowlist.txt", "engine/time/src/Wait.cpp R2 2  # moves later\n")
        self.assertClean()

    def test_new_site_fails(self):
        self.write("engine/time/src/Wait.cpp", self.SITE + "#ifdef __linux__\n#endif\n")
        self.write("tests/common/os/os_boundary_allowlist.txt", "engine/time/src/Wait.cpp R2 2\n")
        self.assertReported("Wait.cpp R2 has 3 findings, allowlisted 2")

    def test_removed_site_asks_to_shrink(self):
        self.write("engine/time/src/Wait.cpp", "#ifdef _WIN32\n#endif\n")
        self.write("tests/common/os/os_boundary_allowlist.txt", "engine/time/src/Wait.cpp R2 2\n")
        self.assertReported("shrink the allowlist")

    def test_missing_file_fails(self):
        self.write("tests/common/os/os_boundary_allowlist.txt", "engine/time/src/Gone.cpp R2 1\n")
        self.assertReported("does not exist")

    def test_malformed_and_duplicate_entries_fail(self):
        self.write("engine/time/src/Wait.cpp", self.SITE)
        self.write("tests/common/os/os_boundary_allowlist.txt",
                   "engine/time/src/Wait.cpp R2 2\nengine/time/src/Wait.cpp R2 2\nengine/time/src/Wait.cpp R9 1\n")
        self.assertReported("duplicate entry")
        self.assertReported("expected 'path rule count'")

    def test_engine_allowlist_covers_only_engine_and_common_tests(self):
        self.write("apps/sample/src/a.cpp", "#ifdef __linux__\n#endif\n")
        self.write("tests/sample/SampleTests.cpp", "#ifdef __linux__\n#endif\n")
        for path in ("apps/sample/src/a.cpp", "tests/sample/SampleTests.cpp"):
            with self.subTest(path=path):
                self.write("tests/common/os/os_boundary_allowlist.txt", f"{path} R2 1\n")
                self.assertReported(f"{path} is outside this allowlist's directory (engine/, tests/common/)")

    def test_product_allowlist_covers_only_its_own_tree(self):
        self.write("apps/sample/src/a.cpp", "#ifdef __linux__\n#endif\n")
        self.write("apps/sample/os_boundary_allowlist.txt", "apps/sample/src/a.cpp R2 1\n")
        self.assertClean()
        self.write("apps/sample/os_boundary_allowlist.txt",
                   "apps/sample/src/a.cpp R2 1\nengine/base/src/Base.cpp R2 1\n")
        self.assertReported("outside this allowlist's directory")

    def test_removing_a_product_needs_no_common_change(self):
        self.write("apps/sample/src/a.cpp", "#ifdef __linux__\n#endif\n")
        self.write("apps/sample/os_boundary_allowlist.txt", "apps/sample/src/a.cpp R2 1\n")
        self.write("apps/other/src/b.cpp", "int B() { return 0; }\n")
        self.assertClean()
        for path in sorted((self.root / "apps/sample").rglob("*"), reverse=True):
            path.unlink() if path.is_file() else path.rmdir()
        (self.root / "apps/sample").rmdir()
        self.assertClean()

    def test_product_test_and_acceptance_allowlists_cover_their_own_trees(self):
        self.write("tests/sample/SampleTests.cpp", "#ifdef __linux__\n#endif\n")
        self.write("tests/sample/os_boundary_allowlist.txt", "tests/sample/SampleTests.cpp R2 1  # permanent\n")
        self.write("build/acceptance/sample/main.cpp", "#ifdef _WIN32\n#endif\n")
        self.write("build/acceptance/sample/os_boundary_allowlist.txt", "build/acceptance/sample/main.cpp R2 1\n")
        self.assertClean()
        self.write("tests/sample/os_boundary_allowlist.txt",
                   "tests/sample/SampleTests.cpp R2 1\nbuild/acceptance/sample/main.cpp R2 1\n")
        self.assertReported("outside this allowlist's directory")

    def test_unlisted_product_site_is_reported(self):
        self.write("apps/sample/src/a.cpp", "#ifdef __linux__\n#endif\n")
        self.assertReported("apps/sample/src/a.cpp:1: R2 __linux__")


if __name__ == "__main__":
    unittest.main()
