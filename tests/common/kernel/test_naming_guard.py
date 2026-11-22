"""Self-test of naming_guard.py on synthetic trees (no repository file is read)."""
import shutil
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
namespace B = GYO::Engine::Base;
void Touch() { ++helper; }
}
"""


class GuardTree(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.root = Path(self._directory.name)
        self.write("engine/ecs/kernel/include/engine/kernel/Clean.hpp", CLEAN_HEADER)
        self.write("engine/ecs/kernel/src/Clean.cpp", CLEAN_SOURCE)
        self.unit("engine/core/time", "GYO::Engine::Time")
        self.unit("engine/ecs/entity_store", "GYO::Engine::EntityStore")
        self.unit("engine/core/io", "GYO::Engine::IO")
        self.unit("engine/orchestration", "GYO::Engine::Orchestration")
        self.unit("engine/modules/world_time", "GYO::Engine::WorldTime")
        self.write("tests/common/support/Support.hpp", "namespace GYO::Test {\nstruct Probe {};\n}\n")
        self.write("tests/common/time/Helpers.hpp", "namespace GYO::Test::Time {\nstruct Fake {};\n}\n")

    def tearDown(self):
        self._directory.cleanup()

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def unit(self, directory, namespace):
        name = Path(directory).name
        self.write(f"{directory}/include/engine/{name}/Clean.hpp", f"namespace {namespace} {{\nstruct Unit {{}};\n}}\n")

    def kernel_header(self, text):
        self.write("engine/ecs/kernel/include/engine/kernel/Added.hpp", text)

    def assertReported(self, rule, needle):
        problems = guard.check(self.root)
        hits = [p for p in problems if f": {rule} " in p and needle in p]
        self.assertTrue(hits, f"expected {rule} {needle!r} among {problems}")

    def assertClean(self):
        self.assertEqual(guard.check(self.root), [])

    def test_clean_tree_passes(self):
        self.assertClean()

    # Units ------------------------------------------------------------------------------------------------------

    def test_unit_name_is_derived_from_the_directory(self):
        self.assertEqual(guard.unit_name("world_time"), "WorldTime")
        self.assertEqual(guard.unit_name("entity_store"), "EntityStore")
        self.assertEqual(guard.unit_name("base"), "Base")

    def test_acronyms_are_io_and_os(self):
        self.assertEqual(guard.ACRONYMS, {"io": "IO", "os": "OS"})
        self.assertEqual(guard.unit_name("io"), "IO")
        self.assertEqual(guard.unit_name("os"), "OS")

    def test_engine_layers_are_pinned(self):
        self.assertEqual(guard.ENGINE_LAYERS, {"core": "units", "ecs": "units", "modules": "units",
                                               "orchestration": "unit"})

    def test_gyo_roots_are_pinned(self):
        self.assertEqual(guard.GYO_ROOTS, {"Kernel": "units", "Engine": "units", "Plugins": "plugins",
                                           "Test": "tests"})

    def test_n1_acronym_unit_spelled_otherwise_is_reported(self):
        self.write("engine/core/io/src/Added.cpp", "namespace GYO::Engine::Io {\n}\n")
        self.assertReported("N1", "namespace GYO::Engine::Io (expected GYO::Engine::IO)")

    def test_n1_a_new_unit_is_checked_without_changing_the_guard(self):
        self.write("engine/modules/new_thing/src/A.cpp", "namespace Engine::NewThing {\n}\n")
        self.assertReported("N1", "namespace Engine::NewThing (expected GYO::Engine::NewThing)")

    def test_n1_removing_a_unit_needs_no_guard_change(self):
        shutil.rmtree(self.root / "engine/modules/world_time")
        (self.root / "engine/modules/other").mkdir()
        self.write("engine/modules/other/src/A.cpp", "namespace GYO::Engine::Other {\n}\n")
        self.assertClean()

    def test_n1_unknown_layer_is_reported(self):
        self.write("engine/extra/thing/src/A.cpp", "namespace GYO::Engine::Thing {\n}\n")
        self.assertIn("engine/extra: not a layer of the naming table (ENGINE_LAYERS); add it with the naming rule "
                      "(N17)", guard.check(self.root))

    def test_n1_unit_without_source_is_reported(self):
        for path in (self.root / "engine/core/time").rglob("*.hpp"):
            path.unlink()
        self.assertIn("engine/core/time: no source to scan (moved or renamed? update this guard)",
                      guard.check(self.root))

    def test_n1_kernel_override_is_applied(self):
        self.kernel_header("namespace GYO::Engine::Kernel {\n}\n")
        self.assertReported("N1", "namespace GYO::Engine::Kernel (expected GYO::Kernel)")

    def test_unit_overrides_are_pinned(self):
        self.assertEqual(guard.UNIT_OVERRIDES, {"engine/ecs/kernel": "GYO::Kernel"})

    def test_unit_name_applies_acronyms_per_part(self):
        self.assertEqual(guard.unit_name("file_io"), "FileIO")
        self.assertEqual(guard.unit_name("json"), "Json")

    def test_n1_layer_of_units_missing_or_empty_is_reported(self):
        shutil.rmtree(self.root / "engine/modules")
        self.assertIn("engine/modules: no source to scan (moved or renamed? update this guard)", guard.check(self.root))
        (self.root / "engine/modules").mkdir()
        self.assertIn("engine/modules: no source to scan (moved or renamed? update this guard)", guard.check(self.root))

    def test_n1_test_root_inside_a_unit_is_reported(self):
        self.write("engine/core/time/tests/A.cpp", "namespace GYO::Test::Time {\n}\n")
        self.assertReported("N1", "namespace GYO::Test::Time (expected GYO::Engine::Time)")

    # N1 forward -------------------------------------------------------------------------------------------------

    def test_n1_orchestration_other_namespace_is_reported(self):
        self.write("engine/orchestration/src/Added.cpp", "namespace GYO::Kernel {\n}\n")
        self.assertReported("N1", "namespace GYO::Kernel (expected GYO::Engine::Orchestration)")

    def test_n1_orchestration_namespace_opened_outside_is_reported(self):
        self.write("tests/common/orchestration/Added.cpp", "namespace GYO::Engine::Orchestration {\n}\n")
        self.assertReported("N1", "GYO::Engine::Orchestration opened outside its unit")

    def test_n1_missing_orchestration_directory_is_reported(self):
        shutil.rmtree(self.root / "engine/orchestration")
        self.assertIn("engine/orchestration: no source to scan (moved or renamed? update this guard)",
                      guard.check(self.root))

    def test_n1_world_time_other_namespace_is_reported(self):
        self.write("engine/modules/world_time/src/Added.cpp", "namespace GYO::Engine::Orchestration {\n}\n")
        self.assertReported("N1", "namespace GYO::Engine::Orchestration (expected GYO::Engine::WorldTime)")

    def test_n1_world_time_namespace_opened_outside_is_reported(self):
        self.write("tests/common/world_time/Added.cpp", "namespace GYO::Engine::WorldTime {\n}\n")
        self.assertReported("N1", "GYO::Engine::WorldTime")

    def test_n1_old_engine_namespace_in_kernel_is_reported(self):
        self.kernel_header("namespace Engine::Kernel {\n}\n")
        self.assertReported("N1", "namespace Engine::Kernel (expected GYO::Kernel)")

    def test_n1_old_engine_namespace_in_a_core_unit_is_reported(self):
        self.write("engine/core/time/src/Old.cpp", "namespace Engine::Time {\n}\n")
        self.assertReported("N1", "namespace Engine::Time (expected GYO::Engine::Time)")

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

    def test_missing_mapped_root_is_reported(self):
        shutil.rmtree(self.root / "engine/ecs/kernel")
        self.assertEqual(guard.check(self.root),
                         ["engine/ecs/kernel: no source to scan (moved or renamed? update this guard)"])

    def test_missing_engine_is_reported(self):
        shutil.rmtree(self.root / "engine")
        self.assertIn("engine: no layer to scan (moved or renamed? update this guard)", guard.check(self.root))

    def test_n1_second_namespace_in_a_file_is_checked(self):
        self.kernel_header("namespace GYO::Kernel {\n}\nnamespace Engine::Other {\n}\n")
        self.assertReported("N1", "namespace Engine::Other (expected GYO::Kernel)")

    def test_n1_namespace_inside_a_linkage_block_is_checked(self):
        self.kernel_header('extern "C++" {\nnamespace Evil {\n}\n}\n')
        self.assertReported("N1", "namespace Evil (expected GYO::Kernel)")

    def test_n1_namespace_inside_an_anonymous_namespace_is_checked(self):
        self.kernel_header("namespace {\nnamespace Evil {\n}\n}\n")
        self.assertReported("N1", "namespace Evil (expected GYO::Kernel)")

    def test_n1_attributed_namespace_is_read(self):
        self.kernel_header("namespace [[deprecated]] Evil {\n}\n")
        self.assertReported("N1", "namespace Evil (expected GYO::Kernel)")

    def test_n1_unreadable_namespace_declaration_is_reported(self):
        self.kernel_header("#define OPEN {\nnamespace GYO::Kernel OPEN\n}\n")
        self.assertReported("N1", "unreadable namespace declaration")

    def test_n1_inline_nested_namespace_keeps_its_name(self):
        self.kernel_header("namespace GYO::Kernel::inline Mainline {\n}\nnamespace Mainline {\n}\n")
        self.assertReported("N1", "namespace Mainline (expected GYO::Kernel)")

    # N1 roots ---------------------------------------------------------------------------------------------------

    def test_n1_kernel_namespace_outside_kernel_is_reported(self):
        self.write("engine/core/time/src/Leak.cpp", "namespace GYO::Kernel {\nint leak = 0;\n}\n")
        self.assertReported("N1", "engine/core/time/src/Leak.cpp:1: N1 namespace GYO::Kernel (expected GYO::Engine::Time)")

    def test_n1_nested_kernel_namespace_outside_kernel_is_reported(self):
        self.write("tests/common/time/Leak.cpp", "namespace GYO {\nnamespace Kernel {\n}\n}\n")
        self.assertReported("N1", "tests/common/time/Leak.cpp:2: N1 namespace GYO::Kernel opened outside its unit")

    def test_n1_unit_namespace_in_tests_is_reported(self):
        self.write("tests/common/time/Helpers.hpp", "namespace GYO::Engine::Time::Test {\n}\n")
        self.assertReported("N1", "tests/common/time/Helpers.hpp:1: N1 namespace GYO::Engine::Time::Test opened "
                                  "outside its unit")

    def test_n1_old_root_outside_engine_is_reported(self):
        self.write("tests/common/support/Old.hpp", "namespace Engine::Test {\n}\n")
        self.assertReported("N1", "tests/common/support/Old.hpp:1: N1 namespace Engine::Test uses the old root Engine")

    def test_n1_old_root_nested_spelling_is_reported(self):
        self.write("examples/demo/Old.cpp", "namespace Engine {\nnamespace Base {\n}\n}\n")
        self.assertReported("N1", "examples/demo/Old.cpp:1: N1 namespace Engine uses the old root Engine")

    def test_n1_unreadable_namespace_declaration_outside_a_unit_is_reported(self):
        self.write("tests/common/x/Macro.cpp", "#define OPEN {\nnamespace GYO::Engine::Time OPEN\n}\n")
        self.assertReported("N1", "tests/common/x/Macro.cpp:2: N1 unreadable namespace declaration")

    def test_n1_gyo_alone_outside_engine_is_reported(self):
        self.write("tests/common/x/Bare.cpp", "namespace GYO {\nint value = 0;\n}\n")
        self.assertReported("N1", "tests/common/x/Bare.cpp:1: N1 namespace GYO opened alone")

    def test_n1_unknown_gyo_root_is_reported(self):
        self.write("tests/common/x/Tool.cpp", "namespace GYO::Tools {\n}\n")
        self.assertReported("N1", "N1 namespace GYO::Tools: GYO::Tools is not a root of the naming table")

    def test_n1_engine_root_alone_outside_a_unit_is_reported(self):
        self.write("tests/common/x/Root.cpp", "namespace GYO::Engine {\n}\n")
        self.assertReported("N1", "tests/common/x/Root.cpp:1: N1 namespace GYO::Engine opened outside its unit")

    def test_n1_plugin_root_belongs_to_plugins(self):
        self.write("plugins/sdl/src/A.cpp", "namespace GYO::Plugins::SDL {\n}\nnamespace SdlDetail {\n}\n")
        self.assertClean()
        self.write("tests/common/x/Leak.cpp", "namespace GYO::Plugins::SDL {\n}\n")
        self.write("plugins/sdl/src/B.cpp", "namespace GYO::Engine::Time {\n}\n")
        self.assertReported("N1", "tests/common/x/Leak.cpp:1: N1 namespace GYO::Plugins::SDL opened outside plugins/")
        self.assertReported("N1", "plugins/sdl/src/B.cpp:1: N1 namespace GYO::Engine::Time opened outside its unit")

    def test_n1_test_root_outside_tests_is_reported(self):
        self.write("examples/demo/Probe.cpp", "namespace GYO::Test {\n}\n")
        self.write("apps/demo/src/Probe.cpp", "namespace GYO::Test::Demo {\n}\n")
        self.assertReported("N1", "examples/demo/Probe.cpp:1: N1 namespace GYO::Test opened outside tests/")
        self.assertReported("N1", "apps/demo/src/Probe.cpp:1: N1 namespace GYO::Test::Demo opened outside tests/")

    def test_n1_product_root_passes(self):
        self.write("examples/demo/main.cpp", "namespace Demo {\nnamespace Sim {\n}\n}\n")
        self.write("apps/demo/src/Use.cpp", "using namespace GYO::Kernel;\nnamespace Demo {\n}\n")
        self.assertClean()

    def test_missing_scan_root_is_skipped_not_reported(self):
        for name in guard.SCAN_ROOTS:
            if name not in ("engine", "tests"):
                self.assertFalse((self.root / name).exists())
        self.assertClean()

    def test_n1_second_namespace_outside_a_unit_is_checked(self):
        self.write("tests/common/time/Leak.cpp", "namespace GYO::Test {\n}\nnamespace GYO::Kernel {\n}\n")
        self.assertReported("N1", "tests/common/time/Leak.cpp:3: N1 namespace GYO::Kernel opened outside its unit")

    def test_n1_every_scan_root_is_scanned(self):
        for root in guard.SCAN_ROOTS:
            with self.subTest(root=root):
                self.write(f"{root}/sample/Leak.cpp", "namespace GYO::Kernel {\n}\n")
                if root == "engine":  # engine/sample is not a layer
                    self.assertIn("engine/sample: not a layer of the naming table (ENGINE_LAYERS); add it with the "
                                  "naming rule (N17)", guard.check(self.root))
                else:
                    self.assertReported("N1", f"{root}/sample/Leak.cpp:1: N1 namespace GYO::Kernel opened outside its unit")
                shutil.rmtree(self.root / f"{root}/sample")

    def test_n1_scan_roots_are_pinned(self):
        self.assertEqual(guard.SCAN_ROOTS, ("engine", "apps", "tools", "framework", "plugins", "tests", "examples"))

    def test_n1_every_source_type_is_scanned(self):
        # A fixed list, not guard.SUFFIXES: shrinking the guard's set must fail here.
        for suffix in (".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp", ".tpp", ".inc",
                       ".ixx", ".m", ".mm"):
            with self.subTest(suffix=suffix):
                self.write(f"tests/common/time/Leak{suffix}", "namespace GYO::Kernel {\n}\n")
                self.assertReported("N1", f"tests/common/time/Leak{suffix}:1: N1 namespace GYO::Kernel opened outside")
                (self.root / f"tests/common/time/Leak{suffix}").unlink()

    # N1 target --------------------------------------------------------------------------------------------------

    # N3 ---------------------------------------------------------------------------------------------------------
    def test_n3_every_declaration_keyword_is_reported(self):
        for text, name in (("union DataShell { int a; };", "DataShell"), ("enum struct ModeController { A };", "ModeController"),
                           ("template <typename T>\nconcept TaskManager = sizeof(T) > 0;", "TaskManager"),
                           ("struct [[nodiscard]] TaskRuntime {};", "TaskRuntime"),
                           ("struct alignas(16) GpuHost {};", "GpuHost"), ("typedef int ViewModel;", "ViewModel"),
                           ("typedef int ArrayShell[4];", "ArrayShell"), ("struct Detail::ImplHost {};", "ImplHost")):
            with self.subTest(text=text):
                self.kernel_header("namespace GYO::Kernel {\n" + text + "\n}\n")
                self.assertReported("N3", name)

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

    def test_n3_is_read_in_orchestration_and_world_time(self):
        self.write("engine/orchestration/src/Host.cpp", "namespace GYO::Engine::Orchestration {\nstruct ProductHost {};\n}\n")
        self.write("engine/modules/world_time/src/Manager.cpp", "namespace GYO::Engine::WorldTime {\nstruct ClockManager {};\n}\n")
        self.assertReported("N3", "ProductHost")
        self.assertReported("N3", "ClockManager")

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
