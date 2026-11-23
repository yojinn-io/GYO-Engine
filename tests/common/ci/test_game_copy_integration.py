"""A copied game's assets and identity remain independent."""
from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "build"))
from workspace import TemporaryDirectory
import subprocess
from game_copy_integration import create_copy, mirror_sources

class CopyFixtureTests(unittest.TestCase):
    def test_copies_preserve_asset_ids_and_have_independent_content(self):
        with TemporaryDirectory() as work:
            root = Path(work)
            create_copy(root)
            first = root / "assets/sample_game/marker.txt"
            second = root / "assets/variant_game/marker.txt"
            self.assertEqual(first.read_text(), "sample_game\n")
            self.assertEqual(second.read_text(), "variant_game\n")
            first.write_text("changed")
            self.assertEqual(second.read_text(), "variant_game\n")
            self.assertEqual((root / "games/sample_game/CMakeLists.txt").read_bytes(),
                             (root / "games/variant_game/CMakeLists.txt").read_bytes())
            self.assertIn("fixture.marker", (root / "assets/variant_game/asset_catalog.json").read_text())
            self.assertIn("variant_game,Copy fixture,,1,1,1,1", (root / "config/games.csv").read_text())


class MirrorTests(unittest.TestCase):
    # The configure of the copy reads config/modules.csv and config/plugins.csv
    # (GyoBuild.cmake); a mirror without them failed with "Registry does not
    # exist" (a8065853 added the read, the mirror did not follow).
    def test_mirror_carries_the_engine_registers_and_plugins_but_no_product_register(self):
        with TemporaryDirectory() as work:
            repository, destination = Path(work) / "repository", Path(work) / "copy"
            for name in ("CMakeLists.txt", "config/modules.csv", "config/plugins.csv", "config/games.csv",
                         "config/tools.csv", "plugins/sample/CMakeLists.txt", "games/real/CMakeLists.txt",
                         "tools/real/CMakeLists.txt", "tests/common/CMakeLists.txt"):
                (repository / name).parent.mkdir(parents=True, exist_ok=True)
                (repository / name).write_text("# fixture\n", encoding="utf-8")
            subprocess.run(["git", "init", "-q", str(repository)], check=True)
            copied = set(mirror_sources(repository, destination))
            for name in ("CMakeLists.txt", "config/modules.csv", "config/plugins.csv", "plugins/sample/CMakeLists.txt"):
                self.assertIn(name, copied)
            for name in ("config/games.csv", "config/tools.csv", "games/real/CMakeLists.txt",
                         "tools/real/CMakeLists.txt", "tests/common/CMakeLists.txt"):
                self.assertNotIn(name, copied)
