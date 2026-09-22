"""Read one installed game's shared product contract."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
from package_contract import load_manifest


def load_package_info(package: Path) -> dict:
    package = package.resolve(strict=True)
    manifests = sorted((package / "share/gyo/products").glob("*/manifest.json"))
    if len(manifests) != 1:
        raise ValueError(f"Expected exactly one installed product manifest, found {len(manifests)}")
    path = manifests[0]
    if path.is_symlink() or not path.resolve().is_relative_to(package):
        raise ValueError("Manifest must be a regular file inside the package")
    manifest = load_manifest(package, path.parent.name)
    if manifest["kind"] != "app":
        raise ValueError("Game acceptance requires an app product")
    for executable in manifest["executables"].values():
        target = package / executable["path"]
        if not target.is_file() or target.is_symlink() or not target.resolve().is_relative_to(package):
            raise ValueError("Installed executable must be a regular file inside the package")
    return manifest
