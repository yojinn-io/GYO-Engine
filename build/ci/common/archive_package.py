#!/usr/bin/env python3
"""Archive one installed product, retaining provenance and executable permissions."""

import argparse
import hashlib
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "acceptance/common"))

import platform
import re
import tarfile

sys.path.insert(0, str(Path(__file__).resolve().parent))

from package_contract import (PLATFORMS, load_manifest, installed_required_files,
                              validate_product_namespace, validate_evidence, package_digest, valid_installed_file)
from content_contract import decode_json
from app_registry import CPU_EXECUTION


def package_metadata(revision: str, manifest: dict, smoke_report: Path | None = None, *, package_sha256=None,
                     cpu_execution: str = "native") -> dict:
    """Record provenance: build_* describe the host that built and checked the
    package, platform its target, and cpu_execution how the target binaries
    ran for the recorded acceptance (natively or translated by Rosetta 2)."""
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("Package revision must be a full lowercase Git commit SHA")
    if cpu_execution not in CPU_EXECUTION:
        raise ValueError(f"Unknown CPU execution mode: {cpu_execution!r}")
    smoke = None
    if smoke_report is not None:
        smoke = json.loads(smoke_report.read_text(encoding="utf-8-sig"))
        validate_evidence(smoke, manifest, revision, smoke.get("profile"), package_sha256)
    return {
        "source_revision": revision, "product": manifest["product"], "kind": manifest["kind"], "platform": manifest["platform"],
        "build_os": platform.platform(), "build_architecture": platform.machine(),
        "cpu_execution": cpu_execution, "ci_smoke": smoke,
        "gpu_acceptance": "Physical GPU acceptance remains manual",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--product", required=True)
    parser.add_argument("--revision", required=True)
    # Without --platform the manifest's own target platform, recorded by CMake,
    # identifies the package; the archiving host may be a different architecture.
    parser.add_argument("--platform", choices=PLATFORMS)
    parser.add_argument("--cpu-execution", choices=CPU_EXECUTION, default="native",
                        help="How the recorded acceptance ran the target binaries")
    parser.add_argument("--smoke-report", type=Path)
    args = parser.parse_args()
    stage = args.stage.resolve(strict=True)
    manifest = load_manifest(stage, args.product, args.platform)
    validate_product_namespace((path.relative_to(stage).as_posix()
                            for path in stage.rglob("*") if path.is_file() or path.is_symlink()), args.product, manifest,
                            lambda name: decode_json((stage / name).read_text(encoding="utf-8-sig"), name))
    metadata = package_metadata(args.revision, manifest, args.smoke_report, package_sha256=package_digest(stage),
                                cpu_execution=args.cpu_execution)
    for relative in installed_required_files(stage, manifest):
        if not valid_installed_file(stage, relative, manifest):
            raise ValueError(f"Required package file must be nonempty and confined to the package: {relative}")
    output = args.output.resolve()
    if output.is_relative_to(stage):
        raise ValueError("Archive output must be outside its installed stage")
    output.parent.mkdir(parents=True, exist_ok=True)
    (stage / "build_metadata.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    with tarfile.open(output, "w:gz", dereference=False) as archive:
        archive.add(stage, arcname="gyo-" + args.product)
    with output.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    output.with_suffix(output.suffix + ".sha256").write_text(f"{digest}  {output.name}\n", encoding="ascii")
    print(f"Created {output}\nSHA256 {digest}")


if __name__ == "__main__":
    main()
