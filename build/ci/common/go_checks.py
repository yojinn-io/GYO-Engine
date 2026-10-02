#!/usr/bin/env python3
"""Verify recorded Go modules and cross-build recorded Go services.

CMake writes every Go module and service of one configured build into
gyo-build.json: the engine-layer module always, product modules and services
only when their owner is selected. This script never discovers modules and
knows no product: a workflow row selects owners by its configured product.

  plan    GitHub outputs that decide whether a row provisions Go at all
  check   go vet / go test / go test -race for each selected module
  build   CGO-free cross-builds of each selected service for its declared
          release platforms, optionally archived with a SHA-256 file
"""

import argparse
from dataclasses import dataclass
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tarfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "acceptance/common"))

from package_contract import PLATFORMS, validate_product


ROOT = Path(__file__).resolve().parents[3]
# Reserved owner of engine-layer modules; the toolchain row verifies them.
ENGINE_OWNER = "engine"
TOOLCHAIN_PRODUCT = "toolchain"
# Release package platform -> Go target. Services are pure Go (CGO_ENABLED=0).
GO_TARGETS = {
    "windows-x64": ("windows", "amd64", ".exe"),
    "linux-x64": ("linux", "amd64", ""),
    "macos-arm64": ("darwin", "arm64", ""),
    "macos-x64": ("darwin", "amd64", ""),
}
STEPS = {
    "vet": ("vet", "./..."),
    "test": ("test", "-count=1", "./..."),
    # The race detector requires cgo; CI runs it only on Linux.
    "race": ("test", "-race", "-count=1", "./..."),
}
DEPENDENCY_DELIMITER = "GYO_GO_DEPENDENCY_FILES_EOF"
# Per go command; the CTest TIMEOUT in GyoGo.cmake must stay above it.
COMMAND_TIMEOUT = 900

assert set(GO_TARGETS) == set(PLATFORMS)


class GoCheckError(ValueError):
    """The record or a requested selection is invalid; nothing may be skipped."""


@dataclass(frozen=True)
class Module:
    owner: str
    path: str


@dataclass(frozen=True)
class Service:
    owner: str
    role: str
    module: str
    package: str
    platforms: tuple[str, ...]

    @property
    def name(self) -> str:
        # Identical to the host target name gyo_<owner>-<role>.
        return f"gyo_{self.owner}-{self.role}"


def _identifier(value, label: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[a-z][a-z0-9_]*", value):
        raise GoCheckError(f"Invalid Go {label}: {value!r}")
    return value


def _module_path(value, repository: Path) -> str:
    path = PurePosixPath(value) if isinstance(value, str) else None
    if (path is None or not value or "\\" in value or path.is_absolute()
            or any(part in ("", ".", "..") for part in value.split("/"))):
        raise GoCheckError(f"Go module path must be repository-relative: {value!r}")
    if not (repository / value / "go.mod").is_file():
        raise GoCheckError(f"Recorded Go module has no go.mod: {value}")
    return value


def load_record(build_info: Path, repository: Path = ROOT) -> tuple[list[Module], list[Service]]:
    """Validate the Go part of gyo-build.json; a missing key is a stale build."""
    try:
        document = json.loads(build_info.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as error:
        raise GoCheckError(f"Unable to read {build_info}: {error}") from error
    if not isinstance(document, dict) or not isinstance(document.get("go_modules"), list) \
            or not isinstance(document.get("go_services"), list):
        raise GoCheckError(f"{build_info} records no go_modules/go_services; reconfigure the build")
    modules = []
    for entry in document["go_modules"]:
        if not isinstance(entry, dict) or set(entry) != {"owner", "path"}:
            raise GoCheckError(f"Invalid Go module record: {entry!r}")
        module = Module(_identifier(entry["owner"], "module owner"), _module_path(entry["path"], repository))
        if any(existing.path == module.path for existing in modules):
            raise GoCheckError(f"Duplicate Go module record: {module.path}")
        modules.append(module)
    owners = {module.path: module.owner for module in modules}
    services = []
    for entry in document["go_services"]:
        if not isinstance(entry, dict) or set(entry) != {"owner", "role", "module", "package", "platforms"}:
            raise GoCheckError(f"Invalid Go service record: {entry!r}")
        owner, role = _identifier(entry["owner"], "service owner"), _identifier(entry["role"], "service role")
        if owners.get(entry["module"]) != owner:
            raise GoCheckError(f"Go service {owner}/{role} does not use a module its owner recorded")
        package = entry["package"]
        if (not isinstance(package, str) or not re.fullmatch(r"\./[A-Za-z0-9_./-]*", package)
                or ".." in package.split("/")):
            raise GoCheckError(f"Go service {owner}/{role} package must be module-relative: {package!r}")
        platforms = entry["platforms"]
        if (not isinstance(platforms, list) or not platforms or len(set(platforms)) != len(platforms)
                or any(platform not in GO_TARGETS for platform in platforms)):
            raise GoCheckError(f"Go service {owner}/{role} declares invalid platforms: {platforms!r}")
        service = Service(owner, role, entry["module"], package, tuple(platforms))
        if any((existing.owner, existing.role) == (owner, role) for existing in services):
            raise GoCheckError(f"Duplicate Go service record: {owner}/{role}")
        services.append(service)
    return modules, services


def selected_owners(product: str | None) -> set[str] | None:
    """None selects every record (the L1 build); a packaging row selects its own."""
    if product is None:
        return None
    validate_product(product)
    return {ENGINE_OWNER} if product == TOOLCHAIN_PRODUCT else {product}


def select(modules, services, product: str | None):
    owners = selected_owners(product)
    if owners is None:
        return list(modules), list(services)
    return ([module for module in modules if module.owner in owners],
            [service for service in services if service.owner in owners])


def declared_version(go_mod: str) -> tuple[int, ...]:
    """The version setup-go installs for a go.mod: its toolchain, else its go line."""
    toolchain = re.search(r"(?m)^toolchain\s+go(\d+(?:\.\d+)*)\S*\s*$", go_mod)
    language = re.search(r"(?m)^go\s+(\d+(?:\.\d+)*)\s*$", go_mod)
    match = toolchain or language
    if match is None:
        raise GoCheckError("go.mod declares no go version")
    return tuple(int(part) for part in match.group(1).split("."))


def plan(modules, services, repository: Path = ROOT) -> dict[str, str]:
    outputs = {
        "has_go_modules": str(bool(modules)).lower(),
        "has_go_services": str(bool(services)).lower(),
        "go_version_file": "",
        "go_dependency_files": "",
    }
    if modules:
        versions = {module.path: declared_version((repository / module.path / "go.mod").read_text(encoding="utf-8"))
                    for module in modules}
        newest = max(modules, key=lambda module: (versions[module.path], module.path))
        outputs["go_version_file"] = f"{newest.path}/go.mod"
        outputs["go_dependency_files"] = "\n".join(
            f"{module.path}/{name}" for module in sorted(modules, key=lambda module: module.path)
            for name in ("go.mod", "go.sum") if (repository / module.path / name).is_file())
    return outputs


def write_outputs(outputs: dict[str, str], stream) -> None:
    for name, value in outputs.items():
        if "\n" in value:
            stream.write(f"{name}<<{DEPENDENCY_DELIMITER}\n{value}\n{DEPENDENCY_DELIMITER}\n")
        else:
            stream.write(f"{name}={value}\n")


def go_environment(**overrides: str) -> dict[str, str]:
    # Every module is verified alone, never through a developer go.work file.
    return dict(os.environ, GOWORK="off", **overrides)


def run_go(command: list[str], cwd: Path, env: dict[str, str]) -> int:
    print(f"== {cwd}: {' '.join(command)}", flush=True)
    try:
        return subprocess.run(command, cwd=cwd, env=env, timeout=COMMAND_TIMEOUT, check=False).returncode
    except subprocess.TimeoutExpired:
        print(f"Timed out after {COMMAND_TIMEOUT}s", flush=True)
        return 1


def check(directories: list[Path], steps: list[str], go: str = "go", run=run_go) -> list[str]:
    """Run every step for every module and return the failures; never stop early."""
    if not directories:
        raise GoCheckError("No Go module is selected for checking")
    failures = []
    for directory in directories:
        for step in steps:
            environment = go_environment(CGO_ENABLED="1") if step == "race" else go_environment()
            if run([go, *STEPS[step]], directory, environment):
                failures.append(f"{directory}: go {step}")
    return failures


def query_go_version(go: str) -> str:
    result = subprocess.run([go, "env", "GOVERSION"], capture_output=True, text=True,
                            env=go_environment(), timeout=60, check=False)
    if result.returncode or not result.stdout.strip():
        raise GoCheckError(f"Unable to query the Go version: {result.stderr.strip()}")
    return result.stdout.strip()


def archive_name(service: Service, platform: str) -> str:
    # Never the release package pattern gyo-<product>-<platform>.tar.gz: an
    # owner cannot contain '-', so release verification ignores these files.
    return f"gyo-{service.owner}-{service.role}-{platform}.tar.gz"


def _archive(stage: Path, executable: str, metadata: dict, output: Path, root: str) -> str:
    def entry(name: str, size: int, mode: int, kind=tarfile.REGTYPE) -> tarfile.TarInfo:
        info = tarfile.TarInfo(name)
        info.size, info.mode, info.type, info.mtime = size, mode, kind, 0
        info.uid = info.gid = 0
        info.uname = info.gname = ""
        return info
    document = (json.dumps(metadata, indent=2, sort_keys=True) + "\n").encode("utf-8")
    binary = (stage / "bin" / executable).read_bytes()
    with tarfile.open(output, "w:gz") as archive:
        archive.addfile(entry(root, 0, 0o755, tarfile.DIRTYPE))
        archive.addfile(entry(f"{root}/bin", 0, 0o755, tarfile.DIRTYPE))
        archive.addfile(entry(f"{root}/bin/{executable}", len(binary), 0o755), io.BytesIO(binary))
        archive.addfile(entry(f"{root}/build_metadata.json", len(document), 0o644), io.BytesIO(document))
    with output.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    output.with_name(output.name + ".sha256").write_text(f"{digest}  {output.name}\n", encoding="ascii")
    return digest


def build(services: list[Service], output: Path, *, repository: Path = ROOT, go: str = "go",
          revision: str | None = None, run=run_go, version=query_go_version) -> list[Path]:
    """Cross-build every declared platform; archive each one when a revision is given."""
    if not services:
        raise GoCheckError("No Go service is selected for building")
    if revision is not None and not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise GoCheckError("Service archive revision must be a full lowercase Git commit SHA")
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    go_version = version(go) if revision is not None else None
    produced, failures = [], []
    for service in services:
        for platform in service.platforms:
            goos, goarch, suffix = GO_TARGETS[platform]
            stage = output / "stage" / f"{service.owner}-{service.role}-{platform}"
            executable = service.name + suffix
            command = [go, "build", "-trimpath", "-buildvcs=false", "-o", str(stage / "bin" / executable), service.package]
            if run(command, repository / service.module, go_environment(CGO_ENABLED="0", GOOS=goos, GOARCH=goarch)):
                failures.append(f"{service.owner}/{service.role} for {platform}")
                continue
            if not (stage / "bin" / executable).is_file():
                failures.append(f"{service.owner}/{service.role} for {platform} produced no executable")
                continue
            if revision is None:
                produced.append(stage / "bin" / executable)
                continue
            metadata = {
                "source_revision": revision, "owner": service.owner, "role": service.role,
                "platform": platform, "goos": goos, "goarch": goarch, "cgo": False,
                "go_version": go_version, "module": service.module, "package": service.package,
            }
            archive = output / archive_name(service, platform)
            digest = _archive(stage, executable, metadata, archive, f"gyo-{service.owner}-{service.role}")
            print(f"Created {archive}\nSHA256 {digest}", flush=True)
            produced.append(archive)
    if failures:
        raise GoCheckError("Go service build failed: " + "; ".join(failures))
    return produced


def main(arguments=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    selection = argparse.ArgumentParser(add_help=False)
    selection.add_argument("--build-info", type=Path, help="Configured gyo-build.json")
    selection.add_argument("--product", help="Packaging row product; toolchain selects engine-layer records")
    selection.add_argument("--repository", type=Path, default=ROOT)
    plan_parser = commands.add_parser("plan", parents=[selection])
    plan_parser.add_argument("--output", type=Path, help="Append GitHub Actions outputs")
    check_parser = commands.add_parser("check", parents=[selection])
    check_parser.add_argument("--module", type=Path, help="Check one module directory instead of a record")
    check_parser.add_argument("--step", action="append", choices=tuple(STEPS))
    check_parser.add_argument("--race", action="store_true", help="Also run the race detector")
    check_parser.add_argument("--go", default="go")
    build_parser = commands.add_parser("build", parents=[selection])
    build_parser.add_argument("--output-dir", type=Path, required=True)
    build_parser.add_argument("--revision", help="Archive each build with this source commit")
    build_parser.add_argument("--go", default="go")
    args = parser.parse_args(arguments)
    try:
        repository = args.repository.resolve()
        if args.command == "check" and args.module is not None:
            if args.build_info is not None or args.product is not None:
                raise GoCheckError("--module cannot be combined with --build-info or --product")
            directories = [args.module.resolve()]
        else:
            if args.build_info is None:
                raise GoCheckError("--build-info is required")
            modules, services = select(*load_record(args.build_info, repository), args.product)
            directories = [repository / module.path for module in modules]
        if args.command == "plan":
            outputs = plan(modules, services, repository)
            if args.output:
                with args.output.open("a", encoding="utf-8") as stream:
                    write_outputs(outputs, stream)
            print(json.dumps(outputs))
        elif args.command == "check":
            steps = list(args.step or ("vet", "test"))
            if args.race and "race" not in steps:
                steps.append("race")
            failures = check(directories, steps, args.go)
            if failures:
                raise GoCheckError("Go checks failed: " + "; ".join(failures))
        else:
            build(services, args.output_dir, repository=repository, go=args.go, revision=args.revision)
    except GoCheckError as error:
        print(f"::error::{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
