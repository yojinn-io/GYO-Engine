"""Verify real CMake selection reaches CI matrices and release preflight unchanged."""

import json
from pathlib import Path
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ci_workspace import TemporaryDirectory
from app_registry import ROOT, export_registry


class RegistryMatrixTests(unittest.TestCase):
    def setUp(self):
        temporary = TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.registry = self.root / "projects.csv"

    def write_registry(self, rows):
        self.registry.write_text("name,description,version,enabled,windows,linux,macos\r\n" + rows,
                                 encoding="utf-8-sig")

    def test_export_uses_enabled_and_target_platform_flags(self):
        self.write_registry('sample_app,"中文, ""quoted""",arbitrary,true,true,false,true\r\n'
                            'disabled,,anything,false,true,true,true\r\n'
                            'linux_only,,,true,false,true,false\r\n')
        self.assertEqual(set(export_registry(self.registry)), {
            ("sample_app", "windows-x64"), ("sample_app", "macos-arm64"), ("linux_only", "linux-x64")})

    def test_empty_registry_still_builds_every_core_tool_baseline(self):
        self.write_registry("")
        output = self.root / "outputs.txt"
        command = [sys.executable, str(ROOT / "tools/ci/app_registry.py"),
                   "--registry", str(self.registry), "--output", str(output)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        outputs = dict(line.split("=", 1) for line in output.read_text().splitlines())
        matrix = json.loads(outputs["build_matrix"])["include"]
        self.assertEqual(len(matrix), 3)
        self.assertTrue(all(row["app"] == "" for row in matrix))
        self.assertEqual(outputs["has_apps"], "false")
        rejected = subprocess.run(command + ["--require-apps"], capture_output=True, text=True, timeout=30)
        self.assertNotEqual(rejected.returncode, 0)

    def test_prepare_release_rejects_empty_registry_before_remote_work(self):
        self.write_registry("")
        commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
        event = self.root / "event.json"
        event.write_text(json.dumps({"inputs": {"version": "v9876.5432.100", "prerelease": False}}))
        output = self.root / "outputs.txt"
        result = subprocess.run([sys.executable, str(ROOT / "tools/ci/release_pipeline.py"), "prepare",
            "--registry", str(self.registry), "--event-path", str(event), "--event-name", "workflow_dispatch",
            "--ref", "refs/heads/main", "--commit", commit, "--output", str(output)],
            capture_output=True, text=True, timeout=30)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("at least one enabled app/platform", result.stderr)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
