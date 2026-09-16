"""Exercise the release workflow's actual final gate without GitHub API calls."""

import itertools
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[3]
WORKFLOWS = ROOT / ".github" / "workflows"


def job_block(workflow: str, name: str) -> str:
    """Read one top-level job, not a general-purpose YAML parser."""
    match = re.search(rf"(?m)^  {re.escape(name)}:\s*$", workflow)
    if match is None:
        raise AssertionError(f"Required workflow job is missing: {name}")
    tail = workflow[match.end():]
    next_job = re.search(r"(?m)^  [A-Za-z_][A-Za-z0-9_-]*:\s*$", tail)
    return tail[:next_job.start()] if next_job else tail


def step_using(job: str, action: str) -> str:
    for step in re.split(r"(?m)^      - ", job):
        if re.search(rf"uses: {re.escape(action)}@", step):
            return step
    raise AssertionError(f"Required workflow action is missing: {action}")


def find_bash() -> str | None:
    candidates = []
    if os.name == "nt":
        # Windows' system32/bash.exe is a WSL launcher, not a native Bash.
        git = shutil.which("git")
        if git:
            git_dir = Path(git).resolve().parent
            candidates.extend((git_dir / "bash.exe", git_dir.parent / "bin" / "bash.exe"))
        candidates.append(Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Git/bin/bash.exe")
    elif bash := shutil.which("bash"):
        candidates.append(Path(bash))
    for candidate in candidates:
        if not candidate.is_file():
            continue
        try:
            result = subprocess.run([str(candidate), "--version"], capture_output=True,
                                    text=True, timeout=10, check=False)
        except (OSError, subprocess.TimeoutExpired):
            continue
        if result.returncode == 0 and "GNU bash" in result.stdout:
            return str(candidate)
    return None


class WorkflowGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.quick = (WORKFLOWS / "cross-platform.yml").read_text(encoding="utf-8")
        cls.release = (WORKFLOWS / "prepare-release.yml").read_text(encoding="utf-8")
        cls.shared = (WORKFLOWS / "build-and-validate.yml").read_text(encoding="utf-8")

    def test_actual_final_gate_rejects_failed_skipped_and_cancelled_jobs(self):
        bash = find_bash()
        if bash is None:
            if os.name == "nt":
                self.skipTest("Native Git Bash cannot execute in this Windows environment")
            self.fail("Bash is required by the Ubuntu CI acceptance gate")
        gate = job_block(self.shared, "validation")
        self.assertRegex(gate, r"(?m)^    needs: \[prepare, native\]$")
        self.assertRegex(gate, r"(?m)^    if: always\(\)$")
        runs = list(re.finditer(r"(?m)^        run: \|\s*$", gate))
        self.assertEqual(len(runs), 1, "The final validation job must have one executable policy block")
        script = textwrap.dedent(gate[runs[0].end():]).strip()
        self.assertTrue(script, "The final gate must execute a real policy")
        for prepare, native in itertools.product(("success", "failure", "skipped", "cancelled"), repeat=2):
            with self.subTest(prepare=prepare, native=native):
                environment = dict(os.environ, PREPARE_RESULT=prepare, NATIVE_RESULT=native,
                                   GITHUB_STEP_SUMMARY="/dev/null")
                result = subprocess.run([bash, "--noprofile", "--norc", "-e", "-o", "pipefail", "-c", script],
                                        env=environment, text=True, capture_output=True, timeout=10, check=False)
                self.assertEqual(result.returncode == 0, prepare == native == "success",
                                 result.stdout + result.stderr)

    def test_only_final_draft_job_can_write_repository_contents(self):
        self.assertNotRegex(self.quick + self.shared, r"contents:\s*write")
        draft = job_block(self.release, "draft")
        self.assertRegex(draft, r"(?m)^    needs: \[prepare, build\]$")
        # No always()/failure()/cancelled() override may bypass GitHub's
        # implicit success() gate on the complete reusable build job.
        condition = re.search(r"(?m)^    if:\s*(.+)$", draft)
        if condition:
            self.assertIn(condition.group(1).strip(), ("success()", "${{ success() }}"))
        self.assertRegex(draft, r"permissions:\s*\n      contents: write")
        self.assertNotRegex(self.release.replace(draft, ""), r"contents:\s*write")
        self.assertNotRegex(draft, r"continue-on-error:\s*true")
        native = job_block(self.shared, "native")
        self.assertRegex(native, r"fail-fast:\s*false")
        self.assertNotRegex(native, r"continue-on-error:\s*true")

    def test_release_build_and_upload_share_captured_commit_and_current_run(self):
        prepare = job_block(self.release, "prepare")
        build = job_block(self.release, "build")
        draft = job_block(self.release, "draft")
        self.assertIn("ref: ${{ github.sha }}", step_using(prepare, "actions/checkout"))
        self.assertIn("source_commit: ${{ needs.prepare.outputs.commit }}", build)
        self.assertIn("profile: release", build)
        self.assertIn("ref: ${{ needs.prepare.outputs.commit }}", step_using(draft, "actions/checkout"))
        self.assertIn("SOURCE_COMMIT: ${{ needs.prepare.outputs.commit }}", draft)
        native = job_block(self.shared, "native")
        self.assertIn("ref: ${{ inputs.source_commit }}", step_using(native, "actions/checkout"))
        self.assertIn("SOURCE_COMMIT: ${{ inputs.source_commit }}", native)
        upload = step_using(native, "actions/upload-artifact")
        download = step_using(draft, "actions/download-artifact")
        self.assertIn("name: gyo-object-fps-${{ matrix.platform }}", upload)
        self.assertIn("pattern: gyo-object-fps-*", download)
        self.assertNotRegex(download, r"(?m)^\s*(run-id|repository|github-token):")

    def test_workflow_commands_match_the_release_helper_cli(self):
        # Ask the real CLI parser, without entering any network/mutation path.
        for command, required_options in (
            ("prepare", ("--event-path", "--event-name", "--commit", "--ref", "--output")),
            ("draft", ("--tag", "--commit", "--prerelease", "--package-directory", "--output")),
        ):
            with self.subTest(command=command):
                result = subprocess.run([sys.executable, str(ROOT / "tools/ci/release_pipeline.py"),
                                         command, "--help"], text=True, capture_output=True,
                                        timeout=10, check=False)
                self.assertEqual(result.returncode, 0, result.stderr)
                for option in required_options:
                    self.assertIn(option, result.stdout)


if __name__ == "__main__":
    unittest.main()
