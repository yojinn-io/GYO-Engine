"""Verify real CMake selection reaches CI matrices and release preflight unchanged."""

import contextlib
import csv
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[3]
for folder in ("build", "build/ci/common", "build/acceptance/common"):
    sys.path.insert(0, str(ROOT / folder))
from workspace import TemporaryDirectory
from app_registry import RELEASE_PLATFORMS, ROOT, export_registry
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
        # The macos column also enables the local-only macos-x64 build target,
        # which the release export drops: no CI row builds or packages it.
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
        self.assertEqual([row["platform"] for row in matrix], list(RELEASE_PLATFORMS))
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

    def test_registered_tools_without_a_release_selection_are_rejected(self):
        registry = self.root / "tools.csv"
        registry.write_text("name,description,version,enabled,default,release,windows,linux,macos\n"
                            "local,Local,,1,1,0,1,1,1\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "registry export failed"):
            app_registry.export_tools(registry, repository_root=self.root)

    def test_empty_tool_registry_selects_no_tool_on_every_platform_and_says_so(self):
        registry = self.root / "tools.csv"
        registry.write_text("name,description,version,enabled,default,release,windows,linux,macos\n",
                            encoding="utf-8")
        with contextlib.redirect_stderr(io.StringIO()) as stderr:
            tools = app_registry.export_tools(registry, repository_root=self.root)
        self.assertEqual(tools, {platform: [] for platform in RELEASE_PLATFORMS})
        self.assertIn("No tool is registered", stderr.getvalue())

    def test_selection_empty_on_only_some_platforms_is_rejected(self):
        # CMake never exports this; the consumer still refuses it rather than
        # reading it as "no tool registered".
        def fake_export(command, **_):
            output = Path(next(argument for argument in command if argument.startswith("-DGYO_OUTPUT="))
                          .split("=", 1)[1])
            rows = [{"platform": platform, "tools": ["alpha:native"] if index else []}
                    for index, platform in enumerate(RELEASE_PLATFORMS)]
            output.write_text(json.dumps({"include": rows}), encoding="utf-8")
            return subprocess.CompletedProcess(command, 0, "", "")
        with mock.patch.object(app_registry.subprocess, "run", side_effect=fake_export), \
                self.assertRaisesRegex(ValueError, "invalid tool selection"):
            app_registry.export_tools(self.tool_registry, repository_root=self.root)

    def test_repository_registries_export_for_ci(self):
        # The registries of this checkout, whatever they hold. The release
        # rules (a release tool on every platform, a packageable default
        # variant) are checked by the export alone, and the workflows export
        # only after the pull request gate; this case exports at the gate.
        # No tool is selected exactly when none is registered.
        with (ROOT / "config/tools.csv").open(encoding="utf-8-sig", newline="") as rows:
            registered = [row["name"] for row in csv.DictReader(rows)]
        with contextlib.redirect_stderr(io.StringIO()) as stderr:
            tools = app_registry.export_tools()
            pairs = export_registry()
        self.assertEqual(list(tools), list(RELEASE_PLATFORMS))
        self.assertEqual(all(tools.values()), bool(registered), tools)
        self.assertEqual("No tool is registered" in stderr.getvalue(), not registered)
        selected = {tool.split(":", 1)[0] for row in tools.values() for tool in row}
        self.assertLessEqual(selected, set(registered))
        self.assertTrue(all(platform in RELEASE_PLATFORMS for _, platform in pairs))

    def test_prepare_release_accepts_empty_registry_without_remote_work(self):
        self.write_registry("")
        commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
        event = self.root / "event.json"
        event.write_text(json.dumps({"inputs": {"train": "tools", "version": "v2099.12.99", "prerelease": False}}))
        output = self.root / "outputs.txt"
        result = subprocess.run([sys.executable, str(ROOT / "build/ci/common/release_pipeline.py"), "prepare",
            "--registry", str(self.registry), "--event-path", str(event), "--event-name", "workflow_dispatch",
            "--tool-registry", str(self.tool_registry), "--repository-root", str(self.root),
            "--ref", "refs/heads/main", "--commit", commit, "--output", str(output)],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        outputs = dict(line.split("=", 1) for line in output.read_text().splitlines())
        self.assertEqual((outputs["tag"], outputs["train"], outputs["product"]),
                         ("tools-v2099.12.99", "tools", "toolchain"))


if __name__ == "__main__":
    unittest.main()
