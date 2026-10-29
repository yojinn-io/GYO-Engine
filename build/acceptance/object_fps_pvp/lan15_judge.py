#!/usr/bin/env python3
"""lan15-windows judge: the declared checks of one LAN session (v7.1 D236).

  lan15_judge.py SESSION_DIR [--output judge.json]

The checks are those of docs/object_fps_pvp/plans/v7.1/lan15-declaration.md;
this script only writes its clauses as code and adds none. The session folder:

  marks.txt                   key=value lines written by hand during the session (below)
  mac/match.log, mac/gateway.log, mac/match*-ingress.jsonl, mac/match*-shots.jsonl
  clients/<name>/*.log        every Client process log of one machine (the Mac's too)
  clients/<name>/ping.txt     the Windows ping record of S2 ("<ISO time> True|False" lines)
  quad/probe.log, quad/*.json the quad probe that fills the room for Q2

marks.txt keys (times are local wall clock of the machine named in the key's
comment; "mac" times are the Mac's):
  n=<players in S1>  n_w=<Windows Clients>  s1_end=<mac time>
  trial=<client name>,<2s|7s>,<that Windows' local start time>   (one line per S2 trial)
  q1=<client name>   q2_start=<mac time>  q2_end=<mac time>

Each check is "passed", "failed" or "undetermined" (missing or incomplete
evidence: E, never F; declaration section 7). Exit status: 0 when every check
passed, 1 otherwise, 2 on a usage error.
"""
import argparse
import datetime as dt
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ingress_evidence  # noqa: E402

JUDGE_ID = "pvp-v71-lan15-1"
CPP_LINE = re.compile(r"^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3})([+-]\d{4}) mono_ns=(-?\d+) (.*)$")
GO_LINE = re.compile(r"^(\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}\.\d{6}) (.*)$")
S1_MIN_SECONDS = 900          # J0: at least 15 minutes
S1_END_MARGIN = 30            # S1 ends 30 s before the hand mark s1_end
OFFSET_TOLERANCE = 1.0        # Client clock offset: two estimates within 1 s
RESULTS_MIN_INTERVAL_MS = 33.3
T_MAX_RANGE_MS = (5000, 5100)
TRIAL_RANGE_S = {"2s": (1.5, 4.0), "7s": (6.0, float("inf"))}
MODEL_ERRORS = ("missing player animation", "missing or ambiguous player bone")


class Line:
    def __init__(self, wall, mono, text):
        self.wall, self.mono, self.text = wall, mono, text


def read_cpp(path):
    lines = []
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = CPP_LINE.match(raw)
        if m:
            wall = dt.datetime.strptime(m.group(1) + m.group(2), "%Y-%m-%dT%H:%M:%S.%f%z")
            lines.append(Line(wall, int(m.group(3)), m.group(4)))
        elif raw.strip():
            lines.append(Line(None, None, raw))
    return lines


def read_go(path, tz):
    lines = []
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = GO_LINE.match(raw)
        wall = dt.datetime.strptime(m.group(1), "%Y/%m/%d %H:%M:%S.%f").replace(tzinfo=tz) if m else None
        lines.append(Line(wall, None, m.group(2) if m else raw))
    return lines


def fields(text):
    return dict(re.findall(r"(\w+)=(\S+)", text))


def result(status, **detail):
    return dict(status=status, **detail)


def passed_if(condition, **detail):
    return result("passed" if condition else "failed", **detail)


def parse_marks(path, tz):
    marks = {"trial": []}
    for raw in path.read_text(encoding="utf-8").splitlines():
        raw = raw.split("#", 1)[0].strip()
        if not raw:
            continue
        key, value = raw.split("=", 1)
        if key == "trial":
            name, kind, start = value.split(",")
            marks["trial"].append({"client": name.strip(), "kind": kind.strip(), "start": dt.datetime.fromisoformat(start.strip())})
        elif key in ("s1_end", "q2_start", "q2_end"):
            marks[key] = dt.datetime.fromisoformat(value.strip()).replace(tzinfo=tz)
        elif key in ("n", "n_w"):
            marks[key] = int(value)
        else:
            marks[key] = value.strip()
    return marks


class Session:
    """Everything read once; the checks below only look."""

    def __init__(self, root):
        self.root = Path(root)
        self.problems = []
        mac = self.root / "mac"
        self.match = read_cpp(mac / "match.log") if (mac / "match.log").exists() else None
        self.tz = next((l.wall.tzinfo for l in self.match or [] if l.wall), dt.timezone.utc)
        self.gateway = read_go(mac / "gateway.log", self.tz) if (mac / "gateway.log").exists() else None
        self.ingress = sorted(mac.glob("match*-ingress.jsonl")) if mac.exists() else []
        self.shots = sorted(mac.glob("match*-shots.jsonl")) if mac.exists() else []
        self.marks = parse_marks(self.root / "marks.txt", self.tz) if (self.root / "marks.txt").exists() else None
        self.clients = {}
        for folder in sorted((self.root / "clients").glob("*")) if (self.root / "clients").exists() else []:
            logs = {p.name: read_cpp(p) for p in sorted(folder.glob("*.log"))}
            self.clients[folder.name] = {"logs": logs, "ping": folder / "ping.txt"}
        self.quad_log = self.root / "quad" / "probe.log"
        self.quad_json = sorted((self.root / "quad").glob("**/*.json")) if (self.root / "quad").exists() else []

    def match_lines(self, pattern):
        return [l for l in self.match or [] if re.search(pattern, l.text)]

    def gateway_lines(self, pattern):
        return [l for l in self.gateway or [] if re.search(pattern, l.text)]


# ---- bookkeeping (section 7) and the clock offsets (section 1) ----

def bookkeeping(s):
    missing = []
    if s.match is None: missing.append("mac/match.log")
    if s.gateway is None: missing.append("mac/gateway.log")
    if len(s.ingress) != 1: missing.append(f"one match*-ingress.jsonl (found {len(s.ingress)})")
    if len(s.shots) != 1: missing.append(f"one match*-shots.jsonl (found {len(s.shots)})")
    if s.marks is None or not {"n", "n_w", "s1_end"} <= s.marks.keys(): missing.append("marks.txt with n, n_w, s1_end")
    if not s.clients: missing.append("clients/<name>/")
    if missing:
        return result("undetermined", missing=missing)
    problems = []
    if len(s.match_lines(r"\bstart executable_sha256=")) != 1: problems.append("match.log: not exactly one start line")
    if len(s.gateway_lines(r"Object FPS Gateway start ")) != 1: problems.append("gateway.log: not exactly one start line")
    if not s.match_lines(r"\bend version=1 "): problems.append("match.log: no end line")
    if s.match_lines(r"\[log\] dropped_lines="): problems.append("match.log: dropped_lines")
    for name, client in s.clients.items():
        for log, lines in client["logs"].items():
            texts = [l.text for l in lines]
            if sum("PvP client start " in t for t in texts) != 1: problems.append(f"{name}/{log}: not exactly one start line")
            if any("[log] dropped_lines=" in t for t in texts): problems.append(f"{name}/{log}: dropped_lines")
            if not any("connection summary version=1 " in t for t in texts): problems.append(f"{name}/{log}: no connection summary")
    joined = {int(fields(l.text)["player"]) for l in s.match_lines(r"\bjoin player=\d+ accepted")}
    owners = {}
    for name, client in s.clients.items():
        for log, lines in client["logs"].items():
            for l in lines:
                m = re.search(r"PvP connection phase=playing player=(\d+)", l.text)
                if m: owners.setdefault(int(m.group(1)), set()).add(f"{name}/{log}")
    bots = quad_players(s)
    for player in sorted(joined):
        if player in bots: continue
        if len(owners.get(player, ())) != 1: problems.append(f"player {player}: {len(owners.get(player, ()))} Client logs")
    return result("undetermined" if problems else "passed", problems=problems)


def quad_players(s):
    ids = set()
    for path in s.quad_json:
        try: data = json.loads(path.read_text(encoding="utf-8"))
        except ValueError: continue
        if isinstance(data, dict): ids.update(int(player) for player in data.get("player_ids", []))
    return ids


def client_offsets(s):
    """Per Client log: Mac wall clock minus its wall clock, from two pairs (join and leave)."""
    active = {int(fields(l.text)["player"]): l.wall for l in s.gateway_lines(r"^player active player=")}
    left = {int(fields(l.text)["player"]): l.wall for l in s.gateway_lines(r"^player left player=")}
    offsets = {}
    for name, client in s.clients.items():
        for log, lines in client["logs"].items():
            estimates, last_player = [], None
            for l in lines:
                m = re.search(r"PvP connection phase=(\w+) player=(\d+)", l.text)
                if not m or l.wall is None: continue
                phase, player = m.group(1), int(m.group(2))
                if phase == "playing":
                    last_player = player
                    if player in active: estimates.append((active[player] - l.wall).total_seconds())
                elif phase == "lobby" and last_player in left:
                    estimates.append((left[last_player] - l.wall).total_seconds())
                    last_player = None
            key = f"{name}/{log}"
            if len(estimates) >= 2 and max(estimates) - min(estimates) <= OFFSET_TOLERANCE:
                offsets[key] = sum(estimates) / len(estimates)
            else:
                offsets[key] = None
    return offsets


def in_mac_time(line, offset):
    return None if line.wall is None or offset is None else line.wall + dt.timedelta(seconds=offset)


# ---- S1 (section 2) ----

def s1_range(s):
    joins = s.match_lines(r"\bjoin player=\d+ accepted")
    n = s.marks["n"]
    if len(joins) < n or joins[n - 1].wall is None:
        return None
    return joins[n - 1].wall, s.marks["s1_end"] - dt.timedelta(seconds=S1_END_MARGIN)


def windows_inside(lines, start, end):
    out = []
    for l in lines:
        f = fields(l.text)
        if l.wall is None or "window_ms" not in f: continue
        if l.wall - dt.timedelta(milliseconds=int(f["window_ms"])) >= start and l.wall <= end:
            out.append((l, f))
    return out


def check_j0(s, rng, offsets):
    start, end = rng
    length = (end - start).total_seconds()
    changes = [l.text for l in s.match_lines(r"\b(join|leave) player=") if l.wall and start < l.wall <= end]
    wrong, unknown = [], []
    for name, client in s.clients.items():
        for log, lines in client["logs"].items():
            for l in lines:
                if "PvP summary " not in l.text: continue
                at = in_mac_time(l, offsets.get(f"{name}/{log}"))
                if at is None:
                    if l.wall is not None: unknown.append(f"{name}/{log}")
                    continue
                if start <= at <= end and int(fields(l.text).get("players", -1)) != s.marks["n"]:
                    wrong.append(f"{name}/{log}: {l.text[:80]}")
    if unknown:
        return result("undetermined", reason="Client clock offset not established", logs=sorted(set(unknown)))
    return passed_if(length >= S1_MIN_SECONDS and not changes and not wrong,
                     s1_seconds=round(length, 1), membership_changes=changes, wrong_players=wrong[:10])


def check_j3(s, rng, offsets):
    start, end = rng
    inside = lambda w: w is not None and start <= w <= end  # noqa: E731
    found, unknown = [], []
    for name, client in s.clients.items():
        for log, lines in client["logs"].items():
            key = f"{name}/{log}"
            # Declaration section 1: without an established offset, that Client's J3 is undetermined.
            if offsets.get(key) is None: unknown.append(key); continue
            for l in lines:
                if re.search(r"connection (interrupted|failed) ", l.text) and inside(in_mac_time(l, offsets[key])):
                    found.append(f"{key}: {l.text[:100]}")
    for pattern in (r"^player evicted ", r"^player expired ", r"join refused .*code=protocol_version", r"runtime disconnected"):
        found += [f"gateway: {l.text[:100]}" for l in s.gateway_lines(pattern) if inside(l.wall)]
    for pattern in (r"\bevicted player=", r"\bleave player=\d+ reason="):
        found += [f"match: {l.text[:100]}" for l in s.match_lines(pattern) if inside(l.wall)]
    if unknown:
        return result("undetermined", reason="Client clock offset not established", logs=sorted(set(unknown)), found=found)
    return passed_if(not found, found=found)


def check_j4(s, rng):
    bad = []
    for l, f in windows_inside(s.gateway_lines(r"^player ingress statistics "), *rng):
        nonzero = {k: v for k, v in f.items() if k.startswith("drop_rate_limited_") and v != "0"}
        if nonzero: bad.append(dict(player=f.get("player"), **nonzero))
    windows = len(windows_inside(s.gateway_lines(r"^player ingress statistics "), *rng))
    if windows == 0:
        return result("undetermined", reason="no ingress window inside S1")
    return passed_if(not bad, windows=windows, rate_limited=bad)


def number(value):
    try: return float(value)
    except (TypeError, ValueError): return None


def check_j5(s, rng):
    below, with_results, with_value = [], set(), set()
    for l, f in windows_inside(s.gateway_lines(r"^player send statistics "), *rng):
        player = f.get("player")
        if int(f.get("results", 0)) > 0: with_results.add(player)
        value = number(f.get("results_min_interval_ms"))
        if value is None: continue
        with_value.add(player)
        if value < RESULTS_MIN_INTERVAL_MS: below.append({"player": player, "results_min_interval_ms": value})
    empty = sorted(with_results - with_value)
    if not with_results:
        return result("failed", reason="no player had results inside S1")
    return passed_if(not below and not empty, below=below, players_without_value=empty)


# ---- the whole session (section 3) ----

def trace_end(path):
    ends = [json.loads(t) for t in path.read_text(encoding="utf-8").splitlines() if '"trace_end"' in t]
    return ends


def check_j1(s):
    problems = []
    for path in s.ingress + s.shots:
        ends = trace_end(path)
        if len(ends) != 1 or ends[0].get("dropped") != 0: problems.append(f"{path.name}: trace_end {ends}")
    joined = {int(fields(l.text)["player"]) for l in s.match_lines(r"\bjoin player=\d+ accepted")}
    for kind in ("ingress", "resync"):
        finals = [int(fields(l.text)["player"]) for l in s.match_lines(rf"\b{kind} statistics version=1 .*\bfinal=1\b")]
        for player in sorted(joined):
            if finals.count(player) != 1: problems.append(f"player {player}: {finals.count(player)} {kind} final=1 lines")
    end = s.match_lines(r"\bend version=1 ")
    if not end or "reason=signal" not in end[-1].text or "exit_code=0" not in end[-1].text:
        problems.append(f"match end: {[l.text for l in end]}")
    return passed_if(not problems, problems=problems)


def check_j2(s):
    text = lambda path: path.read_text(encoding="utf-8", errors="replace")  # noqa: E731
    gateway = "\n".join(l.text for l in s.gateway)
    match = "\n".join(l.text for l in s.match)
    evidence = ingress_evidence.analyze_text(gateway, match, text(s.ingress[0]), injection=False)
    status = evidence["status"]
    return result("passed" if status == "passed" else "undetermined" if status == "undetermined" else "failed",
                  ingress_status=status, analyzer_id=evidence.get("analyzer_id"))


def client_sessions(s):
    """Per Client log, per player id: summed PvP summary counts and the last actions_pending."""
    sessions = {}
    for name, client in s.clients.items():
        for log, lines in client["logs"].items():
            for l in lines:
                if "PvP summary " not in l.text: continue
                f = fields(l.text)
                entry = sessions.setdefault(int(f["player"]), {"log": f"{name}/{log}", "submitted": 0, "accepted": 0,
                                                                "rejected": 0, "by_reason": {}, "pending": None})
                for key, target in (("actions_submitted", "submitted"), ("accepted", "accepted"), ("rejected", "rejected")):
                    entry[target] += int(f[key])
                for key, value in f.items():
                    if key.startswith("rejected_"):
                        entry["by_reason"][key[9:]] = entry["by_reason"].get(key[9:], 0) + int(value)
                entry["pending"] = int(f["actions_pending"])
    return sessions


def normal_leavers(s):
    left = {int(fields(l.text)["player"]) for l in s.gateway_lines(r"^player left player=")}
    abnormal = {int(fields(l.text)["player"]) for l in s.gateway_lines(r"^player (expired|evicted) player=")}
    failed = set()
    for client in s.clients.values():
        for lines in client["logs"].values():
            for l in lines:
                m = re.search(r"connection failed player=(\d+) ", l.text)
                if m: failed.add(int(m.group(1)))
    closed = set(q1_players(s)[:1])
    return {p for p in left if p not in abnormal | failed | closed}


def q1_players(s):
    """The Q1 machine's last two session ids (the closed one, then the rejoined one), or []."""
    name = s.marks.get("q1") if s.marks else None
    if not name or name not in s.clients: return []
    ids = [int(m.group(1)) for lines in s.clients[name]["logs"].values() for l in lines
           for m in [re.search(r"PvP connection phase=playing player=(\d+)", l.text)] if m]
    return ids[-2:] if len(ids) >= 2 else []


def check_k1(s):
    sessions, normal = client_sessions(s), normal_leavers(s)
    bad = [dict(player=p, **e) for p, e in sessions.items() if p in normal and
           (e["pending"] != 0 or e["submitted"] != e["accepted"] + e["rejected"])]
    if not normal & sessions.keys():
        return result("undetermined", reason="no session left normally")
    return passed_if(not bad, sessions=sorted(normal & sessions.keys()), mismatches=bad)


def check_k2(s):
    sessions, normal = client_sessions(s), normal_leavers(s)
    decisions = {}
    for t in s.shots[0].read_text(encoding="utf-8").splitlines():
        record = json.loads(t)
        if record.get("kind") != "decision": continue
        counts = decisions.setdefault(int(record["player_id"]), {})
        counts[record["rejection"]] = counts.get(record["rejection"], 0) + 1
    bad = []
    for player in sorted(normal & sessions.keys()):
        e, match = sessions[player], decisions.get(player, {})
        expected = {"none": e["accepted"], **{k: v for k, v in e["by_reason"].items() if v}}
        actual = {k: v for k, v in match.items() if v}
        if {k: v for k, v in expected.items() if v} != actual:
            bad.append({"player": player, "client": expected, "match": actual})
    if not normal & sessions.keys():
        return result("undetermined", reason="no session left normally")
    return passed_if(not bad, mismatches=bad)


# ---- S2, S3, S7 (sections 4 to 6) ----

def ping_outage(path, around):
    """The ping outage (seconds) overlapping [around - 5 s, around + 15 s] on the same machine's clock."""
    if not path.exists(): return None
    samples = []
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        parts = raw.strip().split()
        if len(parts) != 2: continue
        try: samples.append((dt.datetime.fromisoformat(parts[0]), parts[1] == "True"))
        except ValueError: continue
    best, start = None, None
    for when, ok in samples:
        if not ok and start is None: start = when
        if ok and start is not None:
            naive = lambda d: d.replace(tzinfo=None)  # noqa: E731
            if naive(start) <= naive(around) + dt.timedelta(seconds=15) and naive(when) >= naive(around) - dt.timedelta(seconds=5):
                length = (when - start).total_seconds()
                best = length if best is None else max(best, length)
            start = None
    return best


def check_trials(s, offsets):
    """R1 to R3, one result per declared trial."""
    expired = {int(fields(l.text)["player"]): l for l in s.gateway_lines(r"^player expired player=\d+ reason=session_timeout")}
    left = {}
    for l in s.gateway_lines(r"^player left player="): left.setdefault(int(fields(l.text)["player"]), l)
    ended = [(int(fields(l.text)["player"]), l.wall) for l in s.gateway_lines(r"^player (evicted|expired) player=")]
    match_timeout = {int(fields(l.text)["player"]) for l in s.match_lines(r"\bleave player=\d+ reason=session_timeout")}
    joins = [int(fields(l.text)["player"]) for l in s.match_lines(r"\bjoin player=\d+ accepted")]
    out = []
    for trial in s.marks.get("trial", []):
        client = s.clients.get(trial["client"])
        entry = {"client": trial["client"], "kind": trial["kind"]}
        if client is None:
            out.append(dict(entry, status="undetermined", reason="no such client")); continue
        length = ping_outage(client["ping"], trial["start"])
        entry["outage_s"] = length
        low, high = TRIAL_RANGE_S[trial["kind"]]
        if length is None or not low <= length <= high:
            out.append(dict(entry, status="undetermined", reason="outage length outside the valid range")); continue
        lines = [l for lines in client["logs"].values() for l in lines]
        after = [l for l in lines if l.wall and l.wall.replace(tzinfo=None) >= trial["start"].replace(tzinfo=None) - dt.timedelta(seconds=5)]
        interrupted = next((l for l in after if "connection interrupted " in l.text), None)
        if interrupted is None:
            out.append(dict(entry, status="failed", reason="no connection interrupted")); continue
        player = int(fields(interrupted.text)["player"])
        rest = after[after.index(interrupted) + 1:]
        outcome = next((l for l in rest if re.search(r"connection (resumed|failed) ", l.text)), None)
        if trial["kind"] == "2s":
            key = next((f"{trial['client']}/{log}" for log, ls in client["logs"].items() if interrupted in ls), None)
            at = in_mac_time(interrupted, offsets.get(key))
            if at is None:
                out.append(dict(entry, status="undetermined", reason="Client clock offset not established")); continue
            gone = [w.isoformat() for p, w in ended if p == player and w and at <= w <= at + dt.timedelta(seconds=15)]
            ok = outcome is not None and "connection resumed " in outcome.text and not gone
            out.append(dict(entry, status="passed" if ok else "failed", player=player, outcome=outcome.text[:100] if outcome else None))
        else:
            timed_out = outcome is not None and "reason=Gateway connection timed out" in outcome.text
            t_max = None
            if timed_out:
                t_max = (outcome.mono - interrupted.mono) / 1e6 + int(fields(interrupted.text)["gap_ms"])
            gw_ok = player in expired and (player not in left or expired[player].wall <= left[player].wall)
            later = [p for p in joins if p > player]
            ok = (timed_out and T_MAX_RANGE_MS[0] <= t_max <= T_MAX_RANGE_MS[1] and gw_ok and player in match_timeout)
            out.append(dict(entry, status="passed" if ok else "failed", player=player, t_max_ms=t_max,
                            gateway_session_timeout=gw_ok, match_session_timeout=player in match_timeout))
            out.append(dict(client=trial["client"], kind="R3", status="passed" if later else "failed", rejoined_as=later[:1]))
    if not out:
        return [result("undetermined", reason="no trial in marks.txt")]
    return out


def others_disturbed(s, offsets, start, end, skip=None):
    """Client lines (other than machine skip) of an interruption or failure inside [start, end] on the Mac's clock."""
    found, unknown = [], []
    for name, client in s.clients.items():
        if name == skip: continue
        for log, lines in client["logs"].items():
            key = f"{name}/{log}"
            if offsets.get(key) is None: unknown.append(key); continue
            for l in lines:
                at = in_mac_time(l, offsets[key])
                if at and start <= at <= end and re.search(r"connection (interrupted|failed) ", l.text):
                    found.append(f"{key}: {l.text[:100]}")
    return found, unknown


def check_q1(s, offsets):
    name = s.marks.get("q1")
    if not name or name not in s.clients:
        return result("undetermined", reason="q1 client not named or missing")
    ids = q1_players(s)
    if len(s.clients[name]["logs"]) < 2 or len(ids) < 2:
        return result("undetermined", reason="q1 needs two Client logs on that machine")
    old, new = ids
    gone = any(re.search(rf"^player (left|expired) player={old}\b", l.text) for l in s.gateway)
    leave = s.match_lines(rf"\bleave player={old} reason=")
    join = s.match_lines(rf"\bjoin player={new} accepted")
    if not leave or not join or leave[0].wall is None or join[0].wall is None:
        return result("failed", old=old, new=new, gateway_gone=gone, match_leave=bool(leave), match_join=bool(join))
    start, end = leave[0].wall, join[0].wall + dt.timedelta(seconds=10)
    disturbed, unknown = others_disturbed(s, offsets, start, end, skip=name)
    if unknown:
        return result("undetermined", reason="Client clock offset not established", logs=unknown)
    # The other Clients see one player fewer, then the full room again.
    counts = {}
    for other, client in s.clients.items():
        if other == name: continue
        for log, lines in client["logs"].items():
            key = f"{other}/{log}"
            for l in lines:
                at = in_mac_time(l, offsets[key])
                if "PvP summary " in l.text and at and start - dt.timedelta(seconds=2) <= at <= end + dt.timedelta(seconds=10):
                    counts.setdefault(key, []).append(int(fields(l.text)["players"]))
    n = s.marks["n"]
    seen = {key: values for key, values in counts.items() if values}
    dip = bool(seen) and all(n - 1 in v and v[-1] == n and v.index(n - 1) < len(v) - 1 for v in seen.values())
    return passed_if(new != old and gone and dip and not disturbed, old=old, new=new, gateway_gone=gone,
                     others_players=seen, others_disturbed=disturbed)


def check_q2(s, offsets):
    if "q2_start" not in s.marks or "q2_end" not in s.marks or not s.quad_json:
        return result("undetermined", reason="q2 marks or quad output missing")
    start, end = s.marks["q2_start"], s.marks["q2_end"]
    refused = [l for l in s.gateway_lines(r"join refused .*code=room_full") if l.wall and start <= l.wall <= end]
    fifth = None
    for path in s.quad_json:
        try: data = json.loads(path.read_text(encoding="utf-8"))
        except ValueError: continue
        if isinstance(data, dict) and data.get("fifth") is not None: fifth = data["fifth"]
    fifth_ok = isinstance(fifth, dict) and fifth.get("error") == "room_full" and not fifth.get("joined")
    disturbed, unknown = others_disturbed(s, offsets, start, end)
    if unknown:
        return result("undetermined", reason="Client clock offset not established", logs=unknown)
    return passed_if(bool(refused) and fifth_ok and not disturbed, refused_lines=len(refused), fifth=fifth,
                     players_disturbed=disturbed)


def check_s7(s):
    problems = []
    for name, client in s.clients.items():
        texts = [l.text for lines in client["logs"].values() for l in lines]
        if not any("PvP connection phase=" in t for t in texts): problems.append(f"{name}: never reached the Lobby")
        problems += [f"{name}: {t[:100]}" for t in texts if any(e in t for e in MODEL_ERRORS)]
    return passed_if(not problems, problems=problems, note="the look is judged by the user (L3)")


def judge(root):
    s = Session(root)
    report = {"judge_id": JUDGE_ID, "session": str(root), "bookkeeping": bookkeeping(s)}
    if report["bookkeeping"]["status"] == "undetermined" and report["bookkeeping"].get("missing"):
        report["passed"] = False
        return report
    offsets = client_offsets(s)
    report["client_offsets_s"] = offsets
    rng = s1_range(s)
    if rng is None:
        report["s1"] = result("undetermined", reason="fewer joins than n")
    else:
        report["s1_range"] = [rng[0].isoformat(), rng[1].isoformat()]
        report["J0"] = check_j0(s, rng, offsets)
        report["J3"] = check_j3(s, rng, offsets)
        report["J4"] = check_j4(s, rng)
        report["J5"] = check_j5(s, rng)
    report["J1"] = check_j1(s)
    report["J2"] = check_j2(s)
    report["K1"] = check_k1(s)
    report["K2"] = check_k2(s)
    report["R"] = check_trials(s, offsets)
    report["Q1"] = check_q1(s, offsets)
    report["Q2"] = check_q2(s, offsets)
    report["S7"] = check_s7(s)
    statuses = [v["status"] for k, v in report.items() if isinstance(v, dict) and "status" in v]
    statuses += [r["status"] for r in report["R"]]
    report["passed"] = all(status == "passed" for status in statuses)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("session", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    if not args.session.is_dir():
        parser.error("SESSION_DIR is not a directory")
    report = judge(args.session)
    text = json.dumps(report, indent=2, ensure_ascii=False, default=str) + "\n"
    if args.output: args.output.write_text(text, encoding="utf-8")
    sys.stdout.write(text)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
