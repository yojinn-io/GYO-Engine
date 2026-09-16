"""Release policy and package validation, with no network or remote writes."""

from dataclasses import dataclass
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import posixpath
import re
import subprocess
import tarfile


PLATFORMS = ("windows-x64", "linux-x64", "macos-arm64")
ARCHIVE_ROOT = "gyo-object-fps"
METADATA_PATH = f"{ARCHIVE_ROOT}/build_metadata.json"
SHA_PATTERN = re.compile(r"[0-9a-f]{40}\Z")
VERSION_PATTERN = re.compile(
    r"v(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)"
    r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?"
    r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?\Z")


class ReleaseError(RuntimeError):
    """An actionable release validation failure."""


def validate_commit(commit: str) -> str:
    if not SHA_PATTERN.fullmatch(commit):
        raise ReleaseError("Expected a full, lowercase 40-character source commit SHA")
    return commit


def git(*args: str) -> str:
    result = subprocess.run(["git", *args], text=True, capture_output=True, check=False)
    if result.returncode:
        raise ReleaseError(f"git {args[0]} failed: {result.stderr.strip()}")
    return result.stdout.strip()


def validate_version(version: str) -> str:
    match = VERSION_PATTERN.fullmatch(version) if isinstance(version, str) else None
    if match is None:
        raise ReleaseError("Version must use v-prefixed SemVer, for example v1.2.3 or v1.2.3-rc.1")
    if match.group(1):
        for identifier in match.group(1).split("."):
            if identifier.isdigit() and len(identifier) > 1 and identifier.startswith("0"):
                raise ReleaseError("Numeric SemVer prerelease identifiers may not have leading zeroes")
    return version


def parse_boolean(value) -> bool:
    if isinstance(value, bool):
        return value
    if value in ("true", "false"):
        return value == "true"
    raise ReleaseError("Prerelease must be the boolean true or false")


def prepare_event(event_name: str, event: dict, commit: str, ref: str, git_command=git) -> dict:
    """Validate a GUI workflow dispatch without creating a tag or release."""
    if event_name != "workflow_dispatch":
        raise ReleaseError("Prepare Release only accepts an explicit workflow_dispatch event")
    validate_commit(commit)
    if git_command("rev-parse", "HEAD") != commit:
        raise ReleaseError("The checked-out commit does not match the event commit")
    if (not isinstance(ref, str) or not ref.startswith("refs/heads/")
            or any(ord(char) < 32 or ord(char) == 127 for char in ref)):
        raise ReleaseError("Select a branch in Prepare Release; tag refs are not accepted")
    git_command("check-ref-format", ref)
    inputs = event.get("inputs", {})
    if not isinstance(inputs, dict):
        raise ReleaseError("Missing Prepare Release form inputs")
    tag = validate_version(inputs.get("version", ""))
    prerelease = parse_boolean(inputs.get("prerelease", False))
    git_command("check-ref-format", f"refs/tags/{tag}")
    existing = git_command("tag", "--list", tag)
    if existing:
        if existing != tag or git_command("rev-parse", "--verify", f"refs/tags/{tag}^{{commit}}") != commit:
            raise ReleaseError("Existing version tag does not point to the exact selected commit")
    return {"tag": tag, "commit": commit, "prerelease": str(prerelease).lower()}


def archive_name(platform: str) -> str:
    if platform not in PLATFORMS:
        raise ReleaseError(f"Unexpected package platform: {platform}")
    return f"gyo-object-fps-{platform}.tar.gz"


def expected_asset_names() -> set[str]:
    return {name for platform in PLATFORMS
            for name in (archive_name(platform), archive_name(platform) + ".sha256")}


def checksum(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def checksum_document(archive: str, data: bytes) -> bytes:
    return f"{checksum(data)}  {archive}\n".encode("ascii")


def validate_checksum(archive: str, data: bytes, document: bytes) -> None:
    if document.rstrip(b"\r\n") != checksum_document(archive, data).rstrip(b"\n"):
        raise ReleaseError(f"SHA256 mismatch or invalid checksum filename: {archive}")


def _safe_member_name(name: str) -> None:
    path = PurePosixPath(name)
    if (not name or "\\" in name or ":" in name or path.is_absolute()
            or ".." in path.parts or not path.parts or path.parts[0] != ARCHIVE_ROOT):
        raise ReleaseError(f"Unsafe archive member: {name!r}")


def validate_archive(name: str, data: bytes, platform: str, commit: str) -> None:
    """Inspect tar members without extracting anything to the filesystem."""
    validate_commit(commit)
    executable = f"{ARCHIVE_ROOT}/bin/gyo_object_fps" + (".exe" if platform == "windows-x64" else "")
    required_files = {
        executable,
        f"{ARCHIVE_ROOT}/bin/assets/common/asset_catalog.json",
        f"{ARCHIVE_ROOT}/bin/assets/object_fps/asset_catalog.json",
        f"{ARCHIVE_ROOT}/bin/shaders/builtin/manifest.json",
        f"{ARCHIVE_ROOT}/bin/shaders/object_fps/manifest.json",
    }
    try:
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
            seen = set()
            metadata = None
            for index, member in enumerate(archive):
                if index >= 100_000:
                    raise ReleaseError(f"Too many archive members: {name}")
                _safe_member_name(member.name)
                normalized = str(PurePosixPath(member.name))
                if normalized in seen:
                    raise ReleaseError(f"Duplicate archive member: {member.name}")
                seen.add(normalized)
                if member.issym() or member.islnk():
                    if "\\" in member.linkname or ":" in member.linkname:
                        raise ReleaseError(f"Unsafe archive link: {member.name}")
                    destination = (posixpath.join(posixpath.dirname(member.name), member.linkname)
                                   if member.issym() else member.linkname)
                    _safe_member_name(posixpath.normpath(destination))
                elif not (member.isfile() or member.isdir()):
                    raise ReleaseError(f"Unsupported special archive member: {member.name}")
                if normalized in required_files:
                    if not member.isfile() or member.size <= 0:
                        raise ReleaseError(f"Required package file must be nonempty and regular: {member.name}")
                    if normalized == executable and platform != "windows-x64" and not member.mode & 0o100:
                        raise ReleaseError(f"Package executable lacks owner execute permission: {member.name}")
                if normalized == METADATA_PATH:
                    if not member.isfile() or member.size > 64 * 1024:
                        raise ReleaseError(f"Invalid build metadata member: {name}")
                    stream = archive.extractfile(member)
                    metadata = json.loads(stream.read())
            if missing := sorted(required_files - seen):
                raise ReleaseError(f"Missing required package files in {name}: {missing}")
            if not isinstance(metadata, dict):
                raise ReleaseError(f"Missing build_metadata.json: {name}")
            if metadata.get("source_revision") != commit or metadata.get("platform") != platform:
                raise ReleaseError(f"Package provenance does not match {platform} at {commit}: {name}")
            smoke = metadata.get("ci_smoke", {})
            if (not isinstance(smoke, dict) or smoke.get("source_revision") != commit
                    or smoke.get("platform") != platform or smoke.get("profile") != "release"
                    or smoke.get("startup") != "passed" or smoke.get("headless") != "passed"):
                raise ReleaseError(f"Missing successful release startup/gameplay smoke evidence: {name}")
            rendering = smoke.get("rendering", {})
            if not isinstance(rendering, dict):
                raise ReleaseError(f"Invalid rendering smoke evidence: {name}")
            if platform == "linux-x64":
                if (rendering.get("status") != "passed"
                        or rendering.get("driver") != "vulkan" or rendering.get("suite") != "full"
                        or rendering.get("implementation") != "mesa-lavapipe"
                        or type(rendering.get("case_count")) is not int
                        or rendering["case_count"] != 8):
                    raise ReleaseError(f"Missing successful Linux Vulkan CI smoke evidence: {name}")
            elif rendering.get("status") != "not_run":
                raise ReleaseError(f"Hosted Windows/macOS package may not claim physical GPU acceptance: {name}")
    except (tarfile.TarError, OSError, ValueError, UnicodeError) as error:
        raise ReleaseError(f"Invalid package archive {name}: {error}") from error


@dataclass(frozen=True)
class Package:
    platform: str
    name: str
    data: bytes
    checksum_data: bytes


def load_packages(directory: Path, commit: str) -> list[Package]:
    validate_commit(commit)
    if not directory.is_dir():
        raise ReleaseError(f"Package directory does not exist: {directory}")
    names = {path.name for path in directory.iterdir() if path.is_file()}
    if names != expected_asset_names():
        missing = sorted(expected_asset_names() - names)
        extra = sorted(names - expected_asset_names())
        raise ReleaseError(f"Expected all three platform packages and checksums; missing={missing}, extra={extra}")
    packages = []
    for platform in PLATFORMS:
        name = archive_name(platform)
        data = (directory / name).read_bytes()
        document = (directory / (name + ".sha256")).read_bytes()
        validate_checksum(name, data, document)
        validate_archive(name, data, platform, commit)
        packages.append(Package(platform, name, data, document))
    return packages
