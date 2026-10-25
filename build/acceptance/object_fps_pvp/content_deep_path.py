#!/usr/bin/env python3
"""Installed-package check (v7.1 windows-long-path, P5): the package copied to a folder deep enough that its shader
paths exceed the Windows MAX_PATH (260) still loads all of its content.

  content_deep_path.py --package PACKAGE_ROOT --probe PROBE --product PRODUCT --prefix-length 200

P (--prefix-length) is the length of the path up to and including "bin" and its separator, as in the incident
(P = 156, shader path 271). Declared in docs/object_fps_pvp/plans/v7.1/windows-long-path.md: P = 200 lies above
the shader failure depth (145) and below the next known failure, the product's own arenas.json (about 219), and
the executable itself (238). On Windows the check also requires the longest .dxil path to exceed 260 characters,
so a pass shows the long-path read and not a short layout. The probe (content_main.cpp) reads the manifest, the
catalogs, every shader bundle and every catalog asset.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

MIN_PREFIX, MAX_PREFIX = 145, 219
MAX_PATH = 260


def long_form(path: Path) -> str:
    """The form this script itself uses on a deep path: Python is long-path aware only when Windows allows it."""
    text = str(path.resolve())
    return "\\\\?\\" + text if os.name == "nt" and not text.startswith("\\\\") else text


def deep_parent(base: Path, prefix_length: int) -> Path:
    """A folder under base whose 'bin' child, with a separator, is exactly prefix_length characters long."""
    parent = base
    while True:
        rest = prefix_length - len(str(parent / "bin")) - 1
        if rest == 0:
            return parent
        if rest < 2:
            raise ValueError(f"cannot reach P = {prefix_length} from {parent}")
        # A folder of n characters adds n + 1; never leave exactly one character to add.
        parent = parent / ("d" * (rest - 1 if rest <= 41 else min(40, rest - 3)))


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--product", required=True)
    parser.add_argument("--prefix-length", required=True, type=int)
    args = parser.parse_args(argv)
    if not MIN_PREFIX <= args.prefix_length <= MAX_PREFIX:
        raise SystemExit(f"P = {args.prefix_length} is outside the declared range {MIN_PREFIX}..{MAX_PREFIX}")
    package = args.package.resolve(strict=True)
    probe = args.probe.resolve(strict=True).relative_to(package)
    base = Path(tempfile.mkdtemp(prefix="gyo-deep-"))
    try:
        parent = deep_parent(base, args.prefix_length)
        shutil.copytree(long_form(package), long_form(parent), symlinks=True)
        bin_dir = parent / "bin"
        prefix = len(str(bin_dir)) + 1
        dxil = [Path(root) / name for root, _, names in os.walk(long_form(bin_dir)) for name in names
                if name.endswith(".dxil")]
        longest = max((len(str(path).removeprefix("\\\\?\\")) for path in dxil), default=0)
        summary = {"prefix_length": prefix, "dxil_files": len(dxil), "longest_dxil_path": longest}
        if prefix != args.prefix_length:
            raise SystemExit(f"deep folder has P = {prefix}, not {args.prefix_length}")
        if os.name == "nt" and longest <= MAX_PATH:
            raise SystemExit(f"no .dxil path exceeds {MAX_PATH} characters ({summary}); the check would not test a long path")
        work = base / "work"
        work.mkdir()
        result = subprocess.run([str(parent / probe), "--asset-root", str(bin_dir / "assets" / args.product)],
                                cwd=work, text=True, encoding="utf-8", errors="replace",
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90, check=False)
        summary["probe_exit"] = result.returncode
        print(json.dumps(summary))
        print(result.stdout, end="")
        return 0 if result.returncode == 0 else 1
    finally:
        shutil.rmtree(long_form(base), ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
