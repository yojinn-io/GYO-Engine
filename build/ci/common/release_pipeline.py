#!/usr/bin/env python3
"""Deliver one release train: a verified, unpublished draft (Prepare Release)
or a published snapshot prerelease with retention (default-branch pushes)."""

import argparse
from email.utils import parsedate_to_datetime
import http.client
import json
import os
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "acceptance/common"))

import re
import time
import urllib.error
import urllib.parse
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parent))

from release_support import (PLATFORMS, Package, ReleaseError, archive_name, checksum_document, describe_item,
                             load_packages, load_services, parse_boolean, service_archive_name, validate_checksum,
                             validate_commit, validate_item, validate_tag_name, validate_expected_pairs)
from release_channels import (SNAPSHOT_KEEP, Train, derive_trains, latest_other_snapshot, parse_release_tag,
                              parse_snapshot_tag, prepare_event, render_notes, resolve_train,
                              select_expired_snapshots, select_orphan_snapshot_tags, snapshot_tag,
                              train_release_evidence, validate_evidence_reference)
from app_registry import export_registry, export_tools


class ApiError(ReleaseError):
    def __init__(self, status: int, message: str, retry_after: float | None = None):
        super().__init__(f"GitHub API returned HTTP {status}: {message}")
        self.status = status
        self.retry_after = retry_after


class TransientApiError(ReleaseError):
    """An incomplete response whose remote effects must be reconciled before retrying."""


RETRY_DELAYS = (2, 4, 8)
TRANSIENT_HTTP_STATUS = frozenset((500, 502, 503, 504))
_PLATFORM_ALTERNATIVES = "(?:" + "|".join(re.escape(platform) for platform in PLATFORMS) + ")"
MANAGED_PACKAGE_ASSET = re.compile(
    r"gyo-[a-z][a-z0-9_]*-" + _PLATFORM_ALTERNATIVES + r"\.tar\.gz(?:\.sha256)?\Z")
# gyo-<owner>-<role>-<platform>: disjoint from packages because an owner or a
# role never contains '-' and every platform is exactly <os>-<arch>.
MANAGED_SERVICE_ASSET = re.compile(
    r"gyo-[a-z][a-z0-9_]*-[a-z][a-z0-9_]*-" + _PLATFORM_ALTERNATIVES + r"\.tar\.gz(?:\.sha256)?\Z")


def retry_after_seconds(value: str | None) -> float | None:
    if not value:
        return None
    if value.isascii() and value.isdecimal():
        return float(value)
    try:
        return max(0, parsedate_to_datetime(value).timestamp() - time.time())
    except (ValueError, TypeError, OverflowError):
        return None


class SafeRedirectHandler(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, new_url):
        if urllib.parse.urlsplit(new_url).scheme != "https":
            raise ReleaseError("Refusing a non-HTTPS GitHub download redirect")
        redirected = super().redirect_request(request, fp, code, message, headers, new_url)
        if redirected is not None:
            # Release downloads redirect to signed object-storage URLs. Never forward credentials.
            redirected.remove_header("Authorization")
        return redirected


class GitHubApi:
    def __init__(self, repository: str, token: str):
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
            raise ReleaseError("GH_REPO must be an owner/repository name")
        if not token:
            raise ReleaseError("GH_TOKEN is required only by the gated draft preparation step")
        self.repository = repository
        self.prefix = f"/repos/{repository}"
        self.token = token
        self.opener = urllib.request.build_opener(SafeRedirectHandler())

    def _request(self, host: str, path: str, method: str = "GET", data: bytes | None = None,
                 content_type: str = "application/json", binary: bool = False):
        if host not in ("api.github.com", "uploads.github.com") or not path.startswith(self.prefix + "/"):
            raise ReleaseError("Refusing an unexpected GitHub API endpoint")
        operation = f"{method} {host}{urllib.parse.urlsplit(path).path}"
        request = urllib.request.Request(f"https://{host}{path}", data=data, method=method, headers={
            "Authorization": f"Bearer {self.token}",
            "Accept": "application/octet-stream" if binary else "application/vnd.github+json",
            "Content-Type": content_type,
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "GYO-Engine-release-pipeline",
        })
        try:
            with self.opener.open(request, timeout=120) as response:
                result = response.read()
        except urllib.error.HTTPError as error:
            # Deliberately omit headers, request data and response URLs from diagnostics.
            raise ApiError(error.code, operation,
                           retry_after_seconds(error.headers.get("Retry-After") if error.headers else None)) from None
        except urllib.error.URLError as error:
            if isinstance(error.reason, (TimeoutError, ConnectionError, http.client.IncompleteRead)):
                raise TransientApiError(f"{operation}: {type(error.reason).__name__}") from None
            raise ReleaseError(f"{operation}: connection failed ({type(error.reason).__name__})") from None
        except (TimeoutError, ConnectionError, http.client.IncompleteRead) as error:
            raise TransientApiError(f"{operation}: {type(error).__name__}") from None
        if binary:
            return result
        # DELETE answers 204 No Content.
        return json.loads(result) if result else None

    def request(self, method: str, path: str, data=None):
        encoded = None if data is None else json.dumps(data).encode("utf-8")
        return self._request("api.github.com", self.prefix + path, method, encoded)

    def download(self, asset_id: int) -> bytes:
        return self._request("api.github.com", f"{self.prefix}/releases/assets/{asset_id}", binary=True)

    def upload(self, release_id: int, name: str, data: bytes):
        query = urllib.parse.urlencode({"name": name})
        content_type = "application/gzip" if name.endswith(".tar.gz") else "text/plain"
        return self._request("uploads.github.com", f"{self.prefix}/releases/{release_id}/assets?{query}",
                             "POST", data, content_type)


def remote_tag_commit(api, tag: str) -> str | None:
    try:
        reference = api.request("GET", "/git/ref/tags/" + urllib.parse.quote(tag, safe=""))
    except ApiError as error:
        if error.status == 404:
            return None
        raise
    obj = reference.get("object", {})
    for _ in range(16):
        if obj.get("type") == "commit":
            return validate_commit(obj.get("sha", ""))
        if obj.get("type") != "tag" or not re.fullmatch(r"[0-9a-f]{40}", obj.get("sha", "")):
            raise ReleaseError("Release tag does not resolve to a commit")
        obj = api.request("GET", "/git/tags/" + obj["sha"]).get("object", {})
    raise ReleaseError("Too many nested annotated tags")


def verify_remote_tag(api, tag: str, commit: str) -> None:
    if remote_tag_commit(api, tag) != commit:
        raise ReleaseError("Remote version tag is missing or moved away from the verified commit")


def list_releases(api) -> list[dict]:
    # The tag endpoint is not sufficient for drafts. Authenticated release lists
    # include drafts for a token with push access; examine every page.
    releases = []
    for page in range(1, 101):
        batch = api.request("GET", f"/releases?per_page=100&page={page}")
        releases.extend(batch)
        if len(batch) < 100:
            return releases
    raise ReleaseError("Too many releases to safely identify the requested draft")


def find_release(api, tag: str):
    matches = [release for release in list_releases(api) if release.get("tag_name") == tag]
    if len(matches) > 1:
        raise ReleaseError("Multiple releases use this version tag; resolve the conflict manually")
    return matches[0] if matches else None


def verify_draft(release: dict, tag: str, expected_id: int | None = None) -> int:
    release_id = release.get("id")
    if (release.get("tag_name") != tag or not isinstance(release_id, int)
            or isinstance(release_id, bool) or release_id <= 0):
        raise ReleaseError("GitHub returned a different or invalid release")
    if expected_id is not None and release_id != expected_id:
        raise ReleaseError("Draft release ID changed during preparation")
    if release.get("draft") is not True or release.get("immutable", False):
        raise ReleaseError("This version is already published or immutable; no release assets will be changed")
    return release_id


def refresh_draft(api, release_id: int, tag: str, commit: str) -> dict:
    release = api.request("GET", f"/releases/{release_id}")
    verify_draft(release, tag, release_id)
    verify_remote_tag(api, tag, commit)
    return release


def release_assets(api, release_id: int) -> dict:
    assets = {}
    for page in range(1, 101):
        batch = api.request("GET", f"/releases/{release_id}/assets?per_page=100&page={page}")
        for asset in batch:
            name = asset.get("name")
            if name in assets:
                raise ReleaseError(f"Duplicate release asset name: {name}")
            assets[name] = asset
        if len(batch) < 100:
            return assets
    raise ReleaseError("Too many release assets")


def plan_uploads(api, assets: dict, packages: list[Package], commit: str,
                 *, profile: str = "release") -> list[tuple[str, bytes]]:
    """Verify existing assets before any mutation, tolerating nondeterministic rebuild bytes."""
    expected = {name for package in packages for name in (package.name, package.name + ".sha256")}
    unexpected = sorted(name for name in assets if name not in expected and (
        MANAGED_PACKAGE_ASSET.fullmatch(name) or MANAGED_SERVICE_ASSET.fullmatch(name)))
    if unexpected:
        raise ReleaseError(f"Draft contains unexpected managed application package assets: {unexpected}. "
                           "No assets were deleted or replaced.")
    uploads = []
    for package in packages:
        archive_asset = assets.get(package.name)
        checksum_name = package.name + ".sha256"
        checksum_asset = assets.get(checksum_name)
        for asset in (archive_asset, checksum_asset):
            if asset and asset.get("state") != "uploaded":
                raise TransientApiError(
                    f"Incomplete GitHub upload: {asset.get('name')} (state={asset.get('state')}). "
                    "If it remains incomplete, inspect the draft and remove only the failed upload "
                    "placeholder before rerunning the failed job; no asset was deleted automatically")
        if archive_asset:
            existing = api.download(archive_asset["id"])
            validate_item(package, existing, commit, profile=profile)
            if checksum_asset:
                validate_checksum(package.name, existing, api.download(checksum_asset["id"]))
            else:
                # Recover a prior run interrupted between the archive and checksum uploads.
                uploads.append((checksum_name, checksum_document(package.name, existing)))
        elif checksum_asset:
            # Never overwrite a checksum, even if an earlier run left it without its archive.
            validate_checksum(package.name, package.data, api.download(checksum_asset["id"]))
            uploads.append((package.name, package.data))
        else:
            uploads.extend(((package.name, package.data), (checksum_name, package.checksum_data)))
    return uploads


def prepare_draft(api, tag: str, commit: str, prerelease: bool, packages: list[Package], *,
                  expected_pairs: list[tuple[str, str]], expected_services=frozenset(),
                  profile: str = "release", notes: str | None = None, generate_notes: bool = True) -> dict:
    """Tag the commit and fill an unpublished draft with exactly the expected
    archives: native packages (expected_pairs) and service archives
    (expected_services as owner/role/platform). Notes only seed a new draft."""
    validate_commit(commit)
    validate_tag_name(tag)
    prerelease = parse_boolean(prerelease)
    expected = validate_expected_pairs(expected_pairs)
    actual = [package.identity for package in packages if package.role is None]
    services = [package.identity for package in packages if package.role is not None]
    if len(actual) != len(set(actual)) or set(actual) != expected:
        raise ReleaseError("Draft preparation requires exactly the configured product/platform packages")
    if len(services) != len(set(services)) or set(services) != set(expected_services):
        raise ReleaseError("Draft preparation requires exactly the recorded service archives")
    for package in packages:
        name = (archive_name(package.product, package.platform) if package.role is None
                else service_archive_name(package.product, package.role, package.platform))
        if package.name != name:
            raise ReleaseError("Package filename does not match its platform")
        validate_checksum(package.name, package.data, package.checksum_data)
        validate_item(package, package.data, commit, profile=profile)
    release = find_release(api, tag)
    if release is not None:
        release_id = verify_draft(release, tag)
        # Inspect every existing package before uploading anything.
        uploads = plan_uploads(api, release_assets(api, release_id), packages, commit, profile=profile)
    resolved = remote_tag_commit(api, tag)
    if resolved is not None and resolved != commit:
        raise ReleaseError("Existing version tag points to a different commit; tags are never moved")
    if resolved is None and release is not None:
        # target_commitish is not authoritative once a release has a tag. If
        # somebody removed that tag, do not guess its prior identity from a branch.
        raise ReleaseError("Existing draft has no version tag; restore its verified tag or use a new version")
    if resolved is None:
        # This is the first possible remote mutation, after complete package and draft checks.
        try:
            api.request("POST", "/git/refs", {"ref": f"refs/tags/{tag}", "sha": commit})
        except ApiError as error:
            if error.status in (403, 404):
                raise ReleaseError("Cannot create the version tag with this repository token. "
                                   "A branch changing workflow files may require an explicitly configured "
                                   "token with Contents and Workflows write permissions.") from error
            if error.status != 422:  # Another run may have created the identical ref.
                raise
        verify_remote_tag(api, tag, commit)
    if release is None:
        # Recheck for a release created while the tag request was in flight.
        release = find_release(api, tag)
        if release is None:
            try:
                # GitHub prepends a given body to generated notes.
                release = api.request("POST", "/releases", {
                    "tag_name": tag, "target_commitish": commit, "name": tag,
                    "draft": True, "prerelease": prerelease, "generate_release_notes": generate_notes,
                    **({"body": notes} if notes else {}),
                })
            except ApiError as error:
                if error.status in (403, 404):
                    raise ReleaseError("Cannot create the release draft with this repository token. "
                                       "A branch changing workflow files may require an explicitly configured "
                                       "token with Contents and Workflows write permissions.") from error
                if error.status != 422:
                    raise
                release = find_release(api, tag)
                if release is None:
                    raise
        release_id = verify_draft(release, tag)
        uploads = plan_uploads(api, release_assets(api, release_id), packages, commit, profile=profile)
    for name, data in uploads:
        # The human Publish button can be clicked while this job is active.
        # Stop immediately if the draft became public; never PATCH it back to draft.
        refresh_draft(api, release_id, tag, commit)
        print(f"Uploading draft asset: {name}", flush=True)
        try:
            api.upload(release_id, name, data)
        except ApiError as error:
            if error.status != 422:
                raise
            # A concurrent upload may have completed this exact asset. Never delete/replace it.
            refresh_draft(api, release_id, tag, commit)
            current = release_assets(api, release_id).get(name)
            if current is None or api.download(current["id"]) != data:
                raise ReleaseError(f"Draft asset already exists with different bytes: {name}") from error
        print(f"Verified draft asset: {name}")
    refresh_draft(api, release_id, tag, commit)
    remaining = plan_uploads(api, release_assets(api, release_id), packages, commit, profile=profile)
    if remaining:
        raise ReleaseError("Draft is still missing required platform assets after upload")
    release = refresh_draft(api, release_id, tag, commit)
    release_url = release.get("html_url", "")
    parsed_url = urllib.parse.urlsplit(release_url)
    if (parsed_url.scheme != "https" or parsed_url.netloc != "github.com"
            or not parsed_url.path.startswith(f"/{api.repository}/releases/")
            or any(ord(char) < 32 or ord(char) == 127 for char in release_url)):
        raise ReleaseError("GitHub returned an unexpected draft release URL")
    print(f"Draft {tag} ({release_id}) is ready with {len(packages)} verified application packages; publish it manually in GitHub")
    return {"release_id": str(release_id), "release_url": release_url, "tag": tag, "commit": commit}


def with_retries(operation, *, sleep=time.sleep):
    # A failed POST may already have succeeded remotely. Restart reconciliation,
    # never the individual POST: verify the tag, draft and existing packages again.
    attempts = len(RETRY_DELAYS) + 1
    for attempt in range(attempts):
        try:
            return operation()
        except (ApiError, TransientApiError) as error:
            if isinstance(error, ApiError) and error.status not in TRANSIENT_HTTP_STATUS:
                raise
            if attempt == len(RETRY_DELAYS):
                raise ReleaseError(
                    f"Release API recovery exhausted after {attempts} attempts: {error}. "
                    "Verified assets were preserved; rerun the failed job after the service recovers") from error
            delay = max(RETRY_DELAYS[attempt], getattr(error, "retry_after", None) or 0)
            if delay > 60:
                raise ReleaseError(
                    f"GitHub requested a {delay:g}s retry delay, beyond automatic recovery: {error}. "
                    "Rerun the failed job after that delay") from error
            print(f"Release API attempt {attempt + 1}/{attempts} failed: {error}. "
                  f"Retrying in {delay:g}s; rechecking tag, draft and uploaded checksums before any write",
                  file=sys.stderr, flush=True)
            sleep(delay)
    raise AssertionError("Unreachable retry state")


def prepare_draft_with_retries(api, tag: str, commit: str, prerelease: bool,
                               packages: list[Package], *, sleep=time.sleep, **options) -> dict:
    return with_retries(lambda: prepare_draft(api, tag, commit, prerelease, packages, **options), sleep=sleep)


def snapshot_tag_refs(api, train_id: str) -> list[str]:
    prefix = f"{train_id}-snapshot-"
    refs = api.request("GET", "/git/matching-refs/tags/" + urllib.parse.quote(prefix, safe=""))
    if not isinstance(refs, list):
        raise ReleaseError("GitHub returned an invalid tag listing")
    return [ref["ref"].removeprefix("refs/tags/") for ref in refs
            if isinstance(ref, dict) and isinstance(ref.get("ref"), str) and ref["ref"].startswith("refs/tags/")]


def delete_snapshot_tag(api, train_id: str, tag: str) -> None:
    if parse_snapshot_tag(train_id, tag) is None:
        raise ReleaseError(f"Refusing to delete a tag that is not a snapshot tag of {train_id}: {tag!r}")
    try:
        api.request("DELETE", "/git/refs/tags/" + urllib.parse.quote(tag, safe=""))
    except ApiError as error:
        # GitHub answers 422 for a missing ref; anything still present is an error.
        if error.status not in (404, 422) or remote_tag_commit(api, tag) is not None:
            raise


def delete_snapshot(api, train_id: str, release: dict) -> None:
    """Delete one expired snapshot prerelease, then its snapshot tag."""
    tag = release.get("tag_name")
    release_id = release.get("id")
    # Re-checked here so no other tag or release can ever reach a DELETE.
    if (parse_snapshot_tag(train_id, tag) is None or release.get("draft") is not False
            or release.get("prerelease") is not True or not isinstance(release_id, int) or isinstance(release_id, bool)):
        raise ReleaseError(f"Refusing to delete a release that is not a published snapshot: {tag!r}")
    print(f"Deleting expired snapshot {tag}", flush=True)
    try:
        api.request("DELETE", f"/releases/{release_id}")
    except ApiError as error:
        if error.status != 404:  # A concurrent cleanup already removed it.
            raise
    delete_snapshot_tag(api, train_id, tag)


def is_strict_ancestor(api, ancestor: str, descendant: str) -> bool:
    """True when `ancestor` is reachable from `descendant` and differs from it,
    from the repository history (GitHub's compare), not from any date."""
    validate_commit(ancestor)
    validate_commit(descendant)
    comparison = api.request("GET", f"/compare/{descendant}...{ancestor}?per_page=1")
    status = comparison.get("status") if isinstance(comparison, dict) else None
    if status not in ("ahead", "behind", "identical", "diverged"):
        raise ReleaseError("GitHub returned an invalid commit comparison")
    return status == "behind"


def superseding_snapshot(api, releases, train: Train, tag: str, commit: str) -> str | None:
    """The tag of a published snapshot whose commit descends from `commit`.

    A rerun of an older default-branch run then must not publish: publication
    order decides retention, so it would rank above the newer state."""
    latest = latest_other_snapshot(releases, train.id, tag)
    if latest is None:
        return None
    latest_commit = remote_tag_commit(api, latest["tag_name"])
    if latest_commit is None or not is_strict_ancestor(api, commit, latest_commit):
        return None
    return latest["tag_name"]


def publish_snapshot(api, train: Train, tag: str, commit: str, packages: list[Package], *,
                     expected_pairs, expected_services, notes: str, keep: int = SNAPSHOT_KEEP) -> dict:
    """Publish one train's snapshot prerelease, then apply its retention.

    The archives go through the verified draft path first and the release is
    published only when complete, so an interrupted run leaves a draft that a
    rerun reconciles. Reruns of a published snapshot only verify it.
    """
    if parse_snapshot_tag(train.id, tag) is None:
        raise ReleaseError(f"Not a snapshot tag of train {train.id}: {tag!r}")
    releases = list_releases(api)
    existing = [release for release in releases if release.get("tag_name") == tag]
    if len(existing) > 1:
        raise ReleaseError("Multiple releases use this snapshot tag; resolve the conflict manually")
    result = {"tag": tag, "commit": commit, "published": "false", "release_url": ""}
    published = bool(existing) and existing[0].get("draft") is False
    superseded = None if published else superseding_snapshot(api, releases, train, tag, commit)
    if superseded:
        # An interrupted draft of this commit, if any, is left unpublished.
        print(f"Skipping snapshot {tag}: snapshot {superseded} of {train.id} already covers a newer "
              "commit of the default branch", flush=True)
    elif published:
        release = existing[0]
        if release.get("prerelease") is not True:
            raise ReleaseError(f"Snapshot {tag} was promoted to a full release; it is never modified")
        verify_remote_tag(api, tag, commit)
        if plan_uploads(api, release_assets(api, release["id"]), packages, commit, profile="quick"):
            raise ReleaseError(f"Published snapshot {tag} lacks verified archives; it is never modified. "
                               "Delete it manually to republish this commit")
        print(f"Snapshot {tag} is already published and verified", flush=True)
        result.update(published="true", release_url=release.get("html_url", ""))
    else:
        draft = prepare_draft(api, tag, commit, True, packages, expected_pairs=expected_pairs,
                              expected_services=expected_services, profile="quick", notes=notes,
                              generate_notes=False)
        release_id = int(draft["release_id"])
        refresh_draft(api, release_id, tag, commit)
        release = api.request("PATCH", f"/releases/{release_id}",
                              {"draft": False, "prerelease": True, "make_latest": "false"})
        if (not isinstance(release, dict) or release.get("id") != release_id or release.get("tag_name") != tag
                or release.get("draft") is not False or release.get("prerelease") is not True):
            raise ReleaseError("GitHub did not publish the snapshot as a prerelease")
        verify_remote_tag(api, tag, commit)
        print(f"Published snapshot {tag} with {len(packages)} verified archives", flush=True)
        result.update(published="true", release_url=draft["release_url"])
    releases = list_releases(api)
    expired = select_expired_snapshots(releases, train.id, keep, protect={tag})
    for release in expired:
        delete_snapshot(api, train.id, release)
    # The same train's runs are serialized, so a snapshot tag without any
    # release is debris of an interrupted run, never another run's work.
    orphans = select_orphan_snapshot_tags(snapshot_tag_refs(api, train.id), releases, train.id, protect={tag})
    for orphan in orphans:
        print(f"Deleting orphaned snapshot tag {orphan}", flush=True)
        delete_snapshot_tag(api, train.id, orphan)
    result["deleted"] = ",".join([*(release["tag_name"] for release in expired), *orphans])
    return result


def write_outputs(path: Path, result: dict) -> None:
    with path.open("a", encoding="utf-8") as stream:
        for key, value in result.items():
            stream.write(f"{key}={value}\n")


def load_train_archives(train: Train, commit: str, package_directory: Path, service_directory: Path | None,
                        selected_tools: dict[str, list[str]], profile: str):
    """Exactly one train's archives: its packages and its recorded services."""
    tool_owners = {platform: {tool.split(":", 1)[0] for tool in tools} for platform, tools in selected_tools.items()}
    packages = load_packages(package_directory, commit, train.pairs, expected_tool_owners=tool_owners, profile=profile)
    services, expected_services = load_services(service_directory, commit, train.product, train.platforms)
    return packages + services, expected_services


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    registry = argparse.ArgumentParser(add_help=False)
    registry.add_argument("--registry", type=Path, help="Project registry override for isolated validation")
    registry.add_argument("--tool-registry", type=Path, help="Tool registry override for isolated validation")
    registry.add_argument("--repository-root", type=Path, help="Tool descriptor source root for isolated validation")
    registry.add_argument("--acceptance-root", type=Path,
                          help="Root holding build/acceptance/<owner>/checks.json, for isolated validation")
    prepare = commands.add_parser("prepare", parents=[registry])
    prepare.add_argument("--event-path", type=Path, required=True)
    prepare.add_argument("--event-name", required=True)
    prepare.add_argument("--commit", required=True)
    prepare.add_argument("--ref", required=True)
    prepare.add_argument("--output", type=Path, required=True)
    draft = commands.add_parser("draft", parents=[registry])
    draft.add_argument("--train", required=True)
    draft.add_argument("--tag", required=True)
    draft.add_argument("--commit", required=True)
    draft.add_argument("--prerelease", choices=("true", "false"), required=True)
    draft.add_argument("--package-directory", type=Path, required=True)
    draft.add_argument("--service-directory", type=Path, required=True)
    draft.add_argument("--l4-evidence", default="",
                       help="Real-device (L4) evidence reference; required when the train declares items")
    draft.add_argument("--output", type=Path, required=True)
    snapshot = commands.add_parser("snapshot", parents=[registry])
    snapshot.add_argument("--train", required=True)
    snapshot.add_argument("--commit", required=True)
    snapshot.add_argument("--package-directory", type=Path, required=True)
    snapshot.add_argument("--service-directory", type=Path, required=True)
    snapshot.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        selected_tools = export_tools(args.tool_registry, repository_root=args.repository_root)
        # Trains come from the fixed checkout's CMake registry, never from
        # downloaded artifacts or their manifests.
        trains = derive_trains(export_registry(args.registry))
        if args.command == "prepare":
            event = json.loads(args.event_path.read_text(encoding="utf-8"))
            result = prepare_event(args.event_name, event, args.commit, args.ref, trains=trains,
                                   evidence=lambda train: train_release_evidence(train, args.acceptance_root))
        else:
            from release_support import git
            if git("rev-parse", "HEAD") != args.commit:
                raise ReleaseError("Release checkout does not match the selected source commit")
            train = resolve_train(trains, args.train)
            profile = "release" if args.command == "draft" else "quick"
            items, expected_services = load_train_archives(train, args.commit, args.package_directory,
                                                           args.service_directory, selected_tools, profile)
            # A formal draft re-derives the required L4 items from the fixed
            # checkout; snapshots never carry real-device evidence.
            evidence = train_release_evidence(train, args.acceptance_root) if args.command == "draft" else ()
            reference = (validate_evidence_reference(train, evidence, args.l4_evidence)
                         if args.command == "draft" else "")
            notes = render_notes(train=train, commit=args.commit, profile=profile,
                                 descriptions=[describe_item(item, profile) for item in items],
                                 snapshot=args.command == "snapshot", evidence=evidence,
                                 evidence_reference=reference)
            api = GitHubApi(os.environ.get("GH_REPO", ""), os.environ.get("GH_TOKEN", ""))
            if args.command == "draft":
                parse_release_tag(train, args.tag)
                result = prepare_draft_with_retries(api, args.tag, args.commit, parse_boolean(args.prerelease),
                                                    items, expected_pairs=train.pairs,
                                                    expected_services=expected_services, notes=notes)
            else:
                committed_at = int(git("log", "-1", "--format=%ct", args.commit))
                tag = snapshot_tag(train, args.commit, committed_at)
                result = with_retries(lambda: publish_snapshot(
                    api, train, tag, args.commit, items, expected_pairs=train.pairs,
                    expected_services=expected_services, notes=notes))
        write_outputs(args.output, result)
        print(json.dumps(result))
    except (ReleaseError, OSError, ValueError) as error:
        parser.exit(1, f"Release validation failed: {error}\n")


if __name__ == "__main__":
    main()
