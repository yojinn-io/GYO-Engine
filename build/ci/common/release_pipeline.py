#!/usr/bin/env python3
"""Validate a manual release request and prepare a verified, unpublished draft."""

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

from release_support import (PLATFORMS, Package, ReleaseError, archive_name, checksum_document, load_packages,
                             parse_boolean, prepare_event, validate_archive, validate_checksum,
                             validate_commit, validate_version, validate_expected_pairs)
from app_registry import release_products


class ApiError(ReleaseError):
    def __init__(self, status: int, message: str, retry_after: float | None = None):
        super().__init__(f"GitHub API returned HTTP {status}: {message}")
        self.status = status
        self.retry_after = retry_after


class TransientApiError(ReleaseError):
    """An incomplete response whose remote effects must be reconciled before retrying."""


RETRY_DELAYS = (2, 4, 8)
TRANSIENT_HTTP_STATUS = frozenset((500, 502, 503, 504))
MANAGED_PACKAGE_ASSET = re.compile(
    r"gyo-[a-z][a-z0-9_]*-(?:" + "|".join(re.escape(platform) for platform in PLATFORMS)
    + r")\.tar\.gz(?:\.sha256)?\Z")


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
        return result if binary else json.loads(result)

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


def find_release(api, tag: str):
    # The tag endpoint is not sufficient for drafts. Authenticated release lists
    # include drafts for a token with push access; examine every page for conflicts.
    matches = []
    for page in range(1, 101):
        batch = api.request("GET", f"/releases?per_page=100&page={page}")
        matches.extend(release for release in batch if release.get("tag_name") == tag)
        if len(batch) < 100:
            if len(matches) > 1:
                raise ReleaseError("Multiple releases use this version tag; resolve the conflict manually")
            return matches[0] if matches else None
    raise ReleaseError("Too many releases to safely identify the requested draft")


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


def plan_uploads(api, assets: dict, packages: list[Package], commit: str) -> list[tuple[str, bytes]]:
    """Verify existing assets before any mutation, tolerating nondeterministic rebuild bytes."""
    expected = {name for package in packages for name in (package.name, package.name + ".sha256")}
    unexpected = sorted(name for name in assets if MANAGED_PACKAGE_ASSET.fullmatch(name) and name not in expected)
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
            validate_archive(package.name, existing, package.product, package.platform, commit)
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
                  expected_pairs: list[tuple[str, str]]) -> dict:
    validate_commit(commit)
    validate_version(tag)
    prerelease = parse_boolean(prerelease)
    expected = validate_expected_pairs(expected_pairs)
    actual = [(package.product, package.platform) for package in packages]
    if len(actual) != len(set(actual)) or set(actual) != expected:
        raise ReleaseError("Draft preparation requires exactly the configured product/platform packages")
    for package in packages:
        if package.name != archive_name(package.product, package.platform):
            raise ReleaseError("Package filename does not match its platform")
        validate_checksum(package.name, package.data, package.checksum_data)
        validate_archive(package.name, package.data, package.product, package.platform, commit)
    release = find_release(api, tag)
    if release is not None:
        release_id = verify_draft(release, tag)
        # Inspect every existing package before uploading anything.
        uploads = plan_uploads(api, release_assets(api, release_id), packages, commit)
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
                release = api.request("POST", "/releases", {
                    "tag_name": tag, "target_commitish": commit, "name": tag,
                    "draft": True, "prerelease": prerelease, "generate_release_notes": True,
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
        uploads = plan_uploads(api, release_assets(api, release_id), packages, commit)
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
    remaining = plan_uploads(api, release_assets(api, release_id), packages, commit)
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


def prepare_draft_with_retries(api, tag: str, commit: str, prerelease: bool,
                               packages: list[Package], *, expected_pairs: list[tuple[str, str]], sleep=time.sleep) -> dict:
    # A failed POST may already have succeeded remotely. Restart reconciliation,
    # never the individual POST: verify the tag, draft and existing packages again.
    attempts = len(RETRY_DELAYS) + 1
    for attempt in range(attempts):
        try:
            return prepare_draft(api, tag, commit, prerelease, packages, expected_pairs=expected_pairs)
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


def write_outputs(path: Path, result: dict) -> None:
    with path.open("a", encoding="utf-8") as stream:
        for key, value in result.items():
            stream.write(f"{key}={value}\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prepare = commands.add_parser("prepare")
    prepare.add_argument("--event-path", type=Path, required=True)
    prepare.add_argument("--event-name", required=True)
    prepare.add_argument("--commit", required=True)
    prepare.add_argument("--ref", required=True)
    prepare.add_argument("--output", type=Path, required=True)
    prepare.add_argument("--registry", type=Path, help="Project registry override for isolated validation")
    draft = commands.add_parser("draft")
    draft.add_argument("--tag", required=True)
    draft.add_argument("--commit", required=True)
    draft.add_argument("--prerelease", choices=("true", "false"), required=True)
    draft.add_argument("--package-directory", type=Path, required=True)
    draft.add_argument("--output", type=Path, required=True)
    draft.add_argument("--registry", type=Path, help="Project registry override for isolated validation")
    args = parser.parse_args()
    try:
        if args.command == "prepare":
            event = json.loads(args.event_path.read_text(encoding="utf-8"))
            result = prepare_event(args.event_name, event, args.commit, args.ref)
            validate_expected_pairs(release_products(args.registry))
        else:
            # The checkout is fixed by the workflow. Rebuild expectations from its
            # CMake registry, never from downloaded artifacts or their manifests.
            from release_support import git
            if git("rev-parse", "HEAD") != args.commit:
                raise ReleaseError("Draft checkout does not match the selected source commit")
            expected = release_products(args.registry)
            packages = load_packages(args.package_directory, args.commit, expected)
            api = GitHubApi(os.environ.get("GH_REPO", ""), os.environ.get("GH_TOKEN", ""))
            result = prepare_draft_with_retries(api, args.tag, args.commit, parse_boolean(args.prerelease),
                                                packages, expected_pairs=expected)
        write_outputs(args.output, result)
        print(json.dumps(result))
    except (ReleaseError, OSError, ValueError) as error:
        parser.exit(1, f"Release validation failed: {error}\n")


if __name__ == "__main__":
    main()
