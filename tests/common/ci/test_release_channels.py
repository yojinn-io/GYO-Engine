"""Release trains and channels on synthetic registries, archives and a fake GitHub."""

import contextlib
import copy
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import unittest
import urllib.parse

ROOT = Path(__file__).resolve().parents[3]
for folder in ("build", "build/ci/common", "build/acceptance/common"):
    sys.path.insert(0, str(ROOT / folder))
from workspace import TemporaryDirectory

import go_checks
from go_checks import SERVICE_RECORD_NAME, Service
from release_channels import (EVIDENCE_REFERENCE_LIMIT, SNAPSHOT_KEEP, TOOLS_TRAIN, TRIAL_LABEL, Train, derive_trains,
                              parse_release_tag, train_release_evidence, validate_evidence_reference,
                              parse_snapshot_tag, prepare_event, published_snapshots, release_tag, render_evidence, render_notes, resolve_train,
                              latest_other_snapshot, select_expired_snapshots, select_orphan_snapshot_tags, select_trial, snapshot_tag,
                              train_matrix, verification_level)
from release_pipeline import ApiError, prepare_draft, publish_snapshot
from release_support import (PLATFORMS, EvidenceItem, ReleaseError, checksum_document, describe_item,
                             load_packages, load_release_evidence, load_services, validate_release_evidence,
                             validate_service_archive)
from test_release_pipeline import (APP, COMMIT, OTHER_COMMIT, FakeApi, FakeGit, evidence, manifest, package)


SECOND = "second_app"
ROLE = "relay"
# A game on every platform with a service on two of them, and a game without Linux.
REGISTRY_PAIRS = [(APP, platform) for platform in PLATFORMS] + [(SECOND, "windows-x64"), (SECOND, "macos-arm64")]
SERVICE_PLATFORMS = ("linux-x64", "windows-x64")
# 2026-10-02T12:00:00Z
COMMITTED_AT = 1790942400


def quick_package(platform, product=APP, commit=COMMIT):
    return package(platform, commit, product=product,
                   metadata_changes={"ci_smoke": evidence(manifest(platform, product), commit, profile="quick")})


def write_services(directory: Path, *, product=APP, commit=COMMIT, services=None, record=None):
    """Real go_checks archives (fake compiler) plus the row's service record."""
    services = [Service(APP, ROLE, f"apps/{APP}", "./cmd/relay", SERVICE_PLATFORMS)] if services is None else services
    directory.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory() as temporary:
        work = Path(temporary)
        (work / f"apps/{APP}").mkdir(parents=True)

        def run(command, cwd, env):
            output = Path(command[command.index("-o") + 1])
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(f"binary {env['GOOS']}/{env['GOARCH']}".encode())
            return 0
        if services:
            with contextlib.redirect_stdout(io.StringIO()):
                produced = go_checks.build(services, work / "out", repository=work, revision=commit,
                                           run=run, version=lambda go: "go1.23.4")
            for archive in produced:
                for name in (archive.name, archive.name + ".sha256"):
                    shutil.copy(archive.parent / name, directory / name)
    document = go_checks.service_record(product, services, commit) if record is None else record
    (directory / SERVICE_RECORD_NAME).write_text(json.dumps(document), encoding="utf-8")
    return directory


class TrainDerivationTests(unittest.TestCase):
    def test_trains_are_the_tools_train_and_one_train_per_enabled_game(self):
        trains = derive_trains(REGISTRY_PAIRS)
        self.assertEqual(list(trains), [TOOLS_TRAIN, APP, SECOND])
        self.assertEqual(trains[TOOLS_TRAIN].pairs, [("toolchain", platform) for platform in PLATFORMS])
        self.assertEqual(trains[APP].pairs, [(APP, platform) for platform in PLATFORMS])
        # Platforms follow the release platform order, not registry order.
        self.assertEqual(trains[SECOND].platforms, ("windows-x64", "macos-arm64"))
        self.assertEqual(derive_trains([]), {TOOLS_TRAIN: trains[TOOLS_TRAIN]})
        self.assertEqual(train_matrix(trains), {"include": [
            {"train": TOOLS_TRAIN, "product": "toolchain"}, {"train": APP, "product": APP},
            {"train": SECOND, "product": SECOND}]})

    def test_reserved_duplicate_and_invalid_registry_rows_fail(self):
        for pairs in ([("tools", "linux-x64")], [("toolchain", "linux-x64")], [(APP, "linux-x64")] * 2,
                      [(APP, "linux-arm64")], [("Bad-Name", "linux-x64")]):
            with self.subTest(pairs=pairs), self.assertRaises((ReleaseError, ValueError)):
                derive_trains(pairs)

    def test_unknown_or_disabled_trains_are_rejected(self):
        trains = derive_trains(REGISTRY_PAIRS)
        self.assertIs(resolve_train(trains, APP), trains[APP])
        for train in ("toolchain", "disabled_app", "", None, f"{APP}-v1"):
            with self.subTest(train=train), self.assertRaisesRegex(ReleaseError, "Unknown release train"):
                resolve_train(trains, train)

    def test_trains_cli_reads_the_cmake_registry(self):
        with TemporaryDirectory() as temporary:
            registry = Path(temporary) / "projects.csv"
            registry.write_text("name,description,version,enabled,windows,linux,macos\n"
                                f"{APP},,,true,false,true,false\nunused_app,,,false,true,true,true\n", encoding="utf-8")
            output = Path(temporary) / "outputs.txt"
            result = subprocess.run([sys.executable, str(ROOT / "build/ci/common/release_channels.py"), "trains",
                                     "--registry", str(registry), "--output", str(output)],
                                    capture_output=True, text=True, timeout=60, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            name, value = output.read_text(encoding="utf-8").strip().split("=", 1)
            self.assertEqual(name, "trains")
            self.assertEqual(json.loads(value), {"include": [{"train": "tools", "product": "toolchain"},
                                                             {"train": APP, "product": APP}]})

    def test_build_matrix_narrows_to_one_train_beside_the_toolchain_baseline(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            registry = root / "projects.csv"
            registry.write_text("name,description,version,enabled,windows,linux,macos\n"
                                f"{APP},,,true,false,true,false\n{SECOND},,,true,true,false,false\n", encoding="utf-8")
            tools = root / "tools.csv"
            tools.write_text("name,description,version,enabled,default,release,windows,linux,macos\n"
                             "alpha,Fixture,,1,0,1,1,1,1\n", encoding="utf-8")
            (root / "tools/alpha").mkdir(parents=True)
            (root / "tools/alpha/project.json").write_text(json.dumps({"version": 1, "default_variant": "native",
                "requires_apps": {}, "variants": {"native": {"components": [], "packageable": True}}}))

            def products(selection):
                output = root / f"outputs-{selection or 'all'}.txt"
                result = subprocess.run([sys.executable, str(ROOT / "build/ci/common/app_registry.py"),
                                         "--registry", str(registry), "--tool-registry", str(tools),
                                         "--repository-root", str(root), "--output", str(output),
                                         "--product", selection], capture_output=True, text=True, timeout=60)
                if result.returncode:
                    return None
                outputs = dict(line.split("=", 1) for line in output.read_text().splitlines())
                return sorted({row["product"] for row in json.loads(outputs["build_matrix"])["include"]})
            self.assertEqual(products(""), sorted(["toolchain", APP, SECOND]))
            self.assertEqual(products("toolchain"), ["toolchain"])
            self.assertEqual(products(SECOND), sorted(["toolchain", SECOND]))
            self.assertIsNone(products("unused_app"))


class VersionAndTagTests(unittest.TestCase):
    def setUp(self):
        self.trains = derive_trains(REGISTRY_PAIRS)
        self.tools, self.game = self.trains[TOOLS_TRAIN], self.trains[APP]

    def test_tools_use_calendar_versions(self):
        for version in ("v2026.10.1", "v2026.1.12", "v2099.12.3-rc.1", "v2026.10.1+build.7"):
            with self.subTest(version=version):
                self.assertEqual(release_tag(self.tools, version), f"tools-{version}")
        for version in ("v2026.10.0", "v2026.0.1", "v2026.13.1", "v2026.01.1", "v26.10.1", "2026.10.1",
                        "v5.0.0", "v2026.10", "v2026.10.1-01", "v2026.10.1\n", None):
            with self.subTest(version=version), self.assertRaises(ReleaseError):
                release_tag(self.tools, version)

    def test_games_use_semver(self):
        for version in ("v5.0.0", "v0.1.0", "v5.1.0-rc.1", "v2026.10.1"):
            with self.subTest(version=version):
                self.assertEqual(release_tag(self.game, version), f"{APP}-{version}")
        for version in ("5.0.0", "v5.0", "v05.0.0", "v5.0.0-01", "v5.0.0 ", "tools-v5.0.0", ""):
            with self.subTest(version=version), self.assertRaises(ReleaseError):
                release_tag(self.game, version)

    def test_a_tag_belongs_to_exactly_one_train(self):
        self.assertEqual(parse_release_tag(self.game, f"{APP}-v5.0.0"), "v5.0.0")
        self.assertEqual(parse_release_tag(self.tools, "tools-v2026.10.1"), "v2026.10.1")
        prefixed = Train(f"{APP}_two", f"{APP}_two", ("linux-x64",))
        for train, tag in ((self.game, "tools-v2026.10.1"), (self.tools, f"{APP}-v5.0.0"),
                           (self.game, f"{APP}_two-v5.0.0"), (prefixed, f"{APP}-v5.0.0"), (self.game, "v5.0.0"),
                           (self.game, f"{APP}-snapshot-20261002-aaaaaaa"), (self.tools, "tools-v5.0.0")):
            with self.subTest(train=train.id, tag=tag), self.assertRaises(ReleaseError):
                parse_release_tag(train, tag)

    def test_prepare_event_derives_the_tag_and_product_per_train(self):
        def prepare(train, version):
            event = {"inputs": {"train": train, "version": version, "prerelease": False}}
            return prepare_event("workflow_dispatch", event, COMMIT, "refs/heads/master", FakeGit(), trains=self.trains,
                                 evidence=lambda train: ())
        self.assertEqual(prepare("tools", "v2026.10.1"), {"tag": "tools-v2026.10.1", "commit": COMMIT,
            "prerelease": "false", "train": "tools", "product": "toolchain", "l4_items": "", "l4_evidence": ""})
        self.assertEqual(prepare(SECOND, "v1.0.0")["tag"], f"{SECOND}-v1.0.0")
        for train, version in (("tools", "v1.0.0"), (APP, "v2026.10"), ("toolchain", "v2026.10.1"), (None, "v1.0.0")):
            with self.subTest(train=train, version=version), self.assertRaises(ReleaseError):
                prepare(train, version)
        # An existing tag must already point at the selected commit; tags never move.
        with self.assertRaises(ReleaseError):
            prepare_event("workflow_dispatch", {"inputs": {"train": APP, "version": "v1.0.0"}}, COMMIT,
                          "refs/heads/master", FakeGit(tag=OTHER_COMMIT), trains=self.trains,
                          evidence=lambda train: ())

    def test_snapshot_tags_are_dated_by_the_commit_and_never_shared(self):
        tag = snapshot_tag(self.game, COMMIT, COMMITTED_AT)
        self.assertEqual(tag, f"{APP}-snapshot-20261002-aaaaaaa")
        # A rerun of the same commit names the same tag; another commit never does.
        self.assertEqual(snapshot_tag(self.game, COMMIT, COMMITTED_AT), tag)
        self.assertNotEqual(snapshot_tag(self.game, OTHER_COMMIT, COMMITTED_AT), tag)
        self.assertEqual(snapshot_tag(self.tools, COMMIT, COMMITTED_AT - 12 * 3600 - 1), "tools-snapshot-20261001-aaaaaaa")
        self.assertEqual(parse_snapshot_tag(APP, tag), ("20261002", "aaaaaaa"))
        for train_id, other in ((APP, f"{APP}-v5.0.0"), (f"{APP}_two", tag), ("tools", tag),
                                (APP, f"{APP}-snapshot-2026102-aaaaaaa"), (APP, tag + "x"), (APP, None)):
            with self.subTest(train=train_id, tag=other):
                self.assertIsNone(parse_snapshot_tag(train_id, other))
        for commit, timestamp in (("HEAD", COMMITTED_AT), (COMMIT, -1), (COMMIT, "1")):
            with self.subTest(commit=commit, timestamp=timestamp), self.assertRaises(ReleaseError):
                snapshot_tag(self.game, commit, timestamp)


def snapshot_release(train_id, day, sha, release_id, *, draft=False, prerelease=True, published=None):
    # GitHub's created_at is the tagged commit's date, so it never orders snapshots.
    commit_date = f"{day[:4]}-{day[4:6]}-{day[6:]}T00:00:00Z"
    return {"id": release_id, "tag_name": f"{train_id}-snapshot-{day}-{sha}", "draft": draft,
            "prerelease": prerelease, "created_at": commit_date,
            "published_at": None if draft else published or commit_date}


class RetentionTests(unittest.TestCase):
    def releases(self):
        own = [snapshot_release(APP, f"202609{day:02d}", f"{day:07x}", day) for day in range(1, 8)]
        return [
            *own,
            # Same day: publication time orders them.
            snapshot_release(APP, "20260907", "fffffff", 70, published="2026-09-07T09:00:00Z"),
            # Never counted or deleted: drafts, promoted releases, other trains, formal tags.
            snapshot_release(APP, "20260801", "0000001", 80, draft=True),
            snapshot_release(APP, "20260802", "0000002", 81, prerelease=False),
            snapshot_release(f"{APP}_two", "20260803", "0000003", 82),
            snapshot_release("tools", "20260804", "0000004", 83),
            {"id": 84, "tag_name": f"{APP}-v1.0.0", "draft": False, "prerelease": True},
            {"id": 85, "tag_name": "v1.0.0", "draft": False, "prerelease": False},
        ]

    def test_only_this_trains_published_snapshot_prereleases_beyond_the_newest_five_expire(self):
        expired = select_expired_snapshots(self.releases(), APP, SNAPSHOT_KEEP)
        self.assertEqual(SNAPSHOT_KEEP, 5)
        self.assertEqual([release["id"] for release in expired], [3, 2, 1])
        self.assertEqual([release["id"] for release in select_expired_snapshots(self.releases(), APP, 7)], [1])
        self.assertEqual(select_expired_snapshots(self.releases(), "tools", 1), [])
        self.assertEqual(select_expired_snapshots([], APP), [])

    def test_protected_tag_and_invalid_keep(self):
        releases = self.releases()
        protected = f"{APP}-snapshot-20260901-0000001"
        self.assertEqual([release["id"] for release in select_expired_snapshots(releases, APP, protect={protected})],
                         [3, 2])
        for keep in (0, -1, True, "5", None):
            with self.subTest(keep=keep), self.assertRaises(ReleaseError):
                select_expired_snapshots(releases, APP, keep)

    def test_orphan_tags_are_this_trains_snapshot_tags_without_a_release(self):
        releases = self.releases()
        tags = [release["tag_name"] for release in releases] + [
            f"{APP}-snapshot-20260101-1111111", f"{APP}-snapshot-20260102-2222222", f"{APP}-v9.9.9",
            f"tools-snapshot-20260101-1111111", f"{APP}_two-snapshot-20260101-1111111", "v1.0.0"]
        self.assertEqual(select_orphan_snapshot_tags(tags, releases, APP),
                         [f"{APP}-snapshot-20260101-1111111", f"{APP}-snapshot-20260102-2222222"])
        self.assertEqual(select_orphan_snapshot_tags(tags, releases, APP, protect={f"{APP}-snapshot-20260102-2222222"}),
                         [f"{APP}-snapshot-20260101-1111111"])

    def test_retention_follows_publication_order_not_the_commit_date_in_the_tag(self):
        # Five snapshots published 1-5 October, then a push whose head commit
        # is dated 30 September (a fast-forward of older local commits).
        published = [snapshot_release(APP, f"202610{day:02d}", f"{day:07x}", day) for day in range(1, 6)]
        late = snapshot_release(APP, "20260930", "abcdef0", 6, published="2026-10-06T00:00:00Z")
        releases = [*published, late]
        self.assertEqual(published_snapshot_ids(releases), [6, 5, 4, 3, 2, 1])
        self.assertEqual([release["id"] for release in select_expired_snapshots(releases, APP)], [1])
        # The same second: the monotonically assigned release id decides.
        tie = snapshot_release(APP, "20260901", "1234567", 7, published="2026-10-06T00:00:00Z")
        self.assertEqual(published_snapshot_ids([*releases, tie])[:2], [7, 6])

    def test_latest_other_snapshot_is_the_most_recently_published_one(self):
        releases = self.releases()
        self.assertEqual(latest_other_snapshot(releases, APP, f"{APP}-snapshot-20261001-1234567")["id"], 70)
        # The snapshot itself never supersedes its own rerun.
        self.assertEqual(latest_other_snapshot(releases, APP, f"{APP}-snapshot-20260907-fffffff")["id"], 7)
        self.assertIsNone(latest_other_snapshot([], APP, f"{APP}-snapshot-20200101-1234567"))
        self.assertEqual(latest_other_snapshot(releases, "tools", "tools-snapshot-20200101-1234567")["id"], 83)
        self.assertIsNone(latest_other_snapshot(releases, "tools", "tools-snapshot-20260804-0000004"))
        with self.assertRaises(ReleaseError):
            latest_other_snapshot(releases, APP, f"{APP}-v1.0.0")


def published_snapshot_ids(releases, train_id=APP):
    return [release["id"] for release in published_snapshots(releases, train_id)]


def live(labels, state="open"):
    return {"state": state, "labels": labels}


class TrialSelectionTests(unittest.TestCase):
    def event(self, action="synchronize", label=None):
        event = {"action": action, "pull_request": {"number": 7, "labels": [{"name": "stale"}]}}
        if label is not None:
            event["label"] = {"name": label}
        return event

    def test_label_and_manual_dispatch_select_trial_packages(self):
        self.assertEqual(TRIAL_LABEL, "package")
        self.assertEqual(select_trial("workflow_dispatch", {}, None), (True, "manual"))
        for action in ("opened", "synchronize", "reopened"):
            with self.subTest(action=action):
                self.assertEqual(select_trial("pull_request", self.event(action), live(["bug", TRIAL_LABEL])),
                                 (True, "label"))
                self.assertEqual(select_trial("pull_request", self.event(action), live(["bug"])), (False, "no-label"))
        self.assertEqual(select_trial("pull_request", self.event("labeled", TRIAL_LABEL), live([TRIAL_LABEL])),
                         (True, "label"))

    def test_other_labels_and_removed_labels_never_build(self):
        self.assertEqual(select_trial("pull_request", self.event("labeled", "bug"), live([TRIAL_LABEL, "bug"])),
                         (False, "other-label"))
        # A re-run replays the labeled payload; the live labels decide.
        self.assertEqual(select_trial("pull_request", self.event("labeled", TRIAL_LABEL), live([])), (False, "no-label"))
        self.assertEqual(select_trial("pull_request", self.event("labeled", "Package"), live(["Package"])),
                         (False, "other-label"))

    def test_closed_and_merged_pull_requests_never_build(self):
        # GitHub sends labeled for closed and merged pull requests too (merged
        # is state "closed"); a re-run reads the state at that time.
        for action in ("labeled", "synchronize", "reopened"):
            with self.subTest(action=action):
                self.assertEqual(select_trial("pull_request", self.event(action, TRIAL_LABEL),
                                              live([TRIAL_LABEL], "closed")), (False, "closed"))

    def test_unclassifiable_events_fail(self):
        for name, event, state in (("push", {}, live([])), ("pull_request", {}, live([])),
                                   ("pull_request", self.event(), None), ("pull_request", self.event(), "package"),
                                   ("pull_request", self.event(), [TRIAL_LABEL]),
                                   ("pull_request", self.event(), live([1])),
                                   ("pull_request", self.event(), live("package")),
                                   ("pull_request", self.event(), {"labels": [TRIAL_LABEL]}),
                                   ("pull_request", self.event(), live([TRIAL_LABEL], "merged")),
                                   ("pull_request", self.event(), live([TRIAL_LABEL], None))):
            with self.subTest(name=name, state=state), self.assertRaises(ReleaseError):
                select_trial(name, event, state)

    def test_trial_cli_writes_outputs(self):
        with TemporaryDirectory() as temporary:
            event, output = Path(temporary) / "event.json", Path(temporary) / "outputs.txt"
            event.write_text(json.dumps(self.event("labeled", TRIAL_LABEL)), encoding="utf-8")
            command = [sys.executable, str(ROOT / "build/ci/common/release_channels.py"), "trial", "--event-name",
                       "pull_request", "--event-path", str(event), "--output", str(output)]
            for state, expected in (("open", ["run_package=true", "reason=label"]),
                                    ("closed", ["run_package=false", "reason=closed"])):
                with self.subTest(state=state):
                    output.unlink(missing_ok=True)
                    result = subprocess.run([*command, "--live-pull-request", json.dumps(live([TRIAL_LABEL], state))],
                                            capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(output.read_text().splitlines(), expected)
            # Without the live pull request a pull request event cannot be classified.
            self.assertNotEqual(subprocess.run(command, capture_output=True, text=True, timeout=30).returncode, 0)


class TrainArchiveTests(unittest.TestCase):
    def setUp(self):
        temporary = TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.train = derive_trains(REGISTRY_PAIRS)[APP]

    def test_game_train_expects_every_package_and_each_recorded_service_archive(self):
        services, expected = load_services(write_services(self.root / "services"), COMMIT, APP, self.train.platforms)
        self.assertEqual(expected, {(APP, ROLE, platform) for platform in SERVICE_PLATFORMS})
        self.assertEqual(sorted(item.name for item in services), [
            f"gyo-{APP}-{ROLE}-linux-x64.tar.gz", f"gyo-{APP}-{ROLE}-windows-x64.tar.gz"])
        self.assertTrue(all(item.role == ROLE and item.product == APP for item in services))
        # The client/match packages and the services come from the same commit.
        packages = [package(platform) for platform in PLATFORMS]
        api = FakeApi(release=False, commit=None)
        with contextlib.redirect_stdout(io.StringIO()):
            prepare_draft(api, f"{APP}-v5.0.0", COMMIT, False, packages + services,
                          expected_pairs=self.train.pairs, expected_services=expected, notes="Notes")
        self.assertEqual(len(api.assets), 2 * (len(PLATFORMS) + len(SERVICE_PLATFORMS)))
        self.assertEqual(api.release["body"], "Notes")
        self.assertTrue(api.release["generate_release_notes"])
        for missing in (services[:1], []):
            with self.subTest(services=len(missing)), self.assertRaisesRegex(ReleaseError, "recorded service"):
                prepare_draft(FakeApi(release=False, commit=None), f"{APP}-v5.0.0", COMMIT, False,
                              packages + missing, expected_pairs=self.train.pairs, expected_services=expected)

    def test_a_product_without_services_still_records_an_empty_set(self):
        directory = write_services(self.root / "services", services=[])
        self.assertEqual(load_services(directory, COMMIT, APP, self.train.platforms), ([], set()))
        # Engine-layer services belong to the tools train's toolchain row.
        tools = derive_trains([])[TOOLS_TRAIN]
        directory = write_services(self.root / "tools", product="toolchain", services=[])
        self.assertEqual(load_services(directory, COMMIT, "toolchain", tools.platforms), ([], set()))

    def test_missing_extra_foreign_or_mismatched_service_files_fail(self):
        def attempt(directory, product=APP, commit=COMMIT, platforms=None):
            return load_services(directory, commit, product, platforms or self.train.platforms)
        base = write_services(self.root / "base")
        cases = {}
        cases["missing record"] = self.root / "no-record"
        shutil.copytree(base, cases["missing record"])
        (cases["missing record"] / SERVICE_RECORD_NAME).unlink()
        cases["missing archive"] = self.root / "no-archive"
        shutil.copytree(base, cases["missing archive"])
        (cases["missing archive"] / f"gyo-{APP}-{ROLE}-windows-x64.tar.gz").unlink()
        cases["extra file"] = self.root / "extra"
        shutil.copytree(base, cases["extra file"])
        (cases["extra file"] / f"gyo-{APP}-other-linux-x64.tar.gz").write_bytes(b"x")
        cases["bad checksum"] = self.root / "checksum"
        shutil.copytree(base, cases["bad checksum"])
        (cases["bad checksum"] / f"gyo-{APP}-{ROLE}-linux-x64.tar.gz.sha256").write_text("0" * 64)
        record = go_checks.service_record(APP, [Service(APP, ROLE, f"apps/{APP}", "./cmd/relay", SERVICE_PLATFORMS)], COMMIT)
        for label, changes in (("other commit", {"source_revision": OTHER_COMMIT}), ("other product", {"product": SECOND}),
                               ("version", {"version": 2})):
            cases[label] = write_services(self.root / label.replace(" ", "-"), record={**record, **changes})
        foreign = copy.deepcopy(record)
        foreign["services"][0]["owner"] = SECOND
        cases["foreign owner"] = write_services(self.root / "foreign", record=foreign)
        for label, directory in cases.items():
            with self.subTest(case=label), self.assertRaises(ReleaseError):
                attempt(directory)
        # Archives built for another commit fail even with a matching record.
        stale = write_services(self.root / "stale", commit=OTHER_COMMIT, record=record)
        with self.assertRaisesRegex(ReleaseError, "provenance"):
            attempt(stale)
        # A service platform outside the train, or services for a train
        # without a Linux build row, are rejected.
        with self.assertRaisesRegex(ReleaseError, "outside its release train"):
            attempt(base, platforms=("linux-x64", "macos-arm64"))
        with self.assertRaisesRegex(ReleaseError, "without a service build row"):
            attempt(base, platforms=("windows-x64",))
        self.assertEqual(attempt(self.root / "absent", platforms=("windows-x64",)), ([], set()))

    def test_service_archive_contents_are_exact(self):
        directory = write_services(self.root / "services")
        name = f"gyo-{APP}-{ROLE}-linux-x64.tar.gz"
        data = (directory / name).read_bytes()
        validate_service_archive(name, data, APP, ROLE, "linux-x64", COMMIT)

        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as source:
            original = [(member, source.extractfile(member).read() if member.isfile() else None) for member in source]

        def pack(entries):
            output = io.BytesIO()
            with tarfile.open(fileobj=output, mode="w:gz") as target:
                for member, content in entries:
                    target.addfile(member, io.BytesIO(content) if content is not None else None)
            return output.getvalue()

        def metadata(changes):
            entries = []
            for member, content in original:
                if member.name.endswith("build_metadata.json"):
                    member, content = copy.copy(member), json.dumps({**json.loads(content), **changes}).encode()
                    member.size = len(content)
                entries.append((member, content))
            return pack(entries)

        extra = tarfile.TarInfo(f"gyo-{APP}-{ROLE}/bin/extra")
        extra.size = 1
        not_executable = []
        for member, content in original:
            if member.name.endswith(f"-{ROLE}"):
                member = copy.copy(member)
                member.mode = 0o644
            not_executable.append((member, content))
        for label, archive in (("extra member", pack([*original, (extra, b"x")])),
                               ("missing metadata", pack([entry for entry in original
                                                          if not entry[0].name.endswith(".json")])),
                               ("cgo", metadata({"cgo": True})), ("goarch", metadata({"goarch": "arm64"})),
                               ("no build host", metadata({"build_os": ""})), ("owner", metadata({"owner": SECOND})),
                               ("commit", metadata({"source_revision": OTHER_COMMIT})),
                               ("mode", pack(not_executable))):
            with self.subTest(case=label), self.assertRaises(ReleaseError):
                validate_service_archive(name, archive, APP, ROLE, "linux-x64", COMMIT)
        with self.assertRaises(ReleaseError):
            validate_service_archive(name, data, APP, ROLE, "windows-x64", COMMIT)


class ManagedAssetTests(unittest.TestCase):
    def test_package_and_service_asset_names_never_overlap(self):
        from release_pipeline import MANAGED_PACKAGE_ASSET, MANAGED_SERVICE_ASSET
        from release_support import archive_name, service_archive_name
        for platform in PLATFORMS:
            for name in (archive_name(APP, platform), archive_name("toolchain", platform)):
                for asset in (name, name + ".sha256"):
                    self.assertTrue(MANAGED_PACKAGE_ASSET.fullmatch(asset))
                    self.assertIsNone(MANAGED_SERVICE_ASSET.fullmatch(asset), asset)
            name = service_archive_name(APP, ROLE, platform)
            self.assertTrue(MANAGED_SERVICE_ASSET.fullmatch(name))
            self.assertIsNone(MANAGED_PACKAGE_ASSET.fullmatch(name), name)

    def test_unexpected_service_asset_in_a_draft_blocks_every_mutation(self):
        train = derive_trains(REGISTRY_PAIRS)[SECOND]
        items = [package(platform, product=SECOND) for platform in train.platforms]
        api = FakeApi()
        api.release["tag_name"] = f"{SECOND}-v1.0.0"
        api.add_asset(f"gyo-{SECOND}-{ROLE}-windows-x64.tar.gz", b"stray")
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaisesRegex(ReleaseError, "unexpected managed"):
            prepare_draft(api, f"{SECOND}-v1.0.0", COMMIT, False, items, expected_pairs=train.pairs)
        self.assertEqual(api.mutations, [])


class NotesTests(unittest.TestCase):
    def test_verification_levels_come_from_recorded_build_facts(self):
        cross = {"archive": "a", "kind": "package", "platform": "macos-x64", "build_os": "macOS-15.5-arm64-arm-64bit",
                 "build_architecture": "arm64", "cpu_execution": "rosetta2", "profile": "quick", "checks": 3,
                 "gpu_checks": 0}
        self.assertEqual(verification_level(cross), "cross-built on macOS arm64; 3 installed quick checks passed "
                         "under Rosetta 2 translation; no GPU checks; no physical GPU")
        linux = {**cross, "platform": "linux-x64", "build_os": "Linux-6.8.0-x86_64-with-glibc2.39",
                 "build_architecture": "x86_64", "cpu_execution": "native", "gpu_checks": 1}
        self.assertEqual(verification_level(linux), "built on Linux x86_64; 3 installed quick checks passed natively; "
                         "1 on a software Vulkan device; no physical GPU")
        service = {"archive": "s", "kind": "service", "platform": "windows-x64", "build_os": "Linux-6.8.0-x86_64",
                   "build_architecture": "x86_64", "go_version": "go1.23.4"}
        self.assertEqual(verification_level(service), "cross-built on Linux x86_64 with go1.23.4 (CGO disabled); "
                         "the service binary is not run by CI")
        self.assertTrue(verification_level({**service, "platform": "linux-x64"}).startswith("built on Linux x86_64"))
        # Missing facts are reported, never guessed.
        self.assertIn("unrecorded", verification_level({**cross, "build_os": None, "cpu_execution": None}))

    def test_release_notes_list_every_archive(self):
        train = derive_trains(REGISTRY_PAIRS)[APP]
        items = [quick_package(platform) for platform in PLATFORMS]
        descriptions = [describe_item(item, "quick") for item in items]
        self.assertEqual({item["gpu_checks"] for item in descriptions if item["platform"] == "linux-x64"}, {1})
        self.assertEqual({item["checks"] for item in descriptions if item["platform"] == "windows-x64"}, {2})
        notes = render_notes(train=train, commit=COMMIT, profile="quick", descriptions=descriptions, snapshot=True)
        for item in items:
            self.assertIn(f"| `{item.name}` |", notes)
        self.assertIn("only the newest 5 snapshots", notes)
        self.assertIn("not code-signed", notes)
        formal = render_notes(train=train, commit=COMMIT, profile="release", descriptions=descriptions, snapshot=False)
        self.assertNotIn("snapshot", formal)


def write_contract(root: Path, owner: str, release_evidence=None, *, raw=None) -> Path:
    """A synthetic owner acceptance contract; CMake reads only its checks."""
    path = root / "build/acceptance" / owner / "checks.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    document = {"version": 1, "checks": []}
    if release_evidence is not None:
        document["release_evidence"] = release_evidence
    path.write_text(json.dumps(document) if raw is None else raw, encoding="utf-8")
    return path


def evidence_entry(name="visible_latency", platforms=None, description="Timed GUI latency run on a real display"):
    return {"name": name, "description": description, "platforms": list(PLATFORMS) if platforms is None else platforms}


class ReleaseEvidenceTests(unittest.TestCase):
    """Manual real-device (L4) evidence that formal game releases reference."""

    def setUp(self):
        self.trains = derive_trains(REGISTRY_PAIRS)
        self.directory = TemporaryDirectory()
        self.root = Path(self.directory.__enter__())
        self.addCleanup(self.directory.__exit__, None, None, None)

    def prepare(self, train, reference=None, *, evidence=None):
        inputs = {"train": train, "version": "v2026.10.1" if train == TOOLS_TRAIN else "v1.0.0", "prerelease": False}
        if reference is not None:
            inputs["l4_evidence"] = reference
        return prepare_event("workflow_dispatch", {"inputs": inputs}, COMMIT, "refs/heads/master", FakeGit(),
                             trains=self.trains,
                             evidence=evidence or (lambda selected: train_release_evidence(selected, self.root)))

    def test_schema_accepts_declared_items_in_platform_order_and_no_section(self):
        document = {"version": 1, "checks": [], "release_evidence": [
            evidence_entry(), evidence_entry("gpu_visual", ["macos-x64", "linux-x64"], "Physical GPU visual check")]}
        self.assertEqual(validate_release_evidence(document), (
            EvidenceItem("visible_latency", "Timed GUI latency run on a real display", tuple(PLATFORMS)),
            EvidenceItem("gpu_visual", "Physical GPU visual check", ("linux-x64", "macos-x64"))))
        self.assertEqual(validate_release_evidence({"version": 1, "checks": []}), ())
        self.assertEqual(validate_release_evidence({"release_evidence": []}), ())

    def test_schema_rejects_malformed_items(self):
        invalid = [
            [],
            {"release_evidence": {}},
            {"release_evidence": ["visible_latency"]},
            {"release_evidence": [{**evidence_entry(), "required": True}]},
            {"release_evidence": [{key: value for key, value in evidence_entry().items() if key != "platforms"}]},
            {"release_evidence": [evidence_entry(), evidence_entry()]},
            {"release_evidence": [evidence_entry("Visible-Latency")]},
            {"release_evidence": [evidence_entry(description="")]},
            {"release_evidence": [evidence_entry(description=" padded")]},
            {"release_evidence": [evidence_entry(description="two\nlines")]},
            {"release_evidence": [evidence_entry(description="x" * 301)]},
            {"release_evidence": [evidence_entry(platforms=[])]},
            {"release_evidence": [evidence_entry(platforms=["linux-arm64"])]},
            {"release_evidence": [evidence_entry(platforms=["linux-x64", "linux-x64"])]},
        ]
        for document in invalid:
            with self.subTest(document=document), self.assertRaises(ReleaseError):
                validate_release_evidence(document)

    def test_owner_contract_is_read_from_its_own_acceptance_directory(self):
        self.assertEqual(load_release_evidence(APP, self.root), ())
        write_contract(self.root, APP, [evidence_entry()])
        self.assertEqual([item.name for item in load_release_evidence(APP, self.root)], ["visible_latency"])
        self.assertEqual(load_release_evidence(SECOND, self.root), ())
        write_contract(self.root, SECOND, raw="{not json")
        with self.assertRaises(ReleaseError):
            load_release_evidence(SECOND, self.root)

    def test_game_train_requires_its_declared_items_on_its_platforms_only(self):
        write_contract(self.root, SECOND, [evidence_entry(), evidence_entry("linux_gpu", ["linux-x64"])])
        # SECOND has no Linux row, so the Linux-only item does not apply to it.
        required = train_release_evidence(self.trains[SECOND], self.root)
        self.assertEqual(required, (EvidenceItem("visible_latency", "Timed GUI latency run on a real display",
                                                 ("windows-x64", "macos-arm64")),))
        self.assertEqual(train_release_evidence(self.trains[APP], self.root), ())

    def test_tools_train_never_requires_evidence(self):
        for owner in ("toolchain", "tools"):
            write_contract(self.root, owner, [evidence_entry()])
        self.assertEqual(train_release_evidence(self.trains[TOOLS_TRAIN], self.root), ())
        self.assertEqual(self.prepare(TOOLS_TRAIN)["l4_items"], "")
        self.assertEqual(self.prepare(TOOLS_TRAIN, "  ")["l4_evidence"], "")
        with self.assertRaisesRegex(ReleaseError, f"l4_evidence is not used by train {TOOLS_TRAIN}"):
            self.prepare(TOOLS_TRAIN, "discussion #9")

    def test_prepare_fails_clearly_without_a_reference_for_a_declaring_game(self):
        write_contract(self.root, APP, [evidence_entry(), evidence_entry("gpu_visual")])
        for reference in (None, "", "   "):
            with self.subTest(reference=reference), self.assertRaisesRegex(
                    ReleaseError, rf"Train {APP} requires real-device \(L4\) release evidence for: "
                                  r"visible_latency, gpu_visual.*l4_evidence"):
                self.prepare(APP, reference)
        result = self.prepare(APP, "  https://github.com/example/repo/issues/42  ")
        self.assertEqual((result["l4_items"], result["l4_evidence"]),
                         ("visible_latency,gpu_visual", "https://github.com/example/repo/issues/42"))
        # A game without declared items needs no reference and accepts none.
        self.assertEqual(self.prepare(SECOND)["l4_evidence"], "")
        with self.assertRaisesRegex(ReleaseError, f"l4_evidence is not used by train {SECOND}"):
            self.prepare(SECOND, "https://github.com/example/repo/issues/42")

    def test_evidence_reference_is_one_bounded_line(self):
        train = self.trains[APP]
        items = (EvidenceItem("visible_latency", "d", ("linux-x64",)),)
        self.assertEqual(validate_evidence_reference(train, items, "discussion #7"), "discussion #7")
        self.assertEqual(validate_evidence_reference(train, (), None), "")
        self.assertEqual(validate_evidence_reference(train, items, "issue-#1"), "issue-#1")
        with self.assertRaisesRegex(ReleaseError, "not used by train"):
            validate_evidence_reference(train, (), "artifact link")
        for reference in ("issue\n#1", "issue\r#1", "a\tb", "x" * (EVIDENCE_REFERENCE_LIMIT + 1), 42,
                          "-issue#12", "--see-#12", "  -x"):
            with self.subTest(reference=reference), self.assertRaises(ReleaseError):
                validate_evidence_reference(train, items, reference)
        with self.assertRaises(ReleaseError):
            self.prepare(APP, "line\ninjected=true", evidence=lambda selected: items)

    def test_formal_notes_carry_a_checklist_and_snapshots_never_do(self):
        train = self.trains[APP]
        descriptions = [describe_item(quick_package(platform), "quick") for platform in PLATFORMS]
        items = (EvidenceItem("visible_latency", "Timed GUI latency run", ("linux-x64", "macos-x64")),
                 EvidenceItem("gpu_visual", "Physical GPU visual check", ("windows-x64",)))
        notes = render_notes(train=train, commit=COMMIT, profile="release", descriptions=descriptions,
                             snapshot=False, evidence=items, evidence_reference="https://example.test/issues/42")
        self.assertIn("### Real-device evidence (L4)", notes)
        self.assertIn("Evidence: <https://example.test/issues/42>\n", notes)
        self.assertIn("the publisher confirms every item", notes)
        self.assertIn("- [ ] `visible_latency` (linux-x64, macos-x64): Timed GUI latency run\n", notes)
        self.assertIn("- [ ] `gpu_visual` (windows-x64): Physical GPU visual check\n", notes)
        self.assertLess(notes.index("### CI verification"), notes.index("### Real-device evidence (L4)"))
        plain = render_notes(train=train, commit=COMMIT, profile="release", descriptions=descriptions, snapshot=False)
        self.assertNotIn("Real-device", plain)
        with self.assertRaises(ReleaseError):
            render_notes(train=self.trains[TOOLS_TRAIN], commit=COMMIT, profile="release",
                         descriptions=descriptions, snapshot=False, evidence_reference="discussion #9")
        snapshot = render_notes(train=train, commit=COMMIT, profile="quick", descriptions=descriptions, snapshot=True)
        self.assertNotIn("Real-device", snapshot)
        for options in ({"evidence": items}, {"evidence_reference": "x"}):
            with self.subTest(options=options), self.assertRaises(ReleaseError):
                render_notes(train=train, commit=COMMIT, profile="quick", descriptions=descriptions, snapshot=True,
                             **options)

    def test_evidence_reference_renders_as_inert_markdown(self):
        # Only the person running the workflow writes this field; it must not
        # add links, images, HTML, mentions or cross-references to the notes.
        item = (EvidenceItem("visible_latency", "d", ("linux-x64",)),)
        cases = {
            "https://github.com/example/repo/issues/42": "<https://github.com/example/repo/issues/42>",
            "see #12 by @someone": "`see #12 by @someone`",
            "[proof](https://evil.test) ![x](https://evil.test/x.png)":
                "`[proof](https://evil.test) ![x](https://evil.test/x.png)`",
            "<img src=x onerror=alert(1)>": "`<img src=x onerror=alert(1)>`",
            "https://example.test/<b>": "`https://example.test/<b>`",
            "https://example.test/a b": "`https://example.test/a b`",
            "run `check` then ``more``": "``` run `check` then ``more`` ```",
            "`quoted`": "`` `quoted` ``",
        }
        for reference, rendered in cases.items():
            with self.subTest(reference=reference):
                lines = render_evidence(item, reference)
                self.assertIn(f"Evidence: {rendered}", lines)

    def test_every_owner_contract_declares_valid_release_evidence(self):
        # Iterates whatever owners exist; holds with none, assumes no product.
        for path in sorted((ROOT / "build/acceptance").glob("*/checks.json")):
            with self.subTest(owner=path.parent.name):
                load_release_evidence(path.parent.name)


class SnapshotApi(FakeApi):
    """FakeApi plus the publish and delete calls snapshots use."""

    PUBLISHED_AT = "2026-10-08T00:00:00Z"

    def __init__(self, others=(), tags=(), *, tag_commits=None, ancestry=()):
        super().__init__(release=False, commit=None)
        self.other_releases = [copy.deepcopy(release) for release in others]
        self.tags = {release["tag_name"] for release in others} | set(tags)
        self.deleted_tags = []
        # Commits of the other releases' tags, and (ancestor, descendant) pairs.
        self.tag_commits = dict(tag_commits or {})
        self.ancestry = set(ancestry)

    def request(self, method, path, data=None):
        if method == "GET" and path.startswith("/git/ref/tags/"):
            tag = urllib.parse.unquote(path.removeprefix("/git/ref/tags/"))
            if tag in self.tag_commits:
                self.calls.append((method, path, data))
                return {"object": {"type": "commit", "sha": self.tag_commits[tag]}}
        if method == "GET" and path.startswith("/compare/"):
            self.calls.append((method, path, data))
            base, head = path.removeprefix("/compare/").removesuffix("?per_page=1").split("...")
            if base == head:
                return {"status": "identical"}
            return {"status": "behind" if (head, base) in self.ancestry else
                    "ahead" if (base, head) in self.ancestry else "diverged"}
        if method == "GET" and path.startswith("/git/matching-refs/tags/"):
            self.calls.append((method, path, data))
            prefix = urllib.parse.unquote(path.removeprefix("/git/matching-refs/tags/"))
            current = {self.release["tag_name"]} if self.release and self.commit else set()
            return [{"ref": f"refs/tags/{tag}"} for tag in sorted((self.tags | current) - set(self.deleted_tags))
                    if tag.startswith(prefix)]
        if method == "PATCH" and path == "/releases/42":
            self.calls.append((method, path, data))
            self.mutations.append((method, path, data))
            self.release.update(data)
            if data.get("draft") is False:
                self.release["published_at"] = self.PUBLISHED_AT
            return copy.deepcopy(self.release)
        if method == "DELETE" and path.startswith("/releases/"):
            self.calls.append((method, path, data))
            self.mutations.append((method, path, data))
            release_id = int(path.rsplit("/", 1)[1])
            if not any(release["id"] == release_id for release in self.other_releases):
                raise ApiError(404, "Not found")
            self.other_releases = [release for release in self.other_releases if release["id"] != release_id]
            return None
        if method == "DELETE" and path.startswith("/git/refs/tags/"):
            self.calls.append((method, path, data))
            self.mutations.append((method, path, data))
            self.deleted_tags.append(urllib.parse.unquote(path.removeprefix("/git/refs/tags/")))
            return None
        return super().request(method, path, data)


class SnapshotPublishTests(unittest.TestCase):
    def setUp(self):
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))
        self.train = derive_trains(REGISTRY_PAIRS)[SECOND]
        self.items = [quick_package(platform, SECOND) for platform in self.train.platforms]
        self.tag = snapshot_tag(self.train, COMMIT, COMMITTED_AT)
        self.older = [snapshot_release(SECOND, f"202609{day:02d}", f"{day:07x}", day) for day in range(1, 7)]
        self.unrelated = [snapshot_release("tools", "20260901", "0000001", 90),
                          {"id": 91, "tag_name": f"{SECOND}-v1.0.0", "draft": False, "prerelease": True},
                          snapshot_release(SECOND, "20260801", "0000002", 92, draft=True)]

    def publish(self, api, items=None):
        return publish_snapshot(api, self.train, self.tag, COMMIT, self.items if items is None else items,
                                expected_pairs=self.train.pairs, expected_services=set(), notes="Snapshot notes")

    def test_new_snapshot_is_verified_published_and_trims_only_its_own_old_snapshots(self):
        api = SnapshotApi(self.older + self.unrelated)
        result = self.publish(api)
        kinds = [(method, path if method != "UPLOAD" else "upload") for method, path, _ in api.mutations]
        self.assertEqual(kinds[:2], [("POST", "/git/refs"), ("POST", "/releases")])
        self.assertEqual(api.mutations[0][2], {"ref": f"refs/tags/{self.tag}", "sha": COMMIT})
        draft = api.mutations[1][2]
        self.assertEqual((draft["draft"], draft["prerelease"], draft["generate_release_notes"], draft["body"]),
                         (True, True, False, "Snapshot notes"))
        # Published only after every archive was uploaded and verified.
        patch_index = kinds.index(("PATCH", "/releases/42"))
        self.assertEqual(kinds[2:patch_index], [("UPLOAD", "upload")] * (2 * len(self.items)))
        self.assertEqual(api.mutations[patch_index][2], {"draft": False, "prerelease": True, "make_latest": "false"})
        # Five kept: the new one and the four newest older snapshots.
        self.assertEqual(api.deleted_tags, [f"{SECOND}-snapshot-202609{day:02d}-{day:07x}" for day in (2, 1)])
        self.assertEqual(sorted(release["id"] for release in api.other_releases), [3, 4, 5, 6, 90, 91, 92])
        self.assertEqual(result["published"], "true")
        self.assertEqual(result["tag"], self.tag)
        self.assertEqual(result["deleted"], ",".join(api.deleted_tags))

    def test_orphaned_snapshot_tags_of_this_train_are_swept(self):
        orphan = f"{SECOND}-snapshot-20260815-abcdef0"
        api = SnapshotApi(self.older[:1] + self.unrelated,
                          tags=(orphan, f"{SECOND}-v0.9.0", "tools-snapshot-20260815-abcdef0", f"{SECOND}_two-snapshot-20260815-abcdef0"))
        result = self.publish(api)
        # Only the release-less snapshot tag of this train; releases, formal
        # tags and other trains' tags are never touched.
        self.assertEqual(api.deleted_tags, [orphan])
        self.assertEqual(result["deleted"], orphan)
        self.assertEqual(sorted(release["id"] for release in api.other_releases), [1, 90, 91, 92])

    def test_rerun_of_a_published_snapshot_only_verifies_it(self):
        api = SnapshotApi(self.older[:2])
        self.publish(api)
        mutations = len(api.mutations)
        result = self.publish(api)
        self.assertEqual(len(api.mutations), mutations)
        self.assertEqual(result["published"], "true")
        # A published snapshot missing an archive is never modified.
        removed = self.items[0].name
        del api.assets[removed]
        with self.assertRaisesRegex(ReleaseError, "never modified"):
            self.publish(api)
        self.assertEqual(len(api.mutations), mutations)

    def test_interrupted_draft_is_reconciled_then_published(self):
        api = SnapshotApi()
        api.fail_upload_at = 2
        with self.assertRaises(ApiError):
            self.publish(api)
        self.assertTrue(api.release["draft"])
        api.fail_upload_at = None
        self.publish(api)
        self.assertFalse(api.release["draft"])
        self.assertEqual(len(api.assets), 2 * len(self.items))

    def test_rerun_of_an_older_default_branch_commit_is_skipped_without_mutation(self):
        # A newer default-branch commit already has a snapshot; this run's
        # commit is its ancestor, so publishing would rank stale state newest.
        newer = [snapshot_release(SECOND, f"202610{day:02d}", f"{day:07x}", 100 + day) for day in range(3, 5)]
        api = SnapshotApi(newer, tag_commits={newer[-1]["tag_name"]: OTHER_COMMIT}, ancestry={(COMMIT, OTHER_COMMIT)})
        result = self.publish(api)
        self.assertEqual(result["published"], "false")
        self.assertEqual(api.mutations, [])
        self.assertIn(("GET", f"/compare/{OTHER_COMMIT}...{COMMIT}?per_page=1", None), api.calls)
        # An interrupted draft of that older commit is not published either.
        api.release = {"id": 42, "tag_name": self.tag, "draft": True, "prerelease": True,
                       "html_url": "https://github.com/test/repo/releases/tag/x"}
        self.assertEqual(self.publish(api)["published"], "false")
        self.assertEqual(api.mutations, [])

    def test_later_push_of_an_older_dated_commit_is_published_and_kept(self):
        # Five snapshots dated and published 3-7 October; this push's head is
        # dated 2 October but descends from none of them (not an ancestor).
        newer = [snapshot_release(SECOND, f"202610{day:02d}", f"{day:07x}", 100 + day) for day in range(3, 8)]
        api = SnapshotApi(newer, tag_commits={newer[-1]["tag_name"]: OTHER_COMMIT}, ancestry={(OTHER_COMMIT, COMMIT)})
        result = self.publish(api)
        self.assertEqual(result["published"], "true")
        # Retention drops the earliest published snapshot, never the new one.
        self.assertEqual(api.deleted_tags, [newer[0]["tag_name"]])
        self.assertEqual(sorted(release["id"] for release in api.other_releases), [104, 105, 106, 107])
        self.assertFalse(api.release["draft"])
        # Its rerun only verifies it; it stays among the kept snapshots.
        mutations = len(api.mutations)
        self.assertEqual(self.publish(api)["published"], "true")
        self.assertEqual(len(api.mutations), mutations)

    def test_promoted_snapshot_quick_evidence_and_foreign_tags_are_refused(self):
        api = SnapshotApi()
        api.release = {"id": 42, "tag_name": self.tag, "draft": False, "prerelease": False,
                       "html_url": "https://github.com/test/repo/releases/tag/x"}
        api.commit = COMMIT
        with self.assertRaisesRegex(ReleaseError, "promoted"):
            self.publish(api)
        # Release-profile packages are not snapshot evidence, and vice versa.
        release_items = [package(platform, product=SECOND) for platform in self.train.platforms]
        with self.assertRaisesRegex(ReleaseError, "identity/profile"):
            self.publish(SnapshotApi(), release_items)
        with self.assertRaises(ReleaseError):
            publish_snapshot(SnapshotApi(), self.train, f"{SECOND}-v1.0.0", COMMIT, self.items,
                             expected_pairs=self.train.pairs, expected_services=set(), notes="")
        self.assertEqual(api.mutations, [])

    def test_snapshot_packages_load_with_quick_evidence_only(self):
        with TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for item in self.items:
                (directory / item.name).write_bytes(item.data)
                (directory / (item.name + ".sha256")).write_bytes(checksum_document(item.name, item.data))
            self.assertEqual(len(load_packages(directory, COMMIT, self.train.pairs, profile="quick")), 2)
            with self.assertRaisesRegex(ReleaseError, "identity/profile"):
                load_packages(directory, COMMIT, self.train.pairs)


if __name__ == "__main__":
    unittest.main()
