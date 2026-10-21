"""Client->Gateway pure-delay rounds for object_fps_pvp (v7 batch 08c L2 preparation, L2P-injector).

Injection 'client-gateway-pure-delay' (incident 2026-10-10 section 9: later injections delay only): every UDP
datagram the acceptance relay receives from a Client is forwarded to the Gateway later; nothing is dropped, nothing
is kept "latest only", nothing overtakes. Gateway->Client datagrams are forwarded as received. Two profiles:

  x1    from the arm time, for window_ms, every Client->Gateway datagram is due delay_ms after it was received.
  ramp  the delay rises linearly from 0 to peak_ms over rise_ms, then falls linearly to 0 over fall_ms.

No overtaking: a datagram is never due before the previous Client->Gateway datagram (due = max(received + delay,
previous due)). For the ramp this bound never applies, because the falling slope -peak_ms/fall_ms must be >= -1
(checked before anything runs). The x1 profile ends with a step down, so the datagrams received in the delay_ms
after its window wait behind the delayed ones and are released with them (a slope -1 drain), and the run is
released when that drain is empty.

Bounds checked before anything runs: maximum delay x packets_per_second < 256 (the frozen relay's queue bound,
action_probe.py:242-243; default rate 240 = two Sessions x the Gateway's 120 packets per Session window, the
highest rate the Gateway accepts), the ramp slope, and that the injection and its drain end 3 s before the 16 s
gameplay probe does. The relay never drops on its own queue: when the queue reaches QUEUE_NEAR datagrams the round is
recorded as invalid instead.

The three existing drop paths are never reached: _ImpairedGateway._receive (first/burst/5 % random,
impaired_network.py:175-194) is overridden by ActionRelay and here again; ActionRelay._receive (network mode's 5 %,
action_probe.py:232, and the block modes' keep-latest, :254) is overridden by DelayRelay._receive, which has no drop
branch.

How (D50 option b1 applied to the UDP relay): action_probe.py and impaired_network.py are imported unchanged. Before
anything is written, the sha256 of every file of the frozen list (--frozen-list, shasum format, e.g. the analyzers
sha256 list) must match, and the list must include the two files DelayRelay builds on (and those must equal FROZEN),
else the run is refused. For each round this process replaces the module attribute action_probe.ActionRelay (the
class run_case instantiates) with a DelayRelay subclass carrying the round's profile, restores it afterwards, and
calls the unchanged run_case(options, 'upstream', duration_ms). 'upstream' is only the dispatch: run_case then arms
the relay at fault_at on the probe's clock and records the relay's fault as the case's fault; the frozen
'upstream' hold path is never reached because DelayRelay overrides arm() and _receive().

After each round the relay evidence on disk (relay.json, written by run_case) must prove the injection and its
purity (round_problems): the profile fields, delayed datagrams >= 1, received == forwarded in both directions at the
arm and at the end with an empty queue, no unroutable datagram, no relay error, the queue below QUEUE_NEAR, the send
lateness p99 <= LATENESS_P99_MS, and no Gateway rate rejection (a released burst must not make the Gateway drop).
Otherwise the round is 'injection_invalid' and the run stops. run_case's verdict (frozen analyzers, mode
'upstream') is recorded only.

Output (a new or empty directory; nothing is overwritten): plan.json, results.jsonl (one line per round), summary.json
and one case directory per round written by run_case. Exit status 0 only when every round completed.
"""
import argparse
from collections import defaultdict
from contextlib import contextmanager
import hashlib
import heapq
import json
import math
from pathlib import Path
import signal
import subprocess
import sys
import threading
import time
from types import SimpleNamespace

import action_probe
import impaired_network
from acceptance_util import PROTOCOL_VERSION
from run_network import steady_clock_ns

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
MECHANISM = 'client-gateway-pure-delay'
DIRECTION = 'client->gateway'
# The files DelayRelay builds on; equal to analyzers sha256 lists since C1 (pvp-v7-batch09-s1-20261010).
# action_probe.py refrozen by v7.1 D122 (its Gateway limiter check only; ActionRelay unchanged).
FROZEN = {'action_probe.py': 'ccef0053629c60c2b2b51fcaceaf666e3911283e16462e64f19649c8734c657f',
          'impaired_network.py': '9720778bb4bf6c1c3455c0f26590021281aee9f070d10d14e071682a8446d131'}
# Recorded in plan.json (not checked): this runner and the modules run_case uses.
RECORDED = ('run_client_delay.py', 'action_probe.py', 'impaired_network.py', 'backpressure_probe.py',
            'gameplay_evidence.py', 'command_evidence.py', 'action_evidence.py', 'acceptance_util.py',
            'run_network.py')
QUEUE_LIMIT = 256          # action_probe.py:242-243
QUEUE_NEAR = 192           # three quarters of QUEUE_LIMIT: the round is invalid at or above it
PACKETS_PER_SECOND = 240   # two Sessions x 120 packets per Session window (services/gyo_gateway/session)
LATENESS_P99_MS = 10.0
PROBE_SECONDS = 16.0       # gameplay v5 probe
TAIL_SECONDS = 3.0
DELAY_MS = (1, 1000)
SPAN_MS = (100, 8000)
FAULT_AT = (1.0, 12.0)
DISPATCH_MODE = 'upstream'
STATUSES = ('completed', 'runner_error', 'injection_invalid', 'not_run')


class Refused(Exception):
    """The run did not start; nothing was written."""


class Profile:
    """Delay as a function of the time since the arm; None after the profile ends."""

    def __init__(self, kind, delay_ms=0, window_ms=0, peak_ms=0, rise_ms=0, fall_ms=0):
        self.kind = kind
        self.delay_ms, self.window_ms = delay_ms, window_ms
        self.peak_ms, self.rise_ms, self.fall_ms = peak_ms, rise_ms, fall_ms

    @property
    def duration_ms(self):
        return self.window_ms if self.kind == 'x1' else self.rise_ms+self.fall_ms

    @property
    def maximum_delay_ms(self):
        return self.delay_ms if self.kind == 'x1' else self.peak_ms

    @property
    def falling_slope(self):
        """d(delay)/d(time) on the falling edge; the x1 step is drained at -1 by the no-overtaking rule."""
        return -1.0 if self.kind == 'x1' else -self.peak_ms/self.fall_ms

    def delay_seconds(self, elapsed):
        if elapsed < 0:
            return 0.0
        milliseconds = elapsed*1000
        if milliseconds >= self.duration_ms:
            return None
        if self.kind == 'x1':
            return self.delay_ms/1000
        if milliseconds < self.rise_ms:
            return self.peak_ms*milliseconds/self.rise_ms/1000
        return self.peak_ms*(self.duration_ms-milliseconds)/self.fall_ms/1000

    def fields(self):
        if self.kind == 'x1':
            return {'profile': 'x1', 'delay_ms': self.delay_ms, 'window_ms': self.window_ms}
        return {'profile': 'ramp', 'peak_ms': self.peak_ms, 'rise_ms': self.rise_ms, 'fall_ms': self.fall_ms}


def profile_problems(profile, packets_per_second=PACKETS_PER_SECOND, fault_at=()):
    """Parameter checks made before anything runs; an empty list means accepted."""
    problems = []
    if profile.kind not in ('x1', 'ramp'):
        return [f'unknown profile {profile.kind!r}']
    spans = (('window_ms', profile.window_ms),) if profile.kind == 'x1' else \
        (('rise_ms', profile.rise_ms), ('fall_ms', profile.fall_ms))
    delay_name = 'delay_ms' if profile.kind == 'x1' else 'peak_ms'
    if not DELAY_MS[0] <= profile.maximum_delay_ms <= DELAY_MS[1]:
        problems.append(f'{delay_name} {profile.maximum_delay_ms} outside {DELAY_MS[0]}..{DELAY_MS[1]}')
    for name, value in spans:
        if not SPAN_MS[0] <= value <= SPAN_MS[1]:
            problems.append(f'{name} {value} outside {SPAN_MS[0]}..{SPAN_MS[1]}')
    if profile.kind == 'ramp' and profile.fall_ms < profile.peak_ms:
        problems.append(f'falling slope -{profile.peak_ms}/{profile.fall_ms} is below -1 (later datagrams would '
                        'overtake earlier ones); fall_ms must be >= peak_ms')
    if packets_per_second <= 0:
        problems.append('packets_per_second must be positive')
    elif profile.maximum_delay_ms/1000*packets_per_second >= QUEUE_LIMIT:
        problems.append(f'maximum delay {profile.maximum_delay_ms} ms x {packets_per_second} packets/s = '
                        f'{profile.maximum_delay_ms/1000*packets_per_second:g} is not below the queue bound '
                        f'{QUEUE_LIMIT}')
    for at in fault_at:
        if not FAULT_AT[0] <= at <= FAULT_AT[1]:
            problems.append(f'fault_at {at} outside {FAULT_AT[0]}..{FAULT_AT[1]} s')
        end = at+(profile.duration_ms+profile.maximum_delay_ms)/1000
        if end > PROBE_SECONDS-TAIL_SECONDS:
            problems.append(f'fault_at {at} + injection and drain end at {end:g} s, later than '
                            f'{PROBE_SECONDS-TAIL_SECONDS:g} s')
    return problems


def percentiles(values):
    if not values:
        return {'count': 0}
    ordered = sorted(values)

    def rank(p):
        return round(ordered[max(0, math.ceil(p/100*len(ordered))-1)], 3)
    return {'count': len(ordered), 'min': round(ordered[0], 3), 'p50': rank(50), 'p90': rank(90), 'p99': rank(99),
            'max': round(ordered[-1], 3)}


def maximum_window(times_ns, duration=1_000_000_000):
    return action_probe.maximum_window(times_ns, duration)


class DelayLine:
    """The Client->Gateway delay queue and the relay's conservation counters; no sockets, no clock of its own.

    offer() decides the due time of a Client->Gateway datagram received at `now` (None: forward it now, nothing is
    queued and no delay applies); due() pops what is due, in receive order. Callers hold one lock around both.
    """

    def __init__(self, profile):
        self.profile = profile
        self.armed_at = None
        self.queue = []
        self.order = 0
        self.last_due = None
        self.counts = {direction: {'received': 0, 'forwarded': 0} for direction in ('upstream', 'downstream')}
        self.unroutable_downstream = 0
        self.delayed = 0
        self.held_behind = 0
        self.maximum_queue = 0
        self.queue_near_at = None
        self.samples = []           # [elapsed_ms, profile_ms, scheduled_ms, observed_ms] per delayed datagram
        self.lateness_ms = []
        self.sent_ns = defaultdict(list)
        self.received_ns = defaultdict(list)
        self.before = None
        self.release_at = None

    def snapshot(self):
        return {direction: dict(counts) for direction, counts in self.counts.items()} | {
            'queued': len(self.queue), 'unroutable_downstream': self.unroutable_downstream}

    def arm(self, now):
        self.armed_at = now
        self.before = self.snapshot()

    def receive(self, direction, session, now):
        self.counts[direction]['received'] += 1
        if direction == 'upstream':
            self.received_ns[session].append(round(now*1e9))

    def offer(self, item, session, now):
        """Returns None to forward now, else queues item and returns its due time."""
        delay = None
        if self.armed_at is not None:
            delay = self.profile.delay_seconds(now-self.armed_at)
        if not delay and not self.queue:
            return None
        planned = now+(delay or 0.0)
        due = planned if self.last_due is None else max(planned, self.last_due)
        if due > planned:
            self.held_behind += 1
        self.last_due = due
        self.order += 1
        heapq.heappush(self.queue, (due, self.order, now, delay or 0.0, session, item))
        self.delayed += 1
        if len(self.queue) > self.maximum_queue:
            self.maximum_queue = len(self.queue)
        if len(self.queue) >= QUEUE_NEAR and self.queue_near_at is None:
            self.queue_near_at = now
        return due

    def due(self, now):
        """Pops every queued datagram due at `now`, in due (= receive) order: [(item, session, received, due)]."""
        ready = []
        while self.queue and self.queue[0][0] <= now:
            due, _, received, delay, session, item = heapq.heappop(self.queue)
            ready.append((item, session, received, due, delay))
        return ready

    def sent(self, direction, session, received, sent_at, due=None, delay=0.0):
        self.counts[direction]['forwarded'] += 1
        if direction != 'upstream':
            return
        self.sent_ns[session].append(round(sent_at*1e9))
        if due is not None:
            self.lateness_ms.append((sent_at-due)*1000)
            elapsed = (received-self.armed_at)*1000 if self.armed_at is not None else 0.0
            self.samples.append([round(elapsed, 3), round(delay*1000, 3), round((due-received)*1000, 3),
                                 round((sent_at-received)*1000, 3)])

    def maybe_released(self, now):
        """True once the profile has ended and the queue is empty (first time only)."""
        if self.armed_at is None or self.release_at is not None or self.queue:
            return False
        if now-self.armed_at < self.profile.duration_ms/1000:
            return False
        self.release_at = now
        return True

    def evidence(self):
        after = self.snapshot()
        conserved = {name: (state is not None and all(state[d]['received'] == state[d]['forwarded']
                                                      for d in ('upstream', 'downstream')) and state['queued'] == 0)
                     for name, state in (('before', self.before), ('after', after))}
        return {
            'mechanism': MECHANISM, 'direction': DIRECTION, **self.profile.fields(),
            'falling_slope': self.profile.falling_slope, 'maximum_delay_ms': self.profile.maximum_delay_ms,
            'queue_limit': QUEUE_LIMIT, 'queue_near': QUEUE_NEAR, 'maximum_queue': self.maximum_queue,
            'queue_near_reached': self.queue_near_at is not None,
            'counts_before': self.before, 'counts_after': after, 'received_equals_forwarded': conserved,
            'delayed': self.delayed, 'held_behind': self.held_behind,
            'scheduled_delay_ms': percentiles([s[2] for s in self.samples]),
            'observed_delay_ms': percentiles([s[3] for s in self.samples]),
            'lateness_ms': percentiles(self.lateness_ms),
            'profile_excess_ms': percentiles([s[2]-s[1] for s in self.samples]),
            'maximum_received_per_second': {str(s): maximum_window(t) for s, t in self.received_ns.items()},
            'maximum_forwarded_per_second': {str(s): maximum_window(t) for s, t in self.sent_ns.items()},
            'samples_columns': ['elapsed_ms', 'profile_ms', 'scheduled_ms', 'observed_ms'],
            'samples': self.samples}


class DelayRelay(action_probe.ActionRelay):
    """ActionRelay whose only impairment is the DelayLine; PROFILE is set on a per-round subclass (delaying_relay)."""
    PROFILE = None
    CLOCK = staticmethod(time.monotonic)

    def __init__(self, http, udp, mode):
        # Set before ActionRelay.__init__: _ImpairedGateway.__init__ starts the relay thread.
        self._setup_delay()
        super().__init__(http, udp, mode)

    def _setup_delay(self):
        self.line = DelayLine(self.PROFILE)
        self.arm_duration_ms = None

    def arm(self, layer, duration):
        with self.lock:
            self.arm_duration_ms = round(duration*1000)
            self.line.arm(self.CLOCK())
            self.fault.update(start_ns=steady_clock_ns(), release_ns=0, mechanism=MECHANISM, direction=DIRECTION,
                              dispatch_layer=layer, **self.PROFILE.fields())

    def _release(self):
        now = self.CLOCK()
        for (payload, destination), session, received, due, delay in self.line.due(now):
            self._send(payload, destination, True)
            self.line.sent('upstream', session, received, self.CLOCK(), due, delay)
        if self.line.maybe_released(now):
            self.fault['release_ns'] = steady_clock_ns()

    def _receive(self, payload, source):
        # The event and routing of ActionRelay._receive, without any of its drop or hold branches.
        if len(payload) < 24:
            raise ValueError('Truncated product packet')
        now_ns = steady_clock_ns()
        upstream = source != self.upstream_udp
        kind = int.from_bytes(payload[6:8], 'big')
        session = int.from_bytes(payload[8:16], 'big')
        body = payload[24:]
        with self.lock:
            now = self.CLOCK()
            self._release()
            direction = 'upstream' if upstream else 'downstream'
            self.line.receive(direction, session, now)
            if upstream:
                self.clients[session] = source
                destination = self.upstream_udp
            else:
                destination = self.clients.get(session)
            event = {'time_ns': now_ns, 'event': 'received', 'upstream': upstream,
                     'kind': kind, 'session': session, 'bytes': len(payload)}
            if kind == 2:
                self.players[session] = action_probe.scalar(body, 1)
            if kind == 6:
                event['shots'] = [action_probe.scalar(shot, 1) for shot in action_probe.messages(body, 1)]
                event['ack'] = action_probe.scalar(body, 2)
            if kind == 7:
                event['decisions'] = [action_probe.decision(item) for item in action_probe.messages(body, 2)]
                event['retired'] = action_probe.scalar(body, 1)
            self.events.append(event)
            if destination is None:
                # A Gateway datagram for a Session no Client has used yet: counted, and the round is invalid.
                self.line.unroutable_downstream += 1
                self.events.append(dict(event, event='unroutable'))
                return
            if upstream and self.line.offer((payload, destination), session, now) is not None:
                return
            self._send(payload, destination, upstream)
            self.line.sent(direction, session, now, self.CLOCK())

    def evidence(self):
        result = super().evidence()
        with self.lock:
            result['client_delay'] = dict(self.line.evidence(), arm_duration_ms=self.arm_duration_ms)
            result['fault'] = dict(self.fault)
        return result


def delaying_relay(profile):
    return type('DelayRelay', (DelayRelay,), {'PROFILE': profile})


@contextmanager
def replaced_action_relay(profile):
    """Replaces action_probe.ActionRelay inside this process for one round (b1); the files stay unchanged."""
    original = action_probe.ActionRelay
    if original is not FROZEN_ACTION_RELAY:
        raise RuntimeError('action_probe.ActionRelay is not the frozen ActionRelay')
    action_probe.ActionRelay = delaying_relay(profile)
    try:
        yield
    finally:
        action_probe.ActionRelay = original


FROZEN_ACTION_RELAY = action_probe.ActionRelay


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_sha256(path):
    """sha256 of a tracked text file as committed: a Windows checkout may turn its LF line ends into CRLF."""
    return hashlib.sha256(Path(path).read_bytes().replace(b'\r\n', b'\n')).hexdigest()


def read_frozen_list(path):
    """shasum format: '<sha256>  <path relative to the repository root>' per line."""
    entries = {}
    for number, line in enumerate(Path(path).read_text().splitlines(), 1):
        if not line.strip():
            continue
        digest, _, name = line.partition('  ')
        if len(digest) != 64 or not name:
            raise Refused(f'{path}:{number}: not a shasum line')
        entries[name.strip()] = digest
    return entries


def frozen_problems(frozen_list, root=ROOT, modules=(action_probe, impaired_network)):
    problems = []
    entries = read_frozen_list(frozen_list)
    for name, digest in entries.items():
        file = root/name
        if not file.is_file():
            problems.append(f'{name}: listed but missing')
        elif source_sha256(file) != digest:
            problems.append(f'{name}: sha256 {source_sha256(file)} differs from the list {digest}')
    for module in modules:
        path = Path(module.__file__).resolve()
        listed = {str((root/name).resolve()): digest for name, digest in entries.items()}
        if str(path) not in listed:
            problems.append(f'{path}: imported but not in the frozen list')
        actual = source_sha256(path)
        if actual != FROZEN.get(path.name):
            problems.append(f'{path}: sha256 {actual} differs from FROZEN {FROZEN.get(path.name)}')
    return problems


def round_problems(relay, profile):
    """The proof that the replacement took effect and stayed pure, read from relay.json (run_case's evidence)."""
    relay = relay or {}
    delay = relay.get('client_delay')
    if not isinstance(delay, dict):
        return ['relay evidence has no client_delay section (the replacement did not take effect)']
    problems = []
    fault = relay.get('fault') or {}
    if fault.get('mechanism') != MECHANISM or delay.get('mechanism') != MECHANISM:
        problems.append(f'mechanism {fault.get("mechanism")!r}/{delay.get("mechanism")!r}')
    for key, value in profile.fields().items():
        if fault.get(key) != value or delay.get(key) != value:
            problems.append(f'{key} {fault.get(key)!r}/{delay.get(key)!r} differs from {value!r}')
    if delay.get('arm_duration_ms') != profile.duration_ms:
        problems.append(f'armed for {delay.get("arm_duration_ms")} ms, profile lasts {profile.duration_ms} ms')
    start, release = fault.get('start_ns'), fault.get('release_ns')
    if not (isinstance(start, int) and isinstance(release, int) and 0 < start < release):
        problems.append(f'start_ns/release_ns {start}/{release}')
    if not isinstance(delay.get('delayed'), int) or delay['delayed'] < 1:
        problems.append(f'delayed {delay.get("delayed")!r}')
    conserved = delay.get('received_equals_forwarded') or {}
    for name in ('before', 'after'):
        if conserved.get(name) is not True:
            problems.append(f'received != forwarded (or queue not empty) {name} the injection: '
                            f'{delay.get("counts_" + name)}')
    after = delay.get('counts_after') or {}
    if after.get('unroutable_downstream'):
        problems.append(f'unroutable Gateway datagrams {after["unroutable_downstream"]}')
    if relay.get('relay_error') is not None:
        problems.append(f'relay error {relay["relay_error"]}')
    if delay.get('queue_near_reached') or (delay.get('maximum_queue') or 0) >= QUEUE_NEAR:
        problems.append(f'queue reached {delay.get("maximum_queue")} (near the bound {QUEUE_LIMIT})')
    lateness = (delay.get('lateness_ms') or {}).get('p99')
    if lateness is None or lateness > LATENESS_P99_MS:
        problems.append(f'send lateness p99 {lateness} ms exceeds {LATENESS_P99_MS} ms')
    excess = delay.get('profile_excess_ms') or {}
    if excess.get('min', 0) < -0.01 or excess.get('max', 0) > profile.maximum_delay_ms+0.01:
        problems.append(f'scheduled delay departs from the profile: {excess}')
    limited = (relay.get('gateway_counters') or {}).get('rate_limited_packets')
    if limited != 0:
        problems.append(f'Gateway rate rejections {limited} (the released datagrams were not all admitted)')
    return problems


def repository_state():
    def git(*arguments):
        return subprocess.run(['git', '-C', str(ROOT), *arguments], capture_output=True, text=True,
                              check=True).stdout.strip()
    return {'head': git('rev-parse', 'HEAD'),
            'tracked_changes': git('status', '--porcelain', '--untracked-files=no')}


def parse(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    for name in ('match', 'gateway', 'probe', 'arena', 'output', 'frozen_list'):
        parser.add_argument('--'+name.replace('_', '-'), dest=name, type=Path, required=True)
    parser.add_argument('--profile', choices=('x1', 'ramp'), required=True)
    parser.add_argument('--delay-ms', type=int, default=0, help='x1')
    parser.add_argument('--window-ms', type=int, default=0, help='x1')
    parser.add_argument('--peak-ms', type=int, default=0, help='ramp')
    parser.add_argument('--rise-ms', type=int, default=0, help='ramp')
    parser.add_argument('--fall-ms', type=int, default=0, help='ramp')
    parser.add_argument('--fault-at', type=float, action='append', required=True,
                        help='seconds after the probe start; repeat it to alternate across rounds')
    parser.add_argument('--rounds', type=int, default=1)
    parser.add_argument('--fps', type=int, choices=(30, 60), default=60)
    parser.add_argument('--packets-per-second', type=int, default=PACKETS_PER_SECOND)
    args = parser.parse_args(argv)
    given = {'delay_ms': args.delay_ms, 'window_ms': args.window_ms, 'peak_ms': args.peak_ms,
             'rise_ms': args.rise_ms, 'fall_ms': args.fall_ms}
    used = ('delay_ms', 'window_ms') if args.profile == 'x1' else ('peak_ms', 'rise_ms', 'fall_ms')
    stray = [name for name, value in given.items() if value and name not in used]
    if stray:
        parser.error(f'{args.profile} does not take {stray}')
    args.delay = Profile(args.profile, **{name: given[name] for name in used})
    problems = profile_problems(args.delay, args.packets_per_second, args.fault_at)
    if args.rounds < 1:
        problems.append('--rounds must be positive')
    if problems:
        parser.error('; '.join(problems))
    return args


def case_name(profile, index, fault_at):
    shape = (f'x1-{profile.delay_ms}ms-{profile.window_ms}ms' if profile.kind == 'x1' else
             f'ramp-{profile.peak_ms}ms-{profile.rise_ms}up-{profile.fall_ms}down')
    return f'clientdelay-{shape}-at{fault_at:.1f}-r{index}'


def execute(args, case=action_probe.run_case, repository=repository_state):
    """Runs the rounds into a new output directory; returns the summary written to summary.json."""
    problems = frozen_problems(args.frozen_list)
    if problems:
        raise Refused('frozen tools differ: '+'; '.join(problems))
    output = args.output
    if output.exists() and any(output.iterdir()):
        raise Refused(f'{output} is not empty; evidence is never overwritten')
    profile = args.delay
    schedule = [{'round': index, 'fault_at': args.fault_at[(index-1) % len(args.fault_at)]}
                for index in range(1, args.rounds+1)]
    for item in schedule:
        item['case'] = case_name(profile, item['round'], item['fault_at'])
    start_state = repository()
    output.mkdir(parents=True, exist_ok=True)
    plan = {'protocol': PROTOCOL_VERSION, 'b1': 'action_probe.ActionRelay replaced in this process',
            'mechanism': MECHANISM, 'direction': DIRECTION, **profile.fields(),
            'duration_ms': profile.duration_ms, 'maximum_delay_ms': profile.maximum_delay_ms,
            'falling_slope': profile.falling_slope, 'packets_per_second': args.packets_per_second,
            'queue_bound': f'{profile.maximum_delay_ms} ms x {args.packets_per_second}/s = '
                           f'{profile.maximum_delay_ms/1000*args.packets_per_second:g} < {QUEUE_LIMIT}',
            'queue_near': QUEUE_NEAR, 'lateness_p99_ms': LATENESS_P99_MS,
            'gateway_to_client': 'forwarded as received', 'dispatch_mode': DISPATCH_MODE,
            'verdict': 'run_case verdict recorded only', 'fault_at': args.fault_at, 'fps': args.fps,
            'rounds': args.rounds, 'gameplay_v5': True, 'schedule': schedule,
            'frozen': FROZEN, 'frozen_list': {'path': str(args.frozen_list), 'sha256': sha256(args.frozen_list)},
            'tools': {name: source_sha256(HERE/name) for name in RECORDED},
            'artifacts': {name: {'path': str(getattr(args, name)), 'sha256': sha256(getattr(args, name))}
                          for name in ('match', 'gateway', 'probe', 'arena') if Path(getattr(args, name)).is_file()},
            'repository': start_state, 'python': sys.version.split()[0]}
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
                if 'artifacts' not in result:
                    record['status'] = 'runner_error'
                    stopped = f'runner error in round {item["round"]}'
                elif problems:
                    record['status'] = 'injection_invalid'
                    stopped = f'round {item["round"]}: injection invalid: '+'; '.join(problems)
                else:
                    record['status'] = 'completed'
                delay = (relay or {}).get('client_delay') or {}
                record.update(round_problems=problems,
                              client_delay={k: v for k, v in delay.items() if k != 'samples'},
                              fault=(relay or {}).get('fault'),
                              gateway_counters=(relay or {}).get('gateway_counters'),
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
