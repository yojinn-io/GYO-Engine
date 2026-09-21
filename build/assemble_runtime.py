#!/usr/bin/env python3
"""Assemble prepared runtime content for local builds and CI, using only the standard library.

Manual equivalent: copy catalog files and their referenced assets below bin/assets/<product>,
then copy compiled shader bundles to the paths declared in content.json. No asset creation,
conversion or source-tree fallback occurs here.
"""

import argparse
import json
from pathlib import Path, PurePosixPath
import re
import shutil


def relative(value):
    if (not isinstance(value, str) or not value or "\\" in value or ":" in value
            or any(ord(character) < 32 for character in value) or PurePosixPath(value).is_absolute() or ".." in PurePosixPath(value).parts
            or str(PurePosixPath(value)) != value or value == "."):
        raise ValueError(f"Expected a normalized content-relative path: {value!r}")
    return value


def document(path):
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object: {path}")
    return value


def regular(root, path):
    result = root / relative(path)
    if not result.is_file() or result.is_symlink() or not result.resolve().is_relative_to(root.resolve()):
        raise ValueError(f"Required runtime source is absent or outside its root: {result}")
    return result


def content_files(assets, shaders):
    content = document(regular(assets, "content.json"))
    if (type(content.get("version")) is not int or content["version"] != 1
            or not isinstance(content.get("catalogs"), list)
            or not isinstance(content.get("shader_bundles"), list)):
        raise ValueError(f"{assets / 'content.json'}: expected integer version 1, catalogs and shader_bundles arrays")
    files = {}
    catalog_names = set()
    for catalog_name in content["catalogs"]:
        relative(catalog_name)
        if catalog_name == "content.json" or catalog_name in catalog_names:
            raise ValueError(f"Duplicate or reserved catalog path: {catalog_name}")
        catalog_names.add(catalog_name)
        catalog_path = regular(assets, catalog_name)
        files[relative(catalog_name)] = catalog_path
        catalog = document(catalog_path)
        if type(catalog.get("version")) is not int or catalog["version"] != 1 or not isinstance(catalog.get("assets"), list):
            raise ValueError(f"{catalog_path}: expected version 1 and assets array")
        for entry in catalog["assets"]:
            if not isinstance(entry, dict):
                raise ValueError(f"{catalog_path}: invalid asset entry")
            asset_path = relative(entry.get("path"))
            if asset_path == "content.json":
                raise ValueError("Asset paths cannot replace content.json")
            files[asset_path] = regular(assets, asset_path)
    bundles = content["shader_bundles"]
    if not isinstance(bundles, list):
        raise ValueError("content.json shader_bundles must be an array")
    names, outputs, runtime_bundles = set(), set(), []
    for bundle in bundles:
        if not isinstance(bundle, dict):
            raise ValueError("content.json shader_bundles entries must be objects")
        name, output = bundle.get("name"), relative(bundle.get("path"))
        if not isinstance(name, str) or not re.fullmatch(r"[a-z][a-z0-9_]*", name) or name in names:
            raise ValueError(f"Invalid or duplicate shader bundle name: {name!r}")
        if output in outputs or any(output.startswith(p + "/") or p.startswith(output + "/") for p in outputs):
            raise ValueError(f"Overlapping shader output: {output}")
        for occupied in {*files, "content.json"}:
            if output == occupied or occupied.startswith(output + "/") or output.startswith(occupied + "/"):
                raise ValueError(f"Shader output overlaps catalog content: {output} / {occupied}")
        names.add(name)
        outputs.add(output)
        if name not in shaders:
            raise ValueError(f"Missing compiled shader bundle --shader {name}=<directory>")
        source = shaders[name].resolve(strict=True)
        shader_manifest = document(regular(source, "manifest.json"))
        shader_files = {"manifest.json"}
        for program in shader_manifest.get("programs", []):
            for variant in program.get("variants", []):
                for stage in ("vertex", "fragment"):
                    if stage in variant:
                        shader_files.add(relative(variant[stage].get("file")))
        for shader_path in sorted(shader_files):
            destination = output + "/" + shader_path
            files[destination] = regular(source, shader_path)
        runtime_bundles.append({"name": name, "path": output})
    if set(shaders) != names:
        raise ValueError(f"Unexpected shader bundle arguments: {sorted(set(shaders) - names)}")
    return files, {"version": 1, "catalogs": content["catalogs"], "shader_bundles": runtime_bundles}


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
