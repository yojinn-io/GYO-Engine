"""Recorded Go module checks and service builds, on synthetic modules only."""

import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import sys
import tarfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
for folder in ("build", "build/ci/common", "build/acceptance/common"):
    sys.path.insert(0, str(ROOT / folder))
from workspace import TemporaryDirectory
import go_checks
from go_checks import GoCheckError, Module, Service
from release_pipeline import MANAGED_PACKAGE_ASSET
from release_support import ReleaseError, expected_asset_names, load_packages

COMMIT = "c" * 40


class SyntheticRepository:
    """An engine-layer module and two product modules, one declaring a service."""

    def __init__(self, root: Path):
        self.root = root
        self.module("shared", "go 1.21\n")
        self.module("apps/alpha", "go 1.22\n", go_sum=True)
        self.module("apps/beta", "go 1.21\n\ntoolchain go1.23.4\n")

    def module(self, path: str, directives: str, go_sum=False):
        directory = self.root / path
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "go.mod").write_text(f"module example.invalid/{directory.name}\n\n{directives}", encoding="utf-8")
        if go_sum:
            (directory / "go.sum").write_text("", encoding="utf-8")

    def record(self, **changes) -> Path:
        document = {
            "configuration": "Release", "shader_bundles": False, "shader_host_tools": False,
            "go_modules": [{"owner": "engine", "path": "shared"}, {"owner": "alpha", "path": "apps/alpha"},
                           {"owner": "beta", "path": "apps/beta"}],
            "go_services": [{"owner": "alpha", "role": "relay", "module": "apps/alpha", "package": "./cmd/relay",
                             "platforms": ["linux-x64", "windows-x64"]}],
        }
        document.update(changes)
        path = self.root / "gyo-build.json"
        path.write_text(json.dumps(document), encoding="utf-8")
        return path


class GoRecordTests(unittest.TestCase):
    def setUp(self):
        self.workspace = TemporaryDirectory(prefix="gyo-go-checks-")
        self.addCleanup(self.workspace.cleanup)
        self.repository = SyntheticRepository(Path(self.workspace.name))

    def load(self, **changes):
        return go_checks.load_record(self.repository.record(**changes), self.repository.root)

    def test_record_lists_modules_and_services_in_configure_order(self):
        modules, services = self.load()
        self.assertEqual(modules, [Module("engine", "shared"), Module("alpha", "apps/alpha"), Module("beta", "apps/beta")])
        self.assertEqual(services, [Service("alpha", "relay", "apps/alpha", "./cmd/relay", ("linux-x64", "windows-x64"))])
        self.assertEqual(services[0].name, "gyo_alpha-relay")

    def test_stale_or_invalid_records_fail_instead_of_skipping_go(self):
        build_info = self.repository.record()
        document = json.loads(build_info.read_text(encoding="utf-8"))
        del document["go_modules"]
        build_info.write_text(json.dumps(document), encoding="utf-8")
        with self.assertRaisesRegex(GoCheckError, "reconfigure"):
            go_checks.load_record(build_info, self.repository.root)
        service = {"owner": "alpha", "role": "relay", "module": "apps/alpha", "package": "./cmd/relay",
                   "platforms": ["linux-x64"]}
        for label, changes in (
            ("owner", {"go_modules": [{"owner": "Alpha", "path": "apps/alpha"}], "go_services": []}),
            ("relative", {"go_modules": [{"owner": "alpha", "path": "../apps/alpha"}], "go_services": []}),
            ("relative", {"go_modules": [{"owner": "alpha", "path": "/apps/alpha"}], "go_services": []}),
            ("go.mod", {"go_modules": [{"owner": "alpha", "path": "apps/missing"}], "go_services": []}),
            ("Duplicate Go module", {"go_modules": [{"owner": "alpha", "path": "apps/alpha"}] * 2, "go_services": []}),
            ("its owner recorded", {"go_services": [dict(service, owner="beta")]}),
            ("module-relative", {"go_services": [dict(service, package="./../escape")]}),
            ("module-relative", {"go_services": [dict(service, package="cmd/relay")]}),
            ("invalid platforms", {"go_services": [dict(service, platforms=[])]}),
            ("invalid platforms", {"go_services": [dict(service, platforms=["linux-x64", "linux-x64"])]}),
            ("invalid platforms", {"go_services": [dict(service, platforms=["linux-arm64"])]}),
            ("Duplicate Go service", {"go_services": [service, service]}),
            ("Invalid Go service record", {"go_services": [dict(service, extra=True)]}),
        ):
            with self.subTest(label=label, changes=changes), self.assertRaisesRegex(GoCheckError, label):
                self.load(**changes)

    def test_rows_select_engine_layer_or_their_own_product(self):
        modules, services = self.load()
        self.assertEqual(go_checks.select(modules, services, None), (modules, services))
        self.assertEqual(go_checks.select(modules, services, "toolchain"), ([Module("engine", "shared")], []))
        self.assertEqual(go_checks.select(modules, services, "alpha"), ([Module("alpha", "apps/alpha")], services))
        self.assertEqual(go_checks.select(modules, services, "gamma"), ([], []))
        with self.assertRaises(ValueError):
            go_checks.select(modules, services, "../alpha")

    def test_engine_layer_checks_need_no_registered_game(self):
        modules, services = self.load(go_modules=[{"owner": "engine", "path": "shared"}], go_services=[])
        outputs = go_checks.plan(*go_checks.select(modules, services, "toolchain"), self.repository.root)
        self.assertEqual(outputs, {"has_go_modules": "true", "has_go_services": "false",
                                   "go_version_file": "shared/go.mod", "go_dependency_files": "shared/go.mod"})

    def test_plan_selects_the_newest_declared_go_and_every_dependency_file(self):
        outputs = go_checks.plan(*self.load(), self.repository.root)
        # beta's toolchain directive (1.23.4) outranks alpha's go line (1.22).
        self.assertEqual(outputs["go_version_file"], "apps/beta/go.mod")
        self.assertEqual(outputs["go_dependency_files"].splitlines(),
                         ["apps/alpha/go.mod", "apps/alpha/go.sum", "apps/beta/go.mod", "shared/go.mod"])
        self.assertEqual((outputs["has_go_modules"], outputs["has_go_services"]), ("true", "true"))
        nothing = go_checks.plan(*go_checks.select(*self.load(), "gamma"), self.repository.root)
        self.assertEqual(nothing, {"has_go_modules": "false", "has_go_services": "false",
                                   "go_version_file": "", "go_dependency_files": ""})
        with self.assertRaisesRegex(GoCheckError, "no go version"):
            go_checks.declared_version("module example.invalid/none\n")

    def test_plan_cli_writes_multiline_github_outputs(self):
        output = self.repository.root / "github-output"
        status = go_checks.main(["plan", "--build-info", str(self.repository.record()), "--product", "alpha",
                                 "--repository", str(self.repository.root), "--output", str(output)])
        self.assertEqual(status, 0)
        delimiter = go_checks.DEPENDENCY_DELIMITER
        self.assertEqual(output.read_text(encoding="utf-8"),
                         "has_go_modules=true\nhas_go_services=true\ngo_version_file=apps/alpha/go.mod\n"
                         f"go_dependency_files<<{delimiter}\napps/alpha/go.mod\napps/alpha/go.sum\n{delimiter}\n")


class GoCheckTests(unittest.TestCase):
    def test_every_step_runs_per_module_and_failures_do_not_stop_later_checks(self):
        calls = []
        def run(command, cwd, env):
            calls.append((command[1:], Path(cwd).name, env["GOWORK"], env.get("CGO_ENABLED")))
            return 1 if (Path(cwd).name, command[1]) == ("first", "vet") else 0
        failures = go_checks.check([Path("first"), Path("second")], ["vet", "test", "race"], "go-bin", run)
        self.assertEqual(failures, [f"{Path('first')}: go vet"])
        self.assertEqual(calls, [
            (["vet", "./..."], "first", "off", os.environ.get("CGO_ENABLED")),
            (["test", "-count=1", "./..."], "first", "off", os.environ.get("CGO_ENABLED")),
            (["test", "-race", "-count=1", "./..."], "first", "off", "1"),
            (["vet", "./..."], "second", "off", os.environ.get("CGO_ENABLED")),
            (["test", "-count=1", "./..."], "second", "off", os.environ.get("CGO_ENABLED")),
            (["test", "-race", "-count=1", "./..."], "second", "off", "1"),
        ])
        with self.assertRaisesRegex(GoCheckError, "No Go module"):
            go_checks.check([], ["vet"], run=run)

    def test_a_ctest_module_check_cannot_mix_with_a_record_selection(self):
        self.assertEqual(go_checks.main(["check", "--module", ".", "--product", "alpha"]), 1)
        self.assertEqual(go_checks.main(["check"]), 1)


class GoServiceBuildTests(unittest.TestCase):
    def setUp(self):
        self.workspace = TemporaryDirectory(prefix="gyo-go-services-")
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)
        (self.root / "apps/alpha").mkdir(parents=True)
        self.service = Service("alpha", "relay", "apps/alpha", "./cmd/relay", ("linux-x64", "windows-x64"))
        self.calls = []

    def fake_run(self, fail_platform=None):
        def run(command, cwd, env):
            self.calls.append((command, Path(cwd), {key: env[key] for key in ("GOWORK", "CGO_ENABLED", "GOOS", "GOARCH")}))
            if (env["GOOS"], env["GOARCH"]) == fail_platform:
                return 2
            output = Path(command[command.index("-o") + 1])
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(f"binary {env['GOOS']}/{env['GOARCH']}".encode())
            return 0
        return run

    def build(self, **options):
        return go_checks.build([self.service], self.root / "out", repository=self.root, go="go-bin",
                               version=lambda go: "go1.23.4", **options)

    def test_cross_builds_are_cgo_free_and_staged_under_the_host_target_name(self):
        produced = self.build(run=self.fake_run())
        self.assertEqual([path.name for path in produced], ["gyo_alpha-relay", "gyo_alpha-relay.exe"])
        self.assertEqual([call[2] for call in self.calls], [
            {"GOWORK": "off", "CGO_ENABLED": "0", "GOOS": "linux", "GOARCH": "amd64"},
            {"GOWORK": "off", "CGO_ENABLED": "0", "GOOS": "windows", "GOARCH": "amd64"},
        ])
        for command, cwd, _ in self.calls:
            self.assertEqual(cwd, self.root / "apps/alpha")
            self.assertEqual(command[:4], ["go-bin", "build", "-trimpath", "-buildvcs=false"])
            self.assertEqual(command[-1], "./cmd/relay")
        self.assertEqual(sorted(path.name for path in (self.root / "out").iterdir()), ["stage"])

    def test_archives_carry_provenance_and_a_matching_checksum(self):
        produced = self.build(run=self.fake_run(), revision=COMMIT)
        names = ["gyo-alpha-relay-linux-x64.tar.gz", "gyo-alpha-relay-windows-x64.tar.gz"]
        self.assertEqual([path.name for path in produced], names)
        for archive, platform, executable in zip(produced, ("linux-x64", "windows-x64"),
                                                 ("gyo_alpha-relay", "gyo_alpha-relay.exe")):
            with self.subTest(platform=platform):
                data = archive.read_bytes()
                checksum = archive.with_name(archive.name + ".sha256").read_text(encoding="ascii")
                self.assertEqual(checksum, f"{hashlib.sha256(data).hexdigest()}  {archive.name}\n")
                with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as contents:
                    members = {member.name: member for member in contents}
                    self.assertEqual(sorted(members), ["gyo-alpha-relay", "gyo-alpha-relay/bin",
                                                       f"gyo-alpha-relay/bin/{executable}",
                                                       "gyo-alpha-relay/build_metadata.json"])
                    self.assertEqual(members[f"gyo-alpha-relay/bin/{executable}"].mode & 0o777, 0o755)
                    metadata = json.loads(contents.extractfile("gyo-alpha-relay/build_metadata.json").read())
                goos, goarch, _ = go_checks.GO_TARGETS[platform]
                self.assertEqual(metadata, {
                    "source_revision": COMMIT, "owner": "alpha", "role": "relay", "platform": platform,
                    "goos": goos, "goarch": goarch, "cgo": False, "go_version": "go1.23.4",
                    "module": "apps/alpha", "package": "./cmd/relay"})

    def test_service_archives_never_enter_release_package_verification(self):
        produced = self.build(run=self.fake_run(), revision=COMMIT)
        names = [name for path in produced for name in (path.name, path.name + ".sha256")]
        for name in names:
            self.assertIsNone(MANAGED_PACKAGE_ASSET.fullmatch(name), name)
        # The verified release set is still exactly the expected packages: a
        # service archive in the download directory is rejected, never adopted.
        self.assertTrue(set(names).isdisjoint(expected_asset_names([("alpha", "linux-x64"), ("toolchain", "linux-x64")])))
        with self.assertRaisesRegex(ReleaseError, "extra="):
            load_packages(self.root / "out", COMMIT, [("toolchain", "linux-x64")])

    def test_every_failed_platform_fails_the_build_after_trying_the_rest(self):
        with self.assertRaisesRegex(GoCheckError, "alpha/relay for linux-x64"):
            self.build(run=self.fake_run(fail_platform=("linux", "amd64")), revision=COMMIT)
        self.assertEqual(len(self.calls), 2)
        self.assertTrue((self.root / "out/gyo-alpha-relay-windows-x64.tar.gz").is_file())
        with self.assertRaisesRegex(GoCheckError, "full lowercase"):
            self.build(run=self.fake_run(), revision="HEAD")
        with self.assertRaisesRegex(GoCheckError, "No Go service"):
            go_checks.build([], self.root / "out", run=self.fake_run())


@unittest.skipUnless(shutil.which("go"), "Go is not installed")
class RealGoTests(unittest.TestCase):
    """The real command lines against a tiny synthetic module (Go 1.21 syntax)."""

    def setUp(self):
        self.workspace = TemporaryDirectory(prefix="gyo-real-go-")
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)
        module = self.root / "apps/alpha"
        (module / "cmd/relay").mkdir(parents=True)
        (module / "go.mod").write_text("module example.invalid/alpha\n\ngo 1.21\n", encoding="utf-8")
        (module / "cmd/relay/main.go").write_text("package main\n\nfunc main() { println(answer()) }\n", encoding="utf-8")
        (module / "cmd/relay/answer.go").write_text("package main\n\nfunc answer() int { return 42 }\n", encoding="utf-8")
        (module / "cmd/relay/answer_test.go").write_text(
            'package main\n\nimport "testing"\n\nfunc TestAnswer(t *testing.T) {\n'
            '\tif answer() != 42 {\n\t\tt.Fatal("wrong answer")\n\t}\n}\n', encoding="utf-8")

    def test_vet_test_and_a_cgo_free_archive(self):
        self.assertEqual(go_checks.check([self.root / "apps/alpha"], ["vet", "test"]), [])
        service = Service("alpha", "relay", "apps/alpha", "./cmd/relay", ("linux-x64",))
        archive, = go_checks.build([service], self.root / "out", repository=self.root, revision=COMMIT)
        with tarfile.open(archive) as contents:
            metadata = json.loads(contents.extractfile("gyo-alpha-relay/build_metadata.json").read())
            binary = contents.extractfile("gyo-alpha-relay/bin/gyo_alpha-relay").read()
        self.assertTrue(metadata["go_version"].startswith("go"))
        self.assertEqual(binary[:4], b"\x7fELF")
        (self.root / "apps/alpha/cmd/relay/answer.go").write_text("package main\n\nfunc answer() int { return 0 }\n",
                                                                  encoding="utf-8")
        self.assertEqual(go_checks.check([self.root / "apps/alpha"], ["test"]), [f"{self.root / 'apps/alpha'}: go test"])


if __name__ == "__main__":
    unittest.main()
