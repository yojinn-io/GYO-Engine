#!/usr/bin/env python3
"""Release trains and channels: pure policy, no network and no remote writes.

A train is one independently versioned release line derived from registry
data, never from product names in CI code:

  tools      the fixed toolchain package (every release tool) on every platform
  <game>     one registry-enabled game: its package on each enabled platform
             plus the service archives its packaging row records

Channels deliver a train's archives:

  trial      a pull request labelled TRIAL_LABEL, or a manual run: Actions
             artifacts only, never a release
  snapshot   every successful default-branch push: one published prerelease
             per train under a never-reused tag; the SNAPSHOT_KEEP most
             recently published stay
  formal     Prepare Release for one train: a verified, unpublished draft.
             A game train whose game declares release evidence (manual
             real-device L4 checks) also needs a reference to that evidence;
             the tools train and snapshots never do
"""

import argparse
from dataclasses import dataclass, replace
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

from release_support import (PLATFORMS, EvidenceItem, ReleaseError, git, load_release_evidence, single_line_text,
                             validate_commit, validate_product, parse_boolean)
from app_registry import export_registry
from go_checks import TOOLCHAIN_PRODUCT


TOOLS_TRAIN = "tools"
# The pull request label that requests trial packages; package-trial.yml
# repeats it in its concurrency expression (pinned by a test).
TRIAL_LABEL = "package"
# Snapshots kept per train (user decision); older snapshot prereleases and
# their snapshot tags are deleted.
SNAPSHOT_KEEP = 5
RESERVED_TRAINS = frozenset((TOOLS_TRAIN, TOOLCHAIN_PRODUCT))
# One line: an issue, pull request, discussion or artifact link, or a short
# text reference to where people recorded the real-device evidence.
EVIDENCE_REFERENCE_LIMIT = 500

_NUMBER = r"(?:0|[1-9][0-9]*)"
_SUFFIX = r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?\Z"
# Games: SemVer, for example v5.0.0 or v5.1.0-rc.1.
SEMVER_PATTERN = re.compile(rf"v{_NUMBER}\.{_NUMBER}\.{_NUMBER}{_SUFFIX}")
# Tools: calendar versions vYYYY.M.N (month without a leading zero, N from 1),
# still valid SemVer, for example v2026.10.1.
CALVER_PATTERN = re.compile(rf"v[2-9][0-9]{{3}}\.(?:1[0-2]|[1-9])\.[1-9][0-9]*{_SUFFIX}")


@dataclass(frozen=True)
class Train:
    id: str
    product: str
    platforms: tuple[str, ...]

    @property
    def pairs(self) -> list[tuple[str, str]]:
        return [(self.product, platform) for platform in self.platforms]


def derive_trains(app_pairs) -> dict[str, Train]:
    """The tools train first, then one train per enabled game in registry order."""
    games: dict[str, list[str]] = {}
    for product, platform in app_pairs:
        validate_product(product)
        if product in RESERVED_TRAINS:
            raise ReleaseError(f"Game id '{product}' is reserved for the tools train")
        if platform not in PLATFORMS or platform in games.get(product, ()):
            raise ReleaseError(f"Invalid or duplicate registry platform {product}/{platform}")
        games.setdefault(product, []).append(platform)
    trains = {TOOLS_TRAIN: Train(TOOLS_TRAIN, TOOLCHAIN_PRODUCT, tuple(PLATFORMS))}
    for game, platforms in games.items():
        trains[game] = Train(game, game, tuple(platform for platform in PLATFORMS if platform in platforms))
    return trains


def resolve_train(trains: dict[str, Train], train_id) -> Train:
    if not isinstance(train_id, str) or train_id not in trains:
        raise ReleaseError(f"Unknown release train {train_id!r}; expected one of {sorted(trains)}")
    return trains[train_id]


def validate_train_version(train: Train, version) -> str:
    pattern, example = ((CALVER_PATTERN, "vYYYY.M.N, for example v2026.10.1") if train.id == TOOLS_TRAIN
                        else (SEMVER_PATTERN, "v-prefixed SemVer, for example v5.0.0 or v5.1.0-rc.1"))
    match = pattern.fullmatch(version) if isinstance(version, str) else None
    if match is None:
        raise ReleaseError(f"Train {train.id} versions use {example}")
    for identifier in (match.group(1).split(".") if match.group(1) else ()):
        if identifier.isdigit() and len(identifier) > 1 and identifier.startswith("0"):
            raise ReleaseError("Numeric prerelease identifiers may not have leading zeroes")
    return version


def release_tag(train: Train, version) -> str:
    """Formal tags carry their train: tools-v2026.10.1, <game>-v5.0.0."""
    return f"{train.id}-{validate_train_version(train, version)}"


def parse_release_tag(train: Train, tag) -> str:
    prefix = train.id + "-"
    if not isinstance(tag, str) or not tag.startswith(prefix):
        raise ReleaseError(f"Release tag {tag!r} does not belong to train {train.id}")
    return validate_train_version(train, tag[len(prefix):])


def snapshot_tag(train: Train, commit: str, committed_at: int) -> str:
    """<train>-snapshot-<yyyymmdd>-<sha7>, dated by the commit (UTC), so a
    rerun of the same commit reconciles its own tag instead of adding one.
    The date only names the tag; it never orders snapshots."""
    validate_commit(commit)
    if isinstance(committed_at, bool) or not isinstance(committed_at, int) or committed_at < 0:
        raise ReleaseError("Snapshot date requires the commit timestamp")
    day = datetime.fromtimestamp(committed_at, timezone.utc).strftime("%Y%m%d")
    return f"{train.id}-snapshot-{day}-{commit[:7]}"


def parse_snapshot_tag(train_id: str, tag) -> tuple[str, str] | None:
    """(yyyymmdd, sha7) for this train's snapshot tags; None for every other tag."""
    if not isinstance(tag, str):
        return None
    match = re.fullmatch(rf"{re.escape(train_id)}-snapshot-([0-9]{{8}})-([0-9a-f]{{7}})", tag)
    return (match.group(1), match.group(2)) if match else None


def _publish_order(release: dict) -> tuple:
    # Publication order: published_at, then the monotonically assigned release
    # id. Neither the tag's date nor GitHub's created_at (the date of the
    # tagged commit) follows the order in which the default branch moved.
    published = release.get("published_at")
    release_id = release.get("id")
    return (published if isinstance(published, str) else "",
            release_id if isinstance(release_id, int) and not isinstance(release_id, bool) else 0)


def published_snapshots(releases, train_id: str) -> list[dict]:
    """This train's published snapshot prereleases, most recently published
    first. Drafts (an interrupted run) and releases promoted to a full release
    never count."""
    snapshots = [release for release in releases
                 if isinstance(release, dict) and parse_snapshot_tag(train_id, release.get("tag_name"))
                 and release.get("draft") is False and release.get("prerelease") is True]
    return sorted(snapshots, key=_publish_order, reverse=True)


def select_expired_snapshots(releases, train_id: str, keep: int = SNAPSHOT_KEEP, *, protect=()) -> list[dict]:
    """Snapshots beyond the `keep` most recently published, oldest last;
    protected tags stay."""
    if isinstance(keep, bool) or not isinstance(keep, int) or keep < 1:
        raise ReleaseError("Snapshot retention must keep at least one snapshot")
    return [release for release in published_snapshots(releases, train_id)[keep:]
            if release.get("tag_name") not in protect]


def select_orphan_snapshot_tags(tags, releases, train_id: str, *, protect=()) -> list[str]:
    """This train's snapshot tags without any release, left by an interrupted
    cleanup or publication; a rerun of that publication recreates its tag."""
    released = {release.get("tag_name") for release in releases if isinstance(release, dict)}
    return sorted(tag for tag in set(tags) if parse_snapshot_tag(train_id, tag) is not None
                  and tag not in released and tag not in protect)


def latest_other_snapshot(releases, train_id: str, tag: str) -> dict | None:
    """The most recently published snapshot of this train other than `tag`.

    Publishing `tag` is superseded when its commit is a strict ancestor of
    this snapshot's commit (an old run's rerun); the caller decides ancestry
    from history, never from dates.
    """
    if parse_snapshot_tag(train_id, tag) is None:
        raise ReleaseError(f"Not a snapshot tag of train {train_id}: {tag!r}")
    return next((release for release in published_snapshots(releases, train_id)
                 if release.get("tag_name") != tag), None)


def select_trial(event_name: str, event, live_pull_request) -> tuple[bool, str]:
    """Whether a trial packaging run builds packages for this event.

    A re-run replays the original payload, so the live pull request state and
    labels (read when the run starts) decide: only an open pull request
    builds, since GitHub also sends `labeled` for closed and merged ones. A
    `labeled` event for any other label never builds.
    """
    if event_name == "workflow_dispatch":
        return True, "manual"
    if event_name != "pull_request":
        raise ReleaseError(f"Unsupported trial packaging event: {event_name!r}")
    if not isinstance(event, dict) or not isinstance(event.get("pull_request"), dict):
        raise ReleaseError("pull_request event must carry its pull_request object")
    live = live_pull_request if isinstance(live_pull_request, dict) else {}
    state, live_labels = live.get("state"), live.get("labels")
    if (state not in ("open", "closed") or not isinstance(live_labels, list)
            or any(not isinstance(label, str) for label in live_labels)):
        raise ReleaseError("Trial packaging requires the live pull request state and labels (--live-pull-request)")
    if state != "open":
        return False, "closed"
    if event.get("action") == "labeled":
        label = event.get("label")
        if not isinstance(label, dict) or label.get("name") != TRIAL_LABEL:
            return False, "other-label"
    return (True, "label") if TRIAL_LABEL in live_labels else (False, "no-label")


def train_release_evidence(train: Train, repository_root: Path | None = None) -> tuple[EvidenceItem, ...]:
    """The real-device (L4) items a formal release of this train must reference:
    the game's declared items narrowed to the train's platforms. The tools
    train never needs L4 evidence (user decision)."""
    if train.id == TOOLS_TRAIN:
        return ()
    platforms = set(train.platforms)
    return tuple(replace(item, platforms=tuple(value for value in item.platforms if value in platforms))
                 for item in load_release_evidence(train.product, repository_root)
                 if platforms.intersection(item.platforms))


def validate_evidence_reference(train: Train, items: tuple[EvidenceItem, ...], reference) -> str:
    """The normalized evidence reference; required when the train declares items.
    CI only checks that a reference exists; people judge the evidence."""
    if reference is None:
        reference = ""
    if not isinstance(reference, str):
        raise ReleaseError("l4_evidence must be text")
    reference = reference.strip()
    if reference and not items:
        raise ReleaseError(f"l4_evidence is not used by train {train.id}: it declares no real-device (L4) "
                           "evidence items; leave the field empty")
    if reference and not single_line_text(reference, EVIDENCE_REFERENCE_LIMIT):
        raise ReleaseError(f"l4_evidence must be one line of at most {EVIDENCE_REFERENCE_LIMIT} characters")
    # A leading dash would be read as an option by the draft command line;
    # reject it here so Prepare Release fails before the release build.
    if reference.startswith("-"):
        raise ReleaseError("l4_evidence must not start with '-'")
    if items and not reference:
        names = ", ".join(item.name for item in items)
        raise ReleaseError(f"Train {train.id} requires real-device (L4) release evidence for: {names}. "
                           "Record it in an issue, pull request, discussion or artifact and enter that "
                           "link in the l4_evidence field of Prepare Release")
    return reference


def prepare_event(event_name: str, event: dict, commit: str, ref: str, git_command=git, *,
                  trains: dict[str, Train], evidence) -> dict:
    """Validate a GUI Prepare Release dispatch without creating a tag or release.
    `evidence(train)` returns the train's required real-device items."""
    if event_name != "workflow_dispatch":
        raise ReleaseError("Prepare Release only accepts an explicit workflow_dispatch event")
    validate_commit(commit)
    if git_command("rev-parse", "HEAD") != commit:
        raise ReleaseError("The checked-out commit does not match the event commit")
    if (not isinstance(ref, str) or not ref.startswith("refs/heads/")
            or any(ord(char) < 32 or ord(char) == 127 for char in ref)):
        raise ReleaseError("Select a branch in Prepare Release; tag refs are not accepted")
    git_command("check-ref-format", ref)
    inputs = event.get("inputs", {})
    if not isinstance(inputs, dict):
        raise ReleaseError("Missing Prepare Release form inputs")
    train = resolve_train(trains, inputs.get("train"))
    tag = release_tag(train, inputs.get("version", ""))
    prerelease = parse_boolean(inputs.get("prerelease", False))
    items = evidence(train)
    reference = validate_evidence_reference(train, items, inputs.get("l4_evidence"))
    git_command("check-ref-format", f"refs/tags/{tag}")
    existing = git_command("tag", "--list", tag)
    if existing:
        if existing != tag or git_command("rev-parse", "--verify", f"refs/tags/{tag}^{{commit}}") != commit:
            raise ReleaseError("Existing version tag does not point to the exact selected commit")
    return {"tag": tag, "commit": commit, "prerelease": str(prerelease).lower(),
            "train": train.id, "product": train.product,
            "l4_items": ",".join(item.name for item in items), "l4_evidence": reference}


def _host(description: dict) -> str:
    system = str(description.get("build_os") or "unrecorded").split("-", 1)[0]
    return f"{system} {description.get('build_architecture') or 'unrecorded'}"


def _host_platform(description: dict) -> str | None:
    system = str(description.get("build_os") or "").split("-", 1)[0].lower()
    machine = str(description.get("build_architecture") or "").lower()
    os_name = {"linux": "linux", "windows": "windows", "macos": "macos", "darwin": "macos"}.get(system)
    architecture = {"x86_64": "x64", "amd64": "x64", "arm64": "arm64", "aarch64": "arm64"}.get(machine)
    return f"{os_name}-{architecture}" if os_name and architecture else None


def verification_level(description: dict) -> str:
    """One archive's CI verification level, from its own build metadata."""
    built = "built" if _host_platform(description) == description["platform"] else "cross-built"
    if description["kind"] == "service":
        return (f"{built} on {_host(description)} with {description.get('go_version') or 'unrecorded Go'} "
                f"(CGO disabled); the service binary is not run by CI")
    execution = {"native": "natively", "rosetta2": "under Rosetta 2 translation"}.get(
        description.get("cpu_execution"), "with unrecorded CPU execution")
    gpu = (f"{description['gpu_checks']} on a software Vulkan device" if description.get("gpu_checks")
           else "no GPU checks")
    return (f"{built} on {_host(description)}; {description['checks']} installed "
            f"{description['profile']} checks passed {execution}; {gpu}; no physical GPU")


def render_reference(reference: str) -> str:
    """The free-text evidence reference as inert Markdown: a plain web URL
    becomes an autolink, anything else an inline code span, so the field
    cannot add links, images, HTML, mentions or cross-references."""
    if re.fullmatch(r"https?://[^\s<>`]+", reference):
        return f"<{reference}>"
    fence = "`" * (max((len(run) for run in re.findall(r"`+", reference)), default=0) + 1)
    padding = " " if reference.startswith("`") or reference.endswith("`") else ""
    return f"{fence}{padding}{reference}{padding}{fence}"


def render_evidence(items: tuple[EvidenceItem, ...], reference: str) -> list[str]:
    """A checklist of the declared real-device items for the publisher."""
    if not items:
        if reference:
            raise ReleaseError("An evidence reference needs declared real-device (L4) items")
        return []
    lines = ["", "### Real-device evidence (L4)", "",
             f"Evidence: {render_reference(reference)}" if reference else "Evidence: none recorded.", ""]
    return lines + ["CI cannot run these checks; people run them on real devices. Before clicking "
                    "**Publish release**, the publisher confirms every item against the evidence:",
                    "", *(f"- [ ] `{item.name}` ({', '.join(item.platforms)}): {item.description}" for item in items)]


def render_notes(*, train: Train, commit: str, profile: str, descriptions: list[dict], snapshot: bool,
                 evidence: tuple[EvidenceItem, ...] = (), evidence_reference: str = "") -> str:
    if snapshot and (evidence or evidence_reference):
        raise ReleaseError("Snapshots never carry real-device evidence; their notes state CI verification only")
    lines = []
    if snapshot:
        lines += [f"Automated snapshot of `{commit}` from the default branch for testing. "
                  f"It is not a supported release; only the newest {SNAPSHOT_KEEP} snapshots of this train are kept.",
                  ""]
    lines += ["### CI verification", "",
              f"Train `{train.id}`, source `{commit}`, acceptance profile `{profile}`.", "",
              "| Archive | Verification |", "| --- | --- |"]
    lines += [f"| `{item['archive']}` | {verification_level(item)} |"
              for item in sorted(descriptions, key=lambda item: item["archive"])]
    manual = sorted({item["gpu_acceptance"] for item in descriptions if item.get("gpu_acceptance")})
    lines += ["", *(f"{text}." if not text.endswith(".") else text for text in manual),
              "Archives are not code-signed."]
    lines += render_evidence(evidence, evidence_reference)
    return "\n".join(lines) + "\n"


def train_matrix(trains: dict[str, Train]) -> dict:
    return {"include": [{"train": train.id, "product": train.product} for train in trains.values()]}


def main(arguments=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    trains_parser = commands.add_parser("trains", help="GitHub matrix with one row per release train")
    trains_parser.add_argument("--registry", type=Path, help="Project registry override for isolated validation")
    trains_parser.add_argument("--output", type=Path, help="Append GitHub Actions outputs")
    trial_parser = commands.add_parser("trial", help="Decide whether a trial packaging run builds packages")
    trial_parser.add_argument("--event-name", required=True)
    trial_parser.add_argument("--event-path", type=Path, required=True)
    trial_parser.add_argument("--live-pull-request",
                              help='JSON {"state": "open"|"closed", "labels": [names]} of the pull request now')
    trial_parser.add_argument("--output", type=Path, help="Append GitHub Actions outputs")
    trial_parser.add_argument("--summary", type=Path, help="Append a Markdown step summary")
    args = parser.parse_args(arguments)
    try:
        if args.command == "trains":
            outputs = {"trains": json.dumps(train_matrix(derive_trains(export_registry(args.registry))),
                                            separators=(",", ":"))}
        else:
            event = json.loads(args.event_path.read_text(encoding="utf-8-sig"))
            live = None if args.live_pull_request is None else json.loads(args.live_pull_request)
            selected, reason = select_trial(args.event_name, event, live)
            outputs = {"run_package": str(selected).lower(), "reason": reason}
            if args.summary:
                with args.summary.open("a", encoding="utf-8") as stream:
                    stream.write(f"## Trial packages\n\n- Event: `{args.event_name}`\n- Reason: `{reason}`\n"
                                 f"- Build quick packages: `{outputs['run_package']}`\n")
    except (ReleaseError, OSError, ValueError) as error:
        print(f"::error::{error}", file=sys.stderr)
        return 1
    if args.output:
        with args.output.open("a", encoding="utf-8") as stream:
            for name, value in outputs.items():
                stream.write(f"{name}={value}\n")
    print(json.dumps(outputs))
    return 0


if __name__ == "__main__":
    sys.exit(main())
