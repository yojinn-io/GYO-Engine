"""Late-correction count of one run directory, from two sources that must agree (observability-tools OT-2).

  python3 -I -B late_count.py DIR [DIR ...]

Sources (both written by the action probe; INVENTORY N-late-count-detector-increment):
  start_phase  result.json start_phase.<player>.epochs[*].late_corrections: one record per (player, epoch, life)
               holding that span's last counter value. The Client's counter is cleared only by
               LocalPlayerPrediction::Reset(), at the start of each span, so the sum is the increment.
  frames       action-frames.jsonl phase_late_corrections_a/_b: the D48 step 1 "P" rule, per player the sum of
               increases between consecutive frames, a drop (a reset) counting the new value from 0. Suffix _a is
               action-client.json player_ids[0], _b player_ids[1].
Tool problems (the count is not trustworthy, never a judgement): start_phase reports dropped_observations > 0 (its
record holds at most 64 spans, the rest are dropped); action-frames.jsonl's evidence_end reports lost frames; both
sources exist and differ.

The older reading, the maximum of the frame counter over a run (08c judge_l2.py), undercounts across resets
(08c-drop-report.md:733); this module does not offer it.
"""
import json
import sys
from pathlib import Path


def _json(path):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def from_start_phase(result):
    """(count, dropped_observations) from a result.json document, or (None, None) when it has no start_phase."""
    phase = (result or {}).get("start_phase")
    if not phase:
        return None, None
    count = sum(int(e.get("late_corrections", 0)) for p in phase.values() for e in p.get("epochs", []))
    dropped = sum(int(p.get("dropped_observations") or 0) for p in phase.values())
    return count, dropped


def increments(values):
    """Sum of increases of a counter sequence; a drop is a reset, after which the new value counts from 0."""
    total, previous = 0, None
    for value in values:
        if previous is not None:
            total += value - previous if value >= previous else value
        previous = value
    return total


def from_frames(frames_path, player_ids):
    """({player_id: count}, frames lost) from action-frames.jsonl, or (None, None) when the file or the counters are
    missing. The file ends with one {"kind": "evidence_end", "lost": n, "records": m} line."""
    path = Path(frames_path)
    if not path.is_file():
        return None, None
    series, lost = {"a": [], "b": []}, None
    with open(path, encoding="utf-8") as stream:
        for line in stream:
            if not line.strip():
                continue
            frame = json.loads(line)
            if frame.get("kind") == "evidence_end":
                lost = int(frame.get("lost") or 0)
                continue
            for suffix in series:
                value = frame.get(f"phase_late_corrections_{suffix}")
                if value is None:
                    return None, None
                series[suffix].append(int(value))
    ids = list(player_ids or [1, 2])[:2]
    return {ids[i]: increments(series[s]) for i, s in enumerate("ab") if i < len(ids)}, lost


def count(directory):
    directory = Path(directory)
    result = _json(directory / "result.json")
    phase_count, dropped = from_start_phase(result)
    client = _json(directory / "action-client.json") or {}
    frames, lost = from_frames(directory / "action-frames.jsonl", client.get("player_ids"))
    frame_count = None if frames is None else sum(frames.values())
    problems = []
    if dropped:
        problems.append(f"start_phase dropped_observations {dropped}: spans beyond its 64 records are not counted")
    if lost:
        problems.append(f"action-frames.jsonl lost {lost} frame(s): increments between them are not seen")
    if phase_count is not None and frame_count is not None and phase_count != frame_count:
        problems.append(f"start_phase {phase_count} != frames {frame_count}")
    value = phase_count if phase_count is not None else frame_count
    return {"late_corrections": None if problems else value, "start_phase": phase_count, "frames": frame_count,
            "frames_by_player": frames, "frames_lost": lost, "dropped_observations": dropped, "tool_problems": problems}


def main(argv):
    if not argv:
        print(__doc__.splitlines()[2].strip())
        return 2
    for directory in argv:
        print(json.dumps({"dir": directory, **count(directory)}, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
