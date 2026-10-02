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


class MergeChangeTests(unittest.TestCase):
    """Exercise the real git commands against GitHub-shaped merge commits."""

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
