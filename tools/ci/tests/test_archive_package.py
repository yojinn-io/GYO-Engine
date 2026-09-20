"""Generic archive acceptance policy, independent of any repository application."""

import json
from pathlib import Path
import sys
import subprocess
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ci_workspace import TemporaryDirectory
from archive_package import package_metadata
from test_release_pipeline import APP, COMMIT, evidence, manifest, package_contents
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
        self.assertEqual(result["app"], "sample_app")
        self.assertIn("manual", result["gpu_acceptance"])

    def test_identity_and_changed_manifest_are_rejected(self):
        for key, value in (("source_revision", "b" * 40), ("platform", "windows-x64"),
                           ("app", "other_app"), ("manifest_sha256", "0" * 64)):
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
        foreign.write_text(json.dumps(manifest(app="other_app")), encoding="utf-8")
        output = root / "gyo-sample_app-windows-x64.tar.gz"
        result = subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / "archive_package.py"),
            "--stage", str(stage), "--app", APP, "--revision", COMMIT, "--platform", "windows-x64",
            "--output", str(output)], capture_output=True, text=True, timeout=30)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("another app installation: other_app", result.stderr)
        self.assertFalse(output.exists())
        self.assertFalse((stage / "build_metadata.json").exists())


if __name__ == "__main__":
    unittest.main()
