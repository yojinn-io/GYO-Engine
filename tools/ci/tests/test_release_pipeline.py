"""Release policy tests; GitHub is faked and no remote writes are performed."""

import copy
import contextlib
import http.client
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import unittest
from unittest.mock import Mock, patch
import urllib.error
import urllib.request
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from release_pipeline import (ApiError, GitHubApi, SafeRedirectHandler, TransientApiError,
                              prepare_draft, retry_after_seconds, verify_remote_tag)
from release_support import (ARCHIVE_ROOT, METADATA_PATH, PLATFORMS, Package,
                             ReleaseError, archive_name, checksum_document,
                             load_packages, prepare_event, validate_archive)


COMMIT = "a" * 40
OTHER_COMMIT = "b" * 40


def package_contents(platform):
    # Match the deployed directory layout. These short file payloads are unit
    # fixtures, not binaries to execute; real package smoke runs in CI.
    executable = "bin/gyo_object_fps" + (".exe" if platform == "windows-x64" else "")
    return {
        executable: b"unit-test executable fixture\n",
        "bin/assets/common/asset_catalog.json": b'{"assets":[]}\n',
        "bin/assets/object_fps/asset_catalog.json": b'{"assets":[]}\n',
        "bin/shaders/builtin/manifest.json": b'{"programs":[]}\n',
        "bin/shaders/object_fps/manifest.json": b'{"programs":[]}\n',
    }


def package(platform="windows-x64", commit=COMMIT, *, extra=(), metadata_changes=None,
            timestamp=0, omit=(), empty=(), executable_mode=0o755):
    metadata = {
        "source_revision": commit, "platform": platform,
        "ci_smoke": {"source_revision": commit, "platform": platform, "headless": "passed",
                     "profile": "release", "startup": "passed",
                     "rendering": ({"status": "passed", "driver": "vulkan", "suite": "full",
                                    "implementation": "mesa-lavapipe", "case_count": 8}
                                   if platform == "linux-x64" else {"status": "not_run"})},
    }
    metadata.update(metadata_changes or {})
    output = io.BytesIO()
    with tarfile.open(fileobj=output, mode="w:gz") as archive:
        data = json.dumps(metadata).encode()
        member = tarfile.TarInfo(METADATA_PATH)
        member.mtime = timestamp
        member.size = len(data)
        archive.addfile(member, io.BytesIO(data))
        for path, data in package_contents(platform).items():
            if path in omit:
                continue
            member = tarfile.TarInfo(f"{ARCHIVE_ROOT}/{path}")
            member.mtime = timestamp
            member.mode = executable_mode if path.startswith("bin/gyo_object_fps") else 0o644
            data = b"" if path in empty else data
            member.size = len(data)
            archive.addfile(member, io.BytesIO(data))
        for member in extra:
            archive.addfile(member)
    name = archive_name(platform)
    data = output.getvalue()
    return Package(platform, name, data, checksum_document(name, data))


def packages(**kwargs):
    return [package(platform, **kwargs) for platform in PLATFORMS]


class FakeGit:
    def __init__(self, head=COMMIT, tag=None):
        self.head, self.tag = head, tag
        self.calls = []

    def __call__(self, *args):
        self.calls.append(args)
        if args == ("rev-parse", "HEAD"):
            return self.head
        if args[0] == "check-ref-format":
            if "\n" in args[1] or ".." in args[1] or args[1].endswith("/"):
                raise ReleaseError("Invalid ref")
            return ""
        if args[0] == "rev-parse":
            return self.tag
        if args[0] == "tag":
            return args[2] if self.tag is not None else ""
        raise AssertionError(args)


class FakeApi:
    def __init__(self, *, release=True, commit=COMMIT):
        self.repository = "test/repo"
        self.release = ({"id": 42, "tag_name": "v1.2.3", "draft": True, "target_commitish": COMMIT,
                         "html_url": "https://github.com/test/repo/releases/tag/untagged-example",
                         "prerelease": False, "name": "User title", "body": "User release notes"}
                        if release else None)
        self.commit = commit
        self.assets = {}
        self.content = {}
        self.mutations = []
        self.calls = []
        self.upload_attempts = 0
        self.fail_upload_at = None
        self.race_create = False
        self.race_tag = False
        self.race_upload = False
        self.annotated = False
        self.move_after_upload = False
        self.publish_after_upload = False
        self.publish_on_refresh = False
        self.other_releases = []

    def add_asset(self, name, data):
        asset_id = len(self.content) + 1
        self.assets[name] = {"id": asset_id, "name": name, "state": "uploaded"}
        self.content[asset_id] = data

    def request(self, method, path, data=None):
        self.calls.append((method, path, data))
        if method == "GET" and path.startswith("/git/ref/tags/"):
            if self.commit is None:
                raise ApiError(404, "Not found")
            return {"object": {"type": "tag" if self.annotated else "commit", "sha": self.commit}}
        if path.startswith("/git/tags/"):
            return {"object": {"type": "commit", "sha": self.commit}}
        if method == "POST" and path == "/git/refs":
            self.mutations.append((method, path, data))
            self.commit = data["sha"]
            if self.race_tag:
                raise ApiError(422, "Concurrent tag created")
            return {"object": {"type": "commit", "sha": self.commit}}
        if method == "GET" and path.startswith("/releases?"):
            page = int(path.rsplit("page=", 1)[1])
            releases = self.other_releases + ([self.release] if self.release else [])
            return copy.deepcopy(releases[(page - 1) * 100:page * 100])
        if method == "GET" and path == "/releases/42":
            if self.publish_on_refresh:
                self.release["draft"] = False
            return copy.deepcopy(self.release)
        if method == "GET" and path.startswith("/releases/42/assets?"):
            return list(self.assets.values())
        if method == "POST" and path == "/releases":
            self.mutations.append((method, path, data))
            self.release = {"id": 42, "html_url": "https://github.com/test/repo/releases/tag/untagged-example",
                            **data}
            if self.race_create:
                raise ApiError(422, "Already exists")
            return copy.deepcopy(self.release)
        raise AssertionError((method, path, data))

    def download(self, asset_id):
        return self.content[asset_id]

    def upload(self, release_id, name, data):
        self.upload_attempts += 1
        if self.upload_attempts == self.fail_upload_at:
            raise ApiError(503, "Temporarily unavailable")
        if name in self.assets:
            raise ApiError(422, "Already exists")
        self.add_asset(name, data)
        self.mutations.append(("UPLOAD", name, data))
        if self.move_after_upload:
            self.commit = OTHER_COMMIT
        if self.publish_after_upload:
            self.release["draft"] = False
        if self.race_upload:
            raise ApiError(422, "Concurrent upload completed")


class PrepareTests(unittest.TestCase):
    def event(self, version="v1.2.3", prerelease=False):
        return {"inputs": {"version": version, "prerelease": prerelease}}

    def prepare(self, event=None, git=None, ref="refs/heads/feature/rendering", name="workflow_dispatch"):
        return prepare_event(name, event or self.event(), COMMIT, ref, git or FakeGit())

    def test_manual_branch_request_is_bound_to_event_commit(self):
        git = FakeGit()
        self.assertEqual(self.prepare(git=git),
                         {"tag": "v1.2.3", "commit": COMMIT, "prerelease": "false"})
        self.assertIn(("check-ref-format", "refs/heads/feature/rendering"), git.calls)
        self.assertFalse(any(args[0] == "merge-base" for args in git.calls))

    def test_only_explicit_manual_dispatch_is_accepted(self):
        for name in ("push", "pull_request", "release", "schedule"):
            with self.subTest(name=name), self.assertRaises(ReleaseError):
                self.prepare(name=name)

    def test_branch_selection_rejects_tags_and_invalid_refs(self):
        for ref in ("refs/tags/v1.2.3", "main", "refs/heads/../bad", "refs/heads/",
                    "refs/heads/main\ninjected=true"):
            with self.subTest(ref=ref), self.assertRaises(ReleaseError):
                self.prepare(ref=ref)

    def test_new_tag_and_matching_existing_tag_are_allowed(self):
        for tag in (None, COMMIT):
            with self.subTest(tag=tag):
                self.assertEqual(self.prepare(git=FakeGit(tag=tag))["commit"], COMMIT)

    def test_wrong_checkout_and_conflicting_version_fail(self):
        for git in (FakeGit(head=OTHER_COMMIT), FakeGit(tag=OTHER_COMMIT)):
            with self.subTest(git=git), self.assertRaises(ReleaseError):
                self.prepare(git=git)

    def test_semver_and_prerelease_form_validation(self):
        for version in ("v0.0.0", "v1.2.3", "v1.2.3-rc.1", "v1.2.3+build.007",
                        "v1.2.3-alpha.1+sha.abc"):
            for value in (True, False, "true", "false"):
                with self.subTest(version=version, value=value):
                    result = self.prepare(self.event(version, value))
                    self.assertEqual(result["tag"], version)
                    self.assertEqual(result["prerelease"], str(value).lower())
        for version in ("", "1.2.3", "v01.2.3", "v1.2", "v1.2.3-01", "v1.2.3-rc..1",
                        "v1.2.3\ninjected=true", "v1.2.3 ", "v1.2.3/extra", 123):
            with self.subTest(version=version), self.assertRaises(ReleaseError):
                self.prepare(self.event(version))
        for value in ("yes", "False", "1", 1, None):
            with self.subTest(value=value), self.assertRaises(ReleaseError):
                self.prepare(self.event(prerelease=value))


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(__file__).resolve().parents[3] / "build" / "release-policy-tests"
        self.root.mkdir(parents=True, exist_ok=True)
        self.work = self.root / str(uuid.uuid4())
        self.work.mkdir()

    def tearDown(self):
        # Guard recursive removal to this test's newly created workspace directory.
        target = self.work.resolve()
        if target.parent != self.root.resolve():
            raise RuntimeError("Refusing cleanup outside the test workspace")
        shutil.rmtree(target)

    def write_packages(self, inputs=None):
        for item in inputs or packages():
            (self.work / item.name).write_bytes(item.data)
            (self.work / (item.name + ".sha256")).write_bytes(item.checksum_data)

    def test_complete_matrix_loads(self):
        self.write_packages()
        self.assertEqual(len(load_packages(self.work, COMMIT)), 3)

    def test_metadata_only_archives_fail_for_every_platform(self):
        for platform in PLATFORMS:
            item = package(platform, omit=tuple(package_contents(platform)))
            with self.subTest(platform=platform), self.assertRaisesRegex(ReleaseError, "Missing required"):
                validate_archive(item.name, item.data, platform, COMMIT)

    def test_each_required_file_must_exist_and_be_nonempty(self):
        for platform in PLATFORMS:
            for path in package_contents(platform):
                for option, message in (("omit", "Missing required"), ("empty", "nonempty and regular")):
                    item = package(platform, **{option: (path,)})
                    with self.subTest(platform=platform, path=path, option=option), self.assertRaisesRegex(ReleaseError, message):
                        validate_archive(item.name, item.data, platform, COMMIT)

    def test_required_file_cannot_be_a_directory_or_link(self):
        for platform in PLATFORMS:
            for path in package_contents(platform):
                for kind in (tarfile.DIRTYPE, tarfile.SYMTYPE, tarfile.LNKTYPE):
                    member = tarfile.TarInfo(f"{ARCHIVE_ROOT}/{path}")
                    member.type = kind
                    member.linkname = (METADATA_PATH if kind == tarfile.LNKTYPE else "fixture-target")
                    item = package(platform, omit=(path,), extra=(member,))
                    with self.subTest(platform=platform, path=path, kind=kind), self.assertRaisesRegex(ReleaseError, "nonempty and regular"):
                        validate_archive(item.name, item.data, platform, COMMIT)

    def test_unix_executable_requires_owner_execute_mode(self):
        for platform in ("linux-x64", "macos-arm64"):
            for mode in (0o644, 0o001, 0o010):
                item = package(platform, executable_mode=mode)
                with self.subTest(platform=platform, mode=mode), self.assertRaisesRegex(ReleaseError, "execute permission"):
                    validate_archive(item.name, item.data, platform, COMMIT)
        item = package("windows-x64", executable_mode=0o644)
        validate_archive(item.name, item.data, item.platform, COMMIT)

    def test_prepare_cli_writes_github_outputs(self):
        # Exercise the real subprocess entry point, including embedded Python's isolated path.
        commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
        event_path, output_path = self.work / "event.json", self.work / "outputs.txt"
        event_path.write_text(json.dumps({"inputs": {"version": "v9876.5432.10101", "prerelease": "false"}}), encoding="utf-8")
        result = subprocess.run([
            sys.executable, str(Path(__file__).resolve().parents[1] / "release_pipeline.py"),
            "prepare", "--event-path", str(event_path), "--event-name", "workflow_dispatch", "--ref", "refs/heads/main",
            "--commit", commit, "--output", str(output_path),
        ], text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output_path.read_text(encoding="utf-8"),
                         f"tag=v9876.5432.10101\ncommit={commit}\nprerelease=false\n")

    def test_missing_platform_and_unexpected_file_fail(self):
        self.write_packages(packages()[:2])
        with self.assertRaisesRegex(ReleaseError, "missing="):
            load_packages(self.work, COMMIT)
        self.write_packages()
        (self.work / "extra.txt").write_text("unexpected")
        with self.assertRaisesRegex(ReleaseError, "extra="):
            load_packages(self.work, COMMIT)

    def test_bad_sha256_fails(self):
        self.write_packages()
        (self.work / (archive_name("linux-x64") + ".sha256")).write_text("0" * 64)
        with self.assertRaisesRegex(ReleaseError, "SHA256"):
            load_packages(self.work, COMMIT)

    def test_wrong_commit_platform_and_missing_smoke_fail(self):
        for metadata in ({"source_revision": OTHER_COMMIT}, {"platform": "linux-x64"},
                         {"ci_smoke": {}}, {"ci_smoke": {"headless": "passed"}}):
            item = package(metadata_changes=metadata)
            with self.subTest(metadata=metadata), self.assertRaises(ReleaseError):
                validate_archive(item.name, item.data, item.platform, COMMIT)

    def test_linux_requires_gpu_ci_evidence(self):
        smoke = {"source_revision": COMMIT, "platform": "linux-x64", "headless": "passed",
                 "profile": "release", "startup": "passed"}
        for rendering in ({}, {"status": "not_run"},
                          {"status": "passed", "driver": "d3d12", "suite": "full", "case_count": 8},
                          {"status": "passed", "driver": "vulkan", "suite": "full", "case_count": 7},
                          {"status": "passed", "driver": "vulkan", "suite": "quick", "case_count": 1,
                           "implementation": "mesa-lavapipe"}):
            item = package("linux-x64", metadata_changes={"ci_smoke": {**smoke, "rendering": rendering}})
            with self.subTest(rendering=rendering), self.assertRaisesRegex(ReleaseError, "Vulkan"):
                validate_archive(item.name, item.data, item.platform, COMMIT)

    def test_quick_packages_cannot_be_published(self):
        smoke = {"source_revision": COMMIT, "platform": "windows-x64", "headless": "not_run",
                 "profile": "quick", "startup": "passed", "rendering": {"status": "not_run"}}
        item = package(metadata_changes={"ci_smoke": smoke})
        with self.assertRaisesRegex(ReleaseError, "release startup/gameplay"):
            validate_archive(item.name, item.data, item.platform, COMMIT)

    def test_path_traversal_absolute_and_duplicate_members_fail(self):
        for name in ("../escape", "/absolute", "gyo-object-fps/../../escape", "other/file",
                     "gyo-object-fps\\escape", "C:/escape", METADATA_PATH):
            item = package(extra=[tarfile.TarInfo(name)])
            with self.subTest(name=name), self.assertRaises(ReleaseError):
                validate_archive(item.name, item.data, item.platform, COMMIT)

    def test_relative_library_symlinks_are_allowed(self):
        link = tarfile.TarInfo(f"{ARCHIVE_ROOT}/lib/libSDL3.dylib")
        link.type, link.linkname = tarfile.SYMTYPE, "libSDL3.0.dylib"
        item = package(extra=[link])
        validate_archive(item.name, item.data, item.platform, COMMIT)

    def test_escaping_links_and_special_devices_fail(self):
        for target in ("../../escape", "/tmp/escape", "C:\\escape"):
            link = tarfile.TarInfo(f"{ARCHIVE_ROOT}/lib/link")
            link.type, link.linkname = tarfile.SYMTYPE, target
            item = package(extra=[link])
            with self.subTest(target=target), self.assertRaises(ReleaseError):
                validate_archive(item.name, item.data, item.platform, COMMIT)
        device = tarfile.TarInfo(f"{ARCHIVE_ROOT}/device")
        device.type = tarfile.CHRTYPE
        item = package(extra=[device])
        with self.assertRaises(ReleaseError):
            validate_archive(item.name, item.data, item.platform, COMMIT)


class DraftTests(unittest.TestCase):
    def setUp(self):
        redirect = contextlib.redirect_stdout(io.StringIO())
        redirect.__enter__()
        self.addCleanup(redirect.__exit__, None, None, None)

    def test_new_version_creates_tag_then_draft_then_six_assets(self):
        api = FakeApi(release=False, commit=None)
        result = prepare_draft(api, "v1.2.3", COMMIT, True, packages())
        self.assertEqual(result, {
            "release_id": "42", "release_url": api.release["html_url"], "tag": "v1.2.3", "commit": COMMIT})
        self.assertEqual(len(api.assets), 6)
        self.assertEqual(api.mutations[0], ("POST", "/git/refs", {"ref": "refs/tags/v1.2.3", "sha": COMMIT}))
        self.assertEqual(api.mutations[1][1], "/releases")
        self.assertTrue(api.release["draft"])
        self.assertTrue(api.release["prerelease"])
        self.assertEqual(api.release["target_commitish"], COMMIT)
        self.assertFalse(any(method in ("PATCH", "DELETE") for method, _, _ in api.calls))

    def test_existing_tag_is_never_recreated_or_moved(self):
        api = FakeApi(release=False)
        prepare_draft(api, "v1.2.3", COMMIT, False, packages())
        self.assertEqual(api.mutations[0][1], "/releases")
        self.assertFalse(any(path == "/git/refs" for _, path, _ in api.calls))

    def test_repeated_preparation_preserves_notes_prerelease_and_existing_archives(self):
        api = FakeApi()
        original_release = copy.deepcopy(api.release)
        prepare_draft(api, "v1.2.3", COMMIT, True, packages(timestamp=1))
        initial_mutations = len(api.mutations)
        prepare_draft(api, "v1.2.3", COMMIT, True, packages(timestamp=2))
        self.assertEqual(len(api.mutations), initial_mutations)
        self.assertEqual(api.release, original_release)

    def test_branch_target_commitish_is_allowed_with_exact_existing_tag(self):
        api = FakeApi()
        api.release["target_commitish"] = "master"
        prepare_draft(api, "v1.2.3", COMMIT, False, packages())
        self.assertEqual(len(api.assets), 6)
        self.assertEqual(api.release["target_commitish"], "master")

    def test_missing_platform_or_invalid_archive_never_creates_a_tag(self):
        for inputs in (packages()[:2], [package(commit=OTHER_COMMIT), *packages()[1:]],
                       [package(omit=("bin/gyo_object_fps.exe",)), *packages()[1:]]):
            api = FakeApi(release=False, commit=None)
            with self.subTest(inputs=inputs), self.assertRaises(ReleaseError):
                prepare_draft(api, "v1.2.3", COMMIT, False, inputs)
            self.assertEqual(api.calls, [])
            self.assertEqual(api.mutations, [])

    def test_invalid_version_fails_before_any_api_call(self):
        api = FakeApi(release=False, commit=None)
        with self.assertRaises(ReleaseError):
            prepare_draft(api, "bad/version", COMMIT, False, packages())
        self.assertEqual(api.calls, [])

    def test_published_or_immutable_release_blocks_tag_creation_and_uploads(self):
        for missing_tag in (False, True):
            for field, value in (("draft", False), ("immutable", True)):
                api = FakeApi(commit=None if missing_tag else COMMIT)
                api.release[field] = value
                with self.subTest(field=field, missing_tag=missing_tag), self.assertRaisesRegex(ReleaseError, "published or immutable"):
                    prepare_draft(api, "v1.2.3", COMMIT, False, packages())
                self.assertEqual(api.mutations, [])

    def test_existing_draft_without_tag_is_not_guessed_from_target_commitish(self):
        api = FakeApi(commit=None)
        with self.assertRaisesRegex(ReleaseError, "no version tag"):
            prepare_draft(api, "v1.2.3", COMMIT, False, packages())
        self.assertEqual(api.mutations, [])

    def test_moved_tag_prevents_mutation(self):
        api = FakeApi(commit=OTHER_COMMIT)
        with self.assertRaisesRegex(ReleaseError, "different commit"):
            prepare_draft(api, "v1.2.3", COMMIT, False, packages())
        self.assertEqual(api.mutations, [])

    def test_existing_wrong_commit_or_corrupt_checksum_blocks_all_uploads(self):
        for wrong_commit in (True, False):
            api = FakeApi()
            item = package("macos-arm64", OTHER_COMMIT if wrong_commit else COMMIT)
            api.add_asset(item.name, item.data)
            api.add_asset(item.name + ".sha256", item.checksum_data if wrong_commit else b"corrupt")
            with self.subTest(wrong_commit=wrong_commit), self.assertRaises(ReleaseError):
                prepare_draft(api, "v1.2.3", COMMIT, False, packages())
            self.assertEqual(api.mutations, [])

    def test_interrupted_upload_recovers_checksum_without_replacing_archive(self):
        api = FakeApi()
        api.fail_upload_at = 2
        with self.assertRaises(ApiError):
            prepare_draft(api, "v1.2.3", COMMIT, False, packages(timestamp=1))
        self.assertEqual(len(api.assets), 1)
        api.fail_upload_at = None
        prepare_draft(api, "v1.2.3", COMMIT, False, packages(timestamp=2))
        self.assertEqual(len(api.assets), 6)
        self.assertEqual(len(api.mutations), 6)

    def test_orphan_checksum_requires_matching_archive(self):
        api = FakeApi()
        item = package(timestamp=1)
        api.add_asset(item.name + ".sha256", item.checksum_data)
        with self.assertRaises(ReleaseError):
            prepare_draft(api, "v1.2.3", COMMIT, False, packages(timestamp=2))
        self.assertEqual(api.mutations, [])
        prepare_draft(api, "v1.2.3", COMMIT, False, [item, *packages()[1:]])
        self.assertEqual(len(api.assets), 6)

    def test_tag_is_rechecked_between_uploads(self):
        api = FakeApi()
        api.move_after_upload = True
        with self.assertRaisesRegex(ReleaseError, "moved"):
            prepare_draft(api, "v1.2.3", COMMIT, False, packages())
        self.assertEqual(len(api.mutations), 1)

    def test_premature_human_publish_stops_before_the_next_upload(self):
        for before_first in (True, False):
            api = FakeApi()
            api.publish_on_refresh = before_first
            api.publish_after_upload = not before_first
            with self.subTest(before_first=before_first), self.assertRaisesRegex(ReleaseError, "published or immutable"):
                prepare_draft(api, "v1.2.3", COMMIT, False, packages())
            self.assertEqual(len(api.mutations), 0 if before_first else 1)

    def test_tag_draft_and_asset_creation_races_are_idempotent(self):
        api = FakeApi(release=False, commit=None)
        api.race_tag = api.race_create = api.race_upload = True
        prepare_draft(api, "v1.2.3", COMMIT, False, packages())
        self.assertEqual(len(api.assets), 6)
        self.assertTrue(api.release["draft"])

    def test_annotated_tag_is_peeled(self):
        api = FakeApi()
        api.annotated = True
        verify_remote_tag(api, "v1.2.3", COMMIT)
        self.assertTrue(any(path.startswith("/git/tags/") for _, path, _ in api.calls))

    def test_draft_discovery_paginates_without_tag_endpoint(self):
        api = FakeApi()
        api.other_releases = [{"id": 100 + index, "tag_name": f"other-{index}"} for index in range(100)]
        prepare_draft(api, "v1.2.3", COMMIT, False, packages())
        self.assertTrue(any("page=2" in path for _, path, _ in api.calls))
        self.assertFalse(any(path.startswith("/releases/tags/") for _, path, _ in api.calls))

    def test_duplicate_drafts_fail_without_mutations(self):
        api = FakeApi()
        api.other_releases = [copy.deepcopy(api.release)]
        with self.assertRaisesRegex(ReleaseError, "Multiple releases"):
            prepare_draft(api, "v1.2.3", COMMIT, False, packages())
        self.assertEqual(api.mutations, [])


class TransportTests(unittest.TestCase):
    def test_transient_http_preserves_status_delay_and_safe_operation(self):
        api = GitHubApi("test/repo", "not-a-real-token")
        api.opener = Mock()
        api.opener.open.side_effect = urllib.error.HTTPError(
            "https://uploads.github.com/private?signature=secret", 504,
            "Gateway timeout; request body=private", {"Retry-After": "12"}, None)
        with self.assertRaises(ApiError) as failure:
            api.upload(42, "private-name.tar.gz", b"private file payload")
        self.assertEqual(failure.exception.status, 504)
        self.assertEqual(failure.exception.retry_after, 12)
        self.assertIn("POST uploads.github.com/repos/test/repo/releases/42/assets", str(failure.exception))
        for secret in ("not-a-real-token", "signature", "request body", "private-name", "file payload"):
            self.assertNotIn(secret, str(failure.exception))
        # Transport must not blindly replay an ambiguous POST.
        self.assertEqual(api.opener.open.call_count, 1)

    def test_timeout_reset_and_incomplete_body_are_normalized(self):
        for error in (TimeoutError("secret URL"), ConnectionResetError("secret URL"),
                      http.client.IncompleteRead(b"secret partial bytes", 100),
                      urllib.error.URLError(TimeoutError("secret URL"))):
            for phase in ("open", "read"):
                with self.subTest(error=type(error).__name__, phase=phase):
                    api = GitHubApi("test/repo", "token")
                    api.opener = Mock()
                    if phase == "open":
                        api.opener.open.side_effect = error
                    else:
                        response = Mock()
                        response.read.side_effect = error
                        manager = Mock()
                        manager.__enter__ = Mock(return_value=response)
                        manager.__exit__ = Mock(return_value=False)
                        api.opener.open.return_value = manager
                    with self.assertRaises(TransientApiError) as failure:
                        api.download(123)
                    self.assertIn("GET api.github.com/repos/test/repo/releases/assets/123", str(failure.exception))
                    self.assertNotIn("secret", str(failure.exception))

    def test_permanent_connection_failure_is_not_classified_as_transient(self):
        api = GitHubApi("test/repo", "token")
        api.opener = Mock()
        api.opener.open.side_effect = urllib.error.URLError("certificate verification failed; secret URL")
        with self.assertRaises(ReleaseError) as failure:
            api.request("GET", "/releases")
        self.assertNotIsInstance(failure.exception, TransientApiError)
        self.assertNotIn("secret", str(failure.exception))

    def test_retry_after_supports_seconds_and_http_dates(self):
        self.assertEqual(retry_after_seconds("17"), 17)
        with patch("release_pipeline.time.time", return_value=0):
            self.assertEqual(retry_after_seconds("Thu, 01 Jan 1970 00:00:25 GMT"), 25)
        with patch("release_pipeline.time.time", return_value=100):
            self.assertEqual(retry_after_seconds("Thu, 01 Jan 1970 00:00:25 GMT"), 0)
        for value in (None, "", "unknown", "-1"):
            self.assertIsNone(retry_after_seconds(value))

    def test_redirect_removes_authorization(self):
        request = urllib.request.Request("https://api.github.com/repos/test/repo/releases/assets/1",
                                         headers={"Authorization": "Bearer not-a-real-secret"})
        redirected = SafeRedirectHandler().redirect_request(
            request, None, 302, "Found", {}, "https://release-assets.githubusercontent.com/file")
        self.assertIsNone(redirected.get_header("Authorization"))
        with self.assertRaises(ReleaseError):
            SafeRedirectHandler().redirect_request(request, None, 302, "Found", {}, "http://example.com/file")

    def test_repository_and_endpoint_allowlist(self):
        with self.assertRaises(ReleaseError):
            GitHubApi("owner/repo/../../anything", "token")
        api = GitHubApi("owner/repo", "token")
        with self.assertRaises(ReleaseError):
            api._request("untrusted.example", "/repos/owner/repo/releases")
        with self.assertRaises(ReleaseError):
            api._request("api.github.com", "/repos/other/repo/releases")


if __name__ == "__main__":
    unittest.main()
