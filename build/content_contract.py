"""Runtime content data rules shared by assembly and package validation.

The C++ asset module enforces the same version-1 contract at runtime. Shared
fixtures under tests/common/fixtures/asset_contract keep the two boundaries in
agreement; this build-only module has no runtime or game-specific dependencies.
"""

import json
import math
from pathlib import PurePosixPath
import re


def decode_json(text, source="content JSON"):
    """Reject non-JSON constants and numeric overflow, including in metadata."""
    def reject_constant(value):
        raise ValueError(f"Non-finite JSON constant: {value}")

    def finite_float(value):
        result = float(value)
        if not math.isfinite(result):
            raise ValueError("JSON number exceeds the native finite numeric range")
        return result

    def finite_integer(value):
        # nlohmann uses a finite double for integers outside its 64-bit range.
        # Keep Python integer identity while rejecting values native parsing
        # cannot represent, rather than silently accepting arbitrary precision.
        finite_float(value)
        return int(value)

    try:
        result = json.loads(text, parse_constant=reject_constant,
                            parse_float=finite_float, parse_int=finite_integer)
        # Python also permits unpaired surrogate escapes that native UTF-8
        # JSON parsing rejects. Unknown metadata keys/values obey JSON too.
        pending = [result]
        while pending:
            value = pending.pop()
            if isinstance(value, str):
                value.encode("utf-8")
            elif isinstance(value, dict):
                pending.extend(value.keys())
                pending.extend(value.values())
            elif isinstance(value, list):
                pending.extend(value)
        return result
    except ValueError as error:
        raise ValueError(f"{source}: {error}") from error


def relative_path(value):
    if (not isinstance(value, str) or not value or "\\" in value or ":" in value
            or any(ord(character) < 32 for character in value)):
        raise ValueError(f"Expected a normalized content-relative path: {value!r}")
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or str(path) != value or value == ".":
        raise ValueError(f"Expected a normalized content-relative path: {value!r}")
    return value


def overlaps(left, right):
    return left == right or left.startswith(right + "/") or right.startswith(left + "/")


def validate_content(content):
    """Validate content.json and return its runtime-only descriptor."""
    if (not isinstance(content, dict) or type(content.get("version")) is not int
            or content["version"] != 1 or not isinstance(content.get("catalogs"), list)
            or not isinstance(content.get("shader_bundles"), list)):
        raise ValueError("content.json requires integer version 1, catalogs and shader_bundles arrays")
    catalogs = []
    for value in content["catalogs"]:
        name = relative_path(value)
        if name == "content.json" or name in catalogs:
            raise ValueError(f"Duplicate or reserved catalog path: {name}")
        catalogs.append(name)
    names, paths, bundles = set(), ["content.json", *catalogs], []
    for bundle in content["shader_bundles"]:
        if not isinstance(bundle, dict):
            raise ValueError("content.json shader_bundles entries must be objects")
        name = bundle.get("name")
        if not isinstance(name, str) or not re.fullmatch(r"[a-z][a-z0-9_]*", name) or name in names:
            raise ValueError(f"Invalid or duplicate shader bundle name: {name!r}")
        output = relative_path(bundle.get("path"))
        if any(overlaps(output, occupied) for occupied in paths):
            raise ValueError(f"Overlapping shader output: {output}")
        names.add(name)
        paths.append(output)
        bundles.append({"name": name, "path": output})
    return {"version": 1, "catalogs": catalogs, "shader_bundles": bundles}


def catalog_files(content, read_catalog):
    """Validate the complete catalog set and return its required relative files.

    Content must first pass validate_content. File existence and copying belong
    to callers; the reader may access a filesystem or an archive.
    """
    files, identities = set(content["catalogs"]), set()
    for name in content["catalogs"]:
        catalog = read_catalog(name)
        if (not isinstance(catalog, dict) or type(catalog.get("version")) is not int
                or catalog["version"] != 1 or not isinstance(catalog.get("assets"), list)):
            raise ValueError(f"{name}: expected integer version 1 and assets array")
        for entry in catalog["assets"]:
            if not isinstance(entry, dict) or any(
                    not isinstance(entry.get(field), str) or not entry[field]
                    for field in ("id", "type", "path")):
                raise ValueError(f"{name}: asset entries require nonempty string id/type/path")
            if entry["id"] in identities:
                raise ValueError(f"{name}: duplicate asset id across content catalogs: {entry['id']}")
            identities.add(entry["id"])
            path = relative_path(entry["path"])
            if path == "content.json":
                raise ValueError("Asset paths cannot replace content.json")
            if any(overlaps(path, bundle["path"]) for bundle in content["shader_bundles"]):
                raise ValueError(f"Asset path overlaps shader output: {path}")
            files.add(path)
    return files


def shader_files(manifest):
    """List bundle artifacts; shader ABI/bytecode validation remains render-owned."""
    if not isinstance(manifest, dict) or not isinstance(manifest.get("programs", []), list):
        raise ValueError("Invalid shader artifact manifest")
    files = {"manifest.json"}
    for program in manifest.get("programs", []):
        if not isinstance(program, dict) or not isinstance(program.get("variants", []), list):
            raise ValueError("Invalid shader artifact program")
        for variant in program.get("variants", []):
            if not isinstance(variant, dict):
                raise ValueError("Invalid shader artifact variant")
            for stage in ("vertex", "fragment"):
                if stage in variant:
                    if not isinstance(variant[stage], dict):
                        raise ValueError("Invalid shader artifact stage")
                    files.add(relative_path(variant[stage].get("file")))
    return files
