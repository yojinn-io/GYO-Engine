#!/usr/bin/env python3
"""Check Object_FPS content and rejection of incomplete installed packages."""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


MISSING_CONTENT = (
    ("missing-common", "bin/assets/common"),
    ("missing-builtin-shaders", "bin/shaders/builtin/manifest.json"),
    ("missing-game-shaders", "bin/shaders/object_fps/manifest.json"),
)


def validate(stage: Path, logs: Path) -> None:
    stage = stage.resolve(strict=True)
    logs = logs.resolve()
    logs.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((stage / "share/gyo/apps/object_fps/manifest.json").read_text(encoding="utf-8"))
    with tempfile.TemporaryDirectory(prefix="object-fps-content-") as temporary:
        isolated = Path(temporary)
        package = isolated / "package"
        work = isolated / "unrelated-working-directory"
        work.mkdir()
        shutil.copytree(stage, package, symlinks=True)
        command = [str(package / manifest["executable"]), "--validate-package"]

        def check(name: str, expected_success: bool) -> None:
            result = subprocess.run(command, cwd=work, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, encoding="utf-8", errors="replace",
                                    timeout=90, check=False)
            (logs / (name + ".log")).write_text(result.stdout, encoding="utf-8")
            print(f"[{name}] exit={result.returncode}\n{result.stdout}", flush=True)
            if (result.returncode == 0) != expected_success:
                raise RuntimeError(f"Content check '{name}' returned {result.returncode}")

        check("complete", True)
        for name, relative in MISSING_CONTENT:
            target = package / relative
            if not target.exists():
                raise RuntimeError(f"Expected staged content is absent: {relative}")
            hidden = target.with_name(target.name + ".validation-hidden")
            target.rename(hidden)
            try:
                check(name, False)
            finally:
                hidden.rename(target)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", required=True, type=Path)
    parser.add_argument("--logs", required=True, type=Path)
    args = parser.parse_args()
    validate(args.stage, args.logs)


if __name__ == "__main__":
    main()
