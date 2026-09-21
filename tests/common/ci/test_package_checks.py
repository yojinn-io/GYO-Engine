"""Exercise real installed-check subprocesses, isolation, timeout and evidence merging."""

from contextlib import redirect_stdout
import copy
import io
import json
from pathlib import Path
import sys
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[3]
for folder in ("build", "build/ci/common", "build/acceptance/common"):
    sys.path.insert(0, str(ROOT / folder))
from workspace import TemporaryDirectory
from package_contract import PLATFORMS, manifest_path, validate_evidence, validate_manifest, required_checks
from run_package_checks import run_checks
from test_release_pipeline import APP, COMMIT, manifest
from release_support import archive_name, validate_archive


class InstalledCheckTests(unittest.TestCase):
    def setUp(self):
        temporary = TemporaryDirectory(prefix="gyo checks ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.stage, self.logs = self.root / "installed product", self.root / "logs"
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
        for relative in ("bin/sample", "bin/data/config.json"):
            path = self.stage / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture")
        self.write_manifest()

    def write_manifest(self):
        path = self.stage / manifest_path(APP)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(self.contract), encoding="utf-8")

    def run_checks(self, **kwargs):
        self.write_manifest()
        with redirect_stdout(io.StringIO()):
            return run_checks(self.stage, APP, "linux-x64", COMMIT, "release", self.logs, **kwargs)

    def test_repository_toolchain_contract_is_valid_on_every_platform(self):
        contract = json.loads((ROOT / "build/acceptance/ui_editor/checks.json").read_text(encoding="utf-8"))
        self.assertEqual(contract["version"], 1)
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                package = manifest(platform, product="toolchain")
                package["executables"] = {"ui_editor":"bin/gyo_ui_editor"}
                package["checks"] = contract["checks"]
                validate_manifest(package, "toolchain", platform)
                for profile in ("quick", "release"):
                    self.assertEqual({check["name"] for check in required_checks(package, profile)}, {"editor_cli"})

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
        self.assertEqual([item["exit_code"] for item in report["checks"]], [0, 17, -2])

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

    def test_generic_products_need_no_startup_and_paths_are_confined(self):
        validate_manifest({**self.contract, "checks": []}, APP, "linux-x64")
        for change in ({"executables": {APP:"../escape"}},
                       {"required_files": ["/absolute"]}, {"product": "other_app"}):
            contract = {**self.contract, **change}
            with self.subTest(change=change), self.assertRaises(ValueError):
                validate_manifest(contract, APP, "linux-x64")

    def test_runner_and_archiver_cli_produce_a_verifiable_release(self):
        self.contract["platform"] = "windows-x64"
        self.contract["executables"] = {APP: "bin/sample.exe"}
        self.contract["required_files"].append("probe.py")
        for relative in ("bin/sample.exe", "bin/data/config.json"):
            path = self.stage / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture", encoding="utf-8")
        self.write_manifest()
        ci = ROOT / "build/ci/common"
        acceptance = ROOT / "build/acceptance/common"
        command = [sys.executable, str(acceptance / "run_package_checks.py"), "--stage", str(self.stage),
                   "--product", APP, "--platform", "windows-x64", "--revision", COMMIT,
                   "--profile", "release", "--logs", str(self.logs)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        archive = self.root / archive_name(APP, "windows-x64")
        result = subprocess.run([sys.executable, str(ci / "archive_package.py"), "--stage", str(self.stage),
            "--product", APP, "--platform", "windows-x64", "--revision", COMMIT,
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

    def test_external_probe_is_colocated_only_inside_temporary_acceptance_package(self):
        probes = self.root / "probes"
        probes.mkdir()
        probe = probes / "diagnostic.py"
        probe.write_text("from pathlib import Path; assert (Path(__file__).parent / 'data/config.json').is_file()")
        self.contract["checks"] = [self.contract["checks"][0]]
        self.contract["checks"][0]["command"] = ["@PYTHON@", "@PACKAGE_ROOT@/bin/diagnostic.py"]
        self.assertTrue(self.run_checks(probe_directory=probes))
        self.assertFalse((self.stage / "bin/diagnostic.py").exists())
        self.assertTrue(probe.exists())

    def test_gpu_merge_rejects_changed_product_bytes_after_cpu_acceptance(self):
        self.assertTrue(self.run_checks())
        (self.stage / "bin/data/config.json").write_text("changed after CPU checks")
        with self.assertRaisesRegex(ValueError, "different package/profile"):
            self.run_checks(gpu=True, driver="vulkan")

    def test_generated_archive_metadata_does_not_invalidate_content_evidence(self):
        self.assertTrue(self.run_checks())
        (self.stage / "build_metadata.json").write_text('{"source_revision":"archive-only"}')
        self.assertTrue(self.run_checks(gpu=True, driver="vulkan"))

    def test_foreign_game_payload_and_acceptance_executable_are_not_products(self):
        for relative in ("bin/assets/other_game/data.txt", "bin/gyo_other_game", "bin/gyo_sample_app_acceptance.exe"):
            with self.subTest(relative=relative):
                foreign = self.stage / relative
                foreign.parent.mkdir(parents=True, exist_ok=True)
                foreign.write_text("unexpected")
                try:
                    with self.assertRaises(ValueError):
                        self.run_checks()
                finally:
                    foreign.unlink()
                    # Empty foreign asset directories also represent a mixed installation.
                    if relative.startswith("bin/assets/"):
                        foreign.parent.rmdir()

    def test_archive_rejects_mutation_between_acceptance_and_packaging(self):
        self.contract["platform"] = "windows-x64"
        self.contract["checks"] = [self.contract["checks"][0]]
        self.write_manifest()
        with redirect_stdout(io.StringIO()):
            self.assertTrue(run_checks(self.stage, APP, "windows-x64", COMMIT, "release", self.logs))
        (self.stage / "bin/data/config.json").write_text("changed after acceptance")
        archive = self.root / archive_name(APP, "windows-x64")
        result = subprocess.run([sys.executable, str(ROOT / "build/ci/common/archive_package.py"),
            "--stage", str(self.stage), "--product", APP, "--platform", "windows-x64",
            "--revision", COMMIT, "--smoke-report", str(self.logs / "acceptance.json"),
            "--output", str(archive)], capture_output=True, text=True, timeout=30)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("content changed after acceptance", result.stderr)
        self.assertFalse(archive.exists())


if __name__ == "__main__":
    unittest.main()
