#!/usr/bin/env python3
"""Run Object_FPS GPU diagnostics on an explicitly selected local driver."""

import argparse
from contextlib import nullcontext
import json
from pathlib import Path
import platform
import subprocess
import tempfile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path,
                        help="Installed package root containing bin/")
    parser.add_argument("--driver", required=True, choices=("d3d12", "vulkan", "metal"))
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--work-directory", type=Path,
                        help="Fresh directory to create and retain instead of an automatically removed temporary directory")
    args = parser.parse_args()
    package = args.package.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    name = "gyo_object_fps.exe" if platform.system() == "Windows" else "gyo_object_fps"
    executable = package / "bin" / name
    if not executable.is_file():
        parser.error(f"Executable is absent: {executable}")
    cases = [
        ("custom-shader", ["--shader-smoke-test"]),
        ("world", ["--smoke-test"]),
        ("menu", ["--menu-smoke-test"]),
        ("viewmodel", ["--viewmodel-smoke-test"]),
        ("reload", ["--reload-smoke-test"]),
        ("muzzle-16x9", ["--muzzle-smoke-test"]),
        ("muzzle-4x3", ["--muzzle-smoke-test", "--preview-4x3"]),
        ("muzzle-21x9", ["--muzzle-smoke-test", "--preview-21x9"]),
    ]
    results = []
    if args.work_directory is not None:
        retained = args.work_directory.resolve()
        try:
            retained.mkdir(parents=True)
        except FileExistsError:
            parser.error(f"--work-directory must not already exist: {retained}")
        workspace = nullcontext(retained)
    else:
        workspace = tempfile.TemporaryDirectory(prefix="gyo-gpu-smoke-")

    with workspace as working:
        for case, flags in cases:
            capture = output / case
            command = [str(executable), "--gpu-driver", args.driver, *flags,
                       "--capture-dir", str(capture)]
            try:
                result = subprocess.run(command, cwd=working, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, text=True,
                                        encoding="utf-8", errors="replace", timeout=120)
                code, log = result.returncode, result.stdout
            except subprocess.TimeoutExpired as error:
                code = -1
                captured = error.stdout or b""
                log = captured.decode("utf-8", errors="replace") if isinstance(captured, bytes) else captured
                log += "\nDiagnostic timed out after 120 seconds.\n"
            except OSError as error:
                code, log = -2, f"Unable to start diagnostic: {error}\n"
            (output / f"{case}.log").write_text(log, encoding="utf-8")
            print(f"[{case}] exit={code}\n{log}", flush=True)
            results.append({"case": case, "exit_code": code, "command": command})
    metadata = package / "build_metadata.json"
    (output / "summary.json").write_text(json.dumps({
        "os": platform.platform(), "architecture": platform.machine(),
        "requested_driver": args.driver, "package": str(package), "cases": results,
        "build_metadata": json.loads(metadata.read_text(encoding="utf-8")) if metadata.is_file() else None,
    }, indent=2), encoding="utf-8")
    raise SystemExit(0 if all(item["exit_code"] == 0 for item in results) else 1)


if __name__ == "__main__":
    main()
