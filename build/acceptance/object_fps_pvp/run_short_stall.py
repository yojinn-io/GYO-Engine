"""Gateway->Match IPC input-delay rounds for object_fps_pvp movement phase tracking (D48 step 2, D50 option b1).

Injection 'ipc-input-delay' (D50 condition 1): from the arm time (fault_at seconds after the probe's start) for
window_ms, every Gateway->Match byte the relay reads is forwarded delay_ms after it was read. That is ALL
Gateway->Match traffic, not only movement inputs: actions, ACKs and controls share the one IPC stream and are delayed
together. Byte order is kept: bytes read after the window wait behind the delayed ones (a TCP stream cannot overtake).
Match->Gateway (snapshots, results) is never delayed or paused. One injection per round.

Topology (D50 condition 2): the Python relay (backpressure_probe.IpcPause, here its subclass IpcDelay) sits between
the Gateway and the Match for the whole round, because run_case's 'host-ipc' mode starts it before the Gateway
(action_probe.py:531-533). This is not the production topology. Every Gateway->Match byte also passes the relay's
reader->sender thread hand-off, inside and outside the window.

How (option b1): action_probe.py and backpressure_probe.py are imported unchanged. Before anything is written, the
sha256 of the module files actually imported must equal FROZEN, else the session is refused (D50 condition 3). For each
round this process replaces the module attribute action_probe.IpcPause (the class run_case instantiates for
'host-ipc') with IpcDelay and restores it afterwards, then calls the unchanged run_case(options, 'host-ipc', window_ms).
After each round the fault recorded in the case's result.json must carry the delay fields (mechanism, delay_ms,
window_ms, delayed_chunks >= 1, start_ns, release_ns > start_ns): proof that the replacement took effect; otherwise
the round is 'injection_unproven' and the session stops (D50 condition 3).

Verdict (D50 condition 4): run_case's verdict (the host-ipc rules of the frozen analyzers) is recorded only, never
decisive; the judgement is made afterwards by the frozen D48 step 2 measurement script from the traces.

Output (a new or empty directory; evidence is never overwritten): plan.json (parameters, schedule, tool, artifact and
sleeper hashes, repository state), results.jsonl (one line per executed round, written when it ends), idle_gate.jsonl
(with --idle-gate), summary.json, and one case directory per round written by run_case. Round k uses
fault_at[(k-1) % len(fault_at)]. Failed rounds are kept; nothing is rerun. The session stops (remaining rounds
'not_run') on: a runner error (run_case raised: no artifacts), injection_unproven, a changed repository (HEAD or
tracked files), a sleeper failure, the idle gate not passing within 30 minutes, or SIGINT/SIGTERM (after the current
round). Exit status 0 only when every round completed.
"""
import argparse
from collections import deque
from contextlib import contextmanager
import hashlib
import json
from pathlib import Path
import signal
import subprocess
import sys
import threading
import time
from types import SimpleNamespace

import action_probe
import backpressure_probe
from acceptance_util import PROTOCOL_VERSION
from run_network import steady_clock_ns

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
# D50 condition 3: the frozen files (equal to every list since C1; pvp-v7-batch09-s1-20261010/analyzers.sha256).
FROZEN = {'action_probe.py': '59002b4339df7856d387b6872a7436c3cf55b817c1874bc9d812cacb65a77906',
          'backpressure_probe.py': 'c88ceb8908b74fc8909b41d01660ddc09efc485491ca5f27b07b4f7b19f3b7e9'}
# Recorded in plan.json (not checked): the other modules run_case uses.
RECORDED = ('run_short_stall.py', 'action_probe.py', 'backpressure_probe.py', 'gameplay_evidence.py',
            'command_evidence.py', 'acceptance_util.py', 'run_network.py', 'impaired_network.py')
MECHANISM = 'ipc-input-delay'
DELAY_MS = (30, 150)
WINDOW_MS = (30, 150)
# The v5 gameplay probe runs 16 s; keep the fault and 3 s after it inside the trace.
FAULT_AT = (1.0, 12.0)
SLEEPER_SECONDS = 5
# Idle gate of C1 session 2 (as S1): three consecutive 5 s sleepers, each maximum < 10 ms, retried every 30 s for up
# to 30 minutes.
IDLE_GATE = {'checks': 3, 'max_ms_below': 10.0, 'retry_seconds': 30, 'limit_seconds': 1800}
STATUSES = ('completed', 'runner_error', 'injection_unproven', 'not_run')


class Refused(Exception):
    """The session did not start; nothing was written."""


class IpcDelay(backpressure_probe.IpcPause):
    """IpcPause whose arm() delays the Gateway->Match stream instead of pausing Match->Gateway.

    DELAY_MS is set on a per-round subclass (delaying_relay), because run_case constructs the relay with the port only.
    stats (run_case returns them as the case's fault): start_ns = arm, window_end_ns = arm + window (planned end, steady
    clock), last_delayed_sent_ns = when the last delayed chunk was written to the Match, release_ns = the first chunk
    forwarded undelayed after the window (or the first idle check after the window with nothing queued).
    """
    DELAY_MS = None

    def __init__(self, upstream_port):
        self.window_until = None
        self.line = deque()
        self.ready = threading.Condition()
        super().__init__(upstream_port)

    def arm(self, duration):
        # The base class would pause Match->Gateway (self.duration); this class never sets it.
        with self.lock:
            start = steady_clock_ns()
            self.window_until = time.monotonic()+duration
            self.stats.update(mechanism=MECHANISM, direction='gateway->match', start_ns=start,
                              window_end_ns=start+round(duration*1e9), delay_ms=self.DELAY_MS,
                              window_ms=round(duration*1000), delayed_chunks=0, delayed_bytes=0,
                              last_delayed_sent_ns=0, maximum_send_lateness_ms=0.0)

    def _commands(self, source, destination):
        sender = threading.Thread(target=self._sender, args=(destination,), name='pvp-ipc-delay', daemon=True)
        self.workers.append(sender)
        sender.start()
        last = 0.0
        try:
            while not self.stop.is_set():
                chunk = source.recv(65536)
                if not chunk:
                    break
                now = time.monotonic()
                with self.lock:
                    inside = self.window_until is not None and now < self.window_until
                    if inside:
                        self.stats['delayed_chunks'] += 1
                        self.stats['delayed_bytes'] += len(chunk)
                due = max(now+(self.DELAY_MS/1000 if inside else 0), last)
                last = due
                with self.ready:
                    self.line.append((due, chunk, inside))
                    self.ready.notify()
        except OSError as failure:
            if not self.stop.is_set():
                with self.lock:
                    self.stats['relay_error'] = str(failure)
        finally:
            with self.ready:
                self.line.append(None)
                self.ready.notify()

    def _sender(self, destination):
        try:
            while True:
                with self.ready:
                    while not self.line:
                        self.ready.wait(.005)
                        self._maybe_release(idle=True)
                    item = self.line.popleft()
                if item is None:
                    return
                due, chunk, inside = item
                pause = due-time.monotonic()
                if pause > 0:
                    time.sleep(pause)
                destination.sendall(chunk)
                if inside:
                    with self.lock:
                        self.stats['last_delayed_sent_ns'] = steady_clock_ns()
                        lateness = max(0.0, (time.monotonic()-due)*1000)
                        self.stats['maximum_send_lateness_ms'] = round(max(self.stats['maximum_send_lateness_ms'],
                                                                           lateness), 3)
                else:
                    self._maybe_release(idle=False)
        except OSError as failure:
            if not self.stop.is_set():
                with self.lock:
                    self.stats['relay_error'] = str(failure)

    def _maybe_release(self, idle):
        with self.lock:
            if self.window_until is None or time.monotonic() < self.window_until or self.stats['release_ns']:
                return
            if idle and self.line:
                return
            self.stats['release_ns'] = steady_clock_ns()
        self.released.set()


def delaying_relay(delay_ms):
    return type('IpcDelay', (IpcDelay,), {'DELAY_MS': delay_ms})


@contextmanager
def replaced_ipc_relay(delay_ms):
    """Replaces action_probe.IpcPause inside this process for one round (b1); the files stay unchanged."""
    original = action_probe.IpcPause
    if original is not backpressure_probe.IpcPause:
        raise RuntimeError('action_probe.IpcPause is not the frozen backpressure_probe.IpcPause')
    action_probe.IpcPause = delaying_relay(delay_ms)
    try:
        yield
    finally:
        action_probe.IpcPause = original


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def frozen_problems():
    problems = []
    for module in (action_probe, backpressure_probe):
        path = Path(module.__file__).resolve()
        expected, actual = FROZEN.get(path.name), sha256(path)
        if actual != expected:
            problems.append(f'{path}: sha256 {actual} differs from frozen {expected}')
    return problems


def delay_problems(fault, delay_ms, window_ms):
    """D50 condition 3: the delay fields that prove the replacement took effect."""
    fault = fault or {}
    problems = []
    if fault.get('mechanism') != MECHANISM:
        problems.append(f'mechanism {fault.get("mechanism")!r}')
    if fault.get('delay_ms') != delay_ms or fault.get('window_ms') != window_ms:
        problems.append(f'delay_ms/window_ms {fault.get("delay_ms")}/{fault.get("window_ms")} '
                        f'differ from {delay_ms}/{window_ms}')
    if not isinstance(fault.get('delayed_chunks'), int) or fault['delayed_chunks'] < 1:
        problems.append(f'delayed_chunks {fault.get("delayed_chunks")!r}')
    start, release = fault.get('start_ns'), fault.get('release_ns')
    if not (isinstance(start, int) and isinstance(release, int) and 0 < start < release):
        problems.append(f'start_ns/release_ns {start}/{release}')
    return problems


def repository_state():
    def git(*arguments):
        return subprocess.run(['git', '-C', str(ROOT), *arguments], capture_output=True, text=True,
                              check=True).stdout.strip()
    return {'head': git('rev-parse', 'HEAD'),
            'tracked_changes': git('status', '--porcelain', '--untracked-files=no')}


def host_sample(sleeper, label):
    """One 5 s sleeper (absolute 1/60 s deadlines) and the five busiest processes, as C1 and S1."""
    out = subprocess.run([sys.executable, str(sleeper), str(SLEEPER_SECONDS)], capture_output=True, text=True,
                         timeout=60, check=True)
    top = subprocess.run(['ps', '-Ao', 'pcpu,comm', '-r'], capture_output=True, text=True).stdout.splitlines()[1:6]
    return {'when': label, **json.loads(out.stdout), 'top_cpu': [line.strip()[:120] for line in top]}


def idle_gate(sample, log, sleep=time.sleep, clock=time.monotonic):
    deadline = clock()+IDLE_GATE['limit_seconds']
    while True:
        checks = [sample('gate') for _ in range(IDLE_GATE['checks'])]
        passed = all(check['max_ms'] < IDLE_GATE['max_ms_below'] for check in checks)
        log({'checks': checks, 'passed': passed})
        if passed:
            return True
        if clock()+IDLE_GATE['retry_seconds'] >= deadline:
            return False
        sleep(IDLE_GATE['retry_seconds'])


def bounded(parser, name, value, bounds, unit):
    if not bounds[0] <= value <= bounds[1]:
        parser.error(f'{name} must be within {bounds[0]}..{bounds[1]} {unit}')


def parse(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    for name in ('match', 'gateway', 'probe', 'arena', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--delay-ms', type=int, required=True)
    parser.add_argument('--window-ms', type=int, required=True)
    parser.add_argument('--fault-at', type=float, action='append', required=True,
                        help='seconds after the probe start; repeat it to alternate across rounds')
    parser.add_argument('--rounds', type=int, default=1)
    parser.add_argument('--fps', type=int, choices=(30, 60), default=60)
    parser.add_argument('--sleeper', type=Path, help='5 s sleeper before and after every round (C1 sleeper.py)')
    parser.add_argument('--idle-gate', action='store_true', help='C1 session 2 idle gate before round 1 (needs --sleeper)')
    args = parser.parse_args(argv)
    bounded(parser, '--delay-ms', args.delay_ms, DELAY_MS, 'ms')
    bounded(parser, '--window-ms', args.window_ms, WINDOW_MS, 'ms')
    for fault_at in args.fault_at:
        bounded(parser, '--fault-at', fault_at, FAULT_AT, 's')
    if args.rounds < 1:
        parser.error('--rounds must be positive')
    if args.idle_gate and args.sleeper is None:
        parser.error('--idle-gate needs --sleeper')
    return args


def case_name(args, index, fault_at):
    return f'ipcdelay{args.delay_ms}-window{args.window_ms}-at{fault_at:.1f}-r{index}'


def execute(args, case=action_probe.run_case, sample=None, repository=repository_state, sleep=time.sleep,
            clock=time.monotonic):
    """Runs the rounds into a new output directory; returns the summary written to summary.json."""
    problems = frozen_problems()
    if problems:
        raise Refused('frozen tools differ (D50 condition 3): '+'; '.join(problems))
    output = args.output
    if output.exists() and any(output.iterdir()):
        raise Refused(f'{output} is not empty; evidence is never overwritten')
    if sample is None and args.sleeper is not None:
        sample = lambda label: host_sample(args.sleeper, label)  # noqa: E731
    schedule = [{'round': index, 'fault_at': args.fault_at[(index-1) % len(args.fault_at)]}
                for index in range(1, args.rounds+1)]
    for item in schedule:
        item['case'] = case_name(args, item['round'], item['fault_at'])
    start_state = repository()
    output.mkdir(parents=True, exist_ok=True)
    plan = {'protocol': PROTOCOL_VERSION, 'd50': 'b1', 'mechanism': MECHANISM, 'direction': 'gateway->match',
            'delayed_traffic': 'all Gateway->Match IPC bytes: movement inputs, actions, ACKs, controls',
            'match_to_gateway': 'never delayed or paused',
            'relay': 'Python relay (IpcPause subclass) between Gateway and Match for the whole round '
                     '(action_probe.py:531-533); not the production topology',
            'verdict': 'run_case verdict recorded only; judgement by the frozen measurement script',
            'delay_ms': args.delay_ms, 'window_ms': args.window_ms, 'fault_at': args.fault_at, 'fps': args.fps,
            'rounds': args.rounds, 'gameplay_v5': True, 'schedule': schedule,
            'frozen': FROZEN, 'tools': {name: sha256(HERE/name) for name in RECORDED},
            'artifacts': {name: {'path': str(getattr(args, name)), 'sha256': sha256(getattr(args, name))}
                          for name in ('match', 'gateway', 'probe', 'arena')
                          if Path(getattr(args, name)).is_file()},
            'sleeper': {'path': str(args.sleeper), 'sha256': sha256(args.sleeper),
                        'seconds': SLEEPER_SECONDS} if args.sleeper else None,
            'idle_gate': IDLE_GATE if args.idle_gate else None,
            'repository': start_state, 'python': sys.version.split()[0]}
    (output/'plan.json').write_text(json.dumps(plan, indent=2)+'\n')
    stop, rounds, gate = [], [], []
    stopped = None

    def finish_current_round(_signal, _frame):
        stop.append(True)

    previous = [signal.signal(s, finish_current_round) for s in (signal.SIGINT, signal.SIGTERM)]
    try:
        if args.idle_gate:
            def log_gate(record):
                gate.append(record)
                with (output/'idle_gate.jsonl').open('a') as log:
                    log.write(json.dumps(record)+'\n')
            try:
                if not idle_gate(sample, log_gate, sleep, clock):
                    stopped = 'idle gate did not pass within 30 minutes'
            except Exception as failure:  # noqa: BLE001 - a sleeper failure stops the session
                stopped = f'idle gate sleeper failed: {type(failure).__name__}: {failure}'
        for item in schedule:
            if stopped or stop:
                rounds.append(dict(item, status='not_run'))
                continue
            state = repository()
            if state != start_state:
                stopped = f'repository changed before round {item["round"]}: {state}'
                rounds.append(dict(item, status='not_run'))
                continue
            record = dict(item, sleeper=[])
            try:
                if sample:
                    record['sleeper'].append(sample('before'))
                options = SimpleNamespace(match=args.match, gateway=args.gateway, probe=args.probe,
                                          arena=args.arena, output=output, gameplay_v5=True,
                                          case_name=item['case'], fps=args.fps, fault_at=item['fault_at'])
                with replaced_ipc_relay(args.delay_ms):
                    result = case(options, 'host-ipc', args.window_ms)
                # The proof is read from the evidence on disk (the case's result.json), which the measurement reads.
                written = output/item['case']/'result.json'
                fault = json.loads(written.read_text()).get('fault') if written.is_file() else None
                missing = delay_problems(fault, args.delay_ms, args.window_ms)
                if 'artifacts' not in result:
                    record['status'] = 'runner_error'
                    stopped = f'runner error in round {item["round"]}'
                elif missing:
                    record['status'] = 'injection_unproven'
                    stopped = f'round {item["round"]}: delay fields missing (D50 condition 3): '+'; '.join(missing)
                else:
                    record['status'] = 'completed'
                record.update(fault=fault, delay_problems=missing,
                              run_case={'passed': result.get('passed'), 'errors': result.get('errors', [])[:8],
                                        'recorded_only': True})
                if sample:
                    record['sleeper'].append(sample('after'))
            except Exception as failure:  # noqa: BLE001 - sleeper or relay replacement failure: stop
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
    summary = {'plan': plan, 'idle_gate': gate if args.idle_gate else None, 'rounds': rounds,
               'interrupted': bool(stop), 'stopped': stopped, 'repository_end': repository(),
               'completed': all(r['status'] == 'completed' for r in rounds)}
    (output/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    return summary


def main(argv=None):
    args = parse(argv)
    for name in ('match', 'gateway', 'probe', 'arena') + (('sleeper',) if args.sleeper else ()):
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
