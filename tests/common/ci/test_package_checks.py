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
from package_contract import PLATFORMS, manifest_path, validate_evidence, validate_manifest, required_checks, required_files, installed_required_files
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
        self.contract["required_files"].append("probe.py")
        for check in self.contract["checks"]:
            check["command"] = ["@PYTHON@", "@PACKAGE_ROOT@/probe.py", "a b; $(not-a-shell)",
                                "@PROFILE@", "full", "@DRIVER@"]
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

    def test_real_argv_environment_isolation_and_gpu_merge(self):
        self.assertTrue(self.run_checks())
        partial = json.loads((self.logs / "acceptance.json").read_text())
        with self.assertRaises(ValueError):
            validate_evidence(partial, self.contract, COMMIT, "release")
        self.assertTrue(self.run_checks(gpu=True, driver="vulkan"))
        report = json.loads((self.logs / "acceptance.json").read_text())
        validate_evidence(report, self.contract, COMMIT, "release")
        self.assertIn("a b; $(not-a-shell)", (self.logs / f"{APP}.startup.log").read_text())
        self.assertIn("'release', 'full', 'vulkan'", (self.logs / f"{APP}.render.log").read_text())

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
        self.assertIn("started", (self.logs / f"{APP}.startup.log").read_text())
        self.assertIn("timed out", (self.logs / f"{APP}.startup.log").read_text())

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

    def test_generic_package_rejects_catalog_the_runtime_cannot_load(self):
        assets = self.stage / "bin/assets" / APP
        assets.mkdir(parents=True)
        (assets / "content.json").write_text(json.dumps({
            "version": 1, "catalogs": ["catalog.json"], "shader_bundles": []}))
        (assets / "catalog.json").write_text(json.dumps({
            "version": 1, "assets": [{"path": "data.txt"}]}))
        (assets / "data.txt").write_text("present")
        self.contract["required_files"].append(f"bin/assets/{APP}/content.json")
        self.contract["checks"] = []
        with self.assertRaises(ValueError):
            installed_required_files(self.stage, self.contract)

    def test_package_content_uses_shared_runtime_contract_fixtures(self):
        cases = json.loads((ROOT / "tests/common/fixtures/asset_contract/cases.json").read_text(encoding="utf-8"))["cases"]
        root = f"bin/assets/{APP}"
        self.contract["required_files"].append(root + "/content.json")
        for case in cases:
            with self.subTest(case=case["name"]):
                documents = {root + "/content.json": case["content"]}
                documents.update((root + "/" + name, catalog) for name, catalog in case["catalogs"].items())
                if isinstance(case["content"], dict):
                    for bundle in case["content"]["shader_bundles"]:
                        documents[root + "/" + bundle["path"] + "/manifest.json"] = {"programs": []}
                if case["valid"]:
                    files = required_files(self.contract, documents.__getitem__)
                    self.assertIn(root + "/content.json", files)
                else:
                    with self.assertRaises(ValueError):
                        required_files(self.contract, documents.__getitem__)

    def write_declared_content(self):
        root = f"bin/assets/{APP}"
        documents = {
            root + "/content.json": {"version": 1, "catalogs": ["catalog.json"],
                "shader_bundles": [{"name": "builtin", "path": "shaders/builtin"}]},
            root + "/catalog.json": {"version": 1, "assets": [
                {"id": "data", "type": "custom", "path": "nested/data.bin"}]},
            root + "/shaders/builtin/manifest.json": {"programs": [{"variants": [
                {"vertex": {"file": "dxil/vertex.bin"}, "fragment": {"file": "dxil/fragment.bin"}}]}]},
        }
        for relative, document in documents.items():
            path = self.stage / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps(document), encoding="utf-8")
        for relative in ("nested/data.bin", "shaders/builtin/dxil/vertex.bin", "shaders/builtin/dxil/fragment.bin"):
            path = self.stage / root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"declared artifact")
        self.contract["required_files"].append(root + "/content.json")
        self.contract["checks"] = []
        return self.stage / root

    def test_declared_catalog_and_shader_inventory_is_accepted(self):
        self.write_declared_content()
        self.assertTrue(self.run_checks())

    def test_undeclared_programs_nested_in_product_assets_are_rejected(self):
        assets = self.write_declared_content()
        for relative in ("nested/rogue.exe", "shaders/builtin/dxil/rogue"):
            path = assets / relative
            path.write_bytes(b"undeclared executable")
            with self.subTest(relative=relative), self.assertRaisesRegex(ValueError, "Unregistered file"):
                self.run_checks()
            path.unlink()

    def test_undeclared_programs_outside_runtime_directories_are_rejected(self):
        for relative in ("rogue", "share/extra/rogue.exe"):
            path = self.stage / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"undeclared executable")
            with self.subTest(relative=relative), self.assertRaisesRegex(ValueError, "Unregistered file"):
                self.run_checks()
            path.unlink()

    def test_runner_and_archiver_cli_produce_a_verifiable_release(self):
        self.contract["platform"] = "windows-x64"
        self.contract["executables"][f"{APP}.main"]["path"] = "bin/sample.exe"
        (self.stage / "bin/sample").unlink()
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
        self.contract["checks"][0]["command"] = ["@PYTHON@", "@PROBE:main@"]
        context = self.write_context(probe=probe)
        self.assertTrue(self.run_checks(context=context))
        self.assertFalse((self.stage / "bin/diagnostic.py").exists())
        self.assertTrue(probe.exists())

    def write_context(self, probe=None):
        self.write_manifest()
        document = {"schema_version": 1, "product": APP, "platform": "linux-x64",
                    "configuration": "Release", "manifest": copy.deepcopy(self.contract),
                    "owners": {APP: {"root": str(self.root), "probes": {}}}}
        if probe:
            document["owners"][APP]["probes"]["main"] = {"path": str(probe), "runtime_files": []}
        path = self.root / "context.json"
        path.write_text(json.dumps(document))
        return path

    def test_context_is_required_for_probe_and_cannot_change_manifest_or_gpu_identity(self):
        self.contract["checks"][0]["command"] = ["@PYTHON@", "@CHECK_ROOT@/external.py"]
        (self.root / "external.py").write_text("print('external check')")
        with self.assertRaisesRegex(ValueError, "context"):
            self.run_checks()
        context = self.write_context()
        self.assertTrue(self.run_checks(context=context))
        with self.assertRaisesRegex(ValueError, "different package/profile"):
            self.run_checks(gpu=True, driver="vulkan")
        document = json.loads(context.read_text())
        document["manifest"]["checks"][0]["timeout"] += 1
        context.write_text(json.dumps(document))
        with self.assertRaisesRegex(ValueError, "context"):
            self.run_checks(context=context)

    def test_legacy_and_unknown_role_tokens_are_rejected(self):
        for token in ("@EXECUTABLE@", "@PROBE@", "@GPU_SUITE@", "@ACCEPTANCE_ROOT@", "@EXECUTABLE:missing@"):
            self.contract["checks"][0]["command"] = [token]
            with self.subTest(token=token), self.assertRaises(ValueError):
                validate_manifest(self.contract, APP, "linux-x64")

    def test_foreign_platform_roles_do_not_require_current_platform_targets(self):
        check = copy.deepcopy(self.contract["checks"][0])
        check.update(name="foreign_target", platforms=["windows-x64"], command=["@EXECUTABLE:windows_only@"])
        self.contract["checks"].append(check)
        validate_manifest(self.contract, APP, "linux-x64")
        check["platforms"] = ["linux-x64"]
        with self.assertRaisesRegex(ValueError, "Unregistered executable role"):
            validate_manifest(self.contract, APP, "linux-x64")
        check.update(platforms=["windows-x64"], command=["@EXECUTABLE@"])
        with self.assertRaisesRegex(ValueError, "Unsupported acceptance token"):
            validate_manifest(self.contract, APP, "linux-x64")

    def test_explicit_context_is_checked_even_when_checks_do_not_need_it(self):
        for field, value in (("product", "other"), ("platform", "macos-arm64"),
                             ("configuration", ""), ("owners", {"unregistered": {}})):
            context = self.write_context()
            document = json.loads(context.read_text())
            document[field] = value
            context.write_text(json.dumps(document))
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "context"):
                self.run_checks(context=context)

    def test_probe_cannot_replace_a_product_even_when_its_bytes_match(self):
        probe = self.root / "sample"
        probe.write_bytes((self.stage / "bin/sample").read_bytes())
        self.contract["checks"] = [self.contract["checks"][0]]
        self.contract["checks"][0]["command"] = ["@PYTHON@", "@PROBE:main@"]
        with self.assertRaisesRegex(ValueError, "overwrite"):
            self.run_checks(context=self.write_context(probe=probe))

    def test_probe_role_missing_from_context_is_not_guessed(self):
        self.contract["checks"] = [self.contract["checks"][0]]
        self.contract["checks"][0]["command"] = ["@PROBE:main@"]
        with self.assertRaisesRegex(ValueError, "no requested probe role"):
            self.run_checks(context=self.write_context())

    def test_probe_dependencies_can_reuse_equal_product_libraries_only(self):
        probe = self.root / "unconventional.py"
        probe.write_text("print('probe')")
        dependency = self.root / "native.dll"
        dependency.write_text("library")
        (self.stage / "bin/native.dll").write_text("library")
        self.contract["native_files"] = ["bin/native.dll"]
        self.contract["checks"] = [self.contract["checks"][0]]
        self.contract["checks"][0]["command"] = ["@PYTHON@", "@PROBE:main@"]
        context = self.write_context(probe=probe)
        document = json.loads(context.read_text())
        document["owners"][APP]["probes"]["main"]["runtime_files"] = [str(dependency)]
        context.write_text(json.dumps(document))
        self.assertTrue(self.run_checks(context=context))
        dependency.write_text("different library")
        with self.assertRaisesRegex(ValueError, "overwrite"):
            self.run_checks(context=context)

    def test_two_tools_same_roles_and_check_names_do_not_depend_on_order(self):
        product = "toolchain"
        self.contract.update(product=product, kind=product)
        executables, checks = {}, []
        for owner in ("first", "second"):
            (self.stage / "bin" / f"{owner}.py").write_text("print('" + owner + "')")
            executables[f"{owner}.main"] = dict(owner=owner, role="main", path=f"bin/{owner}.py", runtime_dependencies=[])
            check = copy.deepcopy(self.contract["checks"][0])
            check.update(owner=owner, command=["@PYTHON@", "@EXECUTABLE:main@"])
            checks.append(check)
        self.contract.update(executables=executables, checks=checks)
        (self.stage / "bin/sample").unlink()
        old_manifest = self.stage / manifest_path(APP)
        old_manifest.unlink()
        old_manifest.parent.rmdir()
        path = self.stage / manifest_path(product)
        path.parent.mkdir(parents=True)
        for reverse in (False, True):
            if reverse:
                self.contract["executables"] = dict(reversed(list(executables.items())))
                self.contract["checks"] = list(reversed(checks))
            path.write_text(json.dumps(self.contract))
            with self.subTest(reverse=reverse), redirect_stdout(io.StringIO()):
                self.assertTrue(run_checks(self.stage, product, "linux-x64", COMMIT, "release", self.logs))
            report = json.loads((self.logs / "acceptance.json").read_text())
            validate_evidence(report, self.contract, COMMIT, "release")
            for owner in ("first", "second"):
                self.assertEqual((self.logs / f"{owner}.startup.log").read_text().strip(), owner)

    def test_context_configuration_change_cannot_merge_cpu_and_gpu_evidence(self):
        context = self.write_context()
        self.assertTrue(self.run_checks(context=context))
        document = json.loads(context.read_text())
        document["configuration"] = "Debug"
        context.write_text(json.dumps(document))
        with self.assertRaisesRegex(ValueError, "context does not match"):
            self.run_checks(gpu=True, driver="vulkan", context=context)

    def test_context_configuration_mismatch_is_rejected_before_any_cpu_check(self):
        context = self.write_context()
        document = json.loads(context.read_text())
        document["configuration"] = "Debug"
        context.write_text(json.dumps(document))
        with self.assertRaisesRegex(ValueError, "context does not match"):
            self.run_checks(context=context)
        self.assertFalse((self.logs / "acceptance.json").exists())

    def test_manifest_configuration_is_required_and_nonempty(self):
        for configuration in (None, "", "  ", False):
            invalid = dict(self.contract, configuration=configuration)
            with self.subTest(configuration=configuration), self.assertRaisesRegex(ValueError, "configuration"):
                validate_manifest(invalid, APP, "linux-x64")
        invalid = dict(self.contract)
        invalid.pop("configuration")
        with self.assertRaisesRegex(ValueError, "configuration"):
            validate_manifest(invalid, APP, "linux-x64")

    def test_two_probe_roles_with_the_same_filename_are_rejected(self):
        self.contract["checks"] = [self.contract["checks"][0]]
        self.contract["checks"][0]["command"] = ["@PYTHON@", "@PROBE:main@", "@PROBE:other@"]
        first, second = self.root / "one/diagnostic.py", self.root / "two/diagnostic.py"
        for path in (first, second):
            path.parent.mkdir()
            path.write_text("print('equal bytes')")
        context = self.write_context(probe=first)
        document = json.loads(context.read_text())
        document["owners"][APP]["probes"]["other"] = {"path": str(second), "runtime_files": []}
        context.write_text(json.dumps(document))
        with self.assertRaisesRegex(ValueError, "overwrite another probe"):
            self.run_checks(context=context)

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
