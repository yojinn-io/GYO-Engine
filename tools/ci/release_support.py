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


from package_contract import (PLATFORMS, manifest_path, required_files, validate_app,
                              validate_app_namespace, validate_evidence, validate_manifest)
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


def archive_name(app: str, platform: str) -> str:
    validate_app(app)
    if platform not in PLATFORMS:
        raise ReleaseError(f"Unexpected package platform: {platform}")
    return f"gyo-{app}-{platform}.tar.gz"


def expected_asset_names(expected_pairs: list[tuple[str, str]]) -> set[str]:
    return {name for app, platform in expected_pairs
            for name in (archive_name(app, platform), archive_name(app, platform) + ".sha256")}


def validate_expected_pairs(expected_pairs: list[tuple[str, str]]) -> set[tuple[str, str]]:
    pairs = set(expected_pairs)
    if not pairs:
        raise ReleaseError("Prepare Release requires at least one enabled app/platform tuple")
    if len(pairs) != len(expected_pairs):
        raise ReleaseError("Duplicate expected app/platform tuple")
    for app, platform in pairs:
        archive_name(app, platform)
    return pairs


def checksum(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def checksum_document(archive: str, data: bytes) -> bytes:
    return f"{checksum(data)}  {archive}\n".encode("ascii")


def validate_checksum(archive: str, data: bytes, document: bytes) -> None:
    if document.rstrip(b"\r\n") != checksum_document(archive, data).rstrip(b"\n"):
        raise ReleaseError(f"SHA256 mismatch or invalid checksum filename: {archive}")


def _safe_member_name(name: str, root: str) -> None:
    path = PurePosixPath(name)
    if (not name or "\\" in name or ":" in name or path.is_absolute()
            or ".." in path.parts or not path.parts or path.parts[0] != root):
        raise ReleaseError(f"Unsafe archive member: {name!r}")


def validate_archive(name: str, data: bytes, app: str, platform: str, commit: str) -> None:
    """Inspect tar members without extracting or executing package content."""
    validate_commit(commit)
    if name != archive_name(app, platform):
        raise ReleaseError("Package filename does not match its app/platform")
    root = f"gyo-{app}"
    metadata_path = f"{root}/build_metadata.json"
    contract_path = f"{root}/{manifest_path(app)}"
    try:
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
            seen = {}
            documents = {}
            for index, member in enumerate(archive):
                if index >= 100_000:
                    raise ReleaseError(f"Too many archive members: {name}")
                _safe_member_name(member.name, root)
                normalized = str(PurePosixPath(member.name))
                if normalized in seen:
                    raise ReleaseError(f"Duplicate archive member: {member.name}")
                seen[normalized] = member
                if member.issym() or member.islnk():
                    if "\\" in member.linkname or ":" in member.linkname:
                        raise ReleaseError(f"Unsafe archive link: {member.name}")
                    destination = (posixpath.join(posixpath.dirname(member.name), member.linkname)
                                   if member.issym() else member.linkname)
                    _safe_member_name(posixpath.normpath(destination), root)
                elif not (member.isfile() or member.isdir()):
                    raise ReleaseError(f"Unsupported special archive member: {member.name}")
                if normalized in (metadata_path, contract_path):
                    if not member.isfile() or not 0 < member.size <= 256 * 1024:
                        raise ReleaseError(f"Metadata/manifest must be nonempty and regular: {member.name}")
                    documents[normalized] = json.loads(archive.extractfile(member).read())
            if contract_path not in documents:
                raise ReleaseError(f"Missing required package manifest in {name}")
            validate_app_namespace((str(PurePosixPath(path).relative_to(root)) for path in seen), app)
            manifest = validate_manifest(documents[contract_path], app, platform)
            required = {f"{root}/{relative}" for relative in required_files(manifest)}
            if missing := sorted(required - seen.keys()):
                raise ReleaseError(f"Missing required package files in {name}: {missing}")
            for relative in required:
                member = seen[relative]
                if not member.isfile() or member.size <= 0:
                    raise ReleaseError(f"Required package file must be nonempty and regular: {relative}")
            executable = seen[f"{root}/{manifest['executable']}"]
            if platform != "windows-x64" and not executable.mode & 0o100:
                raise ReleaseError(f"Package executable lacks owner execute permission: {executable.name}")
            metadata = documents.get(metadata_path)
            if not isinstance(metadata, dict):
                raise ReleaseError(f"Missing build_metadata.json: {name}")
            if (metadata.get("source_revision") != commit or metadata.get("app") != app
                    or metadata.get("platform") != platform):
                raise ReleaseError(f"Package provenance does not match {app}/{platform} at {commit}: {name}")
            validate_evidence(metadata.get("ci_smoke"), manifest, commit, "release")
    except (tarfile.TarError, OSError, ValueError, UnicodeError, TypeError) as error:
        raise ReleaseError(f"Invalid package archive {name}: {error}") from error


@dataclass(frozen=True)
class Package:
    app: str
    platform: str
    name: str
    data: bytes
    checksum_data: bytes


def load_packages(directory: Path, commit: str, expected_pairs: list[tuple[str, str]]) -> list[Package]:
    validate_commit(commit)
    pairs = validate_expected_pairs(expected_pairs)
    if not directory.is_dir():
        raise ReleaseError(f"Package directory does not exist: {directory}")
    names = {path.name for path in directory.iterdir()}
    expected = expected_asset_names(expected_pairs)
    if names != expected:
        raise ReleaseError(f"Expected every configured app/platform package and checksum; "
                           f"missing={sorted(expected - names)}, extra={sorted(names - expected)}")
    packages = []
    for app, platform in sorted(pairs):
        name = archive_name(app, platform)
        data = (directory / name).read_bytes()
        document = (directory / (name + ".sha256")).read_bytes()
        validate_checksum(name, data, document)
        validate_archive(name, data, app, platform, commit)
        packages.append(Package(app, platform, name, data, document))
    return packages
