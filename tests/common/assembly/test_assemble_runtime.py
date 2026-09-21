"""Exercise content staging independently of CMake and CI."""
import json
from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "build"))
from workspace import TemporaryDirectory
from assemble_runtime import assemble

class AssemblyTests(unittest.TestCase):
    def setUp(self):
        temp = TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.assets = self.root / "source"
        self.assets.mkdir()
        self.stage = self.root / "stage"
        self.content = {"version":1,"catalogs":["asset_catalog.json"],"shader_bundles":[]}
        self.catalog = {"version":1,"assets":[{"id":"fixture","type":"text","path":"data.txt"}]}
        (self.assets / "data.txt").write_text("first")
        self.write_documents()
    def write_documents(self):
        (self.assets / "content.json").write_text(json.dumps(self.content))
        (self.assets / "asset_catalog.json").write_text(json.dumps(self.catalog))
    def run_assembly(self, shaders=None):
        self.write_documents()
        assemble("sample", "app", self.stage, self.assets, shaders)
        return self.stage / "bin/assets/sample"
    def test_catalog_driven_copy_and_updates_do_not_require_compilation(self):
        (self.assets / "private-source.blend").write_text("not runtime")
        output = self.run_assembly()
        self.assertEqual((output / "data.txt").read_text(), "first")
        self.assertFalse((output / "private-source.blend").exists())
        (self.assets / "data.txt").write_text("second")
        self.run_assembly()
        self.assertEqual((output / "data.txt").read_text(), "second")
    def test_removed_catalog_entry_prunes_stale_files_and_preserves_other_products(self):
        output = self.run_assembly()
        other = self.stage / "bin/assets/other/keep.txt"
        other.parent.mkdir(parents=True)
        other.write_text("keep")
        self.catalog["assets"] = []
        self.run_assembly()
        self.assertFalse((output / "data.txt").exists())
        self.assertEqual(other.read_text(), "keep")
    def test_missing_source_fails_even_if_stale_copy_exists(self):
        self.run_assembly()
        (self.assets / "data.txt").unlink()
        with self.assertRaisesRegex(ValueError, "absent"):
            self.run_assembly()
    def test_shader_outputs_are_declared_and_build_source_paths_are_removed(self):
        shader = self.root / "shader"
        shader.mkdir()
        (shader / "manifest.json").write_text(json.dumps({"programs":[{"variants":[{"vertex":{"file":"compiled.bin"}}]}]}))
        (shader / "bundle.d").write_text("private absolute source paths")
        (shader / "compiled.bin").write_bytes(b"compiled")
        self.content["shader_bundles"] = [{"name":"builtin","source":"../../engine/source.json","path":"shaders/builtin"}]
        output = self.run_assembly({"builtin":shader})
        deployed = json.loads((output / "content.json").read_text())
        self.assertNotIn("source", deployed["shader_bundles"][0])
        self.assertFalse((output / "shaders/builtin/bundle.d").exists())
        self.assertEqual((output / "shaders/builtin/compiled.bin").read_bytes(), b"compiled")
        with self.assertRaisesRegex(ValueError, "Missing compiled"):
            self.run_assembly()
    def test_content_cannot_escape_its_game_root(self):
        self.catalog["assets"][0]["path"] = "../outside.txt"
        with self.assertRaises(ValueError):
            self.run_assembly()
    def test_unexpected_shader_is_not_silently_deployed(self):
        with self.assertRaisesRegex(ValueError, "Unexpected shader"):
            self.run_assembly({"other":self.root})

    def test_catalog_requires_runtime_identity_and_type(self):
        for field in ("id", "type"):
            original = self.catalog["assets"][0].pop(field)
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.run_assembly()
            self.catalog["assets"][0][field] = original

    def test_catalog_identity_is_unique_across_all_declared_catalogs(self):
        self.content["catalogs"].append("second.json")
        (self.assets / "second.json").write_text(json.dumps(self.catalog), encoding="utf-8")
        with self.assertRaises(ValueError):
            self.run_assembly()

    def test_content_schema_matches_runtime_and_output_paths_do_not_overlap(self):
        original = dict(self.content)
        for change in ({"version":True}, {"catalogs":["asset_catalog.json", "asset_catalog.json"]},
                       {"shader_bundles":[{"name":"bad","path":"asset_catalog.json/nested"}]}):
            self.content = {**original, **change}
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.run_assembly()
        self.content = dict(original)
        del self.content["shader_bundles"]
        with self.assertRaises(ValueError):
            self.run_assembly()


if __name__ == "__main__":
    unittest.main()
