"""A copied game's assets and identity remain independent."""
from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "build"))
from workspace import TemporaryDirectory
from app_copy_integration import create_copy

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
            self.assertEqual((root / "apps/sample_game/CMakeLists.txt").read_bytes(),
                             (root / "apps/variant_game/CMakeLists.txt").read_bytes())
            self.assertIn("fixture.marker", (root / "assets/variant_game/asset_catalog.json").read_text())
            self.assertIn("variant_game,Copy fixture,,1,1,1,1", (root / "engine/config/projects.csv").read_text())
