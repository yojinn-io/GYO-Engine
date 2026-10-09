"""Action latency segments (PvP v7 batch 08, analyzer v7): per gameplay case directory, the latency of accepted actions
along the path, with measures that are not quantized by the probe's frame.

  python3 action_latency_v7.py CASE_DIR [CASE_DIR ...]

The frozen analyzers' legal_client_p95_ms ends at the probe frame that drained the decision, so it is a whole number
of frames; at 30 FPS a phase shift moves its P95 between two and three frames. These measures end elsewhere:

  generation -> Match tick     the resolved tick's snapshot_produced (as legal_match_p95_ms)
  generation -> relay down     first downstream ActionResults datagram carrying the decision
  generation -> worker receipt the Client network worker's receipt ("received_ns", batch 08 and later)

and record each run's phases: the probe frame and the relay's result arrival against the Match tick grid.
All stamps share the C++ steady_clock domain (test_steady_clock.py keeps the relay on it).
"""
import json
import math
import sys
from pathlib import Path

TICK_NS = 1e9 / 60


def _lines(path):
    with open(path, encoding="utf-8") as stream:
        for line in stream:
            try:
                yield json.loads(line)
            except ValueError:
                pass


def percentile(values, q):
    """Nearest rank: the ceil(q * n)-th smallest value."""
    values = sorted(values)
    if not values:
        return None
    return values[max(0, math.ceil(q * len(values)) - 1)]


def analyze(case_dir):
    case_dir = Path(case_dir)
    submitted, decided = {}, {}
    for event in _lines(case_dir / "actions.jsonl"):
        if event.get("kind") == "submitted":
            submitted[(event["player_id"], event["request"]["action_id"])] = event["time_ns"]
        elif event.get("kind") == "decision" and event["decision"].get("rejection") == 0:
            decided[(event["player_id"], event["decision"]["action_id"])] = event
    ticks = {}
    for event in _lines(case_dir / "match-commands.jsonl"):
        if event.get("kind") == "snapshot_produced":
            ticks.setdefault(event["authority_tick"], event["time_ns"])
    relay = json.loads((case_dir / "relay.json").read_text())
    players = {str(session): int(player) for session, player in relay["players"].items()}
    down = {}
    for event in relay["events"]:
        if event.get("event") == "received" and not event.get("upstream") and event.get("kind") == 7:
            for decision in event.get("decisions", []):
                down.setdefault((players.get(str(event["session"])), decision["action_id"]), event["time_ns"])
    rows = []
    for key, generated in submitted.items():
        event = decided.get(key)
        tick = ticks.get(event["decision"]["resolved_tick"]) if event else None
        if event is None or tick is None:
            continue
        row = {"to_tick_ms": (tick - generated) / 1e6,
               "to_relay_down_ms": (down[key] - generated) / 1e6 if key in down else None,
               "to_worker_ms": (event["received_ns"] - generated) / 1e6 if event.get("received_ns") else None,
               "to_frame_ms": (event["time_ns"] - generated) / 1e6,
               "frame_phase_vs_tick_ms": ((generated - tick) % TICK_NS) / 1e6,
               "down_phase_vs_tick_ms": ((down[key] - tick) % (2 * TICK_NS)) / 1e6 if key in down else None}
        rows.append(row)
    summary = {"case_dir": str(case_dir), "actions": len(rows)}
    for name in ("to_tick_ms", "to_relay_down_ms", "to_worker_ms", "to_frame_ms", "frame_phase_vs_tick_ms", "down_phase_vs_tick_ms"):
        values = [row[name] for row in rows if row[name] is not None]
        summary[name] = {"count": len(values), "p50": percentile(values, 0.5), "p95": percentile(values, 0.95)}
    return summary


def main(argv=None):
    paths = (argv if argv is not None else sys.argv[1:])
    if not paths:
        print(__doc__.splitlines()[2].strip(), file=sys.stderr)
        return 2
    print(json.dumps([analyze(path) for path in paths], indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
