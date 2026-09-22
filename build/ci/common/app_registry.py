"""Export the CMake-owned project registry; Python never interprets CSV rows."""

import argparse
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "acceptance/common"))

import subprocess
import re

sys.path.insert(0, str(Path(__file__).resolve().parent))

from workspace import TemporaryDirectory

from package_contract import PLATFORMS, validate_product


ROOT = Path(__file__).resolve().parents[3]
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
        command.extend(["-P", str(ROOT / "build/cmake/ExportAppRegistry.cmake")])
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=30)
        if result.returncode:
            raise ValueError(f"CMake project registry export failed:\n{result.stdout}\n{result.stderr}")
        document = json.loads(output.read_text(encoding="utf-8-sig"))
    if not isinstance(document, dict) or not isinstance(document.get("include"), list):
        raise ValueError("CMake registry exporter returned an invalid matrix")
    tuples = []
    for row in document["include"]:
        if not isinstance(row, dict) or row.get("platform") not in PLATFORMS:
            raise ValueError("CMake registry exporter returned an invalid product/platform tuple")
        pair = (validate_product(row.get("app")), row["platform"])
        if pair in tuples:
            raise ValueError(f"Duplicate product/platform tuple from registry: {pair}")
        tuples.append(pair)
    return tuples


def export_tools(registry: Path | None = None, *, cmake="cmake", repository_root: Path | None = None) -> dict[str, list[str]]:
    """Consume CMake's release selection, including each tool's default variant."""
    with TemporaryDirectory(prefix="gyo-tools-registry-") as work:
        output = Path(work) / "tools.json"
        command = [cmake, f"-DGYO_OUTPUT={output}"]
        if registry is not None:
            command.append(f"-DGYO_TOOL_REGISTRY_FILE={registry.resolve()}")
        if repository_root is not None:
            command.append(f"-DGYO_REPOSITORY_ROOT={repository_root.resolve()}")
        command.extend(["-P", str(ROOT / "build/cmake/ExportToolRegistry.cmake")])
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=30)
        if result.returncode:
            raise ValueError(f"CMake tool registry export failed:\n{result.stdout}\n{result.stderr}")
        document = json.loads(output.read_text(encoding="utf-8-sig"))
    if not isinstance(document, dict) or not isinstance(document.get("include"), list):
        raise ValueError("CMake tool registry exporter returned an invalid matrix")
    selected = {}
    for row in document["include"]:
        if not isinstance(row, dict) or row.get("platform") not in PLATFORMS:
            raise ValueError("CMake tool registry exporter returned an invalid platform")
        platform, tools = row["platform"], row.get("tools")
        if (platform in selected or not isinstance(tools, list) or not tools
                or any(not isinstance(tool, str) or not re.fullmatch(r"[a-z][a-z0-9_]*:[a-z][a-z0-9_]*", tool)
                       for tool in tools)
                or len({tool.split(":", 1)[0] for tool in tools}) != len(tools)):
            raise ValueError("CMake tool registry exporter returned an invalid tool selection")
        selected[platform] = tools
    if set(selected) != set(PLATFORMS):
        raise ValueError("CMake tool registry exporter must select tools for every release platform")
    return selected


def release_products(registry=None):
    """Fixed toolchain products plus explicitly enabled games; never discover apps."""
    return [("toolchain", platform) for platform in PLATFORMS] + export_registry(registry)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--registry", type=Path)
    parser.add_argument("--tool-registry", type=Path)
    parser.add_argument("--repository-root", type=Path, help="Tool source root override for isolated validation")
    parser.add_argument("--output", type=Path, help="Append GitHub Actions outputs")
    args = parser.parse_args()
    pairs = export_registry(args.registry)
    tools = export_tools(args.tool_registry, repository_root=args.repository_root)
    matrix = {"include": [dict(product=product, kind="app", platform=platform, tools=[], **TOOLCHAINS[platform]) for product, platform in pairs]}
    baseline = {"include": [dict(product="toolchain", kind="toolchain", platform=platform, tools=tools[platform], **TOOLCHAINS[platform]) for platform in PLATFORMS]}
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
