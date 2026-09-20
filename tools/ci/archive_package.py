#!/usr/bin/env python3
"""Archive one installed application, retaining provenance and executable permissions."""

import argparse
import hashlib
import json
from pathlib import Path
import sys
import platform
import re
import tarfile

sys.path.insert(0, str(Path(__file__).resolve().parent))

from package_contract import (PLATFORMS, load_manifest, required_files,
                              validate_app_namespace, validate_evidence)


def native_platform() -> str:
    system, architecture = platform.system(), platform.machine().lower()
    if system in ("Windows", "Linux") and architecture in ("amd64", "x86_64"):
        return "windows-x64" if system == "Windows" else "linux-x64"
    if system == "Darwin" and architecture in ("arm64", "aarch64"):
        return "macos-arm64"
    raise ValueError(f"No package platform for {system}/{architecture}")


def package_metadata(revision: str, manifest: dict, smoke_report: Path | None = None) -> dict:
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("Package revision must be a full lowercase Git commit SHA")
    smoke = None
    if smoke_report is not None:
        smoke = json.loads(smoke_report.read_text(encoding="utf-8-sig"))
        validate_evidence(smoke, manifest, revision, smoke.get("profile"))
    return {
        "source_revision": revision, "app": manifest["app"], "platform": manifest["platform"],
        "build_os": platform.platform(), "build_architecture": platform.machine(), "ci_smoke": smoke,
        "gpu_acceptance": "Physical GPU acceptance remains manual",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--app", required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--platform", choices=PLATFORMS)
    parser.add_argument("--smoke-report", type=Path)
    args = parser.parse_args()
    stage = args.stage.resolve(strict=True)
    manifest = load_manifest(stage, args.app, args.platform or native_platform())
    validate_app_namespace((path.relative_to(stage).as_posix()
                            for path in (stage / "share/gyo/apps").iterdir()), args.app)
    metadata = package_metadata(args.revision, manifest, args.smoke_report)
    for relative in required_files(manifest):
        path = stage / relative
        if not path.is_file() or path.is_symlink() or path.stat().st_size == 0:
            raise ValueError(f"Required package file must be nonempty and regular: {relative}")
    output = args.output.resolve()
    if output.is_relative_to(stage):
        raise ValueError("Archive output must be outside its installed stage")
    output.parent.mkdir(parents=True, exist_ok=True)
    (stage / "build_metadata.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    with tarfile.open(output, "w:gz", dereference=False) as archive:
        archive.add(stage, arcname="gyo-" + args.app)
    with output.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    output.with_suffix(output.suffix + ".sha256").write_text(f"{digest}  {output.name}\n", encoding="ascii")
    print(f"Created {output}\nSHA256 {digest}")


if __name__ == "__main__":
    main()
