#!/usr/bin/env python3
"""Windows release regression for a real, independently editable app copy.

This is a test fixture, not an app creation tool. It preserves its fresh scratch
workspace and logs for diagnosis, and never changes the repository's registry.
Run from an initialized MSVC environment. Existing dependency sources/native
shader tools can be reused without sharing or modifying another build graph.
"""

from __future__ import annotations

import argparse
import copy
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from package_contract import load_manifest, validate_app
from release_support import archive_name, validate_archive, validate_checksum, validate_commit


ROOT = Path(__file__).resolve().parents[3]
ORIGINAL = "object_fps"
ASSET_MARKER = "integration/copy-only.txt"
ASSET_ID = "integration.copy_only"
SHADER_ID = "game/integration/copy_only"
MODEL_ASSET_ID = "object_fps.model.mark23"
BUILD_ROOTS = {"cmake", "collision", "config", "docs", "engine", "framework", "game",
               "input", "model", "platform", "render", "systems", "tests", "text",
               "third_party", "ui"}


def is_build_input(relative: str) -> bool:
    """Bound the fixture to reviewed build inputs, including working-tree edits."""
    path = PurePosixPath(relative)
    if path.is_absolute() or ".." in path.parts or "\\" in relative or ":" in relative:
        raise ValueError(f"Invalid source path: {relative!r}")
    if path.name in {"AGENTS.md", "CMakeUserPresets.json"}:
        return False
    if relative in {"CMakeLists.txt", "CMakePresets.json", "apps/CMakeLists.txt", "tools/CMakeLists.txt"}:
        return True
    if path.parts[0] in BUILD_ROOTS or (len(path.parts) == 1 and path.name.startswith("README")):
        return True
    if path.parts[:2] in (("assets", "common"), ("assets", ORIGINAL)):
        return True
    if path.parts[:2] in (("tools", "shader_pipeline"), ("tools", "editor")):
        return True
    return (path.parts[:2] == ("apps", ORIGINAL)
            and (len(path.parts) < 3 or path.parts[2] not in {"art_source", "tools"}))


def mirror_sources(repository: Path, destination: Path) -> list[str]:
    result = subprocess.run(["git", "-C", str(repository), "ls-files", "-z", "--cached",
                             "--others", "--exclude-standard"], capture_output=True, check=True)
    selected = sorted({name for name in result.stdout.decode("utf-8").split("\0")
                       if name and is_build_input(name)})
    if destination.exists():
        raise ValueError(f"Fixture source directory must be fresh: {destination}")
    destination.mkdir(parents=True)
    copied = []
    for relative in selected:
        source = repository / relative
        if not source.exists():  # A tracked file deleted in the working tree stays deleted.
            continue
        if not source.is_file() or source.is_symlink() or not source.resolve().is_relative_to(repository):
            raise ValueError(f"Build input must be a regular repository file: {relative}")
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        copied.append(relative)
    for required in ("CMakeLists.txt", "cmake/GyoAppProject.cmake",
                     "apps/object_fps/CMakeLists.txt", "assets/object_fps/asset_catalog.json"):
        if required not in copied:
            raise ValueError(f"Required build input is absent: {required}")
    return copied


def create_copy(source: Path, app: str) -> None:
    validate_app(app)
    if app == ORIGINAL:
        raise ValueError("The test copy must have a distinct application identity")
    original_assets = source / "assets" / ORIGINAL
    assets = source / "assets" / app
    shutil.copytree(source / "apps" / ORIGINAL, source / "apps" / app)
    shutil.copytree(original_assets, assets)
    catalog_path = assets / "asset_catalog.json"
    if catalog_path.samefile(original_assets / "asset_catalog.json"):
        raise ValueError("The copy's assets must have independent storage")
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    if any(entry["id"] == ASSET_ID for entry in catalog["assets"]):
        raise ValueError("Integration asset marker already exists in the original app")
    marker = assets / ASSET_MARKER
    marker.parent.mkdir(parents=True)
    marker.write_text("Only the independently copied app owns this asset.\n", encoding="utf-8")
    catalog["assets"].append(dict(id=ASSET_ID, type="text", path=ASSET_MARKER))
    catalog_path.write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")

    bundle_path = source / "apps" / app / "shaders/bundle.json"
    bundle = json.loads(bundle_path.read_text(encoding="utf-8"))
    program = copy.deepcopy(bundle["programs"][0])
    if any(entry["id"] == SHADER_ID for entry in bundle["programs"]):
        raise ValueError("Integration shader marker already exists in the original app")
    program["id"] = SHADER_ID  # Reuse valid sources; preserve all existing game IDs.
    bundle["programs"].append(program)
    bundle_path.write_text(json.dumps(bundle, indent=2) + "\n", encoding="utf-8")
    (source / "config/engine/projects.csv").write_text(
        "name,description,version,enabled,windows,linux,macos\n"
        f"{ORIGINAL},Original integration fixture,,1,1,0,0\n"
        f"{app},Independent integration fixture,,1,1,0,0\n", encoding="utf-8")


def read_cache(path: Path) -> dict[str, str]:
    if path.is_dir():
        path /= "CMakeCache.txt"
    values = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        match = re.fullmatch(r"([^/#][^:]*):[^=]+=(.*)", line)
        if match:
            values[match[1]] = match[2]
    return values


def dependency_sources(directory: Path) -> dict[str, str]:
    """Reuse populated sources only; never reuse dependency binaries or caches."""
    if not directory.is_dir():
        raise ValueError(f"Dependency source cache directory is absent: {directory}")
    return {"FETCHCONTENT_SOURCE_DIR_" + path.name[:-4].upper(): str(path.resolve())
            for path in directory.glob("*-src") if path.is_dir() and any(path.iterdir())}


def configuration_options(args) -> tuple[str, dict[str, str]]:
    cache = read_cache(args.reuse_cache) if args.reuse_cache else {}
    values = {}
    if args.reuse_cache:
        cache_dir = args.reuse_cache if args.reuse_cache.is_dir() else args.reuse_cache.parent
        if (cache_dir / "_deps").is_dir():
            values.update(dependency_sources(cache_dir / "_deps"))
    for directory in args.dependency_source_cache:
        values.update(dependency_sources(directory))
    for key, value in cache.items():
        if value and (key.startswith("FETCHCONTENT_SOURCE_DIR_") or key in {
                "CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER", "CMAKE_MAKE_PROGRAM",
                "GYO_SHADER_TOOL_EXECUTABLE"}):
            values[key] = value
    for key, value in (("CMAKE_C_COMPILER", args.c_compiler), ("CMAKE_CXX_COMPILER", args.cxx_compiler),
                       ("CMAKE_MAKE_PROGRAM", args.make_program),
                       ("GYO_SHADER_TOOL_EXECUTABLE", args.shader_tool.resolve(strict=True) if args.shader_tool else None)):
        if value:
            values[key] = str(value)
    # An existing cache may force no apps, source paths, or an editor graph.
    # Only the explicitly allowlisted native tools/source directories cross over.
    values.update(CMAKE_BUILD_TYPE=args.configuration, GYO_BUILD_UI_EDITOR="OFF")
    return args.generator or cache.get("CMAKE_GENERATOR", "Ninja"), values


class Commands:
    def __init__(self, logs: Path, cwd: Path):
        self.logs, self.cwd = logs, cwd

    def run(self, name: str, arguments: list[str], timeout=3600) -> str:
        command = [str(value) for value in arguments]
        print(f"[{name}] {subprocess.list2cmdline(command)}", flush=True)
        path = self.logs / (name + ".log")
        with path.open("w", encoding="utf-8") as stream:
            stream.write(subprocess.list2cmdline(command) + "\n")
            stream.flush()
            try:
                result = subprocess.run(command, cwd=self.cwd, stdout=stream, stderr=subprocess.STDOUT,
                                        timeout=timeout, check=False)
            except subprocess.TimeoutExpired:
                stream.write("\nFAIL: command timed out\n")
                raise
        output = path.read_text(encoding="utf-8", errors="replace")
        if result.returncode:
            raise RuntimeError(f"{name} failed ({result.returncode}); see {path}\n{output[-12000:]}")
        print(f"[{name}] PASS", flush=True)
        return output


@contextmanager
def absent_optional_tooling(source: Path, holding: Path, apps: tuple[str, ...]):
    """Physically move optional sources out of this scratch source tree only."""
    source, holding = source.resolve(strict=True), holding.resolve()
    if holding.is_relative_to(source) or source.is_relative_to(holding):
        raise ValueError("Optional tooling must be held outside the scratch source tree")
    holding.mkdir(parents=True, exist_ok=False)
    paths = [PurePosixPath(path) for path in (".github", "tools/ci", "third_party/doctest")]
    paths.extend(PurePosixPath(path.relative_to(source).as_posix())
                 for path in source.rglob("tests") if path.is_dir())
    for app in apps:
        validate_app(app)
        paths.extend(PurePosixPath(f"apps/{app}/{folder}") for folder in ("ci", "packaging"))
    # Hide parents first and never try to move a child's now-absent path.
    selected = []
    for path in sorted(set(paths), key=lambda value: (len(value.parts), str(value))):
        if not any(parent == path or parent in path.parents for parent in selected):
            selected.append(path)
    moved = []
    try:
        for relative in selected:
            original, hidden = source / relative, holding / relative
            if original.exists():
                if (original.is_symlink() or not original.resolve().is_relative_to(source)
                        or not hidden.resolve().is_relative_to(holding)):
                    raise ValueError("Refusing to move tooling outside the scratch workspace")
                hidden.parent.mkdir(parents=True, exist_ok=True)
                shutil.move(str(original), str(hidden))
                moved.append((original, hidden))
        yield
    finally:
        for original, hidden in reversed(moved):
            if original.exists():
                raise ValueError(f"Refusing to overwrite newly created tooling: {original}")
            shutil.move(str(hidden), str(original))


def assert_product_configuration(build: Path) -> None:
    cache = read_cache(build)
    if any(cache.get(axis) != "OFF" for axis in ("BUILD_TESTING", "GYO_ENABLE_PACKAGING")):
        raise ValueError("An ordinary configure enabled optional quality/packaging work")
    # Explicit source-cache hints are inert; actually finding these packages
    # would populate other keys and violate the default product boundary.
    if any(("Python" in key or "PYTHON" in key or "doctest" in key)
           for key in cache if not key.startswith("FETCHCONTENT_SOURCE_DIR_")):
        raise ValueError("An ordinary configure discovered Python or doctest")


def assert_product_install(stage: Path) -> None:
    if (stage / "share/gyo/apps").exists() or list(stage.rglob("*.py")):
        raise ValueError("Ordinary product installation contains package QA tooling")


def verify_catalog_relocation(commands: Commands, tests: list[dict], app: str, build: Path) -> None:
    """Prove a catalog-only asset rename works without CMake or recompilation."""
    validate_app(app)
    selected = [test for test in tests if test.get("name") == f"{app}.model_assets"]
    if len(selected) != 1:
        raise ValueError(f"Expected exactly one {app}.model_assets test")
    command = selected[0].get("command")
    if not isinstance(command, list) or not command or not all(isinstance(arg, str) for arg in command):
        raise ValueError("The model-assets test must expose its executable command")
    executable = Path(command[0])
    if (not executable.is_absolute() or not executable.is_file() or executable.is_symlink()
            or not executable.resolve().is_relative_to(build.resolve())):
        raise ValueError("The model-assets executable must be a regular file inside the fixture build")
    if commands.cwd.resolve().is_relative_to(executable.parent.resolve()):
        raise ValueError("Catalog relocation must run outside the executable directory")
    assets = executable.parent / "assets" / app
    catalog_path = assets / "asset_catalog.json"
    if (not catalog_path.is_file() or catalog_path.is_symlink()
            or not catalog_path.resolve().is_relative_to(build.resolve())):
        raise ValueError("The relocated catalog must be a staged file inside the fixture build")
    original_catalog = catalog_path.read_bytes()
    catalog = json.loads(original_catalog)
    entries = [entry for entry in catalog.get("assets", []) if entry.get("id") == MODEL_ASSET_ID]
    if len(entries) != 1:
        raise ValueError(f"Expected exactly one model catalog entry: {MODEL_ASSET_ID}")
    relative = entries[0].get("path")
    if (not isinstance(relative, str) or not relative or "\\" in relative or ":" in relative
            or PurePosixPath(relative).is_absolute() or ".." in PurePosixPath(relative).parts):
        raise ValueError("The model catalog path must be a relative asset path")
    original = assets / relative
    if (not original.is_file() or original.is_symlink()
            or not original.resolve().is_relative_to(assets.resolve())):
        raise ValueError("The model catalog path must identify a regular staged asset")
    relocated = original.with_name("catalog-relocated-" + original.name)
    if relocated.exists() or relocated.is_symlink():
        raise ValueError(f"Catalog relocation destination already exists: {relocated}")
    original.rename(relocated)
    try:
        entries[0]["path"] = relocated.relative_to(assets).as_posix()
        catalog_path.write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")
        commands.run(app + "-catalog-relocation", command, timeout=300)
    finally:
        # Preserve the exact catalog bytes and restore even after test failure.
        # Later staging/install steps must see the unmodified copied app.
        try:
            relocated.rename(original)
        finally:
            catalog_path.write_bytes(original_catalog)


def assert_package_isolation(stage: Path, app: str, copy_name: str) -> None:
    manifest = load_manifest(stage, app, "windows-x64")
    if manifest["executable"] != f"bin/gyo_{app}.exe":
        raise ValueError(f"Installed executable did not follow the copied app identity: {manifest['executable']}")
    other = copy_name if app == ORIGINAL else ORIGINAL
    for path in (stage / f"bin/gyo_{other}.exe", stage / f"bin/assets/{other}",
                 stage / f"bin/shaders/{other}", stage / f"share/gyo/apps/{other}"):
        if path.exists():
            raise ValueError(f"Package contains another application's content: {path}")
    catalog = json.loads((stage / f"bin/assets/{app}/asset_catalog.json").read_text(encoding="utf-8"))
    shaders = json.loads((stage / f"bin/shaders/{app}/manifest.json").read_text(encoding="utf-8"))
    copied = app == copy_name
    if ((ASSET_ID in {entry['id'] for entry in catalog['assets']}) != copied
            or (SHADER_ID in {entry['id'] for entry in shaders['programs']}) != copied
            or (stage / f"bin/assets/{app}" / ASSET_MARKER).is_file() != copied):
        raise ValueError(f"Independent asset/shader changes leaked or were lost in {app}")


def integrate(args) -> None:
    if os.name != "nt":
        raise ValueError("This integration acceptance requires native Windows/MSVC")
    repository = args.repository.resolve(strict=True)
    work = args.workdir.resolve()
    if work == repository or repository.is_relative_to(work):
        raise ValueError("Scratch workspace must not contain the repository")
    # Never remove a caller's existing data or reuse a potentially stale stage.
    work.mkdir(parents=True, exist_ok=False)
    logs = (args.logs or work / "logs").resolve()
    logs.mkdir(parents=True, exist_ok=True)
    source, build = work / "source", work / "build"
    revision = validate_commit(subprocess.check_output(
        ["git", "-C", str(repository), "rev-parse", "HEAD"], text=True).strip())
    commands = Commands(logs, work)
    copied = mirror_sources(repository, source)
    (logs / "source-inputs.json").write_text(json.dumps(copied, indent=2), encoding="utf-8")
    create_copy(source, args.copy_name)
    generator, options = configuration_options(args)
    cmake_args = [f"-D{key}={value}" for key, value in sorted(options.items())]
    configure = [args.cmake, "-S", str(source), "-B", str(build), "-G", generator, *cmake_args]
    matrix_path = logs / "fixture-matrix.json"
    commands.run("export-registry", [args.cmake, f"-DGYO_OUTPUT={matrix_path}", "-P",
                                    str(source / "cmake/ExportAppRegistry.cmake")])
    matrix = json.loads(matrix_path.read_text(encoding="utf-8"))
    actual = {(entry["app"], entry["platform"]) for entry in matrix["include"]}
    if actual != {(ORIGINAL, "windows-x64"), (args.copy_name, "windows-x64")}:
        raise ValueError(f"Copied app was not exported into the generic CI matrix: {matrix}")
    # Prove the real root defaults with quality/package directories physically
    # absent, then reuse the native product graph for explicitly requested QA.
    with absent_optional_tooling(source, work / "optional-tooling", (ORIGINAL, args.copy_name)):
        commands.run("product-configure", [*configure, "-DGYO_APPS=AUTO"])
        assert_product_configuration(build)
        commands.run("product-build", [args.cmake, "--build", str(build), "--config", args.configuration,
            "--target", f"gyo_{ORIGINAL}", f"gyo_{args.copy_name}", "--parallel", str(args.parallel)])
        product_stage = work / "product-stage"
        commands.run("product-install", [args.cmake, "--install", str(build), "--config", args.configuration,
                                         "--prefix", str(product_stage)])
        assert_product_install(product_stage)
        for app in (ORIGINAL, args.copy_name):
            commands.run(app + "-product-startup", [str(product_stage / f"bin/gyo_{app}.exe"),
                                                    "--startup-smoke-test"], timeout=60)
    configure.extend(["-DBUILD_TESTING=ON", "-DGYO_ENABLE_PACKAGING=ON"])
    commands.run("both-configure", [*configure, "-DGYO_APPS=AUTO"])
    commands.run("both-build", [args.cmake, "--build", str(build), "--config", args.configuration,
                               "--parallel", str(args.parallel)])
    test_filter = rf"^({ORIGINAL}|{args.copy_name})\."
    test_command = [args.ctest, "--test-dir", str(build), "-C", args.configuration,
                    "-L", "cpu", "-R", test_filter, "--no-tests=error"]
    listed = commands.run("both-test-list", [*test_command, "--show-only=json-v1"])
    # The command line is the first log line; CTest's JSON follows it.
    tests = json.loads(listed.split("\n", 1)[1])["tests"]
    names = {test["name"] for test in tests}
    for app in (ORIGINAL, args.copy_name):
        required = {f"{app}.{suffix}" for suffix in (
            "model_assets", "headless", "startup_smoke", "headless_smoke", "package")}
        if not required.issubset(names):
            raise ValueError(f"Missing independently registered CPU tests: {required - names}")
    verify_catalog_relocation(commands, tests, args.copy_name, build)
    commands.run("both-tests", [*test_command, "--output-on-failure", "--output-junit", str(logs / "both-tests.xml")])

    ci = repository / "tools/ci"
    packages = []
    for app in (ORIGINAL, args.copy_name):
        stage, app_logs = work / "stages" / app, logs / app
        app_logs.mkdir()
        commands.run(app + "-configure", [*configure, f"-DGYO_APPS={app}"])
        commands.run(app + "-build", [args.cmake, "--build", str(build), "--config", args.configuration,
                                     "--parallel", str(args.parallel)])
        commands.run(app + "-install", [args.cmake, "--install", str(build), "--config", args.configuration,
                                       "--prefix", str(stage)])
        assert_package_isolation(stage, app, args.copy_name)
        commands.run(app + "-checks", [sys.executable, str(ci / "run_package_checks.py"), "--stage", str(stage),
            "--app", app, "--platform", "windows-x64", "--revision", revision,
            "--profile", "release", "--logs", str(app_logs)])
        commands.run(app + "-linkage", [sys.executable, str(ci / "validate_package.py"), "--stage", str(stage),
                                       "--app", app, "--logs", str(app_logs)])
        archive = work / "packages" / archive_name(app, "windows-x64")
        commands.run(app + "-archive", [sys.executable, str(ci / "archive_package.py"), "--stage", str(stage),
            "--app", app, "--platform", "windows-x64", "--revision", revision,
            "--smoke-report", str(app_logs / "acceptance.json"), "--output", str(archive)])
        data = archive.read_bytes()
        validate_checksum(archive.name, data, archive.with_suffix(".gz.sha256").read_bytes())
        validate_archive(archive.name, data, app, "windows-x64", revision)
        packages.append(dict(app=app, archive=str(archive), sha256=hashlib.sha256(data).hexdigest()))
    (logs / "integration-result.json").write_text(json.dumps(dict(
        passed=True, ordinary_product_without_tooling=True, source_revision=revision,
        copied_app=args.copy_name, packages=packages), indent=2), encoding="utf-8")
    print(f"Real app-copy integration passed; evidence: {logs}", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", type=Path, default=ROOT)
    parser.add_argument("--workdir", type=Path, required=True, help="Fresh, nonexistent scratch directory; retained after test")
    parser.add_argument("--logs", type=Path, help="Diagnostics directory, e.g. the baseline job's uploaded logs")
    parser.add_argument("--copy-name", default="integration_copy")
    parser.add_argument("--reuse-cache", type=Path, help="Read only native tools and dependency sources from an existing CMake cache")
    parser.add_argument("--dependency-source-cache", type=Path, action="append", default=[], help="Populated _deps source directory; repeatable")
    parser.add_argument("--shader-tool", type=Path)
    parser.add_argument("--generator")
    parser.add_argument("--c-compiler")
    parser.add_argument("--cxx-compiler")
    parser.add_argument("--make-program")
    parser.add_argument("--configuration", default="RelWithDebInfo")
    parser.add_argument("--parallel", type=int, default=2)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--ctest", default="ctest")
    args = parser.parse_args()
    validate_app(args.copy_name)
    if args.parallel < 1:
        parser.error("--parallel must be positive")
    integrate(args)


if __name__ == "__main__":
    main()
