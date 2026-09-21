"""Assembly consumes the same content fixtures as runtime and archive checks."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "build"))
from assemble_runtime import assemble
from workspace import TemporaryDirectory


class ContentContractTests(unittest.TestCase):
    def test_shared_raw_json_is_decoded_strictly(self):
        cases = json.loads((ROOT / "tests/common/fixtures/asset_contract/raw_cases.json").read_text(encoding="utf-8"))["cases"]
        for case in cases:
            with self.subTest(case=case["name"]), TemporaryDirectory(prefix="gyo-content-json-") as work:
                root = Path(work)
                assets = root / "assets"
                assets.mkdir()
                (assets / "content.json").write_text(
                    '{"version":1,"catalogs":["catalog.json"],"shader_bundles":[]}', encoding="utf-8")
                (assets / "catalog.json").write_text('{"version":1,"assets":[]}', encoding="utf-8")
                document = "catalog.json" if case["kind"] == "catalog" else "content.json"
                (assets / document).write_text(case["text"], encoding="utf-8")
                if case["valid"]:
                    assemble("sample", "app", root / "stage", assets)
                else:
                    with self.assertRaises(ValueError):
                        assemble("sample", "app", root / "stage", assets)

    def test_shared_runtime_content_fixtures(self):
        cases = json.loads((ROOT / "tests/common/fixtures/asset_contract/cases.json").read_text(encoding="utf-8"))["cases"]
        for case in cases:
            with self.subTest(case=case["name"]), TemporaryDirectory(prefix="gyo-content-contract-") as work:
                root = Path(work)
                assets = root / "assets"
                assets.mkdir()
                (assets / "content.json").write_text(json.dumps(case["content"]), encoding="utf-8")
                for name, catalog in case["catalogs"].items():
                    (assets / name).write_text(json.dumps(catalog), encoding="utf-8")
                (assets / "data.txt").write_text("present", encoding="utf-8")
                shaders = {}
                content = case["content"]
                if isinstance(content, dict):
                    for bundle in content["shader_bundles"]:
                        source = root / "compiled" / bundle["name"]
                        source.mkdir(parents=True, exist_ok=True)
                        (source / "manifest.json").write_text('{"programs":[]}', encoding="utf-8")
                        shaders[bundle["name"]] = source
                if case["valid"]:
                    assemble("sample", "app", root / "stage", assets, shaders)
                    self.assertTrue((root / "stage/bin/assets/sample/content.json").is_file())
                else:
                    with self.assertRaises(ValueError):
                        assemble("sample", "app", root / "stage", assets, shaders)


if __name__ == "__main__":
    unittest.main()
