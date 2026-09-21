"""Read the identity and executable of one installed application package."""

import json
from pathlib import Path, PurePosixPath
import re


def load_package_info(package: Path) -> dict:
    """Discover the sole app manifest without guessing the project or binary name."""
    package = package.resolve(strict=True)
    manifests = sorted((package / "share/gyo/apps").glob("*/manifest.json"))
    if len(manifests) != 1:
        raise ValueError(f"Expected exactly one installed app manifest, found {len(manifests)}")
    manifest_path = manifests[0]
    if (manifest_path.is_symlink() or not manifest_path.is_file()
            or not manifest_path.resolve(strict=True).is_relative_to(package)):
        raise ValueError("Installed app manifest must be a regular file inside the package")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    if (not isinstance(manifest, dict) or type(manifest.get("schema_version")) is not int
            or manifest["schema_version"] != 1):
        raise ValueError("Unsupported installed app manifest schema")
    app = manifest.get("app")
    if (not isinstance(app, str) or not re.fullmatch(r"[a-z][a-z0-9_]*", app)
            or manifest_path.parent.name != app):
        raise ValueError("Installed app manifest identity does not match its directory")
    executable = manifest.get("executable")
    if (not isinstance(executable, str) or not executable or "\\" in executable
            or ":" in executable or any(ord(character) < 32 for character in executable)):
        raise ValueError("Installed executable must be a normalized package-relative path")
    relative = PurePosixPath(executable)
    if (relative.is_absolute() or ".." in relative.parts or str(relative) != executable
            or executable == "."):
        raise ValueError("Installed executable must be a normalized package-relative path")
    target = package / executable
    if (target.is_symlink() or not target.is_file()
            or not target.resolve(strict=True).is_relative_to(package)):
        raise ValueError("Installed executable must be a regular file inside the package")
    return manifest
