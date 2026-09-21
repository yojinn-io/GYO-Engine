#!/usr/bin/env python3
"""Assemble prepared runtime content for local builds and CI, using only the standard library.

Manual equivalent: copy catalog files and their referenced assets below bin/assets/<product>,
then copy compiled shader bundles to the paths declared in content.json. No asset creation,
conversion or source-tree fallback occurs here.
"""

import argparse
import json
from pathlib import Path
import re
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from content_contract import decode_json, relative_path, validate_content, catalog_files, shader_files


def document(path):
    value = decode_json(path.read_text(encoding="utf-8-sig"), str(path))
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object: {path}")
    return value


def regular(root, path):
    result = root / relative_path(path)
    if not result.is_file() or result.is_symlink() or not result.resolve().is_relative_to(root.resolve()):
        raise ValueError(f"Required runtime source is absent or outside its root: {result}")
    return result


def content_files(assets, shaders):
    content = validate_content(document(regular(assets, "content.json")))
    names = catalog_files(content, lambda name: document(regular(assets, name)))
    files = {name: regular(assets, name) for name in names}
    bundles = content["shader_bundles"]
    for bundle in bundles:
        name, output = bundle["name"], bundle["path"]
        if name not in shaders:
            raise ValueError(f"Missing compiled shader bundle --shader {name}=<directory>")
        source = shaders[name].resolve(strict=True)
        shader_manifest = document(regular(source, "manifest.json"))
        for shader_path in sorted(shader_files(shader_manifest)):
            destination = output + "/" + shader_path
            files[destination] = regular(source, shader_path)
    names = {bundle["name"] for bundle in bundles}
    if set(shaders) != names:
        raise ValueError(f"Unexpected shader bundle arguments: {sorted(set(shaders) - names)}")
    return files, content


def assemble(product, kind, stage, assets=None, shaders=None, executable=None):
    if not re.fullmatch(r"[a-z][a-z0-9_]*", product) or kind not in ("app", "toolchain"):
        raise ValueError("Invalid product identity/kind")
    shaders = shaders or {}
    stage = stage.resolve()
    destination = stage / "bin" / "assets" / product
    if destination.is_symlink() or not destination.resolve().is_relative_to(stage):
        raise ValueError("Runtime content destination escapes the selected stage")
    # Validate the complete source set before touching an earlier successful stage.
    files, content = ({}, None) if assets is None else content_files(assets.resolve(strict=True), shaders)
    if assets is None and shaders:
        raise ValueError("Shader bundles require a content descriptor")
    if executable is not None:
        executable = executable.resolve(strict=True)
        (stage / "bin").mkdir(parents=True, exist_ok=True)
        target = stage / "bin" / executable.name
        if executable != target.resolve():
            shutil.copy2(executable, target)
    if content is None:
        if destination.exists():
            shutil.rmtree(destination)
        return
    destination.mkdir(parents=True, exist_ok=True)
    wanted = set(files) | {"content.json"}
    for path in destination.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"Refusing to follow staged symbolic link: {path}")
    for path in sorted(destination.rglob("*"), key=lambda p: len(p.parts), reverse=True):
        if path.is_file() and path.relative_to(destination).as_posix() not in wanted:
            path.unlink()
        elif path.is_dir() and not any(path.iterdir()):
            path.rmdir()
    for name, source in files.items():
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.is_file() or target.read_bytes() != source.read_bytes():
            shutil.copy2(source, target)
    (destination / "content.json").write_text(json.dumps(content, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--product", required=True)
    parser.add_argument("--kind", choices=("app", "toolchain"), required=True)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--assets", type=Path)
    parser.add_argument("--shader", action="append", default=[])
    parser.add_argument("--executable", type=Path)
    args = parser.parse_args()
    shaders = {}
    try:
        for value in args.shader:
            name, path = value.split("=", 1)
            if not path or name in shaders:
                raise ValueError(f"Invalid or duplicate --shader: {value}")
            shaders[name] = Path(path)
        assemble(args.product, args.kind, args.stage, args.assets, shaders, args.executable)
    except (ValueError, OSError) as error:
        parser.exit(1, f"Runtime assembly failed: {error}\n")


if __name__ == "__main__":
    main()
