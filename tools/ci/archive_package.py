#!/usr/bin/env python3
"""Archive an installed package while preserving Unix executable permissions."""

import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import tarfile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--revision", required=True, help="Source commit used to build the package")
    args = parser.parse_args()
    stage = args.stage.resolve(strict=True)
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    source_root = Path(__file__).resolve().parents[2]
    for name in ("rendering_architecture.zh-Hant.md", "rendering_architecture.ja.md", "architecture.md"):
        shutil.copyfile(source_root / "docs" / name, stage / name)
    shutil.copyfile(source_root / "tools/ci/manual_gpu_smoke.py", stage / "manual_gpu_smoke.py")
    (stage / "build_metadata.json").write_text(json.dumps({
        "source_revision": args.revision,
        "build_os": platform.platform(),
        "build_architecture": platform.machine(),
        "gpu_acceptance": "Not run by the hosted package job",
    }, indent=2), encoding="utf-8")
    with tarfile.open(output, "w:gz", dereference=False) as archive:
        archive.add(stage, arcname="gyo-object-fps")
    with output.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    output.with_suffix(output.suffix + ".sha256").write_text(
        f"{digest}  {output.name}\n", encoding="ascii")
    print(f"Created {output}\nSHA256 {digest}")


if __name__ == "__main__":
    main()
