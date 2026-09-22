"""Generic archive acceptance policy, independent of any repository application."""

import json
from pathlib import Path
import sys
import subprocess
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
for folder in ("build", "build/ci/common", "build/acceptance/common"):
    sys.path.insert(0, str(ROOT / folder))
from workspace import TemporaryDirectory
from archive_package import package_metadata
from test_release_pipeline import APP, COMMIT, evidence, manifest, package_contents, package
from app_registry import release_products
from release_support import validate_archive, PLATFORMS, ReleaseError
from package_contract import installed_required_files
from package_contract import manifest_path


class ArchiveMetadataTests(unittest.TestCase):
    def setUp(self):
        self.workspace = TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.report = Path(self.workspace.name) / "smoke.json"
        self.contract = manifest("linux-x64")

    def metadata(self, report=None):
        self.report.write_text(json.dumps(report or evidence(self.contract)), encoding="utf-8-sig")
        return package_metadata(COMMIT, self.contract, self.report)

    def test_complete_evidence_preserves_physical_gpu_boundary(self):
        result = self.metadata()
        self.assertEqual(result["product"], "sample_app")
        self.assertIn("manual", result["gpu_acceptance"])

    def test_identity_and_changed_manifest_are_rejected(self):
        for key, value in (("source_revision", "b" * 40), ("platform", "windows-x64"),
                           ("product", "other_app"), ("manifest_sha256", "0" * 64)):
            report = evidence(self.contract)
            report[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.metadata(report)

    def test_missing_duplicate_failed_and_extra_check_are_rejected(self):
        for results in ([], evidence(self.contract)["checks"][:-1],
                        evidence(self.contract)["checks"] * 2,
                        [{"name": "startup", "passed": False, "exit_code": 1}],
                        evidence(self.contract)["checks"] + [{"name": "extra", "passed": True, "exit_code": 0}]):
            report = evidence(self.contract)
            report["checks"] = results
            with self.subTest(results=results), self.assertRaises(ValueError):
                self.metadata(report)

    def test_quick_archive_requires_only_declared_quick_checks(self):
        report = evidence(self.contract, profile="quick")
        self.assertEqual(self.metadata(report)["ci_smoke"]["profile"], "quick")

    def test_revision_must_be_unambiguous(self):
        with self.assertRaises(ValueError):
            package_metadata("main", self.contract)

    def test_local_archive_does_not_invent_ci_results(self):
        self.assertIsNone(package_metadata(COMMIT, self.contract)["ci_smoke"])

    def test_archiver_refuses_a_multi_app_installation_before_writing_artifacts(self):
        root = Path(self.workspace.name)
        stage = root / "stage"
        for name, data in package_contents("windows-x64").items():
            path = stage / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        foreign = stage / manifest_path("other_app")
        foreign.parent.mkdir(parents=True)
        foreign.write_text(json.dumps(manifest(product="other_app")), encoding="utf-8")
        output = root / "gyo-sample_app-windows-x64.tar.gz"
        result = subprocess.run([sys.executable, str(ROOT / "build/ci/common/archive_package.py"),
            "--stage", str(stage), "--product", APP, "--revision", COMMIT, "--platform", "windows-x64",
            "--output", str(output)], capture_output=True, text=True, timeout=30)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("another product installation: other_app", result.stderr)
        self.assertFalse(output.exists())
        self.assertFalse((stage / "build_metadata.json").exists())

    def test_toolchain_archives_are_valid_without_any_game(self):
        registry = Path(self.workspace.name) / "empty.csv"
        registry.write_text("name,description,version,enabled,windows,linux,macos\n")
        self.assertEqual(set(release_products(registry)), {("toolchain", platform) for platform in PLATFORMS})
        for platform in PLATFORMS:
            built = package(platform, product="toolchain")
            validate_archive(built.name, built.data, "toolchain", platform, COMMIT)

    def test_catalog_dependencies_are_checked_without_cmake_asset_lists(self):
        stage = Path(self.workspace.name) / "content-stage"
        assets = stage / f"bin/assets/{APP}"
        assets.mkdir(parents=True)
        (assets / "content.json").write_text(json.dumps({"version":1,"catalogs":["catalog.json"],"shader_bundles":[]}))
        (assets / "catalog.json").write_text(json.dumps({"version":1,"assets":[{"id":"new","type":"binary","path":"new.bin"}]}))
        contract = manifest()
        contract["required_files"].append(f"bin/assets/{APP}/content.json")
        self.assertIn(f"bin/assets/{APP}/new.bin", installed_required_files(stage, contract))

    def test_installed_and_archived_content_decode_shared_raw_json_strictly(self):
        cases = json.loads((ROOT / "tests/common/fixtures/asset_contract/raw_cases.json").read_text(encoding="utf-8"))["cases"]
        content_root = f"bin/assets/{APP}"
        contract = manifest()
        contract["required_files"].append(content_root + "/content.json")
        for case in cases:
            with self.subTest(case=case["name"]):
                documents = {
                    content_root + "/content.json": b'{"version":1,"catalogs":["catalog.json"],"shader_bundles":[]}',
                    content_root + "/catalog.json": b'{"version":1,"assets":[]}',
                }
                name = "catalog.json" if case["kind"] == "catalog" else "content.json"
                documents[content_root + "/" + name] = case["text"].encode("utf-8")
                if case["kind"] == "content" and case["valid"]:
                    # An empty-content decoding fixture must not accidentally
                    # include an undeclared catalog in its package inventory.
                    if "catalog.json" not in json.loads(case["text"])["catalogs"]:
                        documents.pop(content_root + "/catalog.json")
                stage = Path(self.workspace.name) / case["name"]
                for name, data in documents.items():
                    path = stage / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(data)
                # Produce matching evidence even for invalid JSON, so validation
                # must reject the raw content rather than a stale checksum.
                with patch("test_release_pipeline.manifest", return_value=contract):
                    archive = package(extra_files=documents.items())
                if case["valid"]:
                    installed_required_files(stage, contract)
                    validate_archive(archive.name, archive.data, APP, "windows-x64", COMMIT)
                else:
                    with self.assertRaises(ValueError):
                        installed_required_files(stage, contract)
                    with self.assertRaises(ReleaseError):
                        validate_archive(archive.name, archive.data, APP, "windows-x64", COMMIT)


if __name__ == "__main__":
    unittest.main()
