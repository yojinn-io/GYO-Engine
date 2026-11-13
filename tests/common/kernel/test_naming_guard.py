"""Self-test of naming_guard.py on synthetic trees (no repository file is read)."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import naming_guard as guard  # noqa: E402

CLEAN_HEADER = """#pragma once
#include <vector>
namespace GYO::Kernel {
namespace Detail {
struct Ghost {};
} // namespace Detail
struct Hostname {};
struct RuntimeStats {};
enum class Zone { C, B };
using EntityList = std::vector<int>;
template <typename T>
concept Readable = sizeof(T) > 0;
// A comment saying namespace Engine::Kernel { struct WorldHost {}; } is not code.
inline const char* text = "namespace Other { class SceneRenderer; }";
} // namespace GYO::Kernel
"""
CLEAN_SOURCE = """#include "engine/kernel/Clean.hpp"
namespace {
int helper = 0;
}
namespace GYO::Kernel::Detail {
using namespace std;
void Touch() { ++helper; }
}
"""


class GuardTree(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.root = Path(self._directory.name)
        self.write("engine/kernel/include/engine/kernel/Clean.hpp", CLEAN_HEADER)
        self.write("engine/kernel/src/Clean.cpp", CLEAN_SOURCE)
        self.write("engine/time/include/engine/time/Clock.hpp", "namespace Engine::Time {\nstruct Clock {};\n}\n")

    def tearDown(self):
        self._directory.cleanup()

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def kernel_header(self, text):
        self.write("engine/kernel/include/engine/kernel/Added.hpp", text)

    def assertReported(self, rule, needle):
        problems = guard.check(self.root)
        hits = [p for p in problems if f": {rule} " in p and needle in p]
        self.assertTrue(hits, f"expected {rule} {needle!r} among {problems}")

    def assertClean(self):
        self.assertEqual(guard.check(self.root), [])

    def test_clean_tree_passes(self):
        self.assertClean()

    # N1 forward -------------------------------------------------------------------------------------------------

    def test_n1_old_engine_namespace_in_kernel_is_reported(self):
        self.kernel_header("namespace Engine::Kernel {\n}\n")
        self.assertReported("N1", "namespace Engine::Kernel (expected GYO::Kernel)")

    def test_n1_split_spelling_at_file_scope_is_reported(self):
        self.kernel_header("namespace GYO {\nnamespace Kernel {\n}\n}\n")
        self.assertReported("N1", "namespace GYO (expected GYO::Kernel)")

    def test_n1_other_namespace_in_kernel_is_reported(self):
        self.kernel_header("namespace GYO::Engine::Render {\n}\n")
        self.assertReported("N1", "GYO::Engine::Render")

    def test_n1_similar_prefix_is_reported(self):
        self.kernel_header("namespace GYO::KernelExtras {\n}\n")
        self.assertReported("N1", "GYO::KernelExtras")

    def test_n1_namespace_alias_at_file_scope_is_reported(self):
        self.kernel_header("namespace K = GYO::Kernel;\n")
        self.assertReported("N1", "alias K")

    # N1 reverse -------------------------------------------------------------------------------------------------

    def test_n1_kernel_namespace_outside_kernel_is_reported(self):
        self.write("engine/time/src/Leak.cpp", "namespace GYO::Kernel {\nint leak = 0;\n}\n")
        self.assertReported("N1", "engine/time/src/Leak.cpp:1: N1 namespace GYO::Kernel opened outside")

    def test_n1_nested_kernel_namespace_outside_kernel_is_reported(self):
        self.write("tests/common/time/Leak.cpp", "namespace GYO {\nnamespace Kernel {\n}\n}\n")
        self.assertReported("N1", "tests/common/time/Leak.cpp:2: N1 namespace GYO::Kernel opened outside")

    def test_n1_kernel_namespace_under_a_future_root_is_reported_once_it_exists(self):
        self.write("plugins/net_link/src/Leak.cpp", "namespace GYO::Kernel::Net {\n}\n")
        self.assertReported("N1", "plugins/net_link/src/Leak.cpp:1: N1 namespace GYO::Kernel::Net opened outside")

    def test_n1_using_the_kernel_namespace_outside_kernel_passes(self):
        self.write("apps/demo/src/Use.cpp", "using namespace GYO::Kernel;\nnamespace Demo {\n}\n")
        self.assertClean()

    def test_missing_reverse_root_is_skipped_not_reported(self):
        # plugins/, apps/, tools/, framework/ and tests/ do not exist in this tree: skipped, so clean.
        for name in guard.REVERSE_ROOTS:
            if name != "engine":
                self.assertFalse((self.root / name).exists())
        self.assertClean()

    def test_missing_mapped_root_is_reported(self):
        for path in sorted((self.root / "engine/kernel").rglob("*"), reverse=True):
            path.unlink() if path.is_file() else path.rmdir()
        (self.root / "engine/kernel").rmdir()
        self.assertEqual(guard.check(self.root),
                         ["engine/kernel: no source to scan (moved or renamed? update this guard)"])

    # N3 ---------------------------------------------------------------------------------------------------------

    def test_n3_host_suffix_is_reported(self):
        self.kernel_header("namespace GYO::Kernel {\nstruct WorldHost {};\n}\n")
        self.assertReported("N3", "WorldHost")

    def test_n3_renderer_suffix_on_class_is_reported(self):
        self.kernel_header("namespace GYO::Kernel {\nclass SceneRenderer;\n}\n")
        self.assertReported("N3", "SceneRenderer")

    def test_n3_runtime_alias_is_reported(self):
        self.kernel_header("namespace GYO::Kernel {\nusing Runtime = int;\n}\n")
        self.assertReported("N3", "Runtime")

    def test_n3_every_suffix_is_reported(self):
        body = "".join(f"struct Some{suffix} {{}};\n" for suffix in guard.ROLE_SUFFIXES)
        self.kernel_header("namespace GYO::Kernel {\n" + body + "}\n")
        problems = guard.check(self.root)
        for suffix in guard.ROLE_SUFFIXES:
            self.assertTrue(any(f"N3 type Some{suffix} " in p for p in problems), suffix)

    def test_n3_enum_class_suffix_is_reported(self):
        self.kernel_header("namespace GYO::Kernel {\nenum class StateController { A };\n}\n")
        self.assertReported("N3", "StateController")

    def test_role_suffixes_are_naming_rule_5(self):
        self.assertEqual(guard.ROLE_SUFFIXES,
                         ("Manager", "Renderer", "ViewModel", "Host", "Shell", "Runtime", "Controller"))

    def test_main_returns_one_on_finding(self):
        self.kernel_header("namespace GYO::Kernel {\nstruct WorldHost {};\n}\n")
        self.assertEqual(guard.main(["--root", str(self.root)]), 1)

    def test_main_returns_zero_when_clean(self):
        self.assertEqual(guard.main(["--root", str(self.root)]), 0)


if __name__ == "__main__":
    unittest.main()
