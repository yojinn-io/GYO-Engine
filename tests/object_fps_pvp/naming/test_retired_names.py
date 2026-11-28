"""Self-test of retired_names.py on synthetic trees (no repository file is read)."""
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import retired_names as guard  # noqa: E402

CLEAN = """#pragma once
// The scene data: a Scene file holds the furnishing over a map.
struct Scene final { int id = 0; };
enum class Screen { Menu, Lobby };
struct ScreenChange final { Screen screen = Screen::Menu; };
inline const char* note = "a longer word is not the name: MySceneChangeWantedX";
"""

# The old names of the declaration's table (interact-door-declaration.zh-Hant.md 7.1), kept apart from the guard's
# list so that a name dropped from the list is caught here.
DECLARED_IDENTIFIERS = (
    "SceneChange", "SceneRequest", "SceneDirector", "ChangeScene", "SceneChangeWanted", "SceneName",
    "PlaceSceneDocument", "PlaceSceneCamera", "BuildSceneCommon", "InSceneUpdate", "kSceneCamera", "kSceneRoot",
    "ClientScene", "GameScene", "firstScene", "documentScene",
)
DECLARED_VALUES = ("Menu", "Splash", "SoloRoom", "SoloGame", "Lobby", "Room", "MultiGame")
DECLARED_FILES = (
    "Scenes.hpp", "Scenes.cpp", "SceneBuilds.hpp", "GatewayScene.cpp", "LobbyScene.cpp", "RoomScene.cpp",
    "SoloScene.cpp",
)


class RetiredNamesTest(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        for directory in guard.SCAN_DIRS:
            (self.root / directory).mkdir(parents=True)

    def tearDown(self):
        shutil.rmtree(self.root)

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def test_clean_tree_has_no_finding(self):
        self.write("games/object_fps_pvp/src/client/Screens.hpp", CLEAN)
        self.assertEqual(guard.findings(self.root), [])

    def test_every_listed_identifier_is_found(self):
        for name in DECLARED_IDENTIFIERS:
            with self.subTest(name=name):
                self.write("tests/object_fps_pvp/client/T.cpp", f"auto x = {name}{{}};\n")
                self.assertEqual(guard.findings(self.root), [f"tests/object_fps_pvp/client/T.cpp:1: retired name {name}"])

    def test_old_enum_and_its_values_are_found(self):
        self.write("games/object_fps_pvp/a.hpp", "enum class Scene { Menu };\n")
        self.assertEqual(guard.findings(self.root), ["games/object_fps_pvp/a.hpp:1: retired name enum class Scene"])
        for value in DECLARED_VALUES:
            with self.subTest(value=value):
                self.write("games/object_fps_pvp/a.hpp", f"auto s = Scene::{value};\n")
                self.assertEqual(guard.findings(self.root), ["games/object_fps_pvp/a.hpp:1: retired name Scene::<value>"])

    def test_old_file_names_are_found_as_files_and_as_text(self):
        for name in DECLARED_FILES:
            with self.subTest(name=name):
                self.write(f"games/object_fps_pvp/src/client/{name}", "int x = 0;\n")
                self.write("games/object_fps_pvp/CMakeLists.txt", f"add_library(a src/client/{name})\n")
                self.assertEqual(
                    guard.findings(self.root),
                    [
                        f"games/object_fps_pvp/CMakeLists.txt:1: retired name {name}",
                        f"games/object_fps_pvp/src/client/{name}: retired file name {name}",
                    ],
                )
                (self.root / f"games/object_fps_pvp/src/client/{name}").unlink()

    def test_files_outside_the_scan_are_not_read(self):
        self.write("games/object_fps_pvp/docs/notes.md", "SceneChange\n")
        self.write("engine/x.hpp", "SceneChange\n")
        self.assertEqual(guard.findings(self.root), [])

    def test_main_fails_on_a_finding(self):
        self.write("games/object_fps_pvp/a.cpp", "SceneDirector d;\n")
        self.assertEqual(guard.main(["--root", str(self.root)]), 1)
        (self.root / "games/object_fps_pvp/a.cpp").unlink()
        self.assertEqual(guard.main(["--root", str(self.root)]), 0)


if __name__ == "__main__":
    unittest.main()
