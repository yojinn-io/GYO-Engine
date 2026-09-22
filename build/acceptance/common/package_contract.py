"""Data-only installed-package contract shared by runners and release validation."""

import hashlib
import json
from pathlib import Path, PurePosixPath

import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from content_contract import decode_json, relative_path, validate_content, catalog_files, shader_files

PLATFORMS = ("windows-x64", "linux-x64", "macos-arm64")
PROFILES = ("quick", "release")


def validate_product(product: str) -> str:
    if not isinstance(product, str) or not re.fullmatch(r"[a-z][a-z0-9_]*", product):
        raise ValueError(f"Invalid product name: {product!r}")
    return product


def manifest_path(product: str) -> str:
    return f"share/gyo/products/{validate_product(product)}/manifest.json"


def validate_product_namespace(paths, product: str, manifest=None, read_document=None) -> None:
    """Validate file/link paths (not directories) against the installed inventory."""
    validate_product(product)
    registered = required_files(manifest, read_document) if manifest is not None else None
    for path in paths:
        parts = PurePosixPath(path).parts
        if len(parts) > 3 and parts[:3] == ("share", "gyo", "products") and parts[3] != product:
            raise ValueError(f"Isolated package for {product} contains another product installation: {parts[3]}")
        if len(parts) > 2 and parts[:2] == ("bin", "assets") and parts[2] != product:
            raise ValueError(f"Isolated package for {product} contains another product's assets: {parts[2]}")
        if registered is not None and path != "build_metadata.json" and path not in registered:
            raise ValueError(f"Unregistered file in {product} package: {path}")


def inventory_digest(entries: dict) -> str:
    """Hash relative names and bytes/link targets; exclude generated archive provenance."""
    entries = {name: value for name, value in entries.items() if name != "build_metadata.json"}
    return hashlib.sha256(json.dumps(entries, sort_keys=True, separators=(",", ":"),
                                     ensure_ascii=False).encode("utf-8")).hexdigest()


def package_digest(stage: Path) -> str:
    stage = stage.resolve(strict=True)
    entries = {}
    for path in stage.rglob("*"):
        relative = path.relative_to(stage).as_posix()
        if relative == "build_metadata.json":
            continue
        if path.is_symlink():
            if not path.resolve(strict=True).is_relative_to(stage):
                raise ValueError(f"Package symbolic link escapes its root: {relative}")
            entries[relative] = {"link": path.readlink().as_posix()}
        elif path.is_file():
            with path.open("rb") as stream:
                entries[relative] = {"sha256": hashlib.file_digest(stream, "sha256").hexdigest()}
    return inventory_digest(entries)


def validate_manifest(manifest: dict, product: str, platform: str) -> dict:
    validate_product(product)
    if platform not in PLATFORMS:
        raise ValueError(f"Unsupported package platform: {platform}")
    if (not isinstance(manifest, dict) or type(manifest.get("schema_version")) is not int
            or manifest["schema_version"] != 3 or manifest.get("product") != product
            or manifest.get("platform") != platform):
        raise ValueError("Package manifest schema or product/platform identity does not match")
    kind = manifest.get("kind")
    if kind not in ("app", "toolchain") or (kind == "toolchain") != (product == "toolchain"):
        raise ValueError("Package kind does not match its product identity")
    configuration = manifest.get("configuration")
    if not isinstance(configuration, str) or not configuration.strip():
        raise ValueError("Package configuration must be a nonempty string")
    executables = manifest.get("executables")
    if not isinstance(executables, dict) or not executables:
        raise ValueError("Package executables must be a nonempty owner/role object")
    paths, owners = set(), set()
    for name, executable in executables.items():
        if not isinstance(executable, dict):
            raise ValueError("Package executable must describe owner, role, path and runtime dependencies")
        owner, role = executable.get("owner"), executable.get("role")
        validate_product(owner)
        validate_product(role)
        if name != f"{owner}.{role}" or (kind == "app" and owner != product):
            raise ValueError("Package executable key must match its owner.role")
        owners.add(owner)
        path = executable.get("path")
        relative_path(path)
        if not path.startswith("bin/"):
            raise ValueError("Package executables must be under bin/")
        normalized = path.casefold() if platform == "windows-x64" else path
        if normalized in paths:
            raise ValueError(f"Duplicate package executable path: {path}")
        paths.add(normalized)
        dependencies = executable.get("runtime_dependencies")
        if (not isinstance(dependencies, list) or any(item != "SDL3" for item in dependencies)
                or len(dependencies) != len(set(dependencies))):
            raise ValueError("Unsupported executable runtime_dependencies in package manifest")
    required = manifest.get("required_files")
    if not isinstance(required, list) or not all(isinstance(path, str) for path in required):
        raise ValueError("Manifest required_files must be an array of relative paths")
    for path in required:
        relative_path(path)
    native = manifest.get("native_files")
    if not isinstance(native, list) or any(not isinstance(path, str) for path in native):
        raise ValueError("Manifest native_files must be an array of relative paths")
    for path in native:
        relative_path(path)
        if PurePosixPath(path).parts[0] not in ("bin", "lib"):
            raise ValueError("Native payloads must be under bin/ or lib/")
        normalized = path.casefold() if platform == "windows-x64" else path
        if normalized in paths:
            raise ValueError(f"Duplicate native or executable path: {path}")
        paths.add(normalized)
    if "runtime_dependencies" in manifest:
        raise ValueError("Package-wide runtime_dependencies have been replaced by executable dependencies")
    checks = manifest.get("checks")
    if not isinstance(checks, list):
        raise ValueError("Manifest checks must be an array")
    names = set()
    for check in checks:
        if (not isinstance(check, dict) or not isinstance(check.get("name"), str)
                or not re.fullmatch(r"[A-Za-z0-9_.-]+", check["name"])):
            raise ValueError("Package checks must have unique, safe names")
        owner = validate_product(check.get("owner"))
        if owner not in owners or check_identity(check) in names:
            raise ValueError("Package checks must have a registered owner and unique owner.name")
        names.add(check_identity(check))
        command = check.get("command")
        if not isinstance(command, list) or not command or any(not isinstance(arg, str) for arg in command):
            raise ValueError(f"Invalid argv for package check {check['name']}")
        if not command[0]:
            raise ValueError("Package check executable must not be empty")
        environment = check.get("environment")
        if not isinstance(environment, list) or any(
                not isinstance(value, str) or not re.match(r"^[A-Za-z_][A-Za-z0-9_]*=", value)
                for value in environment):
            raise ValueError(f"Invalid environment for package check {check['name']}")
        for field, allowed in (("profiles", PROFILES), ("platforms", PLATFORMS)):
            values = check.get(field)
            if (not isinstance(values, list) or not values or any(value not in allowed for value in values)
                    or len(values) != len(set(values))):
                raise ValueError(f"Invalid {field} for package check {check['name']}")
        if type(check.get("gpu")) is not bool:
            raise ValueError("Package check gpu must be a boolean")
        timeout = check.get("timeout")
        if type(timeout) not in (int, float) or not 0 < timeout <= 3600:
            raise ValueError("Package check timeout must be greater than zero and at most 3600 seconds")
        for argument in [*command, *environment]:
            for token in re.findall(r"@[A-Z_]+(?::[^@]*)?@", argument):
                if token in ("@PACKAGE_ROOT@", "@LOG_ROOT@", "@PYTHON@", "@PROFILE@", "@DRIVER@", "@CHECK_ROOT@"):
                    continue
                match = re.fullmatch(r"@(EXECUTABLE|PROBE):([a-z][a-z0-9_]*)@", token)
                if not match:
                    raise ValueError(f"Unsupported acceptance token: {token}")
                if (platform in check["platforms"] and match[1] == "EXECUTABLE"
                        and f"{owner}.{match[2]}" not in executables):
                    raise ValueError(f"Unregistered executable role: {owner}.{match[2]}")
    return manifest


def check_identity(check: dict) -> str:
    return f"{check['owner']}.{check['name']}"


def load_manifest(stage: Path, product: str, platform: str | None = None) -> dict:
    manifest = json.loads((stage / manifest_path(product)).read_text(encoding="utf-8-sig"))
    return validate_manifest(manifest, product, platform or manifest.get("platform"))


def manifest_digest(manifest: dict) -> str:
    return hashlib.sha256(json.dumps(manifest, sort_keys=True, separators=(",", ":"),
                                     ensure_ascii=False).encode("utf-8")).hexdigest()


def required_checks(manifest: dict, profile: str) -> list[dict]:
    if profile not in PROFILES:
        raise ValueError(f"Unknown acceptance profile: {profile}")
    return [check for check in manifest["checks"]
            if profile in check["profiles"] and manifest["platform"] in check["platforms"]]


def validate_evidence(evidence: dict, manifest: dict, revision: str, profile: str, package_sha256=None) -> None:
    if (not isinstance(evidence, dict) or evidence.get("product") != manifest["product"]
            or evidence.get("kind") != manifest["kind"]
            or evidence.get("platform") != manifest["platform"]
            or evidence.get("source_revision") != revision or evidence.get("profile") != profile
            or evidence.get("manifest_sha256") != manifest_digest(manifest)):
        raise ValueError("Acceptance evidence identity/profile/manifest does not match the package")
    digest = evidence.get("package_sha256")
    if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
        raise ValueError("Acceptance evidence requires the package content digest")
    if package_sha256 is not None and digest != package_sha256:
        raise ValueError("Package content changed after acceptance")
    context_digest = evidence.get("context_sha256")
    if context_digest is not None and (not isinstance(context_digest, str)
            or not re.fullmatch(r"[0-9a-f]{64}", context_digest)):
        raise ValueError("Invalid acceptance context digest")
    needs_context = any(re.search(r"@(CHECK_ROOT|PROBE:[a-z][a-z0-9_]*)@", arg)
                        for check in required_checks(manifest, profile)
                        for arg in [*check["command"], *check["environment"]])
    if needs_context and context_digest is None:
        raise ValueError("Acceptance evidence requires its external context digest")
    results = evidence.get("checks")
    if not isinstance(results, list) or any(not isinstance(result, dict) for result in results):
        raise ValueError("Acceptance evidence checks must be an array")
    expected = {"package_files", *[check_identity(check) for check in required_checks(manifest, profile)]}
    names = [result.get("name") for result in results]
    if len(names) != len(set(names)) or set(names) != expected:
        raise ValueError("Acceptance evidence must contain exactly the required package checks")
    for result in results:
        if result.get("passed") is not True or type(result.get("exit_code")) is not int or result["exit_code"] != 0:
            raise ValueError(f"Package check did not pass: {result.get('name')}")


def required_files(manifest: dict, read_document=None) -> set[str]:
    """Derive runtime requirements from installed data, never a second CMake asset list."""
    required = {*[entry["path"] for entry in manifest["executables"].values()],
                manifest_path(manifest["product"]), *manifest["required_files"], *manifest["native_files"]}
    content_path = f"bin/assets/{manifest['product']}/content.json"
    if read_document is None or content_path not in required:
        return required
    content = validate_content(read_document(content_path))
    root = str(PurePosixPath(content_path).parent)
    required.update(root + "/" + name for name in catalog_files(
        content, lambda name: read_document(root + "/" + name)))
    for bundle in content["shader_bundles"]:
        bundle_root = root + "/" + bundle["path"]
        path = bundle_root + "/manifest.json"
        required.update(bundle_root + "/" + name for name in shader_files(read_document(path)))
    return required


def installed_required_files(stage: Path, manifest: dict) -> set[str]:
    return required_files(manifest, lambda name: decode_json((stage / name).read_text(encoding="utf-8-sig"), name))


def valid_installed_file(stage: Path, relative: str, manifest: dict) -> bool:
    """Only declared native libraries may use package-local symbolic links."""
    path = stage / relative
    try:
        if not path.is_file() or not path.stat().st_size or not path.resolve(strict=True).is_relative_to(stage.resolve()):
            return False
        return not path.is_symlink() or relative in manifest["native_files"]
    except (OSError, RuntimeError):
        return False
