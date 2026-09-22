"""Run declared installed checks without a shell or source-tree working directory."""

import argparse
import json
import os
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "acceptance/common"))

import re
import shutil
import subprocess

sys.path.insert(0, str(Path(__file__).resolve().parent))

from workspace import TemporaryDirectory

from package_contract import (check_identity, decode_json, load_manifest, manifest_digest, required_checks,
                              installed_required_files, package_digest, valid_installed_file,
                              validate_product, validate_product_namespace)


def load_context(path: Path | None, manifest: dict) -> dict | None:
    """Build-only locations never supply commands or replace the installed contract."""
    if path is None:
        return None
    context = decode_json(path.read_text(encoding="utf-8-sig"), str(path))
    if (not isinstance(context, dict) or type(context.get("schema_version")) is not int
            or context["schema_version"] != 1 or context.get("product") != manifest["product"]
            or context.get("platform") != manifest["platform"] or context.get("manifest") != manifest
            or context.get("configuration") != manifest["configuration"]):
        raise ValueError("Acceptance context does not match the installed manifest/product/platform/configuration")
    owners = context.get("owners")
    executable_owners = {entry["owner"] for entry in manifest["executables"].values()}
    if not isinstance(owners, dict):
        raise ValueError("Acceptance context owners must be an object")
    for owner, locations in owners.items():
        validate_product(owner)
        if owner not in executable_owners or not isinstance(locations, dict):
            raise ValueError(f"Invalid acceptance context owner: {owner}")
        root = locations.get("root")
        probes = locations.get("probes")
        if not isinstance(root, str) or not Path(root).is_absolute() or not Path(root).is_dir() or not isinstance(probes, dict):
            raise ValueError(f"Invalid acceptance context locations for {owner}")
        for role, probe in probes.items():
            validate_product(role)
            if not isinstance(probe, dict) or not isinstance(probe.get("runtime_files"), list):
                raise ValueError(f"Invalid acceptance context probe: {owner}.{role}")
            for location in [probe.get("path"), *probe["runtime_files"]]:
                if not isinstance(location, str) or not Path(location).is_absolute() or not Path(location).is_file():
                    raise ValueError(f"Missing or invalid acceptance context file: {location!r}")
    return context


def check_locations(check: dict, context: dict | None) -> tuple[str | None, dict]:
    arguments = [*check["command"], *check["environment"]]
    roles = {role for argument in arguments for role in re.findall(r"@PROBE:([a-z][a-z0-9_]*)@", argument)}
    needs_root = any("@CHECK_ROOT@" in argument for argument in arguments)
    if not roles and not needs_root:
        return None, {}
    owner = check["owner"]
    if context is None or owner not in context["owners"]:
        raise ValueError(f"Acceptance check {check_identity(check)} requires its owner context")
    locations = context["owners"][owner]
    if not roles.issubset(locations["probes"]):
        raise ValueError(f"Acceptance context has no requested probe role for {check_identity(check)}")
    return locations["root"], {role: locations["probes"][role] for role in roles}


def stage_probes(package: Path, selected: list[dict], context: dict | None, platform: str) -> dict:
    probes = {}
    for check in selected:
        _, locations = check_locations(check, context)
        for role, location in locations.items():
            probes[(check["owner"], role)] = location
    deployed = {}
    names = set()
    for key, probe in probes.items():
        source = Path(probe["path"])
        target = package / "bin" / source.name
        normalized = source.name.casefold() if platform == "windows-x64" else source.name
        if normalized in names or target.exists():
            raise ValueError(f"Acceptance probe would overwrite another probe or product file: {source.name}")
        names.add(normalized)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        deployed[key] = target
    for probe in probes.values():
        for location in probe["runtime_files"]:
            source = Path(location)
            target = package / "bin" / source.name
            normalized = source.name.casefold() if platform == "windows-x64" else source.name
            if normalized in names:
                raise ValueError(f"Probe dependency would overwrite an acceptance probe: {source.name}")
            if target.exists():
                if target.read_bytes() == source.read_bytes():
                    continue
                raise ValueError(f"Acceptance dependency would overwrite a product file: {source.name}")
            shutil.copy2(source, target)
    return deployed


def run_checks(stage: Path, product: str, platform: str, revision: str, profile: str,
               logs: Path, *, gpu=False, driver="", context: Path | None = None) -> bool:
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("Package revision must be a full lowercase Git commit SHA")
    stage, logs = stage.resolve(strict=True), logs.resolve()
    logs.mkdir(parents=True, exist_ok=True)
    manifest = load_manifest(stage, product, platform)
    validate_product_namespace((path.relative_to(stage).as_posix() for path in stage.rglob("*")
                                if path.is_file() or path.is_symlink()), product, manifest,
                               lambda name: decode_json((stage / name).read_text(encoding="utf-8-sig"), name))
    locations = load_context(context, manifest)
    identity = dict(package_sha256=package_digest(stage), product=product, kind=manifest["kind"], platform=platform, source_revision=revision, profile=profile,
                    manifest_sha256=manifest_digest(manifest),
                    context_sha256=manifest_digest(locations) if locations is not None else None)
    report_path = logs / "acceptance.json"
    report = {**identity, "checks": []}
    if gpu:
        if not report_path.is_file():
            raise ValueError("GPU acceptance requires the preceding installed startup report")
        report = json.loads(report_path.read_text(encoding="utf-8"))
        if any(report.get(key) != value for key, value in identity.items()):
            raise ValueError("Cannot merge acceptance evidence from a different package/profile")
    else:
        missing = [name for name in installed_required_files(stage, manifest)
                   if not valid_installed_file(stage, name, manifest)]
        report["checks"].append(dict(name="package_files", exit_code=1 if missing else 0, passed=not missing))
        (logs / "package_files.log").write_text("Missing or invalid required files: " + repr(missing), encoding="utf-8")
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        if missing:
            return False
    selected = [check for check in required_checks(manifest, profile) if check["gpu"] == gpu]
    if gpu and selected and not driver:
        raise ValueError("GPU acceptance requires --driver")
    replacing = {check_identity(check) for check in selected}
    report["checks"] = [check for check in report["checks"] if check["name"] not in replacing]
    passed = True
    with TemporaryDirectory(prefix="gyo-installed-check-") as work:
        work = Path(work)
        package = work / "package"
        shutil.copytree(stage, package, symlinks=True)
        deployed = stage_probes(package, selected, locations, platform)
        common_substitutions = {
            "@PACKAGE_ROOT@": str(package),
            "@LOG_ROOT@": str(logs), "@PYTHON@": sys.executable, "@PROFILE@": profile,
            "@DRIVER@": driver,
        }

        for check in selected:
            name, owner = check_identity(check), check["owner"]
            substitutions = common_substitutions.copy()
            root, probes = check_locations(check, locations)
            if root is not None:
                substitutions["@CHECK_ROOT@"] = root
            for role in probes:
                substitutions[f"@PROBE:{role}@"] = str(deployed[(owner, role)])
            for executable in manifest["executables"].values():
                if executable["owner"] == owner:
                    substitutions[f"@EXECUTABLE:{executable['role']}@"] = str(package / executable["path"])

            def expand(argument):
                for token, value in substitutions.items():
                    argument = argument.replace(token, value)
                return argument

            command = [expand(argument) for argument in check["command"]]
            environment = os.environ.copy()
            for entry in check["environment"]:
                key, value = entry.split("=", 1)
                environment[key] = expand(value)
            try:
                result = subprocess.run(command, cwd=work, env=environment, text=True,
                                        encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, timeout=check["timeout"], check=False)
                code, output = result.returncode, result.stdout
            except subprocess.TimeoutExpired as error:
                partial = error.stdout or b""
                output = partial.decode("utf-8", errors="replace") if isinstance(partial, bytes) else partial
                code, output = -1, output + "\nPackage check timed out\n"
            except OSError as error:
                code, output = -2, str(error)
            (logs / (name + ".log")).write_text(output, encoding="utf-8")
            report["checks"].append(dict(name=name, exit_code=code, passed=code == 0))
            passed = passed and code == 0
            print(f"[{product}/{name}] exit={code}\n{output}", flush=True)
            # Preserve completed failures even when a later process cannot run.
            report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    return passed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("product", "platform", "revision", "profile"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--stage", required=True, type=Path)
    parser.add_argument("--logs", required=True, type=Path)
    parser.add_argument("--context", type=Path, help="Generated build-only owner/probe locations bound to this package")
    parser.add_argument("--gpu", action="store_true")
    parser.add_argument("--driver", default="")
    parser.add_argument("--inspect", action="store_true", help="Print has_gpu for GitHub outputs, without running checks")
    args = parser.parse_args()
    if args.inspect:
        manifest = load_manifest(args.stage, args.product, args.platform)
        print("has_gpu=" + str(any(check["gpu"] for check in required_checks(manifest, args.profile))).lower())
        return 0
    return 0 if run_checks(args.stage, args.product, args.platform, args.revision, args.profile,
                           args.logs, gpu=args.gpu, driver=args.driver, context=args.context) else 1


if __name__ == "__main__":
    raise SystemExit(main())
