"""Release policy and package validation, with no network or remote writes."""

from dataclasses import dataclass
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "acceptance/common"))

import posixpath
import re
import subprocess
import tarfile

sys.path.insert(0, str(Path(__file__).resolve().parent))

from package_contract import (PLATFORMS, check_identity, manifest_path, required_checks, required_files,
                              validate_product, validate_product_namespace, validate_evidence, validate_manifest,
                              inventory_digest)
from content_contract import decode_json
from go_checks import (GO_TARGETS, SERVICE_BUILD_PLATFORM, SERVICE_RECORD_NAME, SERVICE_RECORD_VERSION,
                       selected_owners)
IDENTIFIER_PATTERN = re.compile(r"[a-z][a-z0-9_]*\Z")
SHA_PATTERN = re.compile(r"[0-9a-f]{40}\Z")


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


def validate_tag_name(tag: str) -> str:
    """A safe release tag ref name; the caller validates its train and version."""
    if (not isinstance(tag, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._+-]{0,199}", tag)
            or ".." in tag or tag.endswith((".", ".lock"))):
        raise ReleaseError(f"Invalid release tag name: {tag!r}")
    return tag


def parse_boolean(value) -> bool:
    if isinstance(value, bool):
        return value
    if value in ("true", "false"):
        return value == "true"
    raise ReleaseError("Prerelease must be the boolean true or false")


def archive_name(product: str, platform: str) -> str:
    validate_product(product)
    if platform not in PLATFORMS:
        raise ReleaseError(f"Unexpected package platform: {platform}")
    return f"gyo-{product}-{platform}.tar.gz"


def expected_asset_names(expected_pairs: list[tuple[str, str]]) -> set[str]:
    return {name for product, platform in expected_pairs
            for name in (archive_name(product, platform), archive_name(product, platform) + ".sha256")}


def validate_expected_pairs(expected_pairs: list[tuple[str, str]]) -> set[tuple[str, str]]:
    pairs = set(expected_pairs)
    if not pairs:
        raise ReleaseError("Release must contain its fixed toolchain/platform products")
    if len(pairs) != len(expected_pairs):
        raise ReleaseError("Duplicate expected product/platform tuple")
    for product, platform in pairs:
        archive_name(product, platform)
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


def validate_archive(name: str, data: bytes, product: str, platform: str, commit: str,
                     *, expected_tool_owners: set[str] | None = None, profile: str = "release") -> None:
    """Inspect tar members without extracting or executing package content.

    profile is the acceptance depth the package must prove: formal drafts
    require release evidence, snapshots publish quick evidence.
    """
    validate_commit(commit)
    if name != archive_name(product, platform):
        raise ReleaseError("Package filename does not match its product/platform")
    root = f"gyo-{product}"
    metadata_path = f"{root}/build_metadata.json"
    contract_path = f"{root}/{manifest_path(product)}"
    try:
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
            seen = {}
            documents = {}
            inventory = {}
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
                relative = str(PurePosixPath(normalized).relative_to(root))
                if member.issym():
                    inventory[relative] = {"link": member.linkname}
                elif member.isfile() or member.islnk():
                    if relative != "build_metadata.json":
                        with archive.extractfile(member) as stream:
                            inventory[relative] = {"sha256": hashlib.file_digest(stream, "sha256").hexdigest()}
                if normalized in (metadata_path, contract_path):
                    if not member.isfile() or not 0 < member.size <= 256 * 1024:
                        raise ReleaseError(f"Metadata/manifest must be nonempty and regular: {member.name}")
                    documents[normalized] = json.loads(archive.extractfile(member).read())
            if contract_path not in documents:
                raise ReleaseError(f"Missing required package manifest in {name}")
            manifest = validate_manifest(documents[contract_path], product, platform)
            def read_document(relative):
                return decode_json(archive.extractfile(f"{root}/{relative}").read(), relative)
            validate_product_namespace((str(PurePosixPath(path).relative_to(root))
                for path, member in seen.items() if not member.isdir()), product, manifest, read_document)
            if product == "toolchain" and expected_tool_owners is not None:
                owners = {executable["owner"] for executable in manifest["executables"].values()}
                if owners != expected_tool_owners:
                    raise ReleaseError(f"Toolchain owners do not match the source registry: "
                        f"missing={sorted(expected_tool_owners - owners)}, extra={sorted(owners - expected_tool_owners)}")
            required = {f"{root}/{relative}" for relative in required_files(manifest, read_document)}
            if missing := sorted(required - seen.keys()):
                raise ReleaseError(f"Missing required package files in {name}: {missing}")
            for relative in required:
                member = seen[relative]
                if member.issym() or member.islnk():
                    if str(PurePosixPath(relative).relative_to(root)) not in manifest["native_files"]:
                        raise ReleaseError(f"Only native files may be linked: {relative}")
                    visited = set()
                    while member.issym() or member.islnk():
                        if member.name in visited:
                            raise ReleaseError(f"Cyclic native library link: {relative}")
                        visited.add(member.name)
                        destination = (posixpath.join(posixpath.dirname(member.name), member.linkname)
                                       if member.issym() else member.linkname)
                        member = seen[posixpath.normpath(destination)]
                if not member.isfile() or member.size <= 0:
                    raise ReleaseError(f"Required package file must be nonempty and regular: {relative}")
            for description in manifest["executables"].values():
                executable = seen[f"{root}/{description['path']}"]
                if platform != "windows-x64" and not executable.mode & 0o100:
                    raise ReleaseError(f"Package executable lacks owner execute permission: {executable.name}")
            metadata = documents.get(metadata_path)
            if not isinstance(metadata, dict):
                raise ReleaseError(f"Missing build_metadata.json: {name}")
            if (metadata.get("source_revision") != commit or metadata.get("product") != product
                    or metadata.get("platform") != platform or metadata.get("kind") != manifest["kind"]):
                raise ReleaseError(f"Package provenance does not match {product}/{platform} at {commit}: {name}")
            validate_evidence(metadata.get("ci_smoke"), manifest, commit, profile, inventory_digest(inventory))
    except (tarfile.TarError, OSError, ValueError, UnicodeError, TypeError, KeyError, AttributeError) as error:
        raise ReleaseError(f"Invalid package archive {name}: {error}") from error


@dataclass(frozen=True)
class Package:
    """One release archive: a native product package, or (with a role) one
    Go service archive whose product field holds the service owner."""
    product: str
    platform: str
    name: str
    data: bytes
    checksum_data: bytes
    role: str | None = None

    @property
    def identity(self) -> tuple:
        return (self.product, self.platform) if self.role is None else (self.product, self.role, self.platform)


def load_packages(directory: Path, commit: str, expected_pairs: list[tuple[str, str]],
                  *, expected_tool_owners: dict[str, set[str]] | None = None,
                  profile: str = "release") -> list[Package]:
    validate_commit(commit)
    pairs = validate_expected_pairs(expected_pairs)
    if not directory.is_dir():
        raise ReleaseError(f"Package directory does not exist: {directory}")
    names = {path.name for path in directory.iterdir()}
    expected = expected_asset_names(expected_pairs)
    if names != expected:
        raise ReleaseError(f"Expected every configured product/platform package and checksum; "
                           f"missing={sorted(expected - names)}, extra={sorted(names - expected)}")
    packages = []
    for product, platform in sorted(pairs):
        name = archive_name(product, platform)
        data = (directory / name).read_bytes()
        document = (directory / (name + ".sha256")).read_bytes()
        validate_checksum(name, data, document)
        validate_archive(name, data, product, platform, commit, profile=profile,
            expected_tool_owners=expected_tool_owners[platform] if expected_tool_owners is not None else None)
        packages.append(Package(product, platform, name, data, document))
    return packages


def service_archive_name(owner: str, role: str, platform: str) -> str:
    """Matches go_checks.archive_name; '-' never occurs in an owner or role."""
    for value, label in ((owner, "owner"), (role, "role")):
        if not isinstance(value, str) or not IDENTIFIER_PATTERN.fullmatch(value):
            raise ReleaseError(f"Invalid service {label}: {value!r}")
    if platform not in PLATFORMS:
        raise ReleaseError(f"Unexpected service platform: {platform}")
    return f"gyo-{owner}-{role}-{platform}.tar.gz"


def validate_service_archive(name: str, data: bytes, owner: str, role: str, platform: str, commit: str) -> None:
    """A service archive holds exactly its executable and provenance."""
    validate_commit(commit)
    if name != service_archive_name(owner, role, platform):
        raise ReleaseError("Service archive filename does not match its owner/role/platform")
    goos, goarch, suffix = GO_TARGETS[platform]
    root = f"gyo-{owner}-{role}"
    executable = f"{root}/bin/gyo_{owner}-{role}{suffix}"
    metadata_path = f"{root}/build_metadata.json"
    try:
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
            members = {}
            for index, member in enumerate(archive):
                if index >= 16 or member.name in members:
                    raise ReleaseError(f"Unexpected or duplicate service archive member: {member.name}")
                members[member.name] = member
            if set(members) != {root, f"{root}/bin", executable, metadata_path}:
                raise ReleaseError(f"Service archive must hold exactly its executable and metadata: {name}")
            if not (members[root].isdir() and members[f"{root}/bin"].isdir()):
                raise ReleaseError(f"Service archive directories are invalid: {name}")
            binary = members[executable]
            if not binary.isfile() or binary.size <= 0 or not binary.mode & 0o100:
                raise ReleaseError(f"Service executable must be a nonempty executable file: {executable}")
            document = members[metadata_path]
            if not document.isfile() or not 0 < document.size <= 64 * 1024:
                raise ReleaseError(f"Service metadata must be nonempty and regular: {metadata_path}")
            metadata = json.loads(archive.extractfile(document).read())
    except (tarfile.TarError, OSError, ValueError, UnicodeError) as error:
        raise ReleaseError(f"Invalid service archive {name}: {error}") from error
    expected = {"source_revision": commit, "owner": owner, "role": role, "platform": platform,
                "goos": goos, "goarch": goarch, "cgo": False}
    if not isinstance(metadata, dict) or any(metadata.get(key) != value for key, value in expected.items()):
        raise ReleaseError(f"Service provenance does not match {owner}/{role}/{platform} at {commit}: {name}")
    for key in ("go_version", "build_os", "build_architecture"):
        if not isinstance(metadata.get(key), str) or not metadata[key]:
            raise ReleaseError(f"Service metadata lacks {key}: {name}")


def read_service_record(path: Path, commit: str, product: str, platforms) -> set[tuple[str, str, str]]:
    """Expected (owner, role, platform) archives from one packaging row's record."""
    try:
        record = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise ReleaseError(f"Unreadable service record {path.name}: {error}") from error
    if (not isinstance(record, dict) or record.get("version") != SERVICE_RECORD_VERSION
            or record.get("product") != product or record.get("source_revision") != commit
            or not isinstance(record.get("services"), list)):
        raise ReleaseError(f"Service record does not belong to {product} at {commit}")
    owners = selected_owners(product)
    expected = set()
    for entry in record["services"]:
        if not isinstance(entry, dict) or set(entry) != {"owner", "role", "platforms"}:
            raise ReleaseError(f"Invalid service record entry: {entry!r}")
        owner, role, targets = entry["owner"], entry["role"], entry["platforms"]
        if owner not in owners:
            raise ReleaseError(f"Service record of {product} lists another owner's service: {owner}")
        if (not isinstance(targets, list) or not targets or len(set(targets)) != len(targets)
                or any(target not in platforms for target in targets)):
            raise ReleaseError(f"Service {owner}/{role} targets platforms outside its release train: {targets!r}")
        for target in targets:
            service_archive_name(owner, role, target)
            if (owner, role, target) in expected:
                raise ReleaseError(f"Duplicate service {owner}/{role} in the record")
            expected.add((owner, role, target))
    return expected


def load_services(directory: Path | None, commit: str, product: str,
                  platforms) -> tuple[list[Package], set[tuple[str, str, str]]]:
    """Load a train's service archives exactly as its product's record declares.

    Services are built only on their product's Linux row, so a train with a
    Linux package must carry that row's record (possibly empty), and a train
    without one cannot have services.
    """
    validate_commit(commit)
    names = {path.name for path in directory.iterdir()} if directory is not None and directory.is_dir() else set()
    if SERVICE_BUILD_PLATFORM not in platforms:
        if names:
            raise ReleaseError(f"Unexpected service files for a train without a service build row: {sorted(names)}")
        return [], set()
    if SERVICE_RECORD_NAME not in names:
        raise ReleaseError(f"Missing the service record {SERVICE_RECORD_NAME} of {product}")
    expected = read_service_record(directory / SERVICE_RECORD_NAME, commit, product, platforms)
    archives = {service_archive_name(*identity): identity for identity in expected}
    wanted = {SERVICE_RECORD_NAME, *archives, *(name + ".sha256" for name in archives)}
    if names != wanted:
        raise ReleaseError(f"Expected exactly the recorded service archives and checksums; "
                           f"missing={sorted(wanted - names)}, extra={sorted(names - wanted)}")
    services = []
    for name, (owner, role, platform) in sorted(archives.items()):
        data = (directory / name).read_bytes()
        document = (directory / (name + ".sha256")).read_bytes()
        validate_checksum(name, data, document)
        validate_service_archive(name, data, owner, role, platform, commit)
        services.append(Package(owner, platform, name, data, document, role))
    return services, expected


def validate_item(item: Package, data: bytes, commit: str, *, profile: str = "release") -> None:
    if item.role is None:
        validate_archive(item.name, data, item.product, item.platform, commit, profile=profile)
    else:
        validate_service_archive(item.name, data, item.product, item.role, item.platform, commit)


def describe_item(item: Package, profile: str) -> dict:
    """Facts for release notes, read from a validated archive's own metadata."""
    root = f"gyo-{item.product}" if item.role is None else f"gyo-{item.product}-{item.role}"
    with tarfile.open(fileobj=io.BytesIO(item.data), mode="r:gz") as archive:
        metadata = json.loads(archive.extractfile(f"{root}/build_metadata.json").read())
        description = {"archive": item.name, "platform": item.platform,
                       "build_os": metadata.get("build_os"), "build_architecture": metadata.get("build_architecture")}
        if item.role is not None:
            return {**description, "kind": "service", "go_version": metadata.get("go_version")}
        manifest = json.loads(archive.extractfile(f"{root}/{manifest_path(item.product)}").read())
    passed = {check.get("name") for check in metadata["ci_smoke"]["checks"]}
    gpu = {check_identity(check) for check in required_checks(manifest, profile) if check["gpu"]}
    return {**description, "kind": "package", "profile": profile, "cpu_execution": metadata.get("cpu_execution"),
            "checks": len(passed), "gpu_checks": len(gpu & passed), "gpu_acceptance": metadata.get("gpu_acceptance")}


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
RELEASE_EVIDENCE_FIELDS = frozenset(("name", "description", "platforms"))
RELEASE_EVIDENCE_TEXT_LIMIT = 300


@dataclass(frozen=True)
class EvidenceItem:
    """One manual real-device (L4) check that CI cannot run, e.g. on a
    physical GPU or a real display."""
    name: str
    description: str
    platforms: tuple[str, ...]


def single_line_text(value, limit: int) -> bool:
    return (isinstance(value, str) and 0 < len(value) <= limit and value == value.strip()
            and not any(ord(char) < 32 or ord(char) == 127 for char in value))


def validate_release_evidence(document, source: str = "acceptance contract") -> tuple[EvidenceItem, ...]:
    """The optional `release_evidence` array of one owner's acceptance
    contract (build/acceptance/<owner>/checks.json). CMake reads only its
    `checks`; release tooling reads this array and nothing else of it."""
    if not isinstance(document, dict):
        raise ReleaseError(f"{source} must be a JSON object")
    declared = document.get("release_evidence", [])
    if not isinstance(declared, list):
        raise ReleaseError(f"{source}: release_evidence must be an array")
    items, names = [], set()
    for item in declared:
        if not isinstance(item, dict) or set(item) != RELEASE_EVIDENCE_FIELDS:
            raise ReleaseError(f"{source}: each release_evidence item has exactly name, description and platforms")
        name, description, platforms = item["name"], item["description"], item["platforms"]
        if not isinstance(name, str) or not IDENTIFIER_PATTERN.fullmatch(name) or name in names:
            raise ReleaseError(f"{source}: release_evidence names must be unique identifiers: {name!r}")
        names.add(name)
        if not single_line_text(description, RELEASE_EVIDENCE_TEXT_LIMIT):
            raise ReleaseError(f"{source}: release_evidence {name} needs a one-line description of at most "
                               f"{RELEASE_EVIDENCE_TEXT_LIMIT} characters")
        if (not isinstance(platforms, list) or not platforms or any(value not in PLATFORMS for value in platforms)
                or len(platforms) != len(set(platforms))):
            raise ReleaseError(f"{source}: release_evidence {name} needs unique platforms from {list(PLATFORMS)}")
        items.append(EvidenceItem(name, description, tuple(value for value in PLATFORMS if value in platforms)))
    return tuple(items)


def load_release_evidence(owner: str, repository_root: Path | None = None) -> tuple[EvidenceItem, ...]:
    """An owner's declared release evidence; an owner without an acceptance
    contract declares none."""
    validate_product(owner)
    relative = f"build/acceptance/{owner}/checks.json"
    path = (repository_root or REPOSITORY_ROOT) / relative
    if not path.is_file():
        return ()
    try:
        document = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as error:
        raise ReleaseError(f"Cannot read {relative}: {error}") from error
    return validate_release_evidence(document, relative)
