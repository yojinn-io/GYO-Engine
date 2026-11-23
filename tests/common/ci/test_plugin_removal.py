"""Pin the copy CI's core step builds without the SDL Plugins (Design Invariant SDL-2) and its fetch check."""

import contextlib
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3]
for folder in ("build", "build/ci/common"):
    sys.path.insert(0, str(ROOT / folder))
from workspace import TemporaryDirectory
import plugin_removal

HEADER = "name,dir,target,description,library_only,default,depends,private_depends,unit,header\n"
PRODUCTS = "name,description,version,enabled,windows,linux,macos\n"


def make_tree_writable(root: Path):
    """Git writes objects read-only, and Windows refuses to delete such files."""
    for directory, _, files in os.walk(root):
        for name in files:
            path = os.path.join(directory, name)
            os.chmod(path, os.stat(path).st_mode | stat.S_IWRITE)


class PluginRemovalTests(unittest.TestCase):
    def setUp(self):
        temporary = TemporaryDirectory(prefix="gyo-plugin-removal-")
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        # Cleanups run last-in first-out, so this clears read-only bits first.
        self.addCleanup(make_tree_writable, self.base)
        self.repository = self.base / "repository"
        self.copy = self.base / "copy"
        self.files = {
            "config/plugins.csv": HEADER
            + "sdl,sdl,GYO::SDL,SDL3,yes,no,,,,\n"
            + "sdl_window,sdl_window,GYO::SDLWindow,The window,no,no,sdl,,A::W,a/W.hpp\n"
            + "imgui,imgui,GYO::ImGui,Not of the family; sdl in prose is not a name,yes,no,,,,\n",
            "config/games.csv": PRODUCTS + "keep,Uses SDL,,1,1,1,1\nmatch,Headless,,1,1,1,1\n",
            "plugins/sdl/CMakeLists.txt": "add_library(gyo_sdl INTERFACE)\n",
            "plugins/sdl_window/CMakeLists.txt": "add_library(gyo_sdl_window STATIC)\n",
            "plugins/imgui/CMakeLists.txt": "add_library(gyo_imgui INTERFACE)  # not sdl\n",
            "games/keep/CMakeLists.txt": "add_executable(keep main.cpp)\ngyo_product_units(keep\n    PLUGINS sdl_window)\n",
            "games/keep/main.cpp": "int main() { return 0; }\n",
            "tests/keep/CMakeLists.txt": "add_test(NAME keep.a COMMAND keep)\n",
            "assets/keep/marker.txt": "keep\n",
            "games/match/CMakeLists.txt": "# The match server never opens an sdl window.\nadd_executable(match main.cpp)\n",
            "tests/common/CMakeLists.txt": "add_subdirectory(os)\n",
        }

    def make_repository(self, extra=None):
        for name, text in {**self.files, **(extra or {})}.items():
            path = self.repository / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
        git = ["git", "-C", str(self.repository)]
        subprocess.run([*git, "init", "-q"], check=True)
        subprocess.run([*git, "add", "-A"], check=True)
        subprocess.run([*git, "-c", "user.name=t", "-c", "user.email=t@example.invalid", "-c", "commit.gpgsign=false",
                        "commit", "-q", "-m", "fixture"], check=True)

    def listed(self, tree):
        return set(plugin_removal.tracked_files(tree))

    def test_copy_removes_the_family_and_the_products_that_carry_it(self):
        self.make_repository()
        removed, products = plugin_removal.copy(self.repository, self.copy, "sdl")
        self.assertEqual(removed, ["sdl", "sdl_window"])
        self.assertEqual(products, ["games/keep"])
        self.assertEqual((self.copy / "config/plugins.csv").read_text(encoding="utf-8"),
                         HEADER + "imgui,imgui,GYO::ImGui,Not of the family; sdl in prose is not a name,yes,no,,,,\n")
        self.assertEqual((self.copy / "config/games.csv").read_text(encoding="utf-8"),
                         PRODUCTS + "match,Headless,,1,1,1,1\n")
        listed = self.listed(self.copy)
        for gone in ("plugins/sdl/CMakeLists.txt", "plugins/sdl_window/CMakeLists.txt", "games/keep/main.cpp",
                     "tests/keep/CMakeLists.txt", "assets/keep/marker.txt"):
            self.assertNotIn(gone, listed)
            self.assertFalse((self.copy / gone).exists(), gone)
        for kept in ("plugins/imgui/CMakeLists.txt", "games/match/CMakeLists.txt", "tests/common/CMakeLists.txt"):
            self.assertIn(kept, listed)
        # The repository itself is untouched.
        self.assertTrue((self.repository / "plugins/sdl/CMakeLists.txt").exists())
        self.assertIn("sdl_window", (self.repository / "config/plugins.csv").read_text(encoding="utf-8"))

    def test_remove_works_in_place_and_restore_puts_head_back(self):
        self.make_repository()
        removed, products = plugin_removal.remove(self.repository, "sdl")
        self.assertEqual((removed, products), (["sdl", "sdl_window"], ["games/keep"]))
        self.assertFalse((self.repository / "plugins/sdl").exists())
        self.assertNotIn("plugins/sdl/CMakeLists.txt", self.listed(self.repository))
        self.assertNotIn("sdl_window", (self.repository / "config/plugins.csv").read_text(encoding="utf-8"))
        (self.repository / "build").mkdir()
        (self.repository / "build/untracked.txt").write_text("a build output\n", encoding="utf-8")
        plugin_removal.restore(self.repository)
        for name, text in self.files.items():
            self.assertEqual((self.repository / name).read_text(encoding="utf-8"), text, name)
        self.assertIn("plugins/sdl/CMakeLists.txt", self.listed(self.repository))
        # Untracked files (build outputs) are left alone.
        self.assertTrue((self.repository / "build/untracked.txt").exists())

    def test_remove_refuses_a_checkout_with_changes(self):
        self.make_repository()
        (self.repository / "games/match/CMakeLists.txt").write_text("# edited\n", encoding="utf-8")
        with self.assertRaisesRegex(plugin_removal.RemovalError, "needs its tracked files unchanged"):
            plugin_removal.remove(self.repository, "sdl")
        self.assertTrue((self.repository / "plugins/sdl/CMakeLists.txt").exists())

    def test_main_removes_and_restores_with_exit_status_0(self):
        self.make_repository()
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(plugin_removal.main(["remove", "--repository", str(self.repository), "--family", "sdl"]), 0)
            self.assertEqual(plugin_removal.main(["restore", "--repository", str(self.repository)]), 0)
        self.assertIn("Plugin removal: removed the Plugins sdl, sdl_window and the products games/keep", output.getvalue())
        self.assertTrue((self.repository / "plugins/sdl_window/CMakeLists.txt").exists())

    def test_copy_starts_again_from_head_over_a_previous_copy(self):
        self.make_repository()
        plugin_removal.copy(self.repository, self.copy, "sdl")
        (self.copy / "stray.txt").write_text("left over\n", encoding="utf-8")
        plugin_removal.copy(self.repository, self.copy, "sdl")
        self.assertFalse((self.copy / "stray.txt").exists())
        self.assertNotIn("plugins/sdl/CMakeLists.txt", self.listed(self.copy))

    def test_a_kept_row_on_a_removed_plugin_is_refused(self):
        self.files["config/plugins.csv"] += "bridge,bridge,GYO::Bridge,Needs SDL,yes,no,,sdl,,\n"
        self.make_repository({"plugins/bridge/CMakeLists.txt": "add_library(gyo_bridge INTERFACE)\n"})
        with self.assertRaisesRegex(plugin_removal.RemovalError,
                                    "config/plugins.csv: 'bridge' depends on sdl, which is removed"):
            plugin_removal.copy(self.repository, self.copy, "sdl")

    def test_another_cmake_file_naming_a_removed_plugin_is_refused(self):
        self.make_repository({"examples/CMakeLists.txt": "gyo_product_units(example PLUGINS sdl_window)\n"})
        with self.assertRaisesRegex(plugin_removal.RemovalError,
                                    "examples/CMakeLists.txt: still names the removed Plugin 'sdl_window'"):
            plugin_removal.copy(self.repository, self.copy, "sdl")

    def test_a_longer_name_or_a_comment_is_not_a_removed_name(self):
        self.make_repository({"examples/CMakeLists.txt": "set(sdl_windowing_note on)  # sdl_window\n",
                              "build/cmake/Other.cmake": "set(GYO::Sdl_like 1)\n# sdl\n"})
        removed, _ = plugin_removal.copy(self.repository, self.copy, "sdl")
        self.assertEqual(removed, ["sdl", "sdl_window"])

    def test_a_family_without_rows_is_refused(self):
        self.make_repository()
        with self.assertRaisesRegex(plugin_removal.RemovalError, "has no Plugin of the family 'vulkan'"):
            plugin_removal.copy(self.repository, self.copy, "vulkan")

    def test_main_reports_a_refusal_with_exit_status_1(self):
        self.make_repository({"examples/CMakeLists.txt": "gyo_product_units(example PLUGINS sdl)\n"})
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            status = plugin_removal.main(["copy", "--repository", str(self.repository), "--destination",
                                          str(self.copy), "--family", "sdl"])
        self.assertEqual(status, 1)
        self.assertIn("Plugin removal: examples/CMakeLists.txt: still names the removed Plugin 'sdl'", output.getvalue())

    def write_build(self, plugins, deps=(), cache="CMAKE_BUILD_TYPE:STRING=Release\nGYO_BUILD_SDL_IMAGE_LOADER:BOOL=OFF\n"):
        build = self.base / "build"
        build.mkdir()
        (build / "gyo-build.json").write_text(json.dumps({"configuration": "Release", "plugins": plugins}),
                                              encoding="utf-8")
        (build / "CMakeCache.txt").write_text(cache, encoding="utf-8")
        for name in deps:
            (build / "_deps" / name).mkdir(parents=True)
        return build

    def test_check_passes_a_build_without_the_family(self):
        build = self.write_build(["imgui"], ("doctest-src", "yyjson-src"))
        self.assertEqual(plugin_removal.check(build, "sdl"), [])

    def test_check_reports_an_added_plugin_and_a_fetched_source(self):
        # The positive case first (AGENTS.md section 14.2): each half of the check must fire.
        build = self.write_build(["sdl", "imgui"], ("sdl3-src", "sdl3-build", "doctest-src"))
        problems = plugin_removal.check(build, "sdl")
        self.assertEqual(len(problems), 3, problems)
        self.assertIn("the build added the Plugin 'sdl'", problems[0])
        self.assertTrue(problems[1].endswith("sdl3-build: the build fetched a sdl dependency"), problems[1])
        self.assertTrue(problems[2].endswith("sdl3-src: the build fetched a sdl dependency"), problems[2])

    def test_check_reports_the_family_configured_without_fetching(self):
        # A vendored add_subdirectory or an ExternalProject leaves no _deps entry, but the family's own options.
        build = self.write_build([], cache="CMAKE_BUILD_TYPE:STRING=Release\nSDL_STATIC:BOOL=ON\n")
        problems = plugin_removal.check(build, "sdl")
        self.assertEqual(len(problems), 1, problems)
        self.assertTrue(problems[0].endswith("CMakeCache.txt: the build configured SDL_STATIC"), problems[0])

    def test_a_product_named_only_by_its_tests_is_removed_too(self):
        self.files["games/keep/CMakeLists.txt"] = "add_executable(keep main.cpp)\n"
        self.files["tests/keep/CMakeLists.txt"] = "gyo_product_units(keep_tests PLUGINS sdl_window)\n"
        self.make_repository()
        _, products = plugin_removal.copy(self.repository, self.copy, "sdl")
        self.assertEqual(products, ["games/keep"])
        self.assertFalse((self.copy / "tests/keep").exists())

    def test_check_refuses_a_build_without_a_plugins_record(self):
        build = self.base / "build"
        build.mkdir()
        (build / "gyo-build.json").write_text("{\"configuration\": \"Release\"}", encoding="utf-8")
        self.assertIn("no build record with a plugins list", plugin_removal.check(build, "sdl")[0])


if __name__ == "__main__":
    unittest.main()
