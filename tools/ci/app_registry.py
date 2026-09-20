"""Export the CMake-owned project registry; Python never interprets CSV rows."""

import argparse
import json
from pathlib import Path
import sys
import subprocess

sys.path.insert(0, str(Path(__file__).resolve().parent))

from ci_workspace import TemporaryDirectory

from package_contract import PLATFORMS, validate_app


ROOT = Path(__file__).resolve().parents[2]
TOOLCHAINS = {
    "windows-x64": dict(runner="windows-2025", preset="ci-windows", toolchain="msvc-vs2026",
                        cc="cl", cxx="cl", parallel=4),
    "linux-x64": dict(runner="ubuntu-24.04", preset="ci-linux", toolchain="gcc-14",
                      cc="gcc-14", cxx="g++-14", parallel=4),
    "macos-arm64": dict(runner="macos-15", preset="ci-macos", toolchain="appleclang-xcode16.4",
                        cc="/usr/bin/clang", cxx="/usr/bin/clang++", parallel=2),
}


def export_registry(registry: Path | None = None, *, cmake="cmake") -> list[tuple[str, str]]:
    with TemporaryDirectory(prefix="gyo-registry-") as work:
        output = Path(work) / "matrix.json"
        command = [cmake, f"-DGYO_OUTPUT={output}"]
        if registry is not None:
            command.append(f"-DGYO_REGISTRY_FILE={registry.resolve()}")
        command.extend(["-P", str(ROOT / "cmake/ExportAppRegistry.cmake")])
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=30)
        if result.returncode:
            raise ValueError(f"CMake project registry export failed:\n{result.stdout}\n{result.stderr}")
        document = json.loads(output.read_text(encoding="utf-8-sig"))
    if not isinstance(document, dict) or not isinstance(document.get("include"), list):
        raise ValueError("CMake registry exporter returned an invalid matrix")
    tuples = []
    for row in document["include"]:
        if not isinstance(row, dict) or row.get("platform") not in PLATFORMS:
            raise ValueError("CMake registry exporter returned an invalid app/platform tuple")
        pair = (validate_app(row.get("app")), row["platform"])
        if pair in tuples:
            raise ValueError(f"Duplicate app/platform tuple from registry: {pair}")
        tuples.append(pair)
    return tuples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--registry", type=Path)
    parser.add_argument("--output", type=Path, help="Append GitHub Actions outputs")
    parser.add_argument("--require-apps", action="store_true")
    args = parser.parse_args()
    pairs = export_registry(args.registry)
    if args.require_apps and not pairs:
        parser.error("Prepare Release requires at least one enabled app/platform tuple")
    matrix = {"include": [dict(app=app, platform=platform, **TOOLCHAINS[platform]) for app, platform in pairs]}
    baseline = {"include": [dict(app="", platform=platform, **TOOLCHAINS[platform]) for platform in PLATFORMS]}
    if args.output:
        with args.output.open("a", encoding="utf-8") as stream:
            stream.write(f"matrix={json.dumps(matrix, separators=(',', ':'))}\n")
            stream.write(f"baseline={json.dumps(baseline, separators=(',', ':'))}\n")
            builds = {"include": baseline["include"] + matrix["include"]}
            stream.write(f"build_matrix={json.dumps(builds, separators=(',', ':'))}\n")
            stream.write(f"has_apps={str(bool(pairs)).lower()}\n")
    print(json.dumps(matrix))


if __name__ == "__main__":
    main()
