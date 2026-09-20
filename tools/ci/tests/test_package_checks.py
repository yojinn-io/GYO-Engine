"""Exercise real installed-check subprocesses, isolation, timeout and evidence merging."""

from contextlib import redirect_stdout
import copy
import io
import json
from pathlib import Path
import sys
import subprocess
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ci_workspace import TemporaryDirectory
from package_contract import manifest_path, validate_evidence, validate_manifest
from run_package_checks import run_checks
from test_release_pipeline import APP, COMMIT, manifest
from release_support import archive_name, validate_archive


class InstalledCheckTests(unittest.TestCase):
    def setUp(self):
        temporary = TemporaryDirectory(prefix="gyo checks ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.stage, self.logs = self.root / "installed app", self.root / "logs"
        self.stage.mkdir()
        self.script = self.stage / "probe.py"
        self.script.write_text(
            "import os, pathlib, sys\n"
            "assert not pathlib.Path.cwd().is_relative_to(pathlib.Path(__file__).parent)\n"
            "assert os.environ['GYO_CHECK_VALUE'] == 'one two;three'\n"
            "print('ARGS:', repr(sys.argv[1:]), flush=True)\n", encoding="utf-8")
        self.contract = manifest("linux-x64")
        for check in self.contract["checks"]:
            check["command"] = ["@PYTHON@", "@PACKAGE_ROOT@/probe.py", "a b; $(not-a-shell)",
                                "@PROFILE@", "@GPU_SUITE@", "@DRIVER@"]
            check["environment"] = ["GYO_CHECK_VALUE=one two;three"]
        self.write_manifest()

    def write_manifest(self):
        path = self.stage / manifest_path(APP)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(self.contract), encoding="utf-8")

    def run_checks(self, **kwargs):
        self.write_manifest()
        with redirect_stdout(io.StringIO()):
            return run_checks(self.stage, APP, "linux-x64", COMMIT, "release", self.logs, **kwargs)

    def test_real_argv_environment_isolation_and_gpu_merge(self):
        self.assertTrue(self.run_checks())
        partial = json.loads((self.logs / "acceptance.json").read_text())
        with self.assertRaises(ValueError):
            validate_evidence(partial, self.contract, COMMIT, "release")
        self.assertTrue(self.run_checks(gpu=True, driver="vulkan"))
        report = json.loads((self.logs / "acceptance.json").read_text())
        validate_evidence(report, self.contract, COMMIT, "release")
        self.assertIn("a b; $(not-a-shell)", (self.logs / "startup.log").read_text())
        self.assertIn("'release', 'full', 'vulkan'", (self.logs / "render.log").read_text())

    def test_nonzero_and_missing_commands_are_failures_and_continue(self):
        self.contract["checks"][0]["command"] = ["@PYTHON@", "-c", "raise SystemExit(17)"]
        self.contract["checks"][1]["command"] = [str(self.root / "absent.exe")]
        self.assertFalse(self.run_checks())
        report = json.loads((self.logs / "acceptance.json").read_text())
        self.assertEqual([item["exit_code"] for item in report["checks"]], [17, -2])

    def test_timeout_preserves_partial_output(self):
        self.contract["checks"][0].update(timeout=0.2, command=["@PYTHON@", "-c",
            "import time; print('started', flush=True); time.sleep(20)"])
        self.assertFalse(self.run_checks())
        self.assertIn("started", (self.logs / "startup.log").read_text())
        self.assertIn("timed out", (self.logs / "startup.log").read_text())

    def test_gpu_cannot_merge_another_manifest_or_missing_startup(self):
        with self.assertRaises(ValueError):
            self.run_checks(gpu=True, driver="vulkan")
        self.assertTrue(self.run_checks())
        self.contract["checks"][0]["timeout"] += 1
        with self.assertRaises(ValueError):
            self.run_checks(gpu=True, driver="vulkan")

    def test_startup_and_relative_paths_are_required(self):
        for change in ({"checks": []}, {"executable": "../escape"},
                       {"required_files": ["/absolute"]}, {"app": "other_app"}):
            contract = {**self.contract, **change}
            with self.subTest(change=change), self.assertRaises(ValueError):
                validate_manifest(contract, APP, "linux-x64")

    def test_runner_and_archiver_cli_produce_a_verifiable_release(self):
        self.contract["platform"] = "windows-x64"
        self.contract["executable"] = "bin/sample.exe"
        self.contract["required_files"].append("probe.py")
        for relative in ("bin/sample.exe", "bin/data/config.json"):
            path = self.stage / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture", encoding="utf-8")
        self.write_manifest()
        ci = Path(__file__).resolve().parents[1]
        command = [sys.executable, str(ci / "run_package_checks.py"), "--stage", str(self.stage),
                   "--app", APP, "--platform", "windows-x64", "--revision", COMMIT,
                   "--profile", "release", "--logs", str(self.logs)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        archive = self.root / archive_name(APP, "windows-x64")
        result = subprocess.run([sys.executable, str(ci / "archive_package.py"), "--stage", str(self.stage),
            "--app", APP, "--platform", "windows-x64", "--revision", COMMIT,
            "--smoke-report", str(self.logs / "acceptance.json"), "--output", str(archive)],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        validate_archive(archive.name, archive.read_bytes(), APP, "windows-x64", COMMIT)
        for field, value in (("gpu", True), ("profiles", ["release"]),
                             ("platforms", ["windows-x64"]), ("timeout", 0)):
            contract = copy.deepcopy(self.contract)
            contract["checks"][0][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                validate_manifest(contract, APP, "linux-x64")


if __name__ == "__main__":
    unittest.main()
