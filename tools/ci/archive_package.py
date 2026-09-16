#!/usr/bin/env python3
"""Archive an installed package while preserving Unix executable permissions."""

import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import shutil
import tarfile


PLATFORMS = ("windows-x64", "linux-x64", "macos-arm64")


def native_platform() -> str:
    system, architecture = platform.system(), platform.machine().lower()
    if system in ("Windows", "Linux") and architecture in ("amd64", "x86_64"):
        return "windows-x64" if system == "Windows" else "linux-x64"
    if system == "Darwin" and architecture in ("arm64", "aarch64"):
        return "macos-arm64"
    raise ValueError(f"No package platform for {system}/{architecture}")


def package_metadata(revision: str, platform_id: str, smoke_report: Path | None = None) -> dict:
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("Package revision must be a full lowercase Git commit SHA")
    if platform_id not in PLATFORMS:
        raise ValueError(f"Unsupported package platform: {platform_id}")
    smoke = None
    if smoke_report is not None:
        smoke = json.loads(smoke_report.read_text(encoding="utf-8-sig"))
        if (not isinstance(smoke, dict) or smoke.get("source_revision") != revision
                or smoke.get("platform") != platform_id or smoke.get("startup") != "passed"):
            raise ValueError("Smoke report must match this commit/platform and pass packaged startup")
        profile = smoke.get("profile")
        if profile not in ("quick", "release"):
            raise ValueError("Smoke report must name the quick or release profile")
        expected_gameplay = "passed" if profile == "release" else "not_run"
        if smoke.get("headless") != expected_gameplay:
            raise ValueError("Gameplay smoke result does not match the selected acceptance profile")
        rendering = smoke.get("rendering", {})
        if not isinstance(rendering, dict):
            raise ValueError("Smoke report rendering result must be an object")
        if platform_id == "linux-x64":
            count = rendering.get("case_count", 0)
            suite, expected_count = ("full", 8) if profile == "release" else ("quick", 1)
            if (rendering.get("status") != "passed" or rendering.get("driver") != "vulkan"
                    or rendering.get("implementation") != "mesa-lavapipe"
                    or rendering.get("suite") != suite or type(count) is not int or count != expected_count):
                raise ValueError(f"Linux {profile} package requires {expected_count} Lavapipe rendering case(s)")
        elif rendering.get("status") != "not_run":
            raise ValueError("Windows/macOS hosted packages must not claim hardware GPU acceptance")
    return {
        "source_revision": revision,
        "platform": platform_id,
        "build_os": platform.platform(),
        "build_architecture": platform.machine(),
        "ci_smoke": smoke,
        "gpu_acceptance": "Physical GPU acceptance remains manual",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--revision", required=True, help="Source commit used to build the package")
    parser.add_argument("--platform", choices=PLATFORMS, help="Canonical target identity; defaults to native host")
    parser.add_argument("--smoke-report", type=Path, help="Matching successful CI smoke report")
    args = parser.parse_args()
    metadata = package_metadata(args.revision, args.platform or native_platform(), args.smoke_report)
    stage = args.stage.resolve(strict=True)
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    source_root = Path(__file__).resolve().parents[2]
    for name in ("rendering_architecture.zh-Hant.md", "rendering_architecture.ja.md", "architecture.md",
                 "releasing.zh-Hant.md", "releasing.ja.md"):
        shutil.copyfile(source_root / "docs" / name, stage / name)
    shutil.copyfile(source_root / "tools/ci/manual_gpu_smoke.py", stage / "manual_gpu_smoke.py")
    (stage / "build_metadata.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    with tarfile.open(output, "w:gz", dereference=False) as archive:
        archive.add(stage, arcname="gyo-object-fps")
    with output.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    output.with_suffix(output.suffix + ".sha256").write_text(
        f"{digest}  {output.name}\n", encoding="ascii")
    print(f"Created {output}\nSHA256 {digest}")


if __name__ == "__main__":
    main()
