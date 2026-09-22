"""Installed diagnostics must follow the manifest, including for unfamiliar apps."""

from contextlib import nullcontext, redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import uuid


OWNER = Path(__file__).resolve().parents[1].name
SCRIPTS = Path(__file__).resolve().parents[3] / "build/acceptance" / OWNER
sys.path.insert(0, str(SCRIPTS))
import manual_gpu_smoke as gpu
from package_info import load_package_info
from package_contract import PLATFORMS, validate_manifest, required_checks
import validate_content as content


class InstalledPackageTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.gettempdir()).resolve() / f"gyo-app-package-test-{uuid.uuid4().hex}"
        self.root.mkdir()
        self.stage = self.root / "installed package"
        self.app = "planet_builder"
        self.relative_executable = "bin/Planet Builder.exe"
        self.manifest_path = self.stage / "share/gyo/products" / self.app / "manifest.json"
        self.manifest_path.parent.mkdir(parents=True)
        self.manifest = {"schema_version": 3, "configuration":"Release", "product": self.app, "kind":"app",
                         "platform":"windows-x64", "executables": {self.app + ".main": {
                             "owner":self.app, "role":"main", "path":self.relative_executable,
                             "runtime_dependencies":[]}},
                         "required_files":[], "native_files":[], "checks":[]}
        executable = self.stage / self.relative_executable
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"executable test fixture")
        self.write_manifest()

    def tearDown(self):
        self.assertEqual(self.root.parent, Path(tempfile.gettempdir()).resolve())
        self.assertTrue(self.root.name.startswith("gyo-app-package-test-"))
        shutil.rmtree(self.root)

    def write_manifest(self):
        self.manifest_path.write_text(json.dumps(self.manifest), encoding="utf-8-sig")

    def test_repository_acceptance_contract_is_valid_and_selects_explicit_platforms(self):
        contract = json.loads((SCRIPTS / "checks.json").read_text(encoding="utf-8"))
        self.assertEqual(contract["version"], 1)
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                package = {**self.manifest, "platform":platform,
                           "checks":[dict(check, owner=self.app) for check in contract["checks"]]}
                validate_manifest(package, self.app, platform)
                self.assertEqual({check["name"] for check in required_checks(package, "quick")}, {"startup"})
                expected = {"startup", "gameplay", "content"}
                if platform == "linux-x64":
                    expected.add("rendering")
                self.assertEqual({check["name"] for check in required_checks(package, "release")}, expected)

    def test_discovers_unfamiliar_app_and_actual_executable_name(self):
        result = load_package_info(self.stage)
        self.assertEqual(result["product"], self.app)
        self.assertEqual(result["executables"][self.app + ".main"]["path"], self.relative_executable)

    def test_external_clis_run_from_an_unrelated_directory(self):
        for script in ("manual_gpu_smoke.py", "validate_content.py"):
            with self.subTest(script=script):
                result = subprocess.run([sys.executable, "-I", str(SCRIPTS / script), "--help"],
                                        cwd=self.root, capture_output=True, text=True, timeout=15)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_copied_support_works_without_the_original_owner(self):
        repository = Path(__file__).resolve().parents[3]
        copied = self.root / "independent source copy"
        owner = "variant_clone"
        copied_tests = copied / "tests" / owner / "package_tools"
        copied_acceptance = copied / "build/acceptance" / owner
        ignore = shutil.ignore_patterns("__pycache__", "*.pyc")
        shutil.copytree(Path(__file__).resolve().parent, copied_tests, ignore=ignore)
        shutil.copytree(SCRIPTS, copied_acceptance, ignore=ignore)
        shutil.copytree(repository / "build/acceptance/common", copied / "build/acceptance/common", ignore=ignore)
        for source in (repository / "build").glob("*.py"):
            shutil.copy2(source, copied / "build" / source.name)
        self.assertFalse((copied / "tests" / OWNER).exists())
        self.assertFalse((copied / "build/acceptance" / OWNER).exists())

        # The child cannot see this checkout on sys.path, and its explicit test
        # list excludes this copy test so it cannot recurse into itself.
        command = (
            "import json, pathlib, sys, unittest; "
            "sys.path.insert(0, sys.argv[1]); "
            "import test_package_info as checks; "
            "assert checks.OWNER == sys.argv[2]; "
            "assert checks.SCRIPTS == pathlib.Path(sys.argv[3]); "
            "assert pathlib.Path(checks.content.__file__).parent == checks.SCRIPTS; "
            "contract = json.loads((checks.SCRIPTS / 'checks.json').read_text(encoding='utf-8')); "
            "assert all('/' + sys.argv[4] + '/' not in arg for check in contract['checks'] for arg in check['command']); "
            "names = ['test_gpu_smoke', "
            "'test_package_info.InstalledPackageTests.test_repository_acceptance_contract_is_valid_and_selects_explicit_platforms', "
            "'test_package_info.InstalledPackageTests.test_discovers_unfamiliar_app_and_actual_executable_name', "
            "'test_package_info.InstalledPackageTests.test_external_clis_run_from_an_unrelated_directory', "
            "'test_package_info.InstalledPackageTests.test_content_checks_follow_manifest_and_preserve_original_install']; "
            "suite = unittest.defaultTestLoader.loadTestsFromNames(names); "
            "result = unittest.TextTestRunner(verbosity=2).run(suite); "
            "sys.exit(0 if result.wasSuccessful() else 1)"
        )
        result = subprocess.run([sys.executable, "-I", "-c", command, str(copied_tests), owner,
                                 str(copied_acceptance), OWNER], cwd=self.root,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_and_ambiguous_manifest_fail(self):
        self.manifest_path.unlink()
        with self.assertRaisesRegex(ValueError, "exactly one.*0"):
            load_package_info(self.stage)
        self.write_manifest()
        other = self.stage / "share/gyo/products/another_game/manifest.json"
        other.parent.mkdir()
        other.write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "exactly one.*2"):
            load_package_info(self.stage)

    def test_rejects_wrong_schema_or_directory_identity(self):
        for replacement in ({"schema_version": True}, {"schema_version": 1},
                            {"product": "other_app"}, {"product": "../escape"}, {"product": None}):
            with self.subTest(replacement=replacement):
                original = self.manifest.copy()
                self.manifest.update(replacement)
                self.write_manifest()
                with self.assertRaises(ValueError):
                    load_package_info(self.stage)
                self.manifest = original

    def test_rejects_unsafe_or_non_normalized_executable_paths(self):
        for executable in ("", ".", "../outside", "bin/../outside", "/outside", "C:/outside",
                           "bin\\game.exe", "bin//game.exe", "./bin/game.exe", "bin/game\n.exe",
                           None, ["bin/game.exe"]):
            with self.subTest(executable=executable):
                self.manifest["executables"][self.app + ".main"]["path"] = executable
                self.write_manifest()
                with self.assertRaisesRegex(ValueError, "normalized content-relative path"):
                    load_package_info(self.stage)

    def test_rejects_missing_executable_and_directory(self):
        for executable in ("bin/missing.exe", "bin"):
            with self.subTest(executable=executable):
                self.manifest["executables"][self.app + ".main"]["path"] = executable
                self.write_manifest()
                with self.assertRaises(ValueError):
                    load_package_info(self.stage)

    def test_rejects_resolved_executable_outside_package(self):
        # Exercise the containment branch without requiring Windows symlink privileges.
        real_resolve = Path.resolve
        target = self.stage / self.relative_executable

        def resolve(path, strict=False):
            return self.root / "outside.exe" if path == target else real_resolve(path, strict=strict)

        with patch.object(Path, "resolve", resolve):
            with self.assertRaisesRegex(ValueError, "regular file inside"):
                load_package_info(self.stage)

    def test_gpu_cli_uses_manifest_executable_as_one_argument(self):
        output = self.root / "gpu logs"
        work = self.root / "unrelated cwd"
        results = [{"passed": True, "case": "viewmodel"}]
        with patch.object(gpu, "run_suite", return_value=results) as run_suite:
            result = gpu.main(["--package", str(self.stage), "--probe", str(self.stage / self.relative_executable), "--driver", "vulkan",
                               "--output", str(output), "--suite", "quick",
                               "--work-directory", str(work)])
        self.assertEqual(result, 0)
        self.assertEqual(run_suite.call_args.args[0], [str(self.stage / self.relative_executable)])
        self.assertEqual(run_suite.call_args.args[1], work)
        summary = json.loads((output / "summary.json").read_text(encoding="utf-8"))
        self.assertEqual(summary["app"], self.app)
        self.assertEqual(summary["cases"], results)

    def test_gpu_cli_rejects_ambiguous_package_before_diagnostics(self):
        other = self.stage / "share/gyo/products/another_game/manifest.json"
        other.parent.mkdir()
        other.write_text("{}", encoding="utf-8")
        with patch.object(gpu, "run_suite") as run_suite, redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as failure:
                gpu.main(["--package", str(self.stage), "--probe", str(self.stage / self.relative_executable), "--driver", "vulkan",
                          "--output", str(self.root / "logs")])
        self.assertEqual(failure.exception.code, 2)
        run_suite.assert_not_called()

    def prepare_content(self):
        for _, relative in content.missing_content(self.app):
            target = self.stage / relative
            if target.suffix:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text("{}", encoding="utf-8")
            else:
                target.mkdir(parents=True, exist_ok=True)
                (target / "asset_catalog.json").write_text("{}", encoding="utf-8")
        isolated = self.root / "isolated"
        isolated.mkdir()
        return isolated

    def test_content_checks_follow_manifest_and_preserve_original_install(self):
        isolated = self.prepare_content()
        seen_missing = []

        def run(command, *, cwd, **kwargs):
            package = isolated / "package"
            self.assertEqual(command, [str(package / self.relative_executable), "--validate-package"])
            self.assertEqual(cwd, isolated / "unrelated-working-directory")
            absent = [name for name, relative in content.missing_content(self.app)
                      if not (package / relative).exists()]
            seen_missing.append(absent)
            return subprocess.CompletedProcess(command, 1 if absent else 0, stdout="validation result")

        with patch.object(content, "TemporaryDirectory", return_value=nullcontext(str(isolated))), \
                patch.object(content.subprocess, "run", side_effect=run), redirect_stdout(io.StringIO()):
            content.validate(self.stage, self.root / "content logs", self.stage / self.relative_executable)
        self.assertEqual(seen_missing, [[], ["missing-common"], ["missing-builtin-shaders"]])
        for _, relative in content.missing_content(self.app):
            self.assertTrue((self.stage / relative).exists())
            self.assertTrue((isolated / "package" / relative).exists())
        self.assertEqual(len(list((self.root / "content logs").glob("*.log"))), 3)

    def test_content_rejects_false_success_and_restores_hidden_content(self):
        isolated = self.prepare_content()
        with patch.object(content, "TemporaryDirectory", return_value=nullcontext(str(isolated))), \
                patch.object(content.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, stdout="ok")), \
                redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(RuntimeError, "missing-common"):
                content.validate(self.stage, self.root / "content logs", self.stage / self.relative_executable)
        self.assertTrue((isolated / f"package/bin/assets/{self.app}/textures/common/white1x1.png").is_file())
        self.assertTrue((self.stage / f"bin/assets/{self.app}/textures/common/white1x1.png").is_file())


if __name__ == "__main__":
    unittest.main()
