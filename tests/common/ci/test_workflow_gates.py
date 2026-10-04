"""Exercise the CI and release workflows' actual gates without GitHub API calls."""

import fnmatch
import itertools
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import textwrap
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
WORKFLOWS = ROOT / ".github" / "workflows"
sys.path.insert(0, str(ROOT / "build/ci/common"))
from go_checks import SERVICE_RECORD_NAME  # noqa: E402
from release_channels import TRIAL_LABEL  # noqa: E402

SNAPSHOT_CONDITION = ("github.event_name == 'push' && github.ref == format('refs/heads/{0}', "
                      "github.event.repository.default_branch) && !github.event.repository.fork")
NATIVE_SETUP = ROOT / ".github" / "actions" / "native-setup" / "action.yml"
HOST_TOOLS_DEFINE = r'''"-DGYO_SHADER_HOST_BUILD_DIR=$($env:GITHUB_WORKSPACE.Replace('\', '/'))/$env:GYO_HOST_TOOLS"'''


def job_block(workflow: str, name: str) -> str:
    """Read one top-level job, not a general-purpose YAML parser."""
    match = re.search(rf"(?m)^  {re.escape(name)}:\s*$", workflow)
    if match is None:
        raise AssertionError(f"Required workflow job is missing: {name}")
    tail = workflow[match.end():]
    next_job = re.search(r"(?m)^  [A-Za-z_][A-Za-z0-9_-]*:\s*$", tail)
    return tail[:next_job.start()] if next_job else tail


def top_level_block(document: str, key: str) -> str:
    """Read one top-level mapping, not a general-purpose YAML parser."""
    match = re.search(rf"(?m)^{re.escape(key)}:\s*$", document)
    if match is None:
        raise AssertionError(f"Required top-level key is missing: {key}")
    tail = document[match.end():]
    following = re.search(r"(?m)^[A-Za-z_][A-Za-z0-9_-]*:", tail)
    return tail[:following.start()] if following else tail


def permission_blocks(workflow: str) -> dict[str, dict[str, str] | str]:
    """Every permissions key of a workflow, keyed "workflow" or by job name.

    A scalar value (write-all, read-all, {}) is returned as a string; a
    mapping as {scope: access}. Any permissions key that is neither top-level
    nor job-level fails, so no block escapes the policy check.
    """
    lines = workflow.splitlines()
    blocks = {}
    job = None
    for index, line in enumerate(lines):
        if heading := re.fullmatch(r"  ([A-Za-z_][A-Za-z0-9_-]*):\s*", line):
            job = heading.group(1)
        entry = re.fullmatch(r"( *)permissions:\s*(.*?)\s*", line)
        if entry is None:
            continue
        indent, value = len(entry.group(1)), entry.group(2)
        if indent == 0:
            location = "workflow"
        elif indent == 4 and job is not None:
            location = job
        else:
            raise AssertionError(f"Unexpected permissions key at line {index + 1}: {line!r}")
        if location in blocks:
            raise AssertionError(f"Duplicate permissions for {location}")
        if value and not value.startswith("#"):
            blocks[location] = value
            continue
        scopes = {}
        for scope_line in lines[index + 1:]:
            if not scope_line.strip() or scope_line.strip().startswith("#"):
                continue
            scope = re.fullmatch(rf"{' ' * (indent + 2)}([a-z-]+):\s*([a-z]+)\s*(?:#.*)?", scope_line)
            if scope is None:
                if len(scope_line) - len(scope_line.lstrip()) > indent:
                    raise AssertionError(f"Unparsed permissions entry: {scope_line!r}")
                break
            scopes[scope.group(1)] = scope.group(2)
        blocks[location] = scopes
    return blocks


def steps_of(job: str, indent: int = 6) -> list[str]:
    """Split workflow job steps (6 spaces) or composite action steps (4)."""
    return re.split(rf"(?m)^{' ' * indent}- ", job)


def step_with(job: str, marker: str, indent: int = 6) -> str:
    matches = [step for step in steps_of(job, indent) if marker in step]
    if len(matches) != 1:
        raise AssertionError(f"Expected exactly one workflow step containing {marker!r}, found {len(matches)}")
    return matches[0]


def run_script(step: str) -> str:
    match = re.search(r"(?m)^        run: \|\s*$", step)
    if match is None:
        raise AssertionError("Workflow step has no executable block")
    # The block ends at the first non-blank line indented like a step key or
    # less, such as a comment introducing the next step.
    lines = []
    for line in step[match.end():].splitlines()[1:]:
        if line.strip() and not line.startswith(" " * 10):
            break
        lines.append(line)
    return textwrap.dedent("\n".join(lines)).strip()


def env_block(block: str, indent: int) -> dict[str, str]:
    """Read one env mapping at the given indent, not a general-purpose YAML parser."""
    match = re.search(rf"(?m)^{' ' * indent}env:\s*$", block)
    if match is None:
        raise AssertionError("Required env block is missing")
    values = {}
    for line in block[match.end():].splitlines()[1:]:
        entry = re.fullmatch(rf"{' ' * (indent + 2)}([A-Z_][A-Z0-9_]*): (.+)", line)
        if entry is None:
            break
        values[entry.group(1)] = entry.group(2)
    return values


PROPAGATE = "if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }"


def assert_native_commands_propagate(case: unittest.TestCase, script: str):
    """Every cmake/ctest line in a pwsh step must be followed by exit propagation."""
    lines = script.splitlines()
    commands = [index for index, line in enumerate(lines) if re.match(r"(cmake|ctest) ", line)]
    case.assertTrue(commands, script)
    for index in commands:
        case.assertLess(index + 1, len(lines), script)
        case.assertEqual(lines[index + 1], PROPAGATE, script)
    case.assertNotRegex(script, r"exit 0|\|\| true|continue-on-error")


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
        cls.trial = (WORKFLOWS / "package-trial.yml").read_text(encoding="utf-8")
        cls.setup = NATIVE_SETUP.read_text(encoding="utf-8")

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

    def test_only_release_draft_and_snapshot_jobs_can_write_repository_contents(self):
        # Every permissions key of every workflow, exactly: contents is the only
        # scope ever written, and pull-requests: read is the only extra scope.
        read, pull_requests = {"contents": "read"}, {"contents": "read", "pull-requests": "read"}
        expected = {
            "cross-platform.yml": {"workflow": read, "scope": pull_requests, "snapshot": {"contents": "write"}},
            "prepare-release.yml": {"workflow": read, "draft": {"contents": "write"}},
            "build-and-validate.yml": {"workflow": read},
            "package-trial.yml": {"workflow": read, "select": pull_requests},
        }
        self.assertEqual(sorted(path.name for path in WORKFLOWS.glob("*.yml")), sorted(expected))
        for name, blocks in expected.items():
            workflow = (WORKFLOWS / name).read_text(encoding="utf-8")
            with self.subTest(workflow=name):
                self.assertEqual(permission_blocks(workflow), blocks)
                self.assertNotRegex(workflow, r"write-all|read-all")
        for workflow in (self.quick, self.trial):
            self.assertNotRegex(workflow, r"pull-requests:\s*write|actions:\s*write")
        self.assertNotRegex(self.shared + self.trial, r"contents:\s*write")
        snapshot = job_block(self.quick, "snapshot")
        self.assertRegex(snapshot, r"(?m)^    permissions:\n      contents: write\n    strategy:$")
        self.assertNotRegex(self.quick.replace(snapshot, ""), r"contents:\s*write")
        self.assertNotRegex(snapshot, r"continue-on-error:\s*true|always\(\)|failure\(\)|cancelled\(\)")
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
        # A train builds the toolchain baseline plus only its own product.
        self.assertIn("product: ${{ needs.prepare.outputs.product }}", build)
        for output in ("tag", "commit", "prerelease", "train", "product"):
            self.assertIn(f"{output}: ${{{{ steps.source.outputs.{output} }}}}", prepare)
        self.assertIn("ref: ${{ needs.prepare.outputs.commit }}", step_using(draft, "actions/checkout"))
        self.assertIn("SOURCE_COMMIT: ${{ needs.prepare.outputs.commit }}", draft)
        native = job_block(self.shared, "native")
        self.assertIn("ref: ${{ inputs.source_commit }}", step_using(native, "actions/checkout"))
        self.assertIn("SOURCE_COMMIT: ${{ inputs.source_commit }}", native)
        upload = step_using(native, "actions/upload-artifact")
        self.assertIn("name: gyo-package-${{ matrix.product }}-${{ matrix.platform }}", upload)
        downloads = [step for step in steps_of(draft) if "uses: actions/download-artifact@" in step]
        self.assertEqual([re.search(r"(?m)^          pattern: (.+)$", step).group(1) for step in downloads],
                         ["gyo-package-${{ needs.prepare.outputs.product }}-*",
                          "gyo-service-${{ needs.prepare.outputs.product }}"])
        for download in downloads:
            self.assertNotRegex(download, r"(?m)^\s*(run-id|repository|github-token):")
            self.assertIn("merge-multiple: true", download)
        command = run_script(step_with(draft, "id: draft"))
        self.assertIn('--train "$RELEASE_TRAIN"', command)
        self.assertIn("--service-directory release-services", command)
        self.assertIn("RELEASE_TRAIN: ${{ needs.prepare.outputs.train }}", draft)

    def test_release_form_selects_one_train_and_serializes_each_train_version(self):
        dispatch = top_level_block(self.release, "on")
        for name in ("train", "version", "prerelease"):
            self.assertRegex(dispatch, rf"(?m)^      {name}:$")
        self.assertRegex(dispatch, r"(?m)^      train:\n        description: .+\n        required: true\n        type: string$")
        group = re.search(r"(?m)^  group:\s*(.+)$", top_level_block(self.release, "concurrency")).group(1)
        self.assertEqual(group, "gyo-prepare-release-${{ inputs.train }}-${{ inputs.version }}")
        self.assertRegex(top_level_block(self.release, "concurrency"), r"(?m)^  cancel-in-progress: false$")
        # The shared workflow keeps building every product unless a train narrows it.
        shared_inputs = top_level_block(self.shared, "on")
        self.assertRegex(shared_inputs, r"(?m)^      product:\n        description: .+\n        required: false\n        default: ''\n        type: string$")
        registry = step_with(job_block(self.shared, "prepare"), "id: registry")
        self.assertIn('--product "$TRAIN_PRODUCT"', registry)
        self.assertIn("TRAIN_PRODUCT: ${{ inputs.product }}", registry)

    def test_release_form_carries_the_real_device_evidence_reference_to_the_draft(self):
        # Optional in the form: the helper decides from the train's declared
        # items whether a reference is required, so tools never need one.
        dispatch = top_level_block(self.release, "on")
        self.assertRegex(dispatch, r"(?m)^      l4_evidence:\n        description: .+\n        required: false\n"
                                   r"        default: ''\n        type: string$")
        prepare = job_block(self.release, "prepare")
        draft = job_block(self.release, "draft")
        # prepare reads the input from the event file and fails before any build.
        self.assertIn('--event-path "$GITHUB_EVENT_PATH"', run_script(step_with(prepare, "id: source")))
        self.assertIn("l4_evidence: ${{ steps.source.outputs.l4_evidence }}", prepare)
        record = step_with(prepare, "name: Record the release candidate")
        self.assertEqual({key: value for key, value in env_block(record, 8).items() if key.startswith("L4_")},
                         {"L4_ITEMS": "${{ steps.source.outputs.l4_items }}",
                          "L4_EVIDENCE": "${{ steps.source.outputs.l4_evidence }}"})
        step = step_with(draft, "id: draft")
        self.assertEqual(env_block(step, 8)["L4_EVIDENCE"], "${{ needs.prepare.outputs.l4_evidence }}")
        # The = form keeps a value starting with '-' from being read as an option.
        self.assertIn('--l4-evidence="$L4_EVIDENCE"', run_script(step))
        self.assertIn("real-device (L4)", run_script(step_with(draft, "name: Show the draft")))
        # Free text reaches scripts only through the environment, never as an expression.
        self.assertNotIn("${{ inputs.l4_evidence }}", self.release)
        for job in (prepare, draft):
            for item in steps_of(job):
                if re.search(r"(?m)^        run: \|", item):
                    self.assertNotIn("l4_evidence }}", run_script(item))
        # Snapshots never carry real-device evidence.
        self.assertNotIn("l4", job_block(self.quick, "snapshot").lower())

    def test_workflow_commands_match_the_release_helper_cli(self):
        # Ask the real CLI parser, without entering any network/mutation path.
        for command, required_options in (
            ("prepare", ("--event-path", "--event-name", "--commit", "--ref", "--output")),
            ("draft", ("--train", "--tag", "--commit", "--prerelease", "--package-directory",
                       "--service-directory", "--l4-evidence", "--output")),
            ("snapshot", ("--train", "--commit", "--package-directory", "--service-directory", "--output")),
        ):
            with self.subTest(command=command):
                result = subprocess.run([sys.executable, str(ROOT / "build/ci/common/release_pipeline.py"),
                                         command, "--help"], text=True, capture_output=True,
                                        timeout=10, check=False)
                self.assertEqual(result.returncode, 0, result.stderr)
                for option in required_options:
                    self.assertIn(option, result.stdout)

    def test_shader_work_is_gated_by_current_build_graph(self):
        native = job_block(self.shared, "native")
        self.assertIn('gyo-build.json" -Raw | ConvertFrom-Json', native)
        steps = re.split(r"(?m)^      - ", native)
        for marker, field in (("xcrun -sdk macosx metal -v", "has_shader_bundles"),
                              ("uses: actions/cache@", "has_shader_tools"),
                              ('ctest --test-dir "$env:GYO_HOST_TOOLS"', "has_shader_tools")):
            step = next(step for step in steps if marker in step)
            self.assertIn(f"steps.configure.outputs.{field} == 'true'", step)
            self.assertNotIn("exit 0", step)

    def test_every_platform_packages_toolchain_without_games(self):
        native = job_block(self.shared, "native")
        self.assertIn("GYO_SELECTED_TOOLS: ${{ join(matrix.tools, ';') }}", native)
        self.assertIn('"-DGYO_TOOLS=$env:GYO_SELECTED_TOOLS"', native)
        # Tools are selected only through GYO_TOOLS: no per-tool GYO_BUILD_<TOOL>
        # switch (GYO_BUILD_CONFIG is the build configuration, not a tool) and no
        # tool-specific GUI switch may come back.
        self.assertNotRegex(native, r"GYO_BUILD_(?!CONFIG\b)[A-Z0-9_]+|GYO_[A-Z0-9_]+_BUILD_GUI")
        self.assertIn('"-DGYO_APPS=$apps"', native)
        self.assertIn("if: inputs.profile == 'release' || matrix.kind == 'toolchain'", native)
        for step in re.split(r"(?m)^      - ", native):
            if "cmake --install" in step or "python build/ci/common/archive_package.py" in step:
                self.assertNotIn("if: matrix.kind", step)

    def test_real_app_copy_is_a_fatal_windows_release_baseline_check(self):
        native = job_block(self.shared, "native")
        step = next(step for step in re.split(r"(?m)^      - ", native)
                    if "python tests/common/ci/app_copy_integration.py" in step)
        self.assertIn("if: runner.os == 'Windows' && matrix.kind == 'toolchain' && inputs.profile == 'release'", step)
        self.assertIn('if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }', step)
        self.assertIn('--logs "build/target/_build/$env:PRESET/logs/app-copy"', step)
        self.assertNotIn("continue-on-error", step)
        upload = step_using(native, "actions/upload-artifact")
        self.assertNotIn("product-copy", upload)

    def test_toolchain_baseline_runs_common_gpu_tests_without_game_checks(self):
        native = job_block(self.shared, "native")
        step = next(step for step in re.split(r"(?m)^      - ", native)
                    if "id: gpu_smoke" in step)
        self.assertIn("matrix.kind == 'toolchain' || steps.package.outputs.has_gpu == 'true'", step)
        self.assertIn('if [ "$GYO_PRODUCT" = toolchain ]; then', step)
        self.assertIn("-L gpu -R '^render\\.'", step)
        self.assertIn("--no-tests=error --output-on-failure", step)
        self.assertIn("set -euo pipefail", step)
        self.assertIn("common-gpu-tests.xml", step)
        self.assertNotIn("continue-on-error", step)

    def test_toolchain_and_declared_product_gpu_checks_are_independent(self):
        native = job_block(self.shared, "native")
        step = next(step for step in re.split(r"(?m)^      - ", native)
                    if "id: gpu_smoke" in step)
        self.assertIn("HAS_PRODUCT_GPU: ${{ steps.package.outputs.has_gpu }}", step)
        self.assertIn('if [ "$HAS_PRODUCT_GPU" = true ]; then', step)
        self.assertNotRegex(step, r"(?m)^\s*else\s*$")
        self.assertIn("--context", step)
        self.assertNotIn("--probe-directory", native)
        self.assertNotIn("GPU_SMOKE_SUITE", native)

    def test_actual_gpu_branches_run_both_and_propagate_either_failure(self):
        bash = find_bash()
        if bash is None:
            self.skipTest("Native Bash is unavailable")
        native = job_block(self.shared, "native")
        step = next(step for step in re.split(r"(?m)^      - ", native) if "id: gpu_smoke" in step)
        start = step.index('          if [ "$GYO_PRODUCT" = toolchain ]; then')
        script = textwrap.dedent(step[start:]).strip()
        # Run the workflow's actual branch selection with external GPU processes
        # substituted; the scripts and failure propagation are not duplicated.
        prefix = '''set -euo pipefail
xvfb-run() {
  case " $* " in
    *" ctest "*) printf 'baseline\\n' >> calls; return "$BASELINE_STATUS" ;;
    *" python "*) printf 'product\\n' >> calls; return "$PRODUCT_STATUS" ;;
    *) return 99 ;;
  esac
}
'''
        for baseline, product in ((0, 0), (1, 0), (0, 1)):
            with self.subTest(baseline=baseline, product=product), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                (root / "build/target/_build/test/logs").mkdir(parents=True)
                environment = dict(os.environ, GYO_PRODUCT="toolchain", HAS_PRODUCT_GPU="true",
                    PRESET="test", GITHUB_WORKSPACE=".", GYO_PACKAGE_PLATFORM="linux-x64", GYO_BUILD_CONFIG="Release",
                    SOURCE_COMMIT="0" * 40, ACCEPTANCE_PROFILE="release",
                    BASELINE_STATUS=str(baseline), PRODUCT_STATUS=str(product))
                result = subprocess.run([bash, "--noprofile", "--norc", "-c", prefix + script],
                    cwd=root, env=environment, capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode == 0, baseline == product == 0, result.stderr)
                calls = (root / "calls").read_text().splitlines()
                self.assertEqual(calls, ["baseline"] if baseline else ["baseline", "product"])

    def test_presets_separate_products_quality_and_engineering(self):
        document = json.loads((ROOT / "CMakePresets.json").read_text(encoding="utf-8"))
        presets = {preset["name"]: preset for preset in document["configurePresets"]}
        def cache(name):
            preset = presets[name]
            parents = preset.get("inherits", [])
            if isinstance(parents, str):
                parents = [parents]
            values = {}
            for parent in reversed(parents):
                values.update(cache(parent))
            values.update(preset.get("cacheVariables", {}))
            return values
        for name, expected in (("dev", ("OFF", "OFF")), ("test", ("ON", "OFF")),
                               ("core", ("ON", "OFF")), ("ci-windows", ("ON", "ON")),
                               ("ci-linux", ("ON", "ON")), ("ci-macos", ("ON", "ON")),
                               ("ci-macos-x64", ("ON", "ON"))):
            with self.subTest(preset=name):
                values = cache(name)
                self.assertEqual((values["BUILD_TESTING"], values["GYO_ENABLE_PACKAGING"]), expected)
        tests = {preset["name"]: preset for preset in document["testPresets"]}
        self.assertNotIn("dev", tests)
        self.assertEqual(tests["test"]["configurePreset"], "test")
        for platform in ("windows", "linux", "macos", "macos-x64"):
            self.assertEqual(tests["ci-" + platform]["inherits"], "test")
        # Both macOS rows share one deployment target and differ only in the
        # target architecture; the host architecture never comes from a preset.
        self.assertEqual({name: (cache(name)["CMAKE_OSX_ARCHITECTURES"], cache(name)["CMAKE_OSX_DEPLOYMENT_TARGET"])
                          for name in ("ci-macos", "ci-macos-x64")},
                         {"ci-macos": ("arm64", "13.3"), "ci-macos-x64": ("x86_64", "13.3")})
        build_presets = {preset["name"]: preset["configurePreset"] for preset in document["buildPresets"]}
        self.assertEqual(build_presets["ci-macos-x64"], "ci-macos-x64")
        self.assertEqual(tests["ci-macos-x64"]["configurePreset"], "ci-macos-x64")
        # MSVC objects must embed /Z7 debug information to be compiler-cacheable.
        self.assertEqual(cache("ci-windows")["CMAKE_MSVC_DEBUG_INFORMATION_FORMAT"],
                         "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
        for name in ("dev", "test", "core", "ci-linux", "ci-macos", "ci-macos-x64"):
            self.assertNotIn("CMAKE_MSVC_DEBUG_INFORMATION_FORMAT", cache(name))
        # MSVC precompiled headers (/Yc, /Fp) are never cacheable; dependencies
        # that enable them must compile without them in CI.
        self.assertEqual(cache("ci-windows")["CMAKE_DISABLE_PRECOMPILE_HEADERS"], "ON")

    def test_only_master_pushes_integrate_and_pull_requests_are_classified_in_a_job(self):
        trigger = [line.strip() for line in top_level_block(self.quick, "on").splitlines()
                   if line.strip() and not line.strip().startswith("#")]
        # No paths/draft/branch filters: a filtered pull request event would
        # leave the required gate check unreported.
        self.assertEqual(trigger, ["push:", "branches: [master]", "pull_request:",
                                   "types: [opened, synchronize, reopened, ready_for_review]",
                                   "workflow_dispatch:"])
        self.assertNotRegex(self.quick, r"paths(-ignore)?:|branches-ignore:|tags(-ignore)?:")
        self.assertNotIn("pull_request.draft", self.quick)

    def test_pull_request_runs_supersede_each_other_but_integration_runs_never_do(self):
        concurrency = top_level_block(self.quick, "concurrency")
        group = re.search(r"(?m)^  group:\s*(.+)$", concurrency).group(1)
        self.assertIn("github.event_name == 'pull_request' && format('pr-{0}', github.event.pull_request.number)", group)
        self.assertIn("|| github.run_id }}", group)
        self.assertNotIn("github.ref", group)
        self.assertRegex(concurrency, r"(?m)^  cancel-in-progress: \$\{\{ github\.event_name == 'pull_request' \}\}$")

    def test_scope_job_alone_selects_tiers_through_the_tested_helper(self):
        scope = job_block(self.quick, "scope")
        self.assertIn("ref: ${{ github.sha }}", step_using(scope, "actions/checkout"))
        self.assertIn("fetch-depth: 2", step_using(scope, "actions/checkout"))
        classifier = step_with(scope, "ci_scope.py")
        self.assertRegex(classifier, r"(?m)^        id: scope$")
        command = run_script(classifier)
        invocations = [line for line in command.splitlines() if "ci_scope.py" in line]
        self.assertEqual(len(invocations), 1, command)
        self.assertTrue(invocations[0].startswith('python build/ci/common/ci_scope.py "${arguments[@]}" '), command)
        help_text = subprocess.run([sys.executable, str(ROOT / "build/ci/common/ci_scope.py"), "--help"],
                                   text=True, capture_output=True, timeout=30, check=False)
        self.assertEqual(help_text.returncode, 0, help_text.stderr)
        python_options = re.findall(r"(--[a-z][a-z-]*)", invocations[0] + re.search(
            r"(?m)^arguments=\((.+)\)$", command).group(1) + re.search(r"arguments\+=\((.+)\)", command).group(1))
        self.assertIn("--live-draft", python_options)
        for option in python_options:
            self.assertIn(option, help_text.stdout)
        self.assertEqual(env_block(classifier, 8), {
            "EVENT_NAME": "${{ github.event_name }}",
            "PR_NUMBER": "${{ github.event.pull_request.number }}",
            "GH_TOKEN": "${{ github.token }}",
        })
        # Reading the live draft state is the only extra permission.
        self.assertRegex(scope, r"(?m)^    permissions:\n      contents: read\n      pull-requests: read\n    outputs:$")
        # The classifier's own policy tests run first, exactly as in Quick.
        steps = steps_of(scope)
        tests = step_with(scope, "unittest discover")
        self.assertLess(steps.index(tests), steps.index(classifier))
        self.assertIn("run: python -m unittest discover -s tests/common/ci -v", tests)
        self.assertIn("run: python -m unittest discover -s tests/common/ci -v", job_block(self.shared, "prepare"))
        for output in ("run_l1", "run_quick", "reason", "platforms"):
            self.assertIn(f"{output}: ${{{{ steps.scope.outputs.{output} }}}}", scope)
        l1 = job_block(self.quick, "l1")
        self.assertRegex(l1, r"(?m)^    needs: scope$")
        self.assertRegex(l1, r"(?m)^    if: needs\.scope\.outputs\.run_l1 == 'true'$")
        quick = job_block(self.quick, "quick")
        self.assertRegex(quick, r"(?m)^    needs: scope$")
        self.assertRegex(quick, r"(?m)^    if: needs\.scope\.outputs\.run_quick == 'true'$")
        self.assertIn("uses: ./.github/workflows/build-and-validate.yml", quick)
        self.assertIn("source_commit: ${{ github.sha }}", quick)
        self.assertIn("profile: quick", quick)

    def test_actual_classifier_reads_the_live_draft_state_only_for_pull_requests(self):
        bash = find_bash()
        if bash is None:
            if os.name == "nt":
                self.skipTest("Native Git Bash cannot execute in this Windows environment")
            self.fail("Bash is required by the Ubuntu CI scope job")
        script = run_script(step_with(job_block(self.quick, "scope"), "ci_scope.py"))
        prefix = '''gh() { printf '%s\\n' "$*" >> gh-calls; [ "$GH_STATUS" = 0 ] || return "$GH_STATUS"; printf '%s\\n' "$LIVE_DRAFT"; }
python() { printf '%s\\n' "$@" > python-arguments; }
'''
        lookup = ["api repos/owner/repository/pulls/7 --jq .draft"]
        for event_name, gh_status, live_draft, expected_gh, expected_draft in (
            ("pull_request", 0, "true", lookup, "true"),
            ("pull_request", 0, "false", lookup, "false"),
            ("push", 0, "true", [], None),
            ("workflow_dispatch", 0, "true", [], None),
            ("pull_request", 1, "false", lookup, None),
        ):
            with self.subTest(event=event_name, gh_status=gh_status, live_draft=live_draft), \
                    tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                environment = dict(os.environ, EVENT_NAME=event_name, PR_NUMBER="7" if event_name == "pull_request" else "",
                                   GITHUB_REPOSITORY="owner/repository", GITHUB_EVENT_PATH="event.json",
                                   GITHUB_OUTPUT="outputs", GITHUB_STEP_SUMMARY="summary",
                                   GH_STATUS=str(gh_status), LIVE_DRAFT=live_draft)
                result = subprocess.run([bash, "--noprofile", "--norc", "-e", "-o", "pipefail", "-c", prefix + script],
                                        cwd=root, env=environment, capture_output=True, text=True, timeout=10)
                calls = (root / "gh-calls").read_text().splitlines() if (root / "gh-calls").exists() else []
                self.assertEqual(calls, expected_gh)
                if gh_status:
                    # A failed lookup must fail the scope job, never classify.
                    self.assertNotEqual(result.returncode, 0)
                    self.assertFalse((root / "python-arguments").exists())
                    continue
                self.assertEqual(result.returncode, 0, result.stderr)
                arguments = (root / "python-arguments").read_text().splitlines()
                self.assertEqual(arguments[:5], ["build/ci/common/ci_scope.py", "--event-name", event_name,
                                                 "--event-path", "event.json"])
                if expected_draft is None:
                    self.assertNotIn("--live-draft", arguments)
                else:
                    self.assertEqual(arguments[arguments.index("--live-draft") + 1], expected_draft)

    def test_actual_ci_gate_accepts_only_selected_successes_and_unselected_skips(self):
        bash = find_bash()
        if bash is None:
            if os.name == "nt":
                self.skipTest("Native Git Bash cannot execute in this Windows environment")
            self.fail("Bash is required by the Ubuntu CI gate")
        gate = job_block(self.quick, "gate")
        self.assertRegex(gate, r"(?m)^    name: CI gate$")
        self.assertRegex(gate, r"(?m)^    needs: \[scope, l1, quick\]$")
        # The policy below is only as good as its wiring to the real jobs.
        self.assertEqual(env_block(gate, 8), {
            "SCOPE_RESULT": "${{ needs.scope.result }}",
            "SCOPE_REASON": "${{ needs.scope.outputs.reason }}",
            "RUN_L1": "${{ needs.scope.outputs.run_l1 }}",
            "RUN_QUICK": "${{ needs.scope.outputs.run_quick }}",
            "L1_RESULT": "${{ needs.l1.result }}",
            "QUICK_RESULT": "${{ needs.quick.result }}",
        })
        # A skipped job counts as passing a required check, so the gate must
        # never be skippable by a failed or cancelled dependency.
        self.assertRegex(gate, r"(?m)^    if: always\(\)$")
        self.assertNotRegex(gate, r"continue-on-error:\s*true")
        runs = list(re.finditer(r"(?m)^        run: \|\s*$", gate))
        self.assertEqual(len(runs), 1, "The gate job must have one executable policy block")
        script = textwrap.dedent(gate[runs[0].end():]).strip()
        outcomes = ("success", "failure", "cancelled", "skipped")
        selections = ("true", "false", "")
        accepted = {("true", "success"), ("false", "skipped")}
        cases = [(scope, ("true", "success"), ("true", "success")) for scope in outcomes]
        cases.append(("failure", ("false", "skipped"), ("false", "skipped")))
        tiers = list(itertools.product(selections, outcomes))
        cases += [("success", tier, ("false", "skipped")) for tier in tiers]
        cases += [("success", ("true", "success"), tier) for tier in tiers]
        for scope, (run_l1, l1), (run_quick, quick) in cases:
            with self.subTest(scope=scope, l1=(run_l1, l1), quick=(run_quick, quick)):
                environment = dict(os.environ, SCOPE_RESULT=scope, SCOPE_REASON="fixture",
                                   RUN_L1=run_l1, L1_RESULT=l1, RUN_QUICK=run_quick, QUICK_RESULT=quick,
                                   GITHUB_STEP_SUMMARY="/dev/null")
                result = subprocess.run([bash, "--noprofile", "--norc", "-e", "-o", "pipefail", "-c", script],
                                        env=environment, text=True, capture_output=True, timeout=10, check=False)
                expected = scope == "success" and (run_l1, l1) in accepted and (run_quick, quick) in accepted
                self.assertEqual(result.returncode == 0, expected, result.stdout + result.stderr)

    def test_l1_builds_every_registered_product_once_per_platform_without_packaging(self):
        l1 = job_block(self.quick, "l1")
        self.assertRegex(l1, r"(?m)^    name: L1 / \$\{\{ matrix\.platform \}\}$")
        self.assertIn("matrix: ${{ fromJSON(needs.scope.outputs.platforms) }}", l1)
        self.assertRegex(l1, r"fail-fast:\s*false")
        self.assertNotRegex(l1, r"continue-on-error:\s*true")
        self.assertIn("ref: ${{ github.sha }}", step_using(l1, "actions/checkout"))
        configure = run_script(step_with(l1, "id: configure"))
        # The ci-<os> preset owns registry AUTO selection; no product or tool
        # list may be injected by the merge gate. Only the host-keyed shader
        # tool directory, shared with packaging, is placed by the workflow.
        configure = configure.replace(HOST_TOOLS_DEFINE + " ", "", 1)
        self.assertTrue(configure.startswith("cmake --preset $env:PRESET 2>&1 | Tee-Object"), configure)
        self.assertNotRegex(configure, r"-D")
        for forbidden in ("cmake --install", "run_package_checks", "validate_package", "archive_package",
                          "app_copy_integration", "gyo-package-", "matrix.product", "matrix.kind"):
            self.assertNotIn(forbidden, l1)
        tests = step_with(l1, "id: tests")
        self.assertNotRegex(tests, r"(?m)^        if:")
        ctest = run_script(tests).splitlines()[0]
        # The ci-<os> test preset owns label selection; L1 adds no filter.
        self.assertTrue(ctest.startswith("ctest --preset $env:PRESET --output-junit "), ctest)
        self.assertEqual(re.findall(r"(?<!\S)(-{1,2}[A-Za-z][A-Za-z-]*)", ctest.split(" 2>&1 ")[0]),
                         ["--preset", "--output-junit"])
        for marker in ("id: configure", "id: build", "id: tests", "id: shaders", "id: core"):
            with self.subTest(step=marker):
                assert_native_commands_propagate(self, run_script(step_with(l1, marker)))
        gpu = run_script(step_with(l1, "id: gpu"))
        self.assertTrue(gpu.startswith("set -euo pipefail\n"), gpu)
        self.assertNotRegex(gpu, r"exit 0|\|\| true")
        for marker in ("id: shaders", "id: gpu", "id: core"):
            with self.subTest(step=marker):
                self.assertIn("runner.os == 'Linux'", step_with(l1, marker))
        self.assertIn("steps.configure.outputs.has_shader_tools == 'true'", step_with(l1, "id: shaders"))
        self.assertIn("steps.configure.outputs.has_shader_tools == 'true'", step_using(l1, "actions/cache"))

    def test_l1_and_toolchain_core_baselines_share_one_cacheable_command(self):
        native = run_script(step_with(job_block(self.shared, "native"), "id: core"))
        l1 = run_script(step_with(job_block(self.quick, "l1"), "id: core"))
        self.assertEqual(native, l1)
        configure = native.splitlines()[0]
        self.assertTrue(configure.startswith("cmake --preset core "), configure)
        # The core preset keeps MSVC /Zi, whose shared per-target PDB is unsafe
        # behind sccache; only embedded debug information may be cached.
        self.assertIn("'-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>'", configure)
        self.assertIn(" -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON ", configure)

    def test_cxx_compiles_are_not_module_scanned(self):
        # CMP0155 scanning adds GCC -fmodules-ts flags that sccache refuses to
        # cache; both CMake projects that set C++20 must disable it.
        for path in ("build/cmake/GyoBuild.cmake", "engine/render/shaders/pipeline/CMakeLists.txt"):
            with self.subTest(project=path):
                text = (ROOT / path).read_text(encoding="utf-8")
                self.assertRegex(text, r"(?m)^set\(CMAKE_CXX_SCAN_FOR_MODULES OFF\)$")
                self.assertLess(text.index("set(CMAKE_CXX_STANDARD 20)"), text.index("set(CMAKE_CXX_SCAN_FOR_MODULES OFF)"))
        build = (ROOT / "build/cmake/GyoBuild.cmake").read_text(encoding="utf-8")
        self.assertLess(build.index("set(CMAKE_CXX_SCAN_FOR_MODULES OFF)"), build.index("add_subdirectory("))

    def test_l1_and_toolchain_gpu_baselines_share_one_command(self):
        native = job_block(self.shared, "native")
        l1 = job_block(self.quick, "l1")
        native_gpu = run_script(step_with(native, "id: gpu_smoke"))
        l1_gpu = run_script(step_with(l1, "id: gpu"))
        setup_end = "vulkaninfo --summary"
        native_setup = native_gpu[:native_gpu.index("\n", native_gpu.index(setup_end))]
        self.assertTrue(l1_gpu.startswith(native_setup + "\n"), "L1 must reuse the Lavapipe/Xvfb setup verbatim")
        native_ctest = native_gpu[native_gpu.index("xvfb-run -a -s '-screen 0 1920x1080x24' ctest"):]
        native_ctest = native_ctest[:native_ctest.index("\nfi")]
        self.assertEqual(textwrap.dedent(native_ctest.replace("\n  ", "\n")).strip(),
                         l1_gpu[len(native_setup):].strip())
        self.assertIn("-L gpu -R '^render\\.'", l1_gpu)

    def test_shared_toolchain_setup_and_compiler_cache_ownership(self):
        native = job_block(self.shared, "native")
        l1 = job_block(self.quick, "l1")
        for name, job in (("native", native), ("l1", l1)):
            with self.subTest(job=name):
                setup = step_with(job, "uses: ./.github/actions/native-setup")
                self.assertIn("id: setup", setup)
                for field in ("platform", "toolchain", "preset", "host"):
                    self.assertIn(f"{field}: ${{{{ matrix.{field} }}}}", setup)
                self.assertIn("cpu-execution: ${{ matrix.cpu_execution }}", setup)
                stats = step_with(job, "sccache --stop-server")
                self.assertIn("if: always() && steps.setup.outcome == 'success'", stats)
        # One owner for hosted toolchain facts: none remain inline in jobs.
        for inline in ("ilammy/msvc-dev-cmd", "apt-get install", "xcode-select", "CMAKE_CXX_COMPILER_LAUNCHER"):
            self.assertNotIn(inline, self.quick + self.shared)
        self.assertIn("steps.setup.outcome == 'success'", step_with(native, "id: core"))
        # The host-targeted core preset runs once per host, never on a cross-built row.
        self.assertIn("matrix.host == matrix.platform", step_with(native, "id: core"))
        self.assertNotIn("steps.tools.", native)
        # Only default-branch L1 integration runs may write the compiler cache.
        self.assertNotIn("actions/cache/save@", self.shared + self.release + self.setup)
        save = step_using(l1, "actions/cache/save")
        self.assertEqual(self.quick.count("actions/cache/save@"), 1)
        self.assertIn("github.event_name != 'pull_request'", save)
        self.assertIn("github.ref == format('refs/heads/{0}', github.event.repository.default_branch)", save)
        self.assertIn("path: ${{ steps.setup.outputs.cache-path }}", save)
        self.assertIn("key: ${{ steps.setup.outputs.cache-key }}", save)
        self.assertLess(l1.index("sccache --stop-server"), l1.index("actions/cache/save@"))
        shader_keys = {re.search(r"(?m)^          key: (shader-tools-.+)$", step_using(job, "actions/cache")).group(1)
                       for job in (native, l1)}
        self.assertEqual(len(shader_keys), 1, "L1 and packaging must share one host-tools cache identity")
        shader_key = shader_keys.pop()
        self.assertIn("-sccache-", shader_key)
        # Host tools run on the runner: rows sharing a host share the cache,
        # and the target platform must not split (or leak into) it.
        self.assertIn("-${{ matrix.host }}-", shader_key)
        self.assertNotIn("matrix.platform", shader_key)
        for name, job in (("native", native), ("l1", l1)):
            with self.subTest(job=name):
                self.assertIn("GYO_HOST_TOOLS: build/target/_build/host-tools-${{ matrix.host }}", job)
                self.assertIn("path: ${{ env.GYO_HOST_TOOLS }}", step_using(job, "actions/cache"))
                self.assertIn(HOST_TOOLS_DEFINE, run_script(step_with(job, "id: configure")))
                self.assertNotIn("/host-tools", job.replace("_build/host-tools-${{ matrix.host }}", ""))

    def test_cross_built_rows_verify_their_host_and_translated_execution(self):
        xcode = step_with(self.setup, "name: Select the macOS compiler toolchain", 4)
        self.assertIn("HOST_PLATFORM: ${{ inputs.host }}", xcode)
        self.assertIn('if [ "$(uname -m)" != "$host_architecture" ]; then', xcode)
        self.assertNotIn("ci-macos requires", xcode)
        rosetta = step_with(self.setup, "name: Provide Rosetta 2 for translated CPU execution", 4)
        self.assertIn("if: inputs.cpu-execution == 'rosetta2'", rosetta)
        self.assertIn("arch -x86_64 /usr/bin/true", rosetta)
        self.assertIn("sudo softwareupdate --install-rosetta --agree-to-license", rosetta)
        self.assertNotRegex(rosetta, r"exit 0|\|\| true|continue-on-error")
        # A failed install falls through (bash -e) to the explicit translation check and its error.
        install = rosetta.index("sudo softwareupdate --install-rosetta --agree-to-license || echo")
        self.assertLess(install, rosetta.index("::error::Rosetta 2 cannot run x86_64 code"))
        steps = steps_of(self.setup, 4)
        # Translation is proven before any compile, so a missing Rosetta fails the row early.
        self.assertLess(steps.index(rosetta), steps.index(step_with(self.setup, "id: toolchain", 4)))
        native = job_block(self.shared, "native")
        self.assertIn("GYO_CPU_EXECUTION: ${{ matrix.cpu_execution }}", native)
        self.assertIn("--cpu-execution $env:GYO_CPU_EXECUTION", run_script(step_with(native, "id: archive")))
        for job in (native, job_block(self.quick, "l1")):
            self.assertIn("Target CPU execution", step_with(job, "GITHUB_STEP_SUMMARY"))

    def test_native_setup_routes_every_compile_through_a_restored_cache(self):
        steps = steps_of(self.setup, 4)
        toolchain = step_with(self.setup, "id: toolchain", 4)
        for language in ("C", "CXX", "OBJC", "OBJCXX"):
            self.assertIn(f'"CMAKE_{language}_COMPILER_LAUNCHER=sccache" >> $env:GITHUB_ENV', toolchain)
        self.assertIn('"SCCACHE_DIR=$cachePath" >> $env:GITHUB_ENV', toolchain)
        self.assertIn("CACHE_FAMILY: sccache-v1-${{ inputs.platform }}-${{ inputs.toolchain }}", toolchain)
        self.assertIn('$prefix = "$env:CACHE_FAMILY-$compiler-$env:PRESET-"', toolchain)
        self.assertIn("PRESET: ${{ inputs.preset }}", toolchain)
        restore = step_with(self.setup, "uses: actions/cache/restore@", 4)
        self.assertIn("path: ${{ steps.toolchain.outputs.cache-path }}", restore)
        self.assertIn("restore-keys: ${{ steps.toolchain.outputs.restore-prefix }}", restore)
        self.assertGreater(steps.index(restore), steps.index(toolchain))
        self.assertNotRegex(self.setup, r"SCCACHE_GHA_ENABLED|actions/cache@")
        outputs = top_level_block(self.setup, "outputs")
        for name in ("compiler", "cache-path", "cache-key"):
            with self.subTest(output=name):
                self.assertRegex(outputs, rf"(?m)^  {name}:\n    description: .+\n"
                                          rf"    value: \$\{{\{{ steps\.toolchain\.outputs\.{name} \}}\}}$")
        self.assertEqual(len(re.findall(r"(?m)^    value: ", outputs)), 3)

    def test_native_setup_installs_a_checksum_pinned_compiler_cache_without_runtime_tokens(self):
        steps = steps_of(self.setup, 4)
        install = step_with(self.setup, "name: Install the pinned compiler cache", 4)
        self.assertLess(steps.index(install), steps.index(step_with(self.setup, "id: toolchain", 4)))
        # A setup action exported ACTIONS_RUNTIME_TOKEN to every later build step.
        self.assertNotIn("uses:", install)
        self.assertNotRegex(self.setup, r"mozilla-actions/|ACTIONS_RUNTIME_TOKEN|ACTIONS_RESULTS_URL")
        self.assertIn("shell: pwsh", install)
        self.assertRegex(install, r"SCCACHE_VERSION: v\d+\.\d+\.\d+\n")
        pinned = dict(re.findall(r"'((?:Linux|Windows|macOS)-(?:X64|ARM64))' = '[a-z0-9_-]+', '([0-9a-f]{64})'", install))
        self.assertEqual(set(pinned), {"Linux-X64", "Windows-X64", "macOS-ARM64"})
        self.assertIn("(Get-FileHash ", install)
        self.assertIn("if ($actual -ne $sha256) { throw ", install)
        self.assertIn('tar -xzf "$name.tar.gz"', install)
        self.assertIn("if ($status -ne 0) { exit $status }", install)
        self.assertIn(">> $env:GITHUB_PATH", install)
        self.assertNotIn(">> $env:GITHUB_ENV", install)

    def test_go_work_runs_on_linux_only_from_the_configured_build_record(self):
        native = job_block(self.shared, "native")
        l1 = job_block(self.quick, "l1")
        record = '--build-info "build/target/_build/$env:PRESET/gyo-build.json"'
        go_help = {command: subprocess.run([sys.executable, str(ROOT / "build/ci/common/go_checks.py"), command, "--help"],
                                           text=True, capture_output=True, timeout=30, check=False)
                   for command in ("plan", "check", "build")}
        for name, job, last_native_step in (("l1", l1, "id: core"), ("native", native, "name: Upload this product and platform package")):
            with self.subTest(job=name):
                steps = steps_of(job)
                plan = step_with(job, "id: go_plan")
                setup = step_using(job, "actions/setup-go")
                check = step_with(job, "id: go\n")
                services = step_with(job, "id: go_services")
                self.assertEqual(job.count("uses: actions/setup-go@"), 1)
                # Go follows every native step so a Go failure cannot skip
                # C++ tests, product acceptance or the compiler cache seed.
                self.assertLess(steps.index(step_with(job, last_native_step)), steps.index(plan))
                self.assertLess(steps.index(plan), steps.index(setup))
                self.assertLess(steps.index(setup), steps.index(check))
                self.assertLess(steps.index(check), steps.index(services))
                self.assertIn("if: ${{ !cancelled() && runner.os == 'Linux' && steps.configure.outcome == 'success' }}", plan)
                self.assertIn("steps.go_plan.outputs.has_go_modules == 'true'", setup)
                self.assertIn("go-version-file: ${{ steps.go_plan.outputs.go_version_file }}", setup)
                self.assertIn("cache-dependency-path: ${{ steps.go_plan.outputs.go_dependency_files }}", setup)
                self.assertRegex(setup, r"(?m)^          cache: true$")
                self.assertIn("steps.go_setup.outcome == 'success'", check)
                self.assertIn("steps.go_setup.outcome == 'success'", services)
                self.assertIn("steps.go_plan.outputs.has_go_services == 'true'", services)
                self.assertNotIn("continue-on-error", plan + setup + check + services)
                for command, step in (("plan", plan), ("check", check), ("build", services)):
                    script = run_script(step).splitlines()
                    self.assertEqual(len(script), 2, script)
                    self.assertTrue(script[0].startswith(f"python build/ci/common/go_checks.py {command} {record} "), script[0])
                    self.assertEqual(script[1], PROPAGATE)
                    self.assertEqual(go_help[command].returncode, 0, go_help[command].stderr)
                    for option in re.findall(r"(?<!\S)(--[a-z][a-z-]*)", script[0]):
                        self.assertIn(option, go_help[command].stdout)
                    # A packaging row selects its own records; L1 checks them all.
                    self.assertEqual("--product $env:GYO_PRODUCT" in script[0], name == "native", script[0])
                # The race detector runs on Linux, the only row that has Go.
                self.assertIn(" --race ", run_script(check))
        # A product row checks its module where its C++ tests run (release);
        # L1 already checks every module for the same commit.
        self.assertIn("(inputs.profile == 'release' || matrix.kind == 'toolchain')", step_with(native, "id: go\n"))
        self.assertNotRegex(step_with(l1, "id: go\n"), r"inputs\.profile|matrix\.kind")
        # Hosted toolchain facts stay in native-setup; Go is provisioned only where recorded.
        self.assertNotIn("setup-go", self.setup + self.release)

    def test_service_records_and_archives_enter_only_their_product_train(self):
        native = job_block(self.shared, "native")
        services = step_with(native, "id: go_services")
        self.assertIn("--revision $env:SOURCE_COMMIT", run_script(services))
        # Every Linux packaging row records its services, even none, bound to the commit.
        plan = run_script(step_with(native, "id: go_plan")).splitlines()[0]
        self.assertIn(f'--record "build/target/_build/$env:PRESET/go-services/{SERVICE_RECORD_NAME}"', plan)
        self.assertIn("--revision $env:SOURCE_COMMIT", plan)
        uploads = [step for step in steps_of(native) if "uses: actions/upload-artifact@" in step]
        upload = next(step for step in uploads if "go-services" in step)
        self.assertIn("if: ${{ !cancelled() && steps.go_plan.outcome == 'success' && "
                      "(steps.go_plan.outputs.has_go_services != 'true' || steps.go_services.outcome == 'success') }}",
                      upload)
        name = re.search(r"(?m)^          name: (.+)$", upload).group(1)
        self.assertEqual(name, "gyo-service-${{ matrix.product }}")
        self.assertIn("if-no-files-found: error", upload)
        self.assertIn(f"go-services/{SERVICE_RECORD_NAME}\n", upload)
        self.assertIn("go-services/gyo-*.tar.gz.sha256", upload)
        package_name = re.search(r"(?m)^          name: (.+)$", step_using(native, "actions/upload-artifact")).group(1)
        for job, product in ((job_block(self.release, "draft"), "${{ needs.prepare.outputs.product }}"),
                             (job_block(self.quick, "snapshot"), "${{ matrix.product }}")):
            patterns = [re.search(r"(?m)^          pattern: (.+)$", step).group(1)
                        for step in steps_of(job) if "uses: actions/download-artifact@" in step]
            self.assertEqual(len(patterns), 2)
            with self.subTest(job=product):
                package_pattern, service_pattern = (pattern.replace(product, "sample") for pattern in patterns)
                artifacts = {artifact.replace("${{ matrix.product }}", owner).replace("${{ matrix.platform }}", platform)
                             for owner in ("sample", "sample_two", "toolchain")
                             for platform in ("linux-x64", "macos-x64")
                             for artifact in (name, package_name)}
                # A train downloads exactly its product's artifacts, never a
                # prefix-sharing product's or the other artifact family.
                self.assertEqual({artifact for artifact in artifacts if fnmatch.fnmatchcase(artifact, package_pattern)},
                                 {"gyo-package-sample-linux-x64", "gyo-package-sample-macos-x64"})
                self.assertEqual({artifact for artifact in artifacts if fnmatch.fnmatchcase(artifact, service_pattern)},
                                 {"gyo-service-sample"})
        # L1 cross-builds services for the merge gate but never publishes them.
        self.assertNotIn("gyo-service-", job_block(self.quick, "l1"))
        self.assertNotIn("--revision", run_script(step_with(job_block(self.quick, "l1"), "id: go_services")))
        self.assertNotIn("--record", job_block(self.quick, "l1"))

    def test_snapshot_channel_publishes_only_after_a_passed_default_branch_push(self):
        plan = job_block(self.quick, "snapshot_plan")
        snapshot = job_block(self.quick, "snapshot")
        # The gate covers L1 and Quick; GitHub's implicit success() applies
        # because no status function overrides it.
        self.assertRegex(plan, r"(?m)^    needs: gate$")
        self.assertRegex(snapshot, r"(?m)^    needs: \[quick, snapshot_plan\]$")
        for job in (plan, snapshot):
            self.assertEqual(re.search(r"(?m)^    if: (.+)$", job).group(1), SNAPSHOT_CONDITION)
            self.assertIn("ref: ${{ github.sha }}", step_using(job, "actions/checkout"))
            self.assertIn("persist-credentials: false", step_using(job, "actions/checkout"))
        self.assertNotRegex(self.quick, r"(?m)^  push:\n    branches: \[(?!master\])")
        self.assertIn('        run: python build/ci/common/release_channels.py trains --output "$GITHUB_OUTPUT"\n',
                      step_with(plan, "id: trains"))
        self.assertIn("trains: ${{ steps.trains.outputs.trains }}", plan)
        self.assertIn("matrix: ${{ fromJSON(needs.snapshot_plan.outputs.trains) }}", snapshot)
        self.assertRegex(snapshot, r"fail-fast:\s*false")
        # One group per train: publication and retention never race.
        self.assertRegex(snapshot, r"(?m)^    concurrency:\n      group: gyo-snapshot-\$\{\{ matrix\.train \}\}\n"
                                   r"      cancel-in-progress: false$")
        command = run_script(step_with(snapshot, "id: snapshot"))
        self.assertTrue(command.startswith('python build/ci/common/release_pipeline.py snapshot --train "$SNAPSHOT_TRAIN" '
                                           '--commit "$SOURCE_COMMIT"'), command)
        self.assertEqual(env_block(step_with(snapshot, "id: snapshot"), 8), {
            "GH_TOKEN": "${{ github.token }}", "GH_REPO": "${{ github.repository }}",
            "SNAPSHOT_TRAIN": "${{ matrix.train }}", "SOURCE_COMMIT": "${{ github.sha }}"})
        help_text = subprocess.run([sys.executable, str(ROOT / "build/ci/common/release_channels.py"), "trains", "--help"],
                                   text=True, capture_output=True, timeout=30, check=False)
        self.assertEqual(help_text.returncode, 0, help_text.stderr)
        self.assertIn("--output", help_text.stdout)
        # The gate stays the only required check and never waits for publication.
        self.assertRegex(job_block(self.quick, "gate"), r"(?m)^    needs: \[scope, l1, quick\]$")

    def test_trial_channel_packages_labelled_pull_requests_and_manual_runs_only(self):
        trigger = [line.strip() for line in top_level_block(self.trial, "on").splitlines()
                   if line.strip() and not line.strip().startswith("#")]
        self.assertEqual(trigger, ["pull_request:", "types: [opened, synchronize, reopened, labeled]",
                                   "workflow_dispatch:"])
        self.assertNotRegex(self.trial, r"paths(-ignore)?:|branches(-ignore)?:|tags(-ignore)?:|push:")
        group = re.search(r"(?m)^  group:\s*(.+)$", top_level_block(self.trial, "concurrency")).group(1)
        # Only an event that can select packaging may supersede a running trial.
        self.assertIn(f"(github.event.action != 'labeled' || github.event.label.name == '{TRIAL_LABEL}')", group)
        self.assertIn("|| github.run_id }}", group)
        select = job_block(self.trial, "select")
        self.assertRegex(select, r"(?m)^    permissions:\n      contents: read\n      pull-requests: read\n    outputs:$")
        package = job_block(self.trial, "package")
        self.assertRegex(package, r"(?m)^    needs: select$")
        self.assertRegex(package, r"(?m)^    if: needs\.select\.outputs\.run_package == 'true'$")
        self.assertIn("uses: ./.github/workflows/build-and-validate.yml", package)
        self.assertIn("source_commit: ${{ github.sha }}", package)
        self.assertIn("profile: quick", package)
        self.assertNotIn("product:", package)
        self.assertNotRegex(self.trial, r"release_pipeline|gh release|/releases")

    def test_actual_trial_selector_reads_the_live_pull_request_only_for_pull_requests(self):
        bash = find_bash()
        if bash is None:
            if os.name == "nt":
                self.skipTest("Native Git Bash cannot execute in this Windows environment")
            self.fail("Bash is required by the Ubuntu trial selector")
        script = run_script(step_with(job_block(self.trial, "select"), "release_channels.py trial"))
        prefix = '''gh() { printf '%s\\n' "$*" >> gh-calls; [ "$GH_STATUS" = 0 ] || return "$GH_STATUS"; printf '%s\\n' "$LIVE_PULL_REQUEST"; }
python() { printf '%s\\n' "$@" > python-arguments; }
'''
        lookup = ["api repos/owner/repository/pulls/7 --jq {state: .state, labels: [.labels[].name]}"]
        live = json.dumps({"state": "open", "labels": [TRIAL_LABEL]})
        for event_name, gh_status, expected_gh in (("pull_request", 0, lookup), ("workflow_dispatch", 0, []),
                                                   ("pull_request", 1, lookup)):
            with self.subTest(event=event_name, gh_status=gh_status), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                environment = dict(os.environ, EVENT_NAME=event_name, PR_NUMBER="7" if event_name == "pull_request" else "",
                                   GITHUB_REPOSITORY="owner/repository", GITHUB_EVENT_PATH="event.json",
                                   GITHUB_OUTPUT="outputs", GITHUB_STEP_SUMMARY="summary",
                                   GH_STATUS=str(gh_status), LIVE_PULL_REQUEST=live)
                result = subprocess.run([bash, "--noprofile", "--norc", "-e", "-o", "pipefail", "-c", prefix + script],
                                        cwd=root, env=environment, capture_output=True, text=True, timeout=10)
                calls = (root / "gh-calls").read_text().splitlines() if (root / "gh-calls").exists() else []
                self.assertEqual(calls, expected_gh)
                if gh_status:
                    self.assertNotEqual(result.returncode, 0)
                    self.assertFalse((root / "python-arguments").exists())
                    continue
                self.assertEqual(result.returncode, 0, result.stderr)
                arguments = (root / "python-arguments").read_text().splitlines()
                self.assertEqual(arguments[:6], ["build/ci/common/release_channels.py", "trial", "--event-name",
                                                 event_name, "--event-path", "event.json"])
                if event_name == "pull_request":
                    self.assertEqual(arguments[arguments.index("--live-pull-request") + 1], live)
                else:
                    self.assertNotIn("--live-pull-request", arguments)
        help_text = subprocess.run([sys.executable, str(ROOT / "build/ci/common/release_channels.py"), "trial", "--help"],
                                   text=True, capture_output=True, timeout=30, check=False)
        for option in ("--event-name", "--event-path", "--live-pull-request", "--output", "--summary"):
            self.assertIn(option, help_text.stdout)

    def test_engine_layer_go_module_is_recorded_in_every_configuration(self):
        build = (ROOT / "build/cmake/GyoBuild.cmake").read_text(encoding="utf-8")
        # Unconditional (top-level) registration, before any game is added.
        registration = re.search(r"(?m)^gyo_register_go_module\(OWNER engine DIRECTORY \"\$\{GYO_REPOSITORY_ROOT\}/[^\"]+\"\)$", build)
        self.assertIsNotNone(registration)
        self.assertLess(registration.start(), build.index("foreach(GYO_CURRENT_APP IN LISTS GYO_ACTIVE_APPS)"))
        self.assertNotIn(" TESTS", registration.group(0))
        write = next(line for line in build.splitlines() if "gyo-build.json" in line and line.startswith("file(WRITE"))
        for field in ("shader_bundles", "shader_host_tools", "go_modules", "go_services"):
            self.assertIn(f'\\"{field}\\":', write)

    def test_every_external_action_is_pinned_to_a_full_commit(self):
        documents = sorted(WORKFLOWS.glob("*.yml")) + sorted((ROOT / ".github" / "actions").glob("*/action.yml"))
        self.assertIn(NATIVE_SETUP, documents)
        for document in documents:
            for reference in re.findall(r"(?m)^\s*(?:- )?uses:\s*([^\s#]+)", document.read_text(encoding="utf-8")):
                with self.subTest(document=document.name, uses=reference):
                    if reference.startswith("./"):
                        self.assertTrue((ROOT / reference).exists(), reference)
                    else:
                        self.assertRegex(reference, r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_./-]+@[0-9a-f]{40}$")

    def test_common_ci_definitions_name_no_product_owner(self):
        # Product selection belongs to registry data; naming an owner in common
        # CI definitions or helpers would make removing that owner edit CI.
        # Registered games and tools require their directories, so directories
        # also cover disabled owners that the registry export leaves out.
        owners = {entry.name for folder in ("apps", "tools", "services") if (ROOT / folder).is_dir()
                  for entry in (ROOT / folder).iterdir() if entry.is_dir() and not entry.name.startswith((".", "_"))}
        self.assertTrue(owners)
        sources = [*sorted(WORKFLOWS.glob("*.yml")), NATIVE_SETUP, *sorted((ROOT / "build/ci/common").glob("*.py"))]
        common = {path.relative_to(ROOT).as_posix(): path.read_text(encoding="utf-8") for path in sources}
        for owner in sorted(owners):
            name = re.escape(owner)
            # Only identifier-like uses count, so an owner named like ordinary
            # CI prose (quick, core, engine) does not collide: an owner path, a
            # quoted literal, an assignment or comparison value, or a product,
            # app, tool or game key.
            usage = re.compile(rf"(?:apps|tools|services)/{name}(?![A-Za-z0-9_])"
                               rf"|(['\"]){name}\1"
                               rf"|(?<![=!<>])=\s?{name}(?![A-Za-z0-9_./-])"
                               rf"|\b(?:products?|apps?|tools?|games?)\s*:\s*\[?\s*{name}(?![A-Za-z0-9_])")
            for path, text in common.items():
                with self.subTest(owner=owner, path=path):
                    match = usage.search(text)
                    self.assertIsNone(match, match and match.group(0))


if __name__ == "__main__":
    unittest.main()
