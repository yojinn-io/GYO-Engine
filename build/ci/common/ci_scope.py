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

# How many commits before a pull request head the documentation-increment check
# examines; past it, L1 simply runs again.
INCREMENT_LIMIT = 20


class ScopeError(ValueError):
    """The event cannot be classified; the gate must fail instead of guessing."""


@dataclass(frozen=True)
class Scope:
    run_l1: bool
    run_quick: bool
    reason: str
    # Why an earlier L1 result was or was not reused; shown in the summary only.
    detail: str = ""


def is_documentation(path: str) -> bool:
    return any(path.startswith(root) for root in DOCUMENTATION_ROOTS)


def _git(repository: Path, git: str, *arguments: str, accept=(0,)) -> subprocess.CompletedProcess:
    result = subprocess.run([git, *arguments], cwd=repository, capture_output=True,
                            text=True, encoding="utf-8", timeout=60, check=False)
    if result.returncode not in accept:
        raise ScopeError(f"git {' '.join(arguments)} failed:\n{result.stderr.strip()}")
    return result


def merge_parents(repository: Path, *, git: str = "git") -> tuple[str, str]:
    """(base, head) of GitHub's pull_request SHA, a test merge commit whose first parent is the base branch."""
    parents = _git(repository, git, "rev-list", "--parents", "-n", "1", "HEAD").stdout.split()
    if len(parents) != 3:
        raise ScopeError("Pull request scope requires GitHub's two-parent test merge commit at HEAD")
    return parents[1], parents[2]


def diff_paths(repository: Path, old: str, new: str, *, git: str = "git") -> list[str]:
    """Paths changed from old to new. Renames are split so moving a file into
    docs/ still reports its non-documentation source path."""
    listing = _git(repository, git, "diff", "--no-renames", "--name-only", "-z", old, new).stdout
    return [path for path in listing.split("\0") if path]


def merge_changes(repository: Path, *, git: str = "git") -> list[str]:
    """Paths a pull request merge commit changes relative to its base parent."""
    base, _ = merge_parents(repository, git=git)
    return diff_paths(repository, base, "HEAD", git=git)


def verified_increment(repository: Path, l1_passed, *, git: str = "git",
                       limit: int = INCREMENT_LIMIT) -> tuple[str | None, str]:
    """An earlier commit of the pull request whose passed L1 also covers the head.

    Walks the head's first-parent history, the head itself excluded, and returns
    (commit, note) for the nearest commit C where
      - the base tip (HEAD^1) is an ancestor of C. The base branch only moves
        forward, so the base C was tested against was an ancestor of C as well:
        GitHub's test merge of C had exactly C's tree, as the merge at HEAD has
        exactly the head's tree;
      - every path changed from C to the head is documentation;
      - l1_passed(C): every L1 row of C completed with success.
    Otherwise it returns (None, why). The walk stops at the first commit that
    fails either history condition: no older commit can pass the first, and
    none is trusted for the second.
    """
    base, head = merge_parents(repository, git=git)
    history = _git(repository, git, "rev-list", "--first-parent", f"--max-count={limit + 1}", head).stdout.split()
    for commit in history[1:]:
        if _git(repository, git, "merge-base", "--is-ancestor", base, commit, accept=(0, 1)).returncode:
            return None, f"the base {base[:12]} is not an ancestor of {commit[:12]}"
        if not all(is_documentation(path) for path in diff_paths(repository, commit, head, git=git)):
            return None, f"{commit[:12]} differs from the head outside docs/"
        if l1_passed(commit):
            return commit, f"every L1 row passed on {commit[:12]}, which differs from the head only under docs/"
    return None, f"no commit with every L1 row passed within {limit} commits before the head"


def l1_rows_passed(listing, platforms) -> bool:
    """Whether a check-run listing of one commit shows every L1 row completed with success.

    Only check runs of GitHub Actions count; a missing, pending, skipped,
    cancelled or failed row means the commit was not verified.
    """
    runs = listing.get("check_runs") if isinstance(listing, dict) else None
    if not isinstance(runs, list) or listing.get("total_count") != len(runs):
        raise ScopeError("The check-run listing is malformed or incomplete")
    actions = [run for run in runs if isinstance(run, dict) and (run.get("app") or {}).get("slug") == "github-actions"]
    for platform in platforms:
        rows = [run for run in actions if run.get("name") == f"L1 / {platform}"]
        if not rows or any((run.get("status"), run.get("conclusion")) != ("completed", "success") for run in rows):
            return False
    return True


def github_l1_lookup(repository_slug: str, platforms, *, gh: str = "gh"):
    """l1_passed for verified_increment, reading the latest check runs of a commit with the gh CLI."""
    def l1_passed(commit: str) -> bool:
        result = subprocess.run([gh, "api", f"repos/{repository_slug}/commits/{commit}/check-runs?filter=latest&per_page=100"],
                                capture_output=True, text=True, encoding="utf-8", timeout=60, check=False)
        if result.returncode:
            raise ScopeError(f"Reading the check runs of {commit[:12]} failed:\n{result.stderr.strip()}")
        return l1_rows_passed(json.loads(result.stdout), platforms)
    return l1_passed


def select_scope(event_name: str, event, changed_paths, live_draft: bool | None = None,
                 increment=None) -> Scope:
    """Map one event to the tiers that must pass; changed_paths is called lazily.

    live_draft is the pull request's draft state read when the run starts. A
    re-run replays the original payload, so the payload's draft flag can be
    stale and never decides whether the merge gate is skipped.

    increment, when given, is called lazily for a ready pull request that also
    changes files outside docs/ and returns verified_increment's (commit, note).
    A commit skips L1 as "docs-increment"; any error runs L1.
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
        if increment is None:
            return Scope(run_l1=True, run_quick=False, reason="pull-request")
        try:
            commit, note = increment()
        except (ScopeError, OSError, subprocess.SubprocessError, ValueError) as error:
            # Reusing a result is only an optimization; when unsure, verify.
            commit, note = None, f"the documentation-increment check failed: {error}"
        if commit:
            return Scope(run_l1=False, run_quick=False, reason="docs-increment", detail=note)
        return Scope(run_l1=True, run_quick=False, reason="pull-request", detail=note)
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
    parser.add_argument("--github-repository", metavar="OWNER/NAME",
                        help="Enables the documentation-increment check for pull_request events: reuse the passed L1 "
                             "of an earlier commit whose code equals the head (check runs read with the gh CLI)")
    parser.add_argument("--repository", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, help="Append GitHub Actions outputs")
    parser.add_argument("--summary", type=Path, help="Append a Markdown step summary")
    args = parser.parse_args()
    try:
        event = json.loads(args.event_path.read_text(encoding="utf-8-sig"))
        live_draft = None if args.live_draft is None else args.live_draft == "true"
        increment = None
        if args.github_repository:
            lookup = github_l1_lookup(args.github_repository, PLATFORMS)
            increment = lambda: verified_increment(args.repository, lookup)  # noqa: E731
        scope = select_scope(args.event_name, event, lambda: merge_changes(args.repository), live_draft, increment)
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
            if scope.detail:
                stream.write(f"- Documentation increment: {scope.detail}\n")
    printed = {name: value for name, value in outputs.items() if name != "platforms"}
    if scope.detail:
        printed["detail"] = scope.detail
    print(json.dumps(printed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
