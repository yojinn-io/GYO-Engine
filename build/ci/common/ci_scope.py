"""Select the CI tiers for one GitHub event.

Every skip is decided here, inside a job, never by workflow trigger filters, so
the single required gate check always reports. Platform rows come from the
toolchain table owned by app_registry; products are selected later by CMake.
"""

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

from app_registry import PLATFORMS, ROOT, TOOLCHAINS


# A change set touching only these roots cannot affect any native build.
DOCUMENTATION_ROOTS = ("docs/",)


class ScopeError(ValueError):
    """The event cannot be classified; the gate must fail instead of guessing."""


@dataclass(frozen=True)
class Scope:
    run_l1: bool
    run_quick: bool
    reason: str


def is_documentation(path: str) -> bool:
    return any(path.startswith(root) for root in DOCUMENTATION_ROOTS)


def merge_changes(repository: Path, *, git: str = "git") -> list[str]:
    """Paths a pull request merge commit changes relative to its base parent.

    GitHub's pull_request SHA is a test merge commit whose first parent is the
    base branch. Renames are split so moving a file into docs/ still reports
    its non-documentation source path.
    """
    def run(*arguments: str) -> str:
        result = subprocess.run([git, *arguments], cwd=repository, capture_output=True,
                                text=True, encoding="utf-8", timeout=60, check=False)
        if result.returncode:
            raise ScopeError(f"git {' '.join(arguments)} failed:\n{result.stderr.strip()}")
        return result.stdout

    parents = run("rev-list", "--parents", "-n", "1", "HEAD").split()
    if len(parents) != 3:
        raise ScopeError("Pull request scope requires GitHub's two-parent test merge commit at HEAD")
    listing = run("diff", "--no-renames", "--name-only", "-z", "HEAD^1", "HEAD")
    return [path for path in listing.split("\0") if path]


def select_scope(event_name: str, event, changed_paths, live_draft: bool | None = None) -> Scope:
    """Map one event to the tiers that must pass; changed_paths is called lazily.

    live_draft is the pull request's draft state read when the run starts. A
    re-run replays the original payload, so the payload's draft flag can be
    stale and never decides whether the merge gate is skipped.
    """
    if event_name in ("push", "workflow_dispatch"):
        # Integration runs keep a complete record for every commit they see.
        return Scope(run_l1=True, run_quick=True, reason="integration")
    if event_name == "pull_request":
        if not isinstance(event, dict) or not isinstance(event.get("pull_request"), dict):
            raise ScopeError("pull_request event must carry its pull_request object")
        if not isinstance(live_draft, bool):
            raise ScopeError("pull_request scope requires the live draft state (--live-draft)")
        if live_draft:
            return Scope(run_l1=False, run_quick=False, reason="draft")
        paths = list(changed_paths())
        # An empty change list is unexpected; verify rather than skip.
        if paths and all(is_documentation(path) for path in paths):
            return Scope(run_l1=False, run_quick=False, reason="docs-only")
        return Scope(run_l1=True, run_quick=False, reason="pull-request")
    raise ScopeError(f"Unsupported CI event: {event_name!r}")


def platform_matrix() -> dict:
    """One L1 row per release platform, straight from the toolchain table."""
    return {"include": [dict(platform=platform, **TOOLCHAINS[platform]) for platform in PLATFORMS]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--event-name", required=True)
    parser.add_argument("--event-path", type=Path, required=True)
    parser.add_argument("--live-draft", choices=("true", "false"),
                        help="Current pull request draft state; required for pull_request events")
    parser.add_argument("--repository", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, help="Append GitHub Actions outputs")
    parser.add_argument("--summary", type=Path, help="Append a Markdown step summary")
    args = parser.parse_args()
    try:
        event = json.loads(args.event_path.read_text(encoding="utf-8-sig"))
        live_draft = None if args.live_draft is None else args.live_draft == "true"
        scope = select_scope(args.event_name, event, lambda: merge_changes(args.repository), live_draft)
    except (OSError, json.JSONDecodeError, ScopeError) as error:
        print(f"::error::{error}", file=sys.stderr)
        return 1
    outputs = {
        "run_l1": str(scope.run_l1).lower(),
        "run_quick": str(scope.run_quick).lower(),
        "reason": scope.reason,
        "platforms": json.dumps(platform_matrix(), separators=(",", ":")),
    }
    if args.output:
        with args.output.open("a", encoding="utf-8") as stream:
            for name, value in outputs.items():
                stream.write(f"{name}={value}\n")
    if args.summary:
        with args.summary.open("a", encoding="utf-8") as stream:
            stream.write(f"## CI scope\n\n- Event: `{args.event_name}`\n- Reason: `{scope.reason}`\n"
                         f"- L1 merge gate: `{outputs['run_l1']}`\n- Quick acceptance: `{outputs['run_quick']}`\n")
    print(json.dumps({name: value for name, value in outputs.items() if name != "platforms"}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
