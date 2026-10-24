"""Outage rounds for object_fps_pvp (v7.1 outage-resync resync-l2-l3; network-architecture §4 "連線中斷與恢復").

The acceptance relay cuts the path between the Clients and the Gateway for one window, then restores it:

  both      every datagram in both directions is dropped during the window.
  uplink    only Client->Gateway datagrams are dropped.
  downlink  only Gateway->Client datagrams are dropped.
  partial   Client->Gateway: one datagram in every `period` passes on time, the others are delivered `late_ms`
            later (partial delivery; late_ms 3000 makes first copies later than N_o). Gateway->Client passes.
  port      both directions are dropped, and after the window every Client's datagrams reach the Gateway from a
            new source port of the relay (a NAT rebinding): the Gateway sees a new endpoint for the same session.
  none      armed like the others, nothing is cut: the clean control.

Nothing else is impaired: no random loss, no reordering outside `partial`, no rewriting. Outside the window every
datagram is forwarded as received.

How (as run_client_delay.py, D50 option b1): action_probe.py and impaired_network.py are imported unchanged and must
match their frozen digests (the same FROZEN and frozen-list check). For each round this process replaces
action_probe.ActionRelay with an OutageRelay subclass carrying the round's profile, calls the unchanged
run_case(options, 'upstream', duration_ms), and restores it. run_case arms the relay at fault_at on the probe's clock.
run_case's verdict is recorded only: an outage past T_max ends a Client session, and the probe then fails by design.

Each round's relay.json carries an `outage` section; round_problems proves from it that the injection took effect
and stayed pure: the window, at least one datagram cut in each cut direction (and none in the others), received ==
forwarded + dropped in both directions with an empty queue at the end, and for `port` every session moved once.

Output (a new or empty directory; nothing is overwritten): plan.json, results.jsonl, summary.json and one case
directory per round. Exit status 0 only when every round completed with a valid injection.
"""
import argparse
import heapq
import json
from pathlib import Path
import select
import socket
import sys
import threading
import signal
import time
from contextlib import contextmanager
from types import SimpleNamespace

import action_probe
import impaired_network
import run_client_delay
from acceptance_util import PROTOCOL_VERSION
from run_network import steady_clock_ns

HERE = Path(__file__).resolve().parent
MECHANISM = 'client-gateway-outage'
KINDS = ('both', 'uplink', 'downlink', 'partial', 'port', 'none')
DISPATCH_MODE = 'upstream'   # run_case's dispatch only: OutageRelay overrides arm() and _receive()
RECORDED = ('run_outage.py', 'run_client_delay.py', 'action_probe.py', 'impaired_network.py', 'acceptance_util.py',
            'run_network.py', 'command_evidence.py')
QUEUE_LIMIT = 4096           # partial's late datagrams: 3 s x two Sessions x at most 120 a second, with margin
Refused = run_client_delay.Refused


class Profile:
    def __init__(self, kind, duration_ms, period=0, late_ms=0):
        self.kind, self.duration_ms, self.period, self.late_ms = kind, duration_ms, period, late_ms

    def fields(self):
        return {'outage_kind': self.kind, 'outage_duration_ms': self.duration_ms,
                'partial_period': self.period, 'partial_late_ms': self.late_ms}

    def cuts(self, direction):
        """Whether the window drops this direction's datagrams."""
        return (self.kind in ('both', 'port') or (self.kind == 'uplink' and direction == 'upstream') or
                (self.kind == 'downlink' and direction == 'downstream'))


def profile_problems(profile):
    problems = []
    if profile.kind not in KINDS:
        problems.append(f'unknown kind {profile.kind!r}')
    if profile.duration_ms <= 0:
        problems.append('--duration-ms must be positive')
    if profile.kind == 'partial':
        if profile.period < 2 or profile.late_ms <= 0:
            problems.append('partial needs --period >= 2 and --late-ms > 0')
    elif profile.period or profile.late_ms:
        problems.append(f'{profile.kind} takes no --period or --late-ms')
    return problems


class OutageLine:
    """The injection's decisions and counts, independent of sockets (unit-tested)."""

    def __init__(self, profile):
        self.profile = profile
        self.armed_at = None
        self.ended_at = None
        self.ended_by = None
        self.counts = {d: {'received': 0, 'forwarded': 0, 'dropped': 0, 'late': 0} for d in ('upstream', 'downstream')}
        self.partial_seen = {}
        self.queue = []
        self.order = 0
        self.maximum_queue = 0
        self.moves = {}          # session -> {'old_port', 'new_port', 'moved_ns'}

    def active(self, now):
        return self.armed_at is not None and self.ended_at is None and now - self.armed_at < self.profile.duration_ms/1000

    def arm(self, now):
        self.armed_at = now

    def ended(self, now):
        """True once, when the window has passed."""
        if self.armed_at is None or self.ended_at is not None or now - self.armed_at < self.profile.duration_ms/1000:
            return False
        self.ended_at, self.ended_by = now, 'window'
        return True

    def close(self, now):
        """The round ended inside the window (an outage past T_max ends the probe first)."""
        if self.armed_at is not None and self.ended_at is None:
            self.ended_at, self.ended_by = now, 'run'

    def decide(self, direction, session, now):
        """'forward', 'drop' or the due time of a late datagram."""
        self.counts[direction]['received'] += 1
        if not self.active(now):
            return 'forward'
        if self.profile.cuts(direction):
            self.counts[direction]['dropped'] += 1
            return 'drop'
        if self.profile.kind == 'partial' and direction == 'upstream':
            seen = self.partial_seen.get(session, 0)
            self.partial_seen[session] = seen + 1
            if seen % self.profile.period == 0:
                return 'forward'
            self.counts[direction]['late'] += 1
            return now + self.profile.late_ms/1000
        return 'forward'

    def hold(self, due, item):
        self.order += 1
        heapq.heappush(self.queue, (due, self.order, item))
        self.maximum_queue = max(self.maximum_queue, len(self.queue))
        if len(self.queue) > QUEUE_LIMIT:
            raise ValueError('Outage relay queue overflow')

    def due(self, now):
        ready = []
        while self.queue and self.queue[0][0] <= now:
            ready.append(heapq.heappop(self.queue)[2])
        return ready

    def forwarded(self, direction):
        self.counts[direction]['forwarded'] += 1

    def evidence(self):
        conserved = all(c['received'] == c['forwarded'] + c['dropped'] for c in self.counts.values()) and not self.queue
        window = None if self.armed_at is None or self.ended_at is None else round((self.ended_at-self.armed_at)*1000, 3)
        return {'mechanism': MECHANISM, **self.profile.fields(), 'window_ms': window, 'ended_by': self.ended_by,
                'counts': self.counts,
                'queued': len(self.queue),
                'maximum_queue': self.maximum_queue, 'conserved': conserved, 'moves': {str(s): m for s, m in self.moves.items()}}


def moved_socket():
    """The relay's new socket toward the Gateway for one session after a port change."""
    moved = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    moved.bind(('127.0.0.1', 0))
    moved.setblocking(False)
    return moved


class OutageRelay(action_probe.ActionRelay):
    """ActionRelay whose only impairment is the OutageLine; PROFILE is set on a per-round subclass."""
    PROFILE = None
    CLOCK = staticmethod(time.monotonic)
    MOVED_SOCKET = staticmethod(moved_socket)

    def __init__(self, http, udp, mode, loss_seed=impaired_network.DEFAULT_LOSS_SEED):
        # Set before ActionRelay.__init__: _ImpairedGateway.__init__ starts the relay thread.
        self.line = OutageLine(self.PROFILE)
        self.moved_sockets = {}  # session -> the relay's new socket toward the Gateway
        super().__init__(http, udp, mode, loss_seed)

    def arm(self, layer, duration):
        with self.lock:
            self.line.arm(self.CLOCK())
            self.fault.update(start_ns=steady_clock_ns(), release_ns=0, mechanism=MECHANISM, dispatch_layer=layer,
                              **self.PROFILE.fields())

    def _send(self, payload, destination, upstream, label='forwarded'):
        session = int.from_bytes(payload[8:16], 'big')
        moved = self.moved_sockets.get(session) if upstream else None
        (moved or self.udp).sendto(payload, destination)
        self.events.append({'time_ns': steady_clock_ns(), 'event': label, 'upstream': upstream,
                            'kind': int.from_bytes(payload[6:8], 'big'), 'session': session, 'bytes': len(payload)})

    def _release(self):
        now = self.CLOCK()
        for payload, destination, upstream in self.line.due(now):
            self._send(payload, destination, upstream, 'late_forwarded')
            self.line.forwarded('upstream')
        if self.line.ended(now):
            self.fault['release_ns'] = steady_clock_ns()
            if self.PROFILE.kind == 'port':
                for session in list(self.clients):
                    moved = self.MOVED_SOCKET()
                    self.moved_sockets[session] = moved
                    self.line.moves[session] = {'old_port': self.udp.getsockname()[1],
                                                'new_port': moved.getsockname()[1], 'moved_ns': steady_clock_ns()}

    def _receive(self, payload, source):
        # The event and routing of ActionRelay._receive, without any of its drop or hold branches.
        if len(payload) < 24:
            raise ValueError('Truncated product packet')
        now_ns = steady_clock_ns()
        upstream = source != self.upstream_udp
        kind = int.from_bytes(payload[6:8], 'big')
        session = int.from_bytes(payload[8:16], 'big')
        with self.lock:
            self._release()
            direction = 'upstream' if upstream else 'downstream'
            if upstream:
                self.clients[session] = source
                destination = self.upstream_udp
            else:
                destination = self.clients.get(session)
                if destination is None:
                    return
            body = payload[24:]
            event = {'time_ns': now_ns, 'event': 'received', 'upstream': upstream, 'kind': kind,
                     'session': session, 'bytes': len(payload)}
            if kind == 2:
                self.players[session] = action_probe.scalar(body, 1)
            if kind == 6:
                event['shots'] = [action_probe.scalar(shot, 1) for shot in action_probe.messages(body, 1)]
                event['ack'] = action_probe.scalar(body, 2)
            if kind == 7:
                event['decisions'] = [action_probe.decision(item) for item in action_probe.messages(body, 2)]
                event['retired'] = action_probe.scalar(body, 1)
            self.events.append(event)
            decision = self.line.decide(direction, session, self.CLOCK())
            if decision == 'drop':
                self.events.append(dict(event, event='outage_dropped'))
                return
            if decision != 'forward':
                self.line.hold(decision, (payload, destination, upstream))
                return
            self._send(payload, destination, upstream)
            self.line.forwarded(direction)

    def _relay(self):
        try:
            while not self.stop.is_set():
                with self.lock:
                    self._release()
                    sockets = [self.udp, *self.moved_sockets.values()]
                readable, _, _ = select.select(sockets, [], [], .002)
                for ready in readable:
                    self._receive(*ready.recvfrom(65535))
        except Exception as failure:  # noqa: BLE001 - recorded as the relay error, as ActionRelay does
            self._fail(str(failure))

    def close(self):
        with self.lock:
            self.line.close(self.CLOCK())
        super().close()
        for moved in self.moved_sockets.values():
            moved.close()

    def evidence(self):
        # run_case reads the evidence when the round ends, also when the probe failed inside the window.
        with self.lock:
            self._release()
            self.line.close(self.CLOCK())
        result = super().evidence()
        with self.lock:
            result['outage'] = self.line.evidence()
            result['fault'] = dict(self.fault)
        return result


def outage_relay(profile):
    return type('OutageRelay', (OutageRelay,), {'PROFILE': profile})


@contextmanager
def replaced_action_relay(profile):
    """Replaces action_probe.ActionRelay inside this process for one round (b1); the files stay unchanged."""
    original = action_probe.ActionRelay
    if original is not run_client_delay.FROZEN_ACTION_RELAY:
        raise RuntimeError('action_probe.ActionRelay is not the frozen ActionRelay')
    action_probe.ActionRelay = outage_relay(profile)
    try:
        yield
    finally:
        action_probe.ActionRelay = original


def round_problems(relay, profile):
    """The proof that the injection took effect and stayed pure, read from relay.json."""
    relay = relay or {}
    outage = relay.get('outage')
    if not isinstance(outage, dict):
        return ['relay evidence has no outage section (the replacement did not take effect)']
    problems = []
    fault = relay.get('fault') or {}
    for key, value in profile.fields().items():
        if fault.get(key) != value or outage.get(key) != value:
            problems.append(f'{key} {fault.get(key)!r}/{outage.get(key)!r} differs from {value!r}')
    start, release = fault.get('start_ns'), fault.get('release_ns')
    window = outage.get('window_ms')
    if outage.get('ended_by') == 'run':
        # Recorded: the round ended inside the window (an outage past T_max ends the probe first).
        if not (isinstance(start, int) and start > 0 and isinstance(window, (int, float)) and window > 0):
            problems.append(f'start_ns {start}, window {window} ms')
    else:
        if not (isinstance(start, int) and isinstance(release, int) and 0 < start < release):
            problems.append(f'start_ns/release_ns {start}/{release}')
        if not isinstance(window, (int, float)) or window < profile.duration_ms:
            problems.append(f'window {window} ms (the relay clock) shorter than {profile.duration_ms} ms')
    counts = outage.get('counts') or {}
    for direction in ('upstream', 'downstream'):
        dropped = (counts.get(direction) or {}).get('dropped', 0)
        if profile.cuts(direction) and dropped < 1:
            problems.append(f'no {direction} datagram was cut')
        if not profile.cuts(direction) and dropped:
            problems.append(f'{dropped} {direction} datagrams cut by a profile that keeps them')
    if profile.kind == 'none' and any((counts.get(d) or {}).get('late', 0) for d in ('upstream', 'downstream')):
        problems.append('late datagrams in the clean control')
    if profile.kind == 'partial' and (counts.get('upstream') or {}).get('late', 0) < 1:
        problems.append('no late upstream datagram')
    if outage.get('conserved') is not True:
        problems.append(f'received != forwarded + dropped, or a queue left: {counts}, queued {outage.get("queued")}')
    if profile.kind == 'port':
        moves = outage.get('moves') or {}
        if len(moves) != len(relay.get('players') or {}) or not moves:
            problems.append(f'{len(moves)} sessions moved, {len(relay.get("players") or {})} joined')
    elif outage.get('moves'):
        problems.append('a session moved in a profile without a port change')
    if relay.get('relay_error') is not None:
        problems.append(f'relay error {relay["relay_error"]}')
    return problems


def parse(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    for name in ('match', 'gateway', 'probe', 'arena', 'output', 'frozen_list'):
        parser.add_argument('--'+name.replace('_', '-'), dest=name, type=Path, required=True)
    parser.add_argument('--kind', choices=KINDS, required=True)
    parser.add_argument('--duration-ms', type=int, required=True)
    parser.add_argument('--period', type=int, default=0, help='partial')
    parser.add_argument('--late-ms', type=int, default=0, help='partial')
    parser.add_argument('--fault-at', type=float, action='append', required=True,
                        help='seconds after the probe start; repeat it to alternate across rounds')
    parser.add_argument('--rounds', type=int, default=1)
    parser.add_argument('--fps', type=int, choices=(30, 60), default=60)
    args = parser.parse_args(argv)
    args.outage = Profile(args.kind, args.duration_ms, args.period, args.late_ms)
    problems = profile_problems(args.outage)
    if args.rounds < 1:
        problems.append('--rounds must be positive')
    if problems:
        parser.error('; '.join(problems))
    return args


def case_name(profile, index, fault_at):
    shape = f'{profile.kind}-{profile.duration_ms}ms'
    if profile.kind == 'partial':
        shape += f'-1of{profile.period}-late{profile.late_ms}ms'
    return f'outage-{shape}-at{fault_at:.1f}-r{index}'


def execute(args, case=action_probe.run_case, repository=run_client_delay.repository_state):
    """Runs the rounds into a new output directory; returns the summary written to summary.json."""
    problems = run_client_delay.frozen_problems(args.frozen_list)
    if problems:
        raise Refused('frozen tools differ: '+'; '.join(problems))
    output = args.output
    if output.exists() and any(output.iterdir()):
        raise Refused(f'{output} is not empty; evidence is never overwritten')
    profile = args.outage
    schedule = [{'round': index, 'fault_at': args.fault_at[(index-1) % len(args.fault_at)]}
                for index in range(1, args.rounds+1)]
    for item in schedule:
        item['case'] = case_name(profile, item['round'], item['fault_at'])
    output.mkdir(parents=True, exist_ok=True)
    plan = {'protocol': PROTOCOL_VERSION, 'b1': 'action_probe.ActionRelay replaced in this process',
            'mechanism': MECHANISM, **profile.fields(), 'dispatch_mode': DISPATCH_MODE,
            'verdict': 'run_case verdict recorded only', 'fault_at': args.fault_at, 'fps': args.fps,
            'rounds': args.rounds, 'gameplay_v5': True, 'schedule': schedule, 'frozen': run_client_delay.FROZEN,
            'frozen_list': {'path': str(args.frozen_list), 'sha256': run_client_delay.sha256(args.frozen_list)},
            'tools': {name: run_client_delay.source_sha256(HERE/name) for name in RECORDED},
            'artifacts': {name: {'path': str(getattr(args, name)), 'sha256': run_client_delay.sha256(getattr(args, name))}
                          for name in ('match', 'gateway', 'probe', 'arena') if Path(getattr(args, name)).is_file()},
            'repository': repository(), 'python': sys.version.split()[0]}
    (output/'plan.json').write_text(json.dumps(plan, indent=2)+'\n')
    stop, rounds = [], []
    stopped = None

    def finish_current_round(_signal, _frame):
        stop.append(True)

    on_main = threading.current_thread() is threading.main_thread()
    previous = [signal.signal(s, finish_current_round) for s in (signal.SIGINT, signal.SIGTERM)] if on_main else []
    try:
        for item in schedule:
            if stopped or stop:
                rounds.append(dict(item, status='not_run'))
                continue
            record = dict(item)
            try:
                options = SimpleNamespace(match=args.match, gateway=args.gateway, probe=args.probe,
                                          arena=args.arena, output=output, gameplay_v5=True,
                                          case_name=item['case'], fps=args.fps, fault_at=item['fault_at'])
                with replaced_action_relay(profile):
                    result = case(options, DISPATCH_MODE, profile.duration_ms)
                written = output/item['case']/'relay.json'
                relay = json.loads(written.read_text()) if written.is_file() else None
                problems = round_problems(relay, profile)
                record['status'] = 'injection_invalid' if problems else 'completed'
                if problems:
                    stopped = f'round {item["round"]}: injection invalid: '+'; '.join(problems)
                record.update(round_problems=problems, outage=(relay or {}).get('outage'),
                              fault=(relay or {}).get('fault'),
                              run_case={'passed': result.get('passed'), 'errors': result.get('errors', [])[:8],
                                        'recorded_only': True})
            except Exception as failure:  # noqa: BLE001 - relay replacement or evidence failure: stop
                record.setdefault('status', 'runner_error')
                stopped = f'round {item["round"]}: {type(failure).__name__}: {failure}'
                record['exception'] = stopped
            rounds.append(record)
            with (output/'results.jsonl').open('a') as log:
                log.write(json.dumps(record)+'\n')
            print(json.dumps({k: record.get(k) for k in ('round', 'case', 'status')}), flush=True)
    finally:
        for s, handler in zip((signal.SIGINT, signal.SIGTERM), previous):
            signal.signal(s, handler)
    summary = {'plan': plan, 'rounds': rounds, 'interrupted': bool(stop), 'stopped': stopped,
               'repository_end': repository(), 'completed': all(r['status'] == 'completed' for r in rounds)}
    (output/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    return summary


def main(argv=None):
    args = parse(argv)
    for name in ('match', 'gateway', 'probe', 'arena', 'frozen_list'):
        setattr(args, name, getattr(args, name).resolve())
        if not getattr(args, name).is_file():
            raise SystemExit('Missing '+name)
    args.output = args.output.resolve()
    try:
        summary = execute(args)
    except Refused as refusal:
        print(json.dumps({'refused': str(refusal)}), flush=True)
        return 2
    if summary['stopped']:
        print(json.dumps({'stopped': summary['stopped']}), flush=True)
    return 0 if summary['completed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
