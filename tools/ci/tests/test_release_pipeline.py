"""Release policy tests; GitHub is faked and no remote writes are performed."""

import copy
import contextlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import unittest
import urllib.request
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from release_pipeline import (ApiError, GitHubApi, SafeRedirectHandler, publish,
                              verify_remote_tag)
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
    def __init__(self, head=COMMIT, tag=COMMIT, ancestor=True):
        self.head, self.tag, self.ancestor = head, tag, ancestor
        self.calls = []

    def __call__(self, *args):
        self.calls.append(args)
        if args == ("rev-parse", "HEAD"):
            return self.head
        if args[0] == "check-ref-format":
            if "\n" in args[1] or ".." in args[1]:
                raise ReleaseError("Invalid ref")
            return ""
        if args[0] == "rev-parse":
            return self.tag
        if args[0] == "merge-base":
            if not self.ancestor:
                raise ReleaseError("Commit is not on the default branch")
            return ""
        raise AssertionError(args)


class FakeApi:
    def __init__(self, *, release=True, commit=COMMIT):
        self.release = ({"id": 42, "tag_name": "v1.2.3", "draft": False,
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
        self.race_upload = False
        self.annotated = False
        self.move_after_upload = False

    def add_asset(self, name, data):
        asset_id = len(self.content) + 1
        self.assets[name] = {"id": asset_id, "name": name, "state": "uploaded"}
        self.content[asset_id] = data

    def request(self, method, path, data=None):
        self.calls.append((method, path, data))
        if path.startswith("/git/ref/tags/"):
            return {"object": {"type": "tag" if self.annotated else "commit", "sha": self.commit}}
        if path.startswith("/git/tags/"):
            return {"object": {"type": "commit", "sha": self.commit}}
        if path.startswith("/releases/tags/"):
            if self.release is None:
                raise ApiError(404, "Not found")
            return copy.deepcopy(self.release)
        if method == "GET" and path.startswith("/releases/42/assets?"):
            return list(self.assets.values())
        if method == "POST" and path == "/releases":
            self.mutations.append((method, path, data))
            self.release = {"id": 42, **data}
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
        if self.race_upload:
            raise ApiError(422, "Concurrent upload completed")


class PrepareTests(unittest.TestCase):
    def event(self, **changes):
        return {"ref": "refs/tags/v1.2.3", "repository": {"default_branch": "master"}, **changes}

    def test_branch_pull_request_and_manual_are_ci_only(self):
        for name, event in (("push", self.event(ref="refs/heads/master")),
                            ("push", self.event(ref="refs/tags/test")),
                            ("push", self.event(deleted=True)),
                            ("pull_request", {}), ("workflow_dispatch", {})):
            with self.subTest(event=name, payload=event):
                result = prepare_event(name, event, COMMIT, FakeGit())
                self.assertEqual(result, {"publish": "false", "tag": "", "commit": COMMIT,
                                          "release_id": ""})

    def test_version_tag_checks_default_branch_ancestry(self):
        git = FakeGit()
        result = prepare_event("push", self.event(), COMMIT, git)
        self.assertEqual(result["publish"], "true")
        self.assertEqual(result["tag"], "v1.2.3")
        self.assertIn(("merge-base", "--is-ancestor", COMMIT, "refs/remotes/origin/master"), git.calls)

    def test_formal_and_prerelease_published_events_are_supported(self):
        for prerelease in (False, True):
            event = self.event(action="published", release={"id": 42, "tag_name": "v1.2.3",
                                                           "draft": False, "prerelease": prerelease})
            result = prepare_event("release", event, COMMIT, FakeGit())
            self.assertEqual(result["release_id"], "42")
            self.assertEqual(result["publish"], "true")

    def test_wrong_checkout_moved_tag_and_foreign_branch_fail(self):
        for git in (FakeGit(head=OTHER_COMMIT), FakeGit(tag=OTHER_COMMIT), FakeGit(ancestor=False)):
            with self.subTest(git=git), self.assertRaises(ReleaseError):
                prepare_event("push", self.event(), COMMIT, git)

    def test_invalid_event_metadata_fails(self):
        bad_release = {"id": 42, "tag_name": "v1.2.3", "draft": False}
        for event in (self.event(action="created", release=bad_release),
                      self.event(action="published", release={**bad_release, "draft": True}),
                      self.event(action="published", release={**bad_release, "id": 0}),
                      self.event(action="published", release={**bad_release, "tag_name": "v1\ninjection=true"}),
                      self.event(action="published", release=bad_release, repository={})):
            with self.subTest(event=event), self.assertRaises(ReleaseError):
                prepare_event("release", event, COMMIT, FakeGit())


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
        event_path.write_text(json.dumps({"ref": "refs/heads/main"}), encoding="utf-8")
        result = subprocess.run([
            sys.executable, str(Path(__file__).resolve().parents[1] / "release_pipeline.py"),
            "prepare", "--event-path", str(event_path), "--event-name", "push",
            "--commit", commit, "--output", str(output_path),
        ], text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output_path.read_text(encoding="utf-8"),
                         f"publish=false\ntag=\ncommit={commit}\nrelease_id=\n")

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


class PublishTests(unittest.TestCase):
    def setUp(self):
        redirect = contextlib.redirect_stdout(io.StringIO())
        redirect.__enter__()
        self.addCleanup(redirect.__exit__, None, None, None)

    def test_create_after_validation_and_upload_all_six(self):
        api = FakeApi(release=False)
        publish(api, "v1.2.3", COMMIT, "", packages())
        self.assertEqual(len(api.assets), 6)
        self.assertEqual(api.mutations[0][0], "POST")
        self.assertEqual(api.release["target_commitish"], COMMIT)

    def test_repeated_publish_preserves_user_notes_and_verified_packages(self):
        api = FakeApi()
        original_release = copy.deepcopy(api.release)
        publish(api, "v1.2.3", COMMIT, "42", packages(timestamp=1))
        initial_mutations = len(api.mutations)
        publish(api, "v1.2.3", COMMIT, "42", packages(timestamp=2))
        self.assertEqual(len(api.mutations), initial_mutations)
        self.assertEqual(api.release, original_release)

    def test_incomplete_matrix_or_bad_package_causes_no_api_calls(self):
        for inputs in (packages()[:2], [package(commit=OTHER_COMMIT), *packages()[1:]]):
            api = FakeApi(release=False)
            with self.subTest(inputs=inputs), self.assertRaises(ReleaseError):
                publish(api, "v1.2.3", COMMIT, "", inputs)
            self.assertEqual(api.calls, [])
            self.assertEqual(api.mutations, [])

    def test_moved_tag_and_mismatched_release_id_prevent_mutations(self):
        for api, expected in ((FakeApi(commit=OTHER_COMMIT), ""), (FakeApi(), "41"),
                              (FakeApi(release=False), "42")):
            with self.subTest(expected=expected), self.assertRaises(ReleaseError):
                publish(api, "v1.2.3", COMMIT, expected, packages())
            self.assertEqual(api.mutations, [])

    def test_draft_release_is_not_changed(self):
        api = FakeApi()
        api.release["draft"] = True
        with self.assertRaisesRegex(ReleaseError, "draft"):
            publish(api, "v1.2.3", COMMIT, "", packages())
        self.assertEqual(api.mutations, [])

    def test_existing_wrong_commit_or_corrupt_checksum_blocks_all_uploads(self):
        for wrong_commit in (True, False):
            api = FakeApi()
            item = package("macos-arm64", OTHER_COMMIT if wrong_commit else COMMIT)
            api.add_asset(item.name, item.data)
            api.add_asset(item.name + ".sha256", item.checksum_data if wrong_commit else b"corrupt")
            with self.subTest(wrong_commit=wrong_commit), self.assertRaises(ReleaseError):
                publish(api, "v1.2.3", COMMIT, "", packages())
            self.assertEqual(api.mutations, [])

    def test_metadata_only_remote_asset_is_not_reused_or_overwritten(self):
        for platform in PLATFORMS:
            api = FakeApi()
            item = package(platform, omit=tuple(package_contents(platform)))
            api.add_asset(item.name, item.data)
            api.add_asset(item.name + ".sha256", item.checksum_data)
            with self.subTest(platform=platform), self.assertRaisesRegex(ReleaseError, "Missing required"):
                publish(api, "v1.2.3", COMMIT, "", packages())
            self.assertEqual(api.mutations, [])

    def test_interrupted_upload_recovers_checksum_from_existing_archive(self):
        api = FakeApi()
        api.fail_upload_at = 2
        with self.assertRaises(ApiError):
            publish(api, "v1.2.3", COMMIT, "", packages(timestamp=1))
        self.assertEqual(len(api.assets), 1)
        api.fail_upload_at = None
        publish(api, "v1.2.3", COMMIT, "", packages(timestamp=2))
        self.assertEqual(len(api.assets), 6)
        self.assertEqual(len(api.mutations), 6)

    def test_orphan_checksum_requires_matching_archive(self):
        api = FakeApi()
        item = package(timestamp=1)
        api.add_asset(item.name + ".sha256", item.checksum_data)
        with self.assertRaises(ReleaseError):
            publish(api, "v1.2.3", COMMIT, "", packages(timestamp=2))
        self.assertEqual(api.mutations, [])
        publish(api, "v1.2.3", COMMIT, "", [item, *packages()[1:]])
        self.assertEqual(len(api.assets), 6)

    def test_tag_is_rechecked_between_uploads(self):
        api = FakeApi()
        api.move_after_upload = True
        with self.assertRaisesRegex(ReleaseError, "moved"):
            publish(api, "v1.2.3", COMMIT, "", packages())
        self.assertEqual(len(api.mutations), 1)

    def test_create_and_upload_races_are_idempotent(self):
        api = FakeApi(release=False)
        api.race_create = api.race_upload = True
        publish(api, "v1.2.3", COMMIT, "", packages())
        self.assertEqual(len(api.assets), 6)

    def test_annotated_tag_is_peeled(self):
        api = FakeApi()
        api.annotated = True
        verify_remote_tag(api, "v1.2.3", COMMIT)
        self.assertTrue(any(path.startswith("/git/tags/") for _, path, _ in api.calls))


class TransportTests(unittest.TestCase):
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
