"""Verify real CMake selection reaches CI matrices and release preflight unchanged."""

import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3]
for folder in ("build", "build/ci/common", "build/acceptance/common"):
    sys.path.insert(0, str(ROOT / folder))
from workspace import TemporaryDirectory
from app_registry import ROOT, export_registry
import app_registry


class RegistryMatrixTests(unittest.TestCase):
    def setUp(self):
        temporary = TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.registry = self.root / "projects.csv"
        self.tool_registry = self.root / "tools.csv"
        self.tool_registry.write_text("name,description,version,enabled,default,release,windows,linux,macos\n"
                                     "alpha,Fixture,,1,0,1,1,1,1\n", encoding="utf-8")
        directory = self.root / "tools/alpha"
        directory.mkdir(parents=True)
        (directory / "project.json").write_text(json.dumps({"version": 1,
            "default_variant": "native", "requires_apps": {},
            "variants": {"native": {"components": [], "packageable": True}}}))

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
        command = [sys.executable, str(ROOT / "build/ci/common/app_registry.py"),
                   "--registry", str(self.registry), "--tool-registry", str(self.tool_registry),
                   "--repository-root", str(self.root), "--output", str(output)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        outputs = dict(line.split("=", 1) for line in output.read_text().splitlines())
        matrix = json.loads(outputs["build_matrix"])["include"]
        self.assertEqual(len(matrix), 3)
        self.assertTrue(all(row["product"] == "toolchain" for row in matrix))
        self.assertTrue(all(row["tools"] for row in matrix))
        self.assertEqual(outputs["has_apps"], "false")

    def test_tool_export_uses_data_owned_modes_and_platform_selection(self):
        registry = self.root / "tools.csv"
        registry.write_text("name,description,version,enabled,default,release,windows,linux,macos\n"
                            "alpha,First,,1,0,1,1,1,1\n"
                            "beta,Second,,1,0,1,0,1,0\n"
                            "local,Local,,1,1,0,1,1,1\n", encoding="utf-8")
        for owner in ("alpha", "beta", "local"):
            directory = self.root / "tools" / owner
            directory.mkdir(parents=True, exist_ok=True)
            (directory / "project.json").write_text(json.dumps({"version": 1,
                "default_variant": "native", "requires_apps": {},
                "variants": {"native": {"components": [], "packageable": owner != "local"}}}))
        tools = app_registry.export_tools(registry, repository_root=self.root)
        self.assertEqual(tools, {"windows-x64": ["alpha:native"],
            "linux-x64": ["alpha:native", "beta:native"], "macos-arm64": ["alpha:native"]})

    def test_empty_release_tool_selection_is_rejected(self):
        registry = self.root / "tools.csv"
        registry.write_text("name,description,version,enabled,default,release,windows,linux,macos\n",
                            encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "registry export failed"):
            app_registry.export_tools(registry, repository_root=self.root)

    def test_prepare_release_accepts_empty_registry_without_remote_work(self):
        self.write_registry("")
        commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
        event = self.root / "event.json"
        event.write_text(json.dumps({"inputs": {"version": "v9876.5432.100", "prerelease": False}}))
        output = self.root / "outputs.txt"
        result = subprocess.run([sys.executable, str(ROOT / "build/ci/common/release_pipeline.py"), "prepare",
            "--registry", str(self.registry), "--event-path", str(event), "--event-name", "workflow_dispatch",
            "--tool-registry", str(self.tool_registry), "--repository-root", str(self.root),
            "--ref", "refs/heads/main", "--commit", commit, "--output", str(output)],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(output.exists())


if __name__ == "__main__":
    unittest.main()
