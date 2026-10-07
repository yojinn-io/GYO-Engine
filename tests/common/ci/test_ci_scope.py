"""Pin the event-to-tier policy that keeps the single CI gate check meaningful."""

import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3]
for folder in ("build", "build/ci/common", "build/acceptance/common"):
    sys.path.insert(0, str(ROOT / folder))
from workspace import TemporaryDirectory
from app_registry import PLATFORMS, TOOLCHAINS
import ci_scope
from ci_scope import Scope, ScopeError, select_scope


def unexpected_changes():
    raise AssertionError("Changed paths must not be computed for this event")


def unexpected_increment():
    raise AssertionError("The documentation-increment check must not run for this event")


def pull_request(draft):
    return {"pull_request": {"number": 7, "draft": draft}}


def make_tree_writable(root: Path):
    """Git writes objects read-only, and Windows refuses to delete such files."""
    for directory, _, files in os.walk(root):
        for name in files:
            path = os.path.join(directory, name)
            os.chmod(path, os.stat(path).st_mode | stat.S_IWRITE)


class ScopePolicyTests(unittest.TestCase):
    def test_integration_events_select_l1_and_quick_without_inspecting_changes(self):
        for event_name in ("push", "workflow_dispatch"):
            with self.subTest(event=event_name):
                self.assertEqual(select_scope(event_name, {}, unexpected_changes),
                                 Scope(run_l1=True, run_quick=True, reason="integration"))

    def test_draft_pull_request_runs_nothing_and_never_inspects_changes(self):
        self.assertEqual(select_scope("pull_request", pull_request(True), unexpected_changes, live_draft=True),
                         Scope(run_l1=False, run_quick=False, reason="draft"))

    def test_live_draft_state_overrides_a_replayed_payload(self):
        # Re-running a run recorded while the pull request was a draft replays
        # draft=true; once the pull request is ready it must still verify L1.
        self.assertEqual(select_scope("pull_request", pull_request(True), lambda: ["engine/a.cpp"], live_draft=False),
                         Scope(run_l1=True, run_quick=False, reason="pull-request"))
        self.assertEqual(select_scope("pull_request", pull_request(False), unexpected_changes, live_draft=True),
                         Scope(run_l1=False, run_quick=False, reason="draft"))

    def test_ready_pull_request_classification(self):
        for paths, expected in (
            (["docs/releasing.ja.md"], Scope(False, False, "docs-only")),
            (["docs/a.md", "docs/deep/b.png"], Scope(False, False, "docs-only")),
            (["docs/a.md", "engine/base/src/Sha256.cpp"], Scope(True, False, "pull-request")),
            (["README.md"], Scope(True, False, "pull-request")),
            # Neither a file named docs nor a sibling prefix is documentation.
            (["docs"], Scope(True, False, "pull-request")),
            (["docsx/a.md"], Scope(True, False, "pull-request")),
            (["build/docs/a.md"], Scope(True, False, "pull-request")),
            # An empty change list is unexpected; verify instead of skipping.
            ([], Scope(True, False, "pull-request")),
        ):
            with self.subTest(paths=paths):
                self.assertEqual(select_scope("pull_request", pull_request(False), lambda: paths, live_draft=False),
                                 expected)

    def test_documentation_increment_reuses_a_verified_l1_result(self):
        code = lambda: ["engine/a.cpp", "docs/a.md"]  # noqa: E731
        self.assertEqual(select_scope("pull_request", pull_request(False), code, False, lambda: ("c0ffee", "why")),
                         Scope(run_l1=False, run_quick=False, reason="docs-increment", detail="why"))
        self.assertEqual(select_scope("pull_request", pull_request(False), code, False, lambda: (None, "why not")),
                         Scope(run_l1=True, run_quick=False, reason="pull-request", detail="why not"))
        # Without the check the classification is unchanged.
        self.assertEqual(select_scope("pull_request", pull_request(False), code, False),
                         Scope(run_l1=True, run_quick=False, reason="pull-request"))

    def test_documentation_increment_errors_run_l1(self):
        for error in (ScopeError("listing"), OSError("gh missing"), subprocess.TimeoutExpired("gh", 60), ValueError("json")):
            def failing(error=error):
                raise error
            with self.subTest(error=type(error).__name__):
                scope = select_scope("pull_request", pull_request(False), lambda: ["engine/a.cpp"], False, failing)
                self.assertEqual((scope.run_l1, scope.run_quick, scope.reason), (True, False, "pull-request"))
                self.assertIn("failed", scope.detail)

    def test_documentation_increment_is_only_checked_for_ready_pull_requests_with_code_changes(self):
        for event_name in ("push", "workflow_dispatch"):
            self.assertEqual(select_scope(event_name, {}, unexpected_changes, None, unexpected_increment).reason,
                             "integration")
        self.assertEqual(select_scope("pull_request", pull_request(True), unexpected_changes, True,
                                      unexpected_increment).reason, "draft")
        self.assertEqual(select_scope("pull_request", pull_request(False), lambda: ["docs/a.md"], False,
                                      unexpected_increment).reason, "docs-only")

    def test_l1_rows_must_all_have_passed_in_github_actions(self):
        def run(platform, status="completed", conclusion="success", app="github-actions"):
            return {"name": f"L1 / {platform}", "status": status, "conclusion": conclusion, "app": {"slug": app}}

        def listing(runs):
            return {"total_count": len(runs), "check_runs": runs}

        passed = [run(platform) for platform in PLATFORMS]
        self.assertTrue(ci_scope.l1_rows_passed(listing(passed + [run("extra", conclusion="failure")]), PLATFORMS))
        for index, platform in enumerate(PLATFORMS):
            with self.subTest(platform=platform):
                self.assertFalse(ci_scope.l1_rows_passed(listing(passed[:index] + passed[index + 1:]), PLATFORMS))
                for status, conclusion in (("completed", "failure"), ("completed", "cancelled"), ("completed", "skipped"),
                                           ("completed", "neutral"), ("in_progress", None), ("queued", None)):
                    changed = passed[:index] + [run(platform, status, conclusion)] + passed[index + 1:]
                    self.assertFalse(ci_scope.l1_rows_passed(listing(changed), PLATFORMS), (status, conclusion))
                # A same-named row from another app never verifies a commit.
                spoofed = passed[:index] + [run(platform, app="other-app")] + passed[index + 1:]
                self.assertFalse(ci_scope.l1_rows_passed(listing(spoofed), PLATFORMS))
                # Every row of a name must have passed.
                twice = passed + [run(platform, conclusion="failure")]
                self.assertFalse(ci_scope.l1_rows_passed(listing(twice), PLATFORMS))
        for malformed in ({"total_count": len(passed) + 1, "check_runs": passed}, {"check_runs": passed},
                          {"total_count": 0}, [], None):
            with self.subTest(malformed=malformed), self.assertRaises(ScopeError):
                ci_scope.l1_rows_passed(malformed, PLATFORMS)

    def test_failed_check_run_lookup_raises_and_runs_l1(self):
        # Any executable that rejects the gh arguments stands in for a failed API call.
        lookup = ci_scope.github_l1_lookup("owner/repository", PLATFORMS, gh=sys.executable)
        with self.assertRaises(ScopeError):
            lookup("0" * 40)
        scope = select_scope("pull_request", pull_request(False), lambda: ["engine/a.cpp"], False,
                             lambda: (lookup("0" * 40), "unreachable"))
        self.assertEqual((scope.run_l1, scope.reason), (True, "pull-request"))

    def test_unclassifiable_events_fail_closed(self):
        for event in ({}, {"pull_request": None}, {"pull_request": []}, []):
            with self.subTest(event=event), self.assertRaises(ScopeError):
                select_scope("pull_request", event, unexpected_changes, live_draft=False)
        # Without the live state a stale payload could skip the merge gate.
        for live_draft in (None, "false", 0, 1):
            with self.subTest(live_draft=live_draft), self.assertRaises(ScopeError):
                select_scope("pull_request", pull_request(False), unexpected_changes, live_draft=live_draft)
        for event_name in ("pull_request_target", "schedule", "release", ""):
            with self.subTest(event_name=event_name), self.assertRaises(ScopeError):
                select_scope(event_name, {}, unexpected_changes)

    def test_platform_rows_come_only_from_the_toolchain_table(self):
        rows = ci_scope.platform_matrix()["include"]
        self.assertEqual([row["platform"] for row in rows], list(PLATFORMS))
        for row in rows:
            with self.subTest(platform=row["platform"]):
                self.assertEqual(row, dict(platform=row["platform"], **TOOLCHAINS[row["platform"]]))
                self.assertFalse({"product", "kind", "tools"} & set(row))

    def test_cross_built_rows_share_their_host_but_not_their_target_caches(self):
        # Each row owns its preset, so its build tree and compiler cache family
        # (keyed by platform and preset) never mix target architectures.
        self.assertEqual(len({row["preset"] for row in TOOLCHAINS.values()}), len(TOOLCHAINS))
        for platform, row in TOOLCHAINS.items():
            with self.subTest(platform=platform):
                host = TOOLCHAINS[row["host"]]
                # Host tools are cached per host, so a host must be a native
                # row on the same runner image with the same toolchain.
                self.assertEqual(host["host"], row["host"])
                self.assertEqual((host["runner"], host["toolchain"]), (row["runner"], row["toolchain"]))
                self.assertEqual(row["cpu_execution"] == "native", row["host"] == platform)
        self.assertEqual((TOOLCHAINS["macos-x64"]["host"], TOOLCHAINS["macos-x64"]["cpu_execution"]),
                         ("macos-arm64", "rosetta2"))


class GitRepositoryCase(unittest.TestCase):
    """A throwaway repository with one base commit on main."""

    def setUp(self):
        temporary = TemporaryDirectory(prefix="gyo-ci-scope-")
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        # Cleanups run last-in first-out, so this clears read-only bits first.
        self.addCleanup(make_tree_writable, root)
        self.repository = root / "repository"
        self.repository.mkdir()
        empty_config = root / "gitconfig"
        empty_config.write_text("", encoding="utf-8")
        self.environment = dict(os.environ, GIT_CONFIG_GLOBAL=str(empty_config), GIT_CONFIG_NOSYSTEM="1",
                                GIT_AUTHOR_NAME="CI Fixture", GIT_AUTHOR_EMAIL="fixture@example.invalid",
                                GIT_COMMITTER_NAME="CI Fixture", GIT_COMMITTER_EMAIL="fixture@example.invalid")
        self.git("init", "-q", "-b", "main")
        self.write("engine/a.cpp", "int a;\n")
        self.write("docs/guide.md", "guide\n")
        self.commit("base")

    def git(self, *arguments):
        return subprocess.run(["git", "-c", "commit.gpgsign=false", "-c", "core.autocrlf=false", *arguments],
                              cwd=self.repository, env=self.environment, check=True,
                              capture_output=True, text=True, timeout=30).stdout

    def write(self, relative, content):
        path = self.repository / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def commit(self, message):
        self.git("add", "-A")
        self.git("commit", "-q", "-m", message)
        return self.git("rev-parse", "HEAD").strip()


class MergeChangeTests(GitRepositoryCase):
    """Exercise the real git commands against GitHub-shaped merge commits."""

    def merge_like_github(self, branch_change, base_change):
        """Merge a topic branch into an advanced base with --no-ff, as refs/pull/N/merge does."""
        self.git("checkout", "-q", "-b", "topic")
        branch_change()
        self.commit("topic")
        self.git("checkout", "-q", "main")
        base_change()
        self.commit("base advanced")
        self.git("merge", "-q", "--no-ff", "-m", "merge", "topic")

    def test_documentation_branch_merged_into_advanced_base_is_docs_only(self):
        self.merge_like_github(lambda: self.write("docs/guide.md", "guide v2\n"),
                               lambda: self.write("engine/a.cpp", "int a = 1;\n"))
        paths = ci_scope.merge_changes(self.repository)
        self.assertEqual(paths, ["docs/guide.md"])
        self.assertEqual(select_scope("pull_request", pull_request(False), lambda: paths, False).reason, "docs-only")

    def test_rename_into_docs_reports_its_source(self):
        self.merge_like_github(lambda: self.git("mv", "engine/a.cpp", "docs/a.cpp"),
                               lambda: self.write("docs/guide.md", "base guide\n"))
        paths = ci_scope.merge_changes(self.repository)
        self.assertEqual(sorted(paths), ["docs/a.cpp", "engine/a.cpp"])
        self.assertEqual(select_scope("pull_request", pull_request(False), lambda: paths, False).reason,
                         "pull-request")

    def test_non_merge_head_is_rejected(self):
        self.write("docs/guide.md", "direct\n")
        self.commit("direct")
        with self.assertRaisesRegex(ScopeError, "two-parent"):
            ci_scope.merge_changes(self.repository)

    def test_cli_classifies_a_ready_documentation_pull_request(self):
        self.merge_like_github(lambda: self.write("docs/new.md", "new\n"),
                               lambda: self.write("engine/a.cpp", "int a = 2;\n"))
        event = self.repository.parent / "event.json"
        event.write_text(json.dumps(pull_request(False)), encoding="utf-8")
        output = self.repository.parent / "outputs.txt"
        result = subprocess.run([sys.executable, str(ROOT / "build/ci/common/ci_scope.py"),
                                 "--event-name", "pull_request", "--event-path", str(event), "--live-draft", "false",
                                 "--repository", str(self.repository), "--output", str(output)],
                                env=self.environment, capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr)
        outputs = dict(line.split("=", 1) for line in output.read_text(encoding="utf-8").splitlines())
        self.assertEqual((outputs["run_l1"], outputs["run_quick"], outputs["reason"]), ("false", "false", "docs-only"))


class DocumentationIncrementTests(GitRepositoryCase):
    """verified_increment against real pull request histories merged as refs/pull/N/merge."""

    def branch(self, *changes):
        """Commit each change on a topic branch from main; returns the commits, oldest first."""
        self.git("checkout", "-q", "-b", "topic")
        commits = []
        for index, change in enumerate(changes):
            change()
            commits.append(self.commit(f"topic {index}"))
        return commits

    def merge_into_main(self):
        self.git("checkout", "-q", "main")
        self.git("merge", "-q", "--no-ff", "-m", "merge", "topic")

    def find(self, passed, limit=ci_scope.INCREMENT_LIMIT):
        asked = []

        def l1_passed(commit):
            asked.append(commit)
            return commit in passed
        commit, note = ci_scope.verified_increment(self.repository, l1_passed, limit=limit)
        return commit, note, asked

    def code(self, content):
        return lambda: self.write("engine/a.cpp", content)

    def docs(self, content):
        return lambda: self.write("docs/guide.md", content)

    def test_nearest_verified_commit_whose_code_equals_the_head(self):
        code, docs1, docs2 = self.branch(self.code("int a = 1;\n"), self.docs("v2\n"), self.docs("v3\n"))
        self.merge_into_main()
        commit, note, asked = self.find({code})
        self.assertEqual(commit, code)
        self.assertIn("only under docs/", note)
        # The head itself is never its own evidence; the walk stops at the first verified commit.
        self.assertEqual(asked, [docs1, code])
        self.assertEqual(self.find({code, docs1})[0], docs1)
        self.assertEqual(ci_scope.merge_parents(self.repository)[1], docs2)

    def test_no_reuse_without_a_verified_commit_or_beyond_the_limit(self):
        code, docs1, docs2 = self.branch(self.code("int a = 1;\n"), self.docs("v2\n"), self.docs("v3\n"))
        self.merge_into_main()
        # Past the code commit the walk reaches the base, which differs in code.
        commit, note, asked = self.find(set())
        self.assertIsNone(commit)
        self.assertIn("outside docs/", note)
        self.assertEqual(asked, [docs1, code])
        self.assertIsNone(self.find({docs2})[0])
        commit, note, asked = self.find({code}, limit=1)
        self.assertIsNone(commit)
        self.assertIn("within 1 commits", note)
        self.assertEqual(asked, [docs1])

    def test_code_after_the_verified_commit_runs_l1(self):
        code, later, _ = self.branch(self.code("int a = 1;\n"), lambda: self.write("engine/b.cpp", "int b;\n"),
                                     self.docs("v2\n"))
        self.merge_into_main()
        commit, note, asked = self.find({code})
        self.assertIsNone(commit)
        self.assertIn("outside docs/", note)
        self.assertEqual(asked, [later])
        self.assertEqual(self.find({later})[0], later)

    def test_moving_code_into_docs_is_a_code_change(self):
        code, _, _ = self.branch(self.code("int a = 1;\n"), lambda: self.git("mv", "engine/a.cpp", "docs/a.cpp"),
                                 self.docs("v2\n"))
        self.merge_into_main()
        self.assertIsNone(self.find({code})[0])

    def test_an_advanced_base_runs_l1(self):
        # The verified commit was tested against an older base; the merge now
        # also contains newer base code, so its tree was never tested.
        code, _ = self.branch(self.code("int a = 1;\n"), self.docs("v2\n"))
        self.git("checkout", "-q", "main")
        self.write("engine/c.cpp", "int c;\n")
        self.commit("base advanced")
        self.git("merge", "-q", "--no-ff", "-m", "merge", "topic")
        commit, note, asked = self.find({code})
        self.assertIsNone(commit)
        self.assertIn("not an ancestor", note)
        self.assertEqual(asked, [])

    def test_a_verified_merge_of_the_base_into_the_branch_is_reused(self):
        self.branch(self.code("int a = 1;\n"))
        self.git("checkout", "-q", "main")
        self.write("engine/c.cpp", "int c;\n")
        self.commit("base advanced")
        self.git("checkout", "-q", "topic")
        self.git("merge", "-q", "--no-ff", "-m", "update from main", "main")
        updated = self.git("rev-parse", "HEAD").strip()
        self.write("docs/guide.md", "v2\n")
        self.commit("docs")
        self.merge_into_main()
        self.assertEqual(self.find({updated})[0], updated)


class CommandLineTests(unittest.TestCase):
    def setUp(self):
        temporary = TemporaryDirectory(prefix="gyo-ci-scope-cli-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def run_scope(self, event_name, event, *extra):
        path = self.root / "event.json"
        path.write_text(json.dumps(event), encoding="utf-8")
        output, summary = self.root / "outputs.txt", self.root / "summary.md"
        output.unlink(missing_ok=True)
        result = subprocess.run([sys.executable, str(ROOT / "build/ci/common/ci_scope.py"),
                                 "--event-name", event_name, "--event-path", str(path), *extra,
                                 "--output", str(output), "--summary", str(summary)],
                                capture_output=True, text=True, timeout=60)
        outputs = (dict(line.split("=", 1) for line in output.read_text(encoding="utf-8").splitlines())
                   if output.exists() else {})
        return result, outputs

    def test_outputs_are_lowercase_flags_and_toolchain_rows(self):
        for event_name, event, extra, expected in (
            ("push", {"ref": "refs/heads/master"}, (), ("true", "true", "integration")),
            ("workflow_dispatch", {"inputs": {}}, (), ("true", "true", "integration")),
            ("pull_request", pull_request(False), ("--live-draft", "true"), ("false", "false", "draft")),
        ):
            with self.subTest(event=event_name):
                result, outputs = self.run_scope(event_name, event, *extra)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual((outputs["run_l1"], outputs["run_quick"], outputs["reason"]), expected)
                # Rows stay valid even when L1 is not selected, so the matrix
                # expression can never fail on an empty include list.
                self.assertEqual(json.loads(outputs["platforms"]), ci_scope.platform_matrix())

    def test_invalid_event_writes_no_outputs_and_fails(self):
        result, outputs = self.run_scope("pull_request", {"pull_request": None}, "--live-draft", "false")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("::error::", result.stderr)
        self.assertEqual(outputs, {})

    def test_pull_request_without_a_valid_live_draft_state_fails(self):
        for extra in ((), ("--live-draft", ""), ("--live-draft", "null")):
            with self.subTest(extra=extra):
                result, outputs = self.run_scope("pull_request", pull_request(True), *extra)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(outputs, {})


if __name__ == "__main__":
    unittest.main()
