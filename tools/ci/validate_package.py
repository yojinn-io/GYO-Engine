#!/usr/bin/env python3
"""Exercise the installed package without a GPU or access to source fallbacks."""

from __future__ import annotations

import argparse
from contextlib import nullcontext
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", required=True, type=Path)
    parser.add_argument("--logs", required=True, type=Path)
    parser.add_argument("--dumpbin", help="Windows dumpbin executable; defaults to PATH")
    parser.add_argument("--work-directory", type=Path,
                        help="Fresh directory to create and retain instead of an automatically removed temporary directory")
    args = parser.parse_args()
    stage = args.stage.resolve(strict=True)
    logs = args.logs.resolve()
    logs.mkdir(parents=True, exist_ok=True)
    executable = "gyo_object_fps.exe" if (stage / "bin/gyo_object_fps.exe").is_file() else "gyo_object_fps"

    if args.work_directory is not None:
        retained = args.work_directory.resolve()
        if retained.is_relative_to(stage):
            parser.error("--work-directory must be outside --stage")
        try:
            retained.mkdir(parents=True)
        except FileExistsError:
            parser.error(f"--work-directory must not already exist: {retained}")
        workspace = nullcontext(retained)
    else:
        workspace = tempfile.TemporaryDirectory(prefix="gyo-package-")

    with workspace as temporary:
        isolated = Path(temporary)
        package = isolated / "package"
        work = isolated / "unrelated-working-directory"
        work.mkdir()
        shutil.copytree(stage, package, symlinks=True)
        command = [str(package / "bin" / executable), "--validate-package"]

        # A copied executable can still accidentally load its old build-tree
        # library. Check the native dependency route as well as successful IO.
        if platform.system() == "Windows":
            dumpbin = args.dumpbin or shutil.which("dumpbin")
            if not dumpbin:
                raise RuntimeError("Windows package checks require an MSVC developer environment or --dumpbin PATH")
            files = {path.name.lower(): path for path in (package / "bin").iterdir() if path.is_file()}
            missing = set()
            linkage = []
            for name, binary in sorted(files.items()):
                if binary.suffix.lower() not in (".exe", ".dll"):
                    continue
                result = subprocess.run([dumpbin, "/nologo", "/dependents", str(binary)],
                                        text=True, check=True, encoding="utf-8", errors="replace",
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
                linkage.append(result.stdout)
                for dependency in re.findall(
                        r"^\s*((?:msvcp|vcruntime|concrt)[a-z0-9_]*\.dll)\s*$",
                        result.stdout, re.IGNORECASE | re.MULTILINE):
                    if dependency.lower() not in files:
                        missing.add(f"{name} -> {dependency}")
            (logs / "package-linkage.log").write_text("\n".join(linkage), encoding="utf-8")
            if missing:
                raise RuntimeError("Package is missing app-local MSVC runtime DLLs (use a Release or RelWithDebInfo build):\n"
                                   + "\n".join(sorted(missing)))
        elif platform.system() == "Linux":
            result = subprocess.run(["ldd", command[0]], text=True, check=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (logs / "package-linkage.log").write_text(result.stdout, encoding="utf-8")
            if "not found" in result.stdout:
                raise RuntimeError(f"Installed dependency is unresolved:\n{result.stdout}")
            sdl = re.search(r"libSDL3[^\n]*=>\s+(.*?)\s+\(", result.stdout)
            if not sdl or not Path(sdl.group(1)).resolve().is_relative_to(package / "lib"):
                raise RuntimeError(f"SDL3 did not resolve inside the package:\n{result.stdout}")
        elif platform.system() == "Darwin":
            dependencies = subprocess.run(["otool", "-L", command[0]], text=True, check=True,
                                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout
            commands = subprocess.run(["otool", "-l", command[0]], text=True, check=True,
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout
            (logs / "package-linkage.log").write_text(dependencies + "\n" + commands, encoding="utf-8")
            if not re.search(r"@rpath/libSDL3[^\n]*\.dylib", dependencies):
                raise RuntimeError(f"SDL3 does not use a relocatable install name:\n{dependencies}")
            if "path @executable_path/../lib (offset" not in commands:
                raise RuntimeError("Installed executable has no package-relative library RPATH")
            if not list((package / "lib").glob("libSDL3*.dylib")):
                raise RuntimeError("Installed package has no SDL3 dylib")

        def check(name: str, expected_success: bool) -> None:
            result = subprocess.run(command, cwd=work, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    encoding="utf-8", errors="replace", timeout=90)
            (logs / f"package-{name}.log").write_text(result.stdout, encoding="utf-8")
            print(f"[{name}] exit={result.returncode}\n{result.stdout}", flush=True)
            if (result.returncode == 0) != expected_success:
                raise RuntimeError(f"Package check '{name}' returned {result.returncode}")

        check("complete", True)
        for name, relative in (
            ("missing-common", "bin/assets/common"),
            ("missing-builtin-shaders", "bin/shaders/builtin/manifest.json"),
            ("missing-game-shaders", "bin/shaders/object_fps/manifest.json"),
        ):
            target = package / relative
            if not target.exists():
                raise RuntimeError(f"Expected staged content is absent: {relative}")
            hidden = target.with_name(target.name + ".validation-hidden")
            target.rename(hidden)
            try:
                check(name, False)
            finally:
                hidden.rename(target)


if __name__ == "__main__":
    main()
