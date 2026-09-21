#!/usr/bin/env python3
"""Check installed native linkage in isolation; application content checks are product-owned."""

import argparse
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "acceptance/common"))

import platform
import re
import shutil
import subprocess

sys.path.insert(0, str(Path(__file__).resolve().parent))

from workspace import TemporaryDirectory

from package_contract import load_manifest, installed_required_files


def validate_linkage(stage: Path, product: str, logs: Path, dumpbin: str | None = None) -> None:
    stage, logs = stage.resolve(strict=True), logs.resolve()
    logs.mkdir(parents=True, exist_ok=True)
    manifest = load_manifest(stage, product)
    for relative in installed_required_files(stage, manifest):
        path = stage / relative
        if not path.is_file() or path.is_symlink() or not path.stat().st_size:
            raise RuntimeError(f"Required package file must be nonempty and regular: {relative}")
    with TemporaryDirectory(prefix="gyo-package-linkage-") as temporary:
        package = Path(temporary) / "package"
        shutil.copytree(stage, package, symlinks=True)
        for executable_path in manifest["executables"].values():
            executable = str(package / executable_path)
            if platform.system() == "Windows":
                dumpbin = dumpbin or shutil.which("dumpbin")
                if not dumpbin:
                    raise RuntimeError("Windows linkage checks require an MSVC environment or --dumpbin")
                files = {path.name.lower(): path for path in (package / "bin").iterdir() if path.is_file()}
                missing, linkage = set(), []
                for name, binary in sorted(files.items()):
                    if binary.suffix.lower() not in (".exe", ".dll"):
                        continue
                    result = subprocess.run([dumpbin, "/nologo", "/dependents", str(binary)], text=True,
                                            check=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                                            stderr=subprocess.STDOUT, timeout=30)
                    linkage.append(result.stdout)
                    for dependency in re.findall(r"^\s*((?:msvcp|vcruntime|concrt)[a-z0-9_]*\.dll)\s*$",
                                                 result.stdout, re.IGNORECASE | re.MULTILINE):
                        if dependency.lower() not in files:
                            missing.add(f"{name} -> {dependency}")
                (logs / ("package-linkage-" + Path(executable_path).name + ".log")).write_text("\n".join(linkage), encoding="utf-8")
                if missing:
                    raise RuntimeError("Package is missing product-local MSVC runtime DLLs:\n" + "\n".join(sorted(missing)))
                if "SDL3" in manifest["runtime_dependencies"] and "sdl3.dll" not in files:
                    raise RuntimeError("Package is missing SDL3.dll")
            elif platform.system() == "Linux":
                result = subprocess.run(["ldd", executable], text=True, check=True, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, timeout=30)
                (logs / ("package-linkage-" + Path(executable_path).name + ".log")).write_text(result.stdout, encoding="utf-8")
                if "not found" in result.stdout:
                    raise RuntimeError(f"Installed dependency is unresolved:\n{result.stdout}")
                if "SDL3" in manifest["runtime_dependencies"]:
                    sdl = re.search(r"libSDL3[^\n]*=>\s+(.*?)\s+\(", result.stdout)
                    if not sdl or not Path(sdl.group(1)).resolve().is_relative_to(package / "lib"):
                        raise RuntimeError(f"SDL3 did not resolve inside the package:\n{result.stdout}")
            elif platform.system() == "Darwin":
                dependencies = subprocess.check_output(["otool", "-L", executable], text=True, timeout=30)
                commands = subprocess.check_output(["otool", "-l", executable], text=True, timeout=30)
                (logs / ("package-linkage-" + Path(executable_path).name + ".log")).write_text(dependencies + "\n" + commands, encoding="utf-8")
                if "SDL3" in manifest["runtime_dependencies"]:
                    if not re.search(r"@rpath/libSDL3[^\n]*\.dylib", dependencies):
                        raise RuntimeError("SDL3 does not use a relocatable install name")
                    if "path @executable_path/../lib (offset" not in commands:
                        raise RuntimeError("Installed executable has no package-relative library RPATH")
                    if not list((package / "lib").glob("libSDL3*.dylib")):
                        raise RuntimeError("Installed package has no SDL3 dylib")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", required=True, type=Path)
    parser.add_argument("--product", required=True)
    parser.add_argument("--logs", required=True, type=Path)
    parser.add_argument("--dumpbin")
    args = parser.parse_args()
    validate_linkage(args.stage, args.product, args.logs, args.dumpbin)


if __name__ == "__main__":
    main()
