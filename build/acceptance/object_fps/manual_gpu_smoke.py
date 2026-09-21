#!/usr/bin/env python3
"""Run packaged Object_FPS GPU diagnostics on an explicitly selected driver."""

from __future__ import annotations

import argparse
from contextlib import nullcontext
import json
from pathlib import Path
import platform
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from package_info import load_package_info
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from workspace import TemporaryDirectory


CASES = (
    ("custom-shader", ["--shader-smoke-test"]),
    ("world", ["--smoke-test"]),
    ("menu", ["--menu-smoke-test"]),
    ("viewmodel", ["--viewmodel-smoke-test"]),
    ("reload", ["--reload-smoke-test"]),
    ("muzzle-16x9", ["--muzzle-smoke-test"]),
    ("muzzle-4x3", ["--muzzle-smoke-test", "--preview-4x3"]),
    ("muzzle-21x9", ["--muzzle-smoke-test", "--preview-21x9"]),
)
DRIVERS = {"d3d12": ("direct3d12", "dxil"), "vulkan": ("vulkan", "spirv"),
           "metal": ("metal", "metallib")}
GPU_INFO = re.compile(r"GYO GPU: driver=([^,\s]+), shader=([^,\s]+), "
                      r"available_formats=\d+, bundle=([^\s]+)")


def run_case(case: str, command: list[str], working: Path, output: Path,
             driver: str, timeout: float) -> dict:
    """A zero exit is insufficient unless the requested GPU path initialized."""
    failure = None
    try:
        result = subprocess.run(command, cwd=working, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True,
                                encoding="utf-8", errors="replace", timeout=timeout)
        code, log = result.returncode, result.stdout
        if code != 0:
            failure = f"Diagnostic exited with code {code}"
    except subprocess.TimeoutExpired as error:
        code = -1
        captured = error.stdout or b""
        log = captured.decode("utf-8", errors="replace") if isinstance(captured, bytes) else captured
        failure = f"Diagnostic timed out after {timeout:g} seconds"
    except OSError as error:
        code, log = -2, ""
        failure = f"Unable to start diagnostic: {error}"

    info = GPU_INFO.search(log)
    actual_driver, shader_format, bundle = info.groups() if info else (None, None, None)
    if failure is None:
        if info is None:
            failure = "Diagnostic did not report an initialized GYO GPU"
        elif (actual_driver, shader_format) != DRIVERS[driver]:
            failure = (f"Expected {DRIVERS[driver][0]}/{DRIVERS[driver][1]}, "
                       f"received {actual_driver}/{shader_format}")
    if failure is not None:
        log += f"\nFAIL: {failure}\n"
    (output / f"{case}.log").write_text(log, encoding="utf-8")
    print(f"[{case}] {'PASS' if failure is None else 'FAIL'} exit={code}\n{log}", flush=True)
    return {"case": case, "passed": failure is None, "exit_code": code,
            "failure": failure, "command": command, "actual_driver": actual_driver,
            "shader_format": shader_format, "bundle_version": bundle}


def run_suite(executable: list[str], working: Path, output: Path,
              driver: str, suite: str, timeout: float) -> list[dict]:
    cases = CASES[:1] if suite == "quick" else CASES[:3] if suite == "ci" else CASES
    results = []
    for case, flags in cases:
        command = [*executable, "--gpu-driver", driver, *flags,
                   "--capture-dir", str(output / case)]
        results.append(run_case(case, command, working, output, driver, timeout))
    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path,
                        help="Installed package root containing bin/")
    parser.add_argument("--probe", required=True, type=Path, help="External acceptance executable colocated with the isolated package content")
    parser.add_argument("--driver", required=True, choices=("d3d12", "vulkan", "metal"))
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--suite", choices=("quick", "ci", "full"), default="full",
                        help="quick: one shader readback; ci: shader, world and menu; full: all eight diagnostics")
    parser.add_argument("--timeout", type=float, default=120,
                        help="Maximum seconds per diagnostic (default: 120)")
    parser.add_argument("--work-directory", type=Path,
                        help="Fresh directory to create and retain instead of an automatically removed temporary directory")
    args = parser.parse_args(argv)
    if not 0 < args.timeout <= 3600:
        parser.error("--timeout must be greater than 0 and at most 3600 seconds")
    package = args.package.resolve(strict=True)
    try:
        manifest = load_package_info(package)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    executable = args.probe.resolve(strict=True)
    if executable.parent != package / "bin":
        parser.error("--probe must be colocated in the isolated package bin directory")
    if args.work_directory is not None:
        retained = args.work_directory.resolve()
        if retained.is_relative_to(package):
            parser.error("--work-directory must be outside --package")
        try:
            retained.mkdir(parents=True)
        except FileExistsError:
            parser.error(f"--work-directory must not already exist: {retained}")
        workspace = nullcontext(retained)
    else:
        workspace = TemporaryDirectory(prefix="gyo-gpu-smoke-")

    with workspace as working:
        results = run_suite([str(executable)], Path(working), output,
                            args.driver, args.suite, args.timeout)
    passed = all(item["passed"] for item in results)
    metadata = package / "build_metadata.json"
    (output / "summary.json").write_text(json.dumps({
        "os": platform.platform(), "architecture": platform.machine(),
        "requested_driver": args.driver, "suite": args.suite, "passed": passed,
        "package": str(package), "app": manifest["product"], "cases": results,
        "build_metadata": json.loads(metadata.read_text(encoding="utf-8")) if metadata.is_file() else None,
    }, indent=2), encoding="utf-8")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
