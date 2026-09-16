#!/usr/bin/env python3
"""Prepare exact-commit CI events and append verified three-platform release assets."""

import argparse
import json
import os
from pathlib import Path
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parent))

from release_support import (PLATFORMS, Package, ReleaseError, archive_name, checksum_document, load_packages,
                             prepare_event, validate_archive, validate_checksum,
                             validate_commit)


class ApiError(ReleaseError):
    def __init__(self, status: int, message: str):
        super().__init__(f"GitHub API returned HTTP {status}: {message}")
        self.status = status


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
            raise ReleaseError("GH_TOKEN is required only by the gated publish step")
        self.prefix = f"/repos/{repository}"
        self.token = token
        self.opener = urllib.request.build_opener(SafeRedirectHandler())

    def _request(self, host: str, path: str, method: str = "GET", data: bytes | None = None,
                 content_type: str = "application/json", binary: bool = False):
        if host not in ("api.github.com", "uploads.github.com") or not path.startswith(self.prefix + "/"):
            raise ReleaseError("Refusing an unexpected GitHub API endpoint")
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
            raise ApiError(error.code, error.reason) from None
        except urllib.error.URLError as error:
            raise ReleaseError(f"GitHub API connection failed: {error.reason}") from None
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


def verify_remote_tag(api, tag: str, commit: str) -> None:
    reference = api.request("GET", "/git/ref/tags/" + urllib.parse.quote(tag, safe=""))
    obj = reference.get("object", {})
    for _ in range(16):
        if obj.get("type") == "commit":
            if obj.get("sha") != commit:
                raise ReleaseError("Remote release tag moved away from the verified event commit")
            return
        if obj.get("type") != "tag" or not re.fullmatch(r"[0-9a-f]{40}", obj.get("sha", "")):
            raise ReleaseError("Release tag does not resolve to a commit")
        obj = api.request("GET", "/git/tags/" + obj["sha"]).get("object", {})
    raise ReleaseError("Too many nested annotated tags")


def find_release(api, tag: str):
    try:
        return api.request("GET", "/releases/tags/" + urllib.parse.quote(tag, safe=""))
    except ApiError as error:
        if error.status == 404:
            return None
        raise


def verify_release(release: dict, tag: str, expected_id: str) -> int:
    release_id = release.get("id")
    if (release.get("tag_name") != tag or not isinstance(release_id, int)
            or isinstance(release_id, bool) or release_id <= 0):
        raise ReleaseError("GitHub returned a different or invalid release")
    if expected_id and str(release_id) != expected_id:
        raise ReleaseError("Release ID no longer matches the published event")
    if release.get("draft", True):
        raise ReleaseError("Refusing to attach assets to an unpublished draft release")
    return release_id


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
    uploads = []
    for package in packages:
        archive_asset = assets.get(package.name)
        checksum_name = package.name + ".sha256"
        checksum_asset = assets.get(checksum_name)
        if archive_asset:
            existing = api.download(archive_asset["id"])
            validate_archive(package.name, existing, package.platform, commit)
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


def publish(api, tag: str, commit: str, expected_release_id: str, packages: list[Package]) -> int:
    validate_commit(commit)
    if sorted(package.platform for package in packages) != sorted(PLATFORMS):
        raise ReleaseError("Publishing requires exactly one package for each of the three platforms")
    for package in packages:
        if package.name != archive_name(package.platform):
            raise ReleaseError("Package filename does not match its platform")
        validate_checksum(package.name, package.data, package.checksum_data)
        validate_archive(package.name, package.data, package.platform, commit)
    if not tag or any(ord(char) < 32 or ord(char) == 127 for char in tag):
        raise ReleaseError("Invalid release tag")
    verify_remote_tag(api, tag, commit)
    release = find_release(api, tag)
    if release is None and expected_release_id:
        raise ReleaseError("The release from the published event no longer exists")
    if release is not None:
        release_id = verify_release(release, tag, expected_release_id)
        uploads = plan_uploads(api, release_assets(api, release_id), packages, commit)
    else:
        # All platform archives were validated by load_packages before this first remote write.
        try:
            release = api.request("POST", "/releases", {
                "tag_name": tag, "target_commitish": commit, "name": tag,
                "draft": False, "prerelease": "-" in tag, "generate_release_notes": True,
            })
        except ApiError as error:
            if error.status != 422:
                raise
            release = find_release(api, tag)
            if release is None:
                raise
        release_id = verify_release(release, tag, expected_release_id)
        uploads = plan_uploads(api, release_assets(api, release_id), packages, commit)
    for name, data in uploads:
        verify_remote_tag(api, tag, commit)
        try:
            api.upload(release_id, name, data)
        except ApiError as error:
            if error.status != 422:
                raise
            # A concurrent publisher may have completed this exact asset. Never delete/replace it.
            current = release_assets(api, release_id).get(name)
            if current is None or api.download(current["id"]) != data:
                raise ReleaseError(f"Release asset already exists with different bytes: {name}") from error
        print(f"Verified release asset: {name}")
    verify_remote_tag(api, tag, commit)
    remaining = plan_uploads(api, release_assets(api, release_id), packages, commit)
    if remaining:
        raise ReleaseError("Release is still missing required platform assets after upload")
    print(f"Release {tag} ({release_id}) contains all three verified platform packages")
    return release_id


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prepare = commands.add_parser("prepare")
    prepare.add_argument("--event-path", type=Path, required=True)
    prepare.add_argument("--event-name", required=True)
    prepare.add_argument("--commit", required=True)
    prepare.add_argument("--output", type=Path, required=True)
    publisher = commands.add_parser("publish")
    publisher.add_argument("--tag", required=True)
    publisher.add_argument("--commit", required=True)
    publisher.add_argument("--release-id", default="")
    publisher.add_argument("--package-directory", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "prepare":
            event = json.loads(args.event_path.read_text(encoding="utf-8"))
            result = prepare_event(args.event_name, event, args.commit)
            with args.output.open("a", encoding="utf-8") as stream:
                for key, value in result.items():
                    stream.write(f"{key}={value}\n")
            print(json.dumps(result))
        else:
            packages = load_packages(args.package_directory, args.commit)
            api = GitHubApi(os.environ.get("GH_REPO", ""), os.environ.get("GH_TOKEN", ""))
            publish(api, args.tag, args.commit, args.release_id, packages)
    except (ReleaseError, OSError, ValueError) as error:
        parser.exit(1, f"Release validation failed: {error}\n")


if __name__ == "__main__":
    main()
