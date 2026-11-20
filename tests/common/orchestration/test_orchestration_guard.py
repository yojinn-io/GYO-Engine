"""Self-test of orchestration_guard.py on synthetic trees (no repository file is read)."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import orchestration_guard as guard  # noqa: E402

CLEAN_HEADER = """#pragma once
#include <vector>
#include "engine/kernel/Registry.hpp"
#include "engine/base/Result.hpp"
#include "engine/orchestration/Module.hpp"
// A comment naming PlanOf( and #include <SDL3/SDL.h> is not code.
namespace GYO::Engine::Orchestration {
inline const char* note = "Plans() and .steps in a string";
struct Unit {};
}
"""
CLEAN_CMAKE = """add_library(gyo_orchestration STATIC src/Product.cpp)
add_library(GYO::Orchestration ALIAS gyo_orchestration)
# target_link_libraries(gyo_orchestration PUBLIC GYO::Kernel)  (a comment)
gyo_link_registered_dependencies(orchestration)
target_link_libraries(gyo_orchestration PRIVATE Threads::Threads)
"""


class GuardTree(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.root = Path(self._directory.name)
        self.write("engine/orchestration/include/engine/orchestration/Clean.hpp", CLEAN_HEADER)
        self.write("engine/orchestration/CMakeLists.txt", CLEAN_CMAKE)
        self.write("engine/kernel/CMakeLists.txt", "gyo_link_registered_dependencies(kernel)\n")

    def tearDown(self):
        self._directory.cleanup()

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def source(self, text):
        self.write("engine/orchestration/src/Added.cpp", text)

    def assertReported(self, rule, needle):
        problems = guard.check(self.root)
        hits = [p for p in problems if f": {rule} " in p and needle in p]
        self.assertTrue(hits, f"expected {rule} {needle!r} among {problems}")

    def test_clean_tree_passes(self):
        self.assertEqual(guard.check(self.root), [])

    def test_o4_third_party_include_is_reported(self):
        self.source("#include <SDL3/SDL.h>\n")
        self.assertReported("O4", "<SDL3/SDL.h>")

    def test_o4_module_include_is_reported(self):
        self.source('#include "engine/world_time/WorldTimeModule.hpp"\n')
        self.assertReported("O4", '"engine/world_time/WorldTimeModule.hpp"')

    def test_o4_other_engine_include_is_reported(self):
        self.source('#include "engine/time/MonotonicClock.hpp"\n')
        self.assertReported("O4", '"engine/time/MonotonicClock.hpp"')

    def test_o4_unreadable_include_is_reported(self):
        self.source("#include MACRO_HEADER\n")
        self.assertReported("O4", "#include MACRO_HEADER")

    def test_o3_every_plan_and_system_call_name_is_reported(self):
        for text, needle in (("auto p = r.Plans();", "Plans("), ("auto q = r.PlanOf(\"Main\");", "PlanOf("),
                             ("auto e = r.ExportPlans();", "ExportPlans("), ("for (auto& s : plan.steps) {}", ".steps"),
                             ("auto k = StepKind::System;", "StepKind"), ("using I = InvokerOf<int>;", "InvokerOf"),
                             ("f.CallFunction();", "CallFunction"), ("f.CallLambda();", "CallLambda")):
            with self.subTest(text=text):
                self.source(text + "\n")
                self.assertReported("O3", needle)

    def test_o6_hand_written_engine_link_is_reported(self):
        self.write("engine/time/CMakeLists.txt",
                   "target_link_libraries(gyo_time\n    PUBLIC GYO::Base  # base\n    PRIVATE Threads::Threads)\n")
        self.assertReported("O6", "GYO::Base")

    def test_missing_orchestration_directory_is_reported(self):
        import shutil
        shutil.rmtree(self.root / "engine/orchestration/include")
        self.write("engine/orchestration/CMakeLists.txt", CLEAN_CMAKE)
        self.assertIn("engine/orchestration: O0 no source to scan (moved or renamed? update this guard)",
                      guard.check(self.root))

    def test_missing_engine_cmake_files_are_reported(self):
        (self.root / "engine/orchestration/CMakeLists.txt").unlink()
        (self.root / "engine/kernel/CMakeLists.txt").unlink()
        self.assertIn("engine: O0 no engine/<module>/CMakeLists.txt to scan", guard.check(self.root))


if __name__ == "__main__":
    unittest.main()
