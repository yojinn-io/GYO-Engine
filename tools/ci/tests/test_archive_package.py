import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


spec = importlib.util.spec_from_file_location("archive_package", Path(__file__).resolve().parents[1] / "archive_package.py")
archive_package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(archive_package)
SHA = "0123456789abcdef0123456789abcdef01234567"


class ArchiveMetadataTests(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.report = Path(self.workspace.name) / "smoke.json"

    def metadata(self, platform_id="linux-x64", **changes):
        report = {
            "source_revision": SHA, "platform": platform_id, "headless": "passed",
            "profile": "release", "startup": "passed",
            "rendering": {"status": "passed", "driver": "vulkan", "implementation": "mesa-lavapipe",
                          "suite": "full", "case_count": 8},
        }
        report.update(changes)
        self.report.write_text(json.dumps(report), encoding="utf-8-sig")
        return archive_package.package_metadata(SHA, platform_id, self.report)

    def test_software_acceptance_keeps_physical_gpu_boundary(self):
        result = self.metadata()
        self.assertEqual(result["ci_smoke"]["rendering"]["implementation"], "mesa-lavapipe")
        self.assertIn("manual", result["gpu_acceptance"])

    def test_wrong_revision_platform_and_failed_headless_are_rejected(self):
        for change in ({"source_revision": "a" * 40}, {"platform": "windows-x64"}, {"headless": "failed"}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.metadata(**change)

    def test_linux_missing_failed_or_partial_gpu_smoke_is_rejected(self):
        for rendering in ({}, {"status": "not_run"}, {"status": "failed"},
                          {"status": "passed", "driver": "vulkan", "implementation": "mesa-lavapipe",
                           "suite": "ci", "case_count": 2}):
            with self.subTest(rendering=rendering), self.assertRaises(ValueError):
                self.metadata(rendering=rendering)

    def test_quick_shader_smoke_can_be_archived_without_gameplay(self):
        result = self.metadata(profile="quick", headless="not_run",
                               rendering={"status": "passed", "driver": "vulkan",
                                          "implementation": "mesa-lavapipe", "suite": "quick", "case_count": 1})
        self.assertEqual(result["ci_smoke"]["profile"], "quick")
        self.assertEqual(result["ci_smoke"]["headless"], "not_run")

    def test_release_cannot_substitute_quick_shader_smoke(self):
        with self.assertRaises(ValueError):
            self.metadata(rendering={"status": "passed", "driver": "vulkan",
                                     "implementation": "mesa-lavapipe", "suite": "quick", "case_count": 1})
        with self.assertRaises(ValueError):
            self.metadata(headless="not_run")

    def test_windows_and_mac_record_headless_only(self):
        for platform_id in ("windows-x64", "macos-arm64"):
            result = self.metadata(platform_id, rendering={"status": "not_run", "reason": "manual GPU acceptance"})
            self.assertEqual(result["ci_smoke"]["headless"], "passed")

    def test_revision_and_platform_must_be_unambiguous(self):
        for revision, platform_id in (("master", "linux-x64"), (SHA, "linux")):
            with self.subTest(revision=revision, platform_id=platform_id), self.assertRaises(ValueError):
                archive_package.package_metadata(revision, platform_id)

    def test_local_archive_does_not_invent_ci_results(self):
        self.assertIsNone(archive_package.package_metadata(SHA, "windows-x64")["ci_smoke"])


if __name__ == "__main__":
    unittest.main()
