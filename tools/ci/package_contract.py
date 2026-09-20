"""Data-only installed-package contract shared by runners and release validation."""

import hashlib
import json
from pathlib import Path, PurePosixPath
import re


PLATFORMS = ("windows-x64", "linux-x64", "macos-arm64")
PROFILES = ("quick", "release")


def validate_app(app: str) -> str:
    if not isinstance(app, str) or not re.fullmatch(r"[a-z][a-z0-9_]*", app):
        raise ValueError(f"Invalid application name: {app!r}")
    return app


def relative_path(value: str) -> str:
    if (not isinstance(value, str) or not value or "\\" in value or ":" in value
            or any(ord(c) < 32 for c in value)):
        raise ValueError(f"Invalid package-relative path: {value!r}")
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or str(path) != value or value == ".":
        raise ValueError(f"Invalid package-relative path: {value!r}")
    return value


def manifest_path(app: str) -> str:
    return f"share/gyo/apps/{validate_app(app)}/manifest.json"


def validate_app_namespace(paths, app: str) -> None:
    """The installed app registry is reserved for exactly this package's app."""
    validate_app(app)
    for path in paths:
        parts = PurePosixPath(path).parts
        if len(parts) > 3 and parts[:3] == ("share", "gyo", "apps") and parts[3] != app:
            raise ValueError(f"Isolated package for {app} contains another app installation: {parts[3]}")


def validate_manifest(manifest: dict, app: str, platform: str) -> dict:
    validate_app(app)
    if platform not in PLATFORMS:
        raise ValueError(f"Unsupported package platform: {platform}")
    if (not isinstance(manifest, dict) or type(manifest.get("schema_version")) is not int
            or manifest["schema_version"] != 1 or manifest.get("app") != app
            or manifest.get("platform") != platform):
        raise ValueError("Package manifest schema or app/platform identity does not match")
    relative_path(manifest.get("executable"))
    required = manifest.get("required_files")
    if not isinstance(required, list) or not all(isinstance(path, str) for path in required):
        raise ValueError("Manifest required_files must be an array of relative paths")
    for path in required:
        relative_path(path)
    dependencies = manifest.get("runtime_dependencies")
    if (not isinstance(dependencies, list) or len(dependencies) != len(set(dependencies))
            or any(item != "SDL3" for item in dependencies)):
        raise ValueError("Unsupported runtime_dependencies in package manifest")
    checks = manifest.get("checks")
    if not isinstance(checks, list):
        raise ValueError("Manifest checks must be an array")
    names = set()
    for check in checks:
        if (not isinstance(check, dict) or not isinstance(check.get("name"), str)
                or not re.fullmatch(r"[A-Za-z0-9_.-]+", check["name"]) or check["name"] in names):
            raise ValueError("Package checks must have unique, safe names")
        names.add(check["name"])
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
    startup = next((check for check in checks if check["name"] == "startup"), None)
    if (startup is None or startup["gpu"] or platform not in startup["platforms"]
            or set(startup["profiles"]) != set(PROFILES)):
        raise ValueError("Every package requires a non-GPU startup check for quick and release")
    return manifest


def load_manifest(stage: Path, app: str, platform: str | None = None) -> dict:
    manifest = json.loads((stage / manifest_path(app)).read_text(encoding="utf-8-sig"))
    return validate_manifest(manifest, app, platform or manifest.get("platform"))


def manifest_digest(manifest: dict) -> str:
    return hashlib.sha256(json.dumps(manifest, sort_keys=True, separators=(",", ":"),
                                     ensure_ascii=False).encode("utf-8")).hexdigest()


def required_checks(manifest: dict, profile: str) -> list[dict]:
    if profile not in PROFILES:
        raise ValueError(f"Unknown acceptance profile: {profile}")
    return [check for check in manifest["checks"]
            if profile in check["profiles"] and manifest["platform"] in check["platforms"]]


def validate_evidence(evidence: dict, manifest: dict, revision: str, profile: str) -> None:
    if (not isinstance(evidence, dict) or evidence.get("app") != manifest["app"]
            or evidence.get("platform") != manifest["platform"]
            or evidence.get("source_revision") != revision or evidence.get("profile") != profile
            or evidence.get("manifest_sha256") != manifest_digest(manifest)):
        raise ValueError("Acceptance evidence identity/profile/manifest does not match the package")
    results = evidence.get("checks")
    if not isinstance(results, list) or any(not isinstance(result, dict) for result in results):
        raise ValueError("Acceptance evidence checks must be an array")
    expected = {check["name"] for check in required_checks(manifest, profile)}
    names = [result.get("name") for result in results]
    if len(names) != len(set(names)) or set(names) != expected:
        raise ValueError("Acceptance evidence must contain exactly the required package checks")
    for result in results:
        if result.get("passed") is not True or type(result.get("exit_code")) is not int or result["exit_code"] != 0:
            raise ValueError(f"Package check did not pass: {result.get('name')}")


def required_files(manifest: dict) -> set[str]:
    return {manifest["executable"], manifest_path(manifest["app"]), *manifest["required_files"]}
