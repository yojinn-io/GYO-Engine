"""Short product-owned action transport matrix: real Match, Go Gateway and ClientConnection.

Only this acceptance relay injects faults. UDP blockage means a bounded relay
holds/drops datagrams; TCP blockage preserves a partially delivered frame. These
cases do not claim that a kernel send buffer was saturated.
"""
import argparse
from collections import Counter, defaultdict, deque
import hashlib
import heapq
import json
from pathlib import Path
import re
import random
import select
import signal
import socket
import struct
import subprocess
import time
import urllib.request

from backpressure_probe import IpcPause, analyze as analyze_recovery
from command_evidence import analyze_commands
from impaired_network import _ImpairedGateway
from run_network import free_port
from action_evidence import frames as action_frames, records, analyze_legal


def varint(value):
    result = bytearray()
    while value > 127:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def fields(payload):
    """Inspect the declared protobuf wire types without generating game state."""
    offset = 0
    def read_int():
        nonlocal offset
        value = 0
        for shift in range(0, 70, 7):
            if offset >= len(payload):
                raise ValueError('Truncated protobuf varint')
            byte = payload[offset]
            offset += 1
            value |= (byte & 127) << shift
            if byte < 128:
                return value
        raise ValueError('Oversized protobuf varint')
    result = []
    while offset < len(payload):
        tag = read_int()
        number, wire = tag >> 3, tag & 7
        if number == 0:
            raise ValueError('Zero protobuf field')
        if wire == 0:
            value = read_int()
        elif wire in (1, 2, 5):
            length = read_int() if wire == 2 else (8 if wire == 1 else 4)
            if offset + length > len(payload):
                raise ValueError('Truncated protobuf field')
            value, offset = payload[offset:offset+length], offset+length
        else:
            raise ValueError('Unsupported protobuf wire type')
        result.append((number, wire, value))
    return result


def scalar(payload, field, default=0):
    return next((value for number, wire, value in fields(payload) if number == field and wire == 0), default)


def messages(payload, field):
    return [value for number, wire, value in fields(payload) if number == field and wire == 2]


def encoded(items):
    result = bytearray()
    for number, wire, value in items:
        result += varint(number*8+wire)
        if wire == 0:
            result += varint(value)
        else:
            if wire == 2:
                result += varint(len(value))
            result += value
    return bytes(result)


def packet_payload(packet, payload):
    return packet[:20]+len(payload).to_bytes(2, 'big')+packet[22:24]+payload


def decision(payload):
    names = ('action_id', 'resolved_tick', 'accepted', 'rejection', 'hit_kind', 'target_id', 'damage')
    result = {name: scalar(payload, field) for field, name in enumerate(names, 1)}
    result['accepted'] = bool(result['accepted'])
    if scalar(payload, 8):
        result['action_kind'] = scalar(payload, 8)-1
        result['life_generation'] = scalar(payload, 9)
        result['target_life_generation'] = scalar(payload, 10)
        result['rejection'] = {4: 5, 5: 6, 6: 4}.get(result['rejection'], result['rejection'])
    return result


class ActionRelay(_ImpairedGateway):
    def __init__(self, http, udp, mode):
        self.mode = mode
        self.events = []
        self.sequences = defaultdict(int)
        self.once = set()
        self.players = {}
        self.held = {}
        self.block_until = None
        self.armed_duration = None
        self.block_layer = None
        self.network_queue = []
        self.network_random = {}
        self.network_counts = Counter()
        self.network_until = None
        self.cross_session = None
        self.cross_until = None
        self.network_order = 0
        self.network_maximum_scheduled_delay_ms = 0
        self.fault = {'start_ns': 0, 'release_ns': 0, 'maximum_held_datagrams': 0,
                      'kernel_socket_saturation_proven': False}
        super().__init__(http, udp, 0)

    def arm(self, layer, duration):
        with self.lock:
            self.block_layer = layer
            if layer in ('upstream', 'socket-path'):
                # Start exactly at an observed real request, so a 250ms hold
                # demonstrably delays its original reference beyond250ms.
                self.armed_duration = duration
            else:
                self.block_until = time.monotonic()+duration
                self.fault['start_ns'] = time.monotonic_ns()

    def _send(self, payload, destination, upstream, label='forwarded'):
        if upstream and self.mode == 'duplicate-reorder-conflict':
            session = int.from_bytes(payload[8:16], 'big')
            self.sequences[session] += 1
            payload = payload[:16]+self.sequences[session].to_bytes(4, 'big')+payload[20:]
        self.udp.sendto(payload, destination)
        self.events.append({'time_ns': time.monotonic_ns(), 'event': label,
                            'upstream': upstream, 'kind': int.from_bytes(payload[6:8], 'big'),
                            'session': int.from_bytes(payload[8:16], 'big'), 'bytes': len(payload)})

    def _release(self):
        now = time.monotonic()
        if self.cross_until is not None and now >= self.cross_until:
            self.cross_until = None
            self.fault['release_ns'] = time.monotonic_ns()
            held = self.held.pop(('cross-life-result', self.cross_session), None)
            if held:
                self._send(*held, 'cross_life_result_released')
        while self.network_queue and self.network_queue[0][0] <= now:
            _, _, payload, destination, upstream = heapq.heappop(self.network_queue)
            self._send(payload, destination, upstream, 'network_forwarded')
        if self.network_until is not None and now >= self.network_until and not self.network_queue and not self.fault['release_ns']:
            self.fault['release_ns'] = time.monotonic_ns()
        if self.block_until is not None and time.monotonic() >= self.block_until:
            self.fault['release_ns'] = time.monotonic_ns()
            self.block_until = None
            for payload, destination, upstream in self.held.values():
                self._send(payload, destination, upstream, 'released')
            self.held.clear()

    def _receive(self, payload, source):
        if len(payload) < 24:
            raise ValueError('Truncated product packet')
        now = time.monotonic_ns()
        upstream = source != self.upstream_udp
        kind = int.from_bytes(payload[6:8], 'big')
        session = int.from_bytes(payload[8:16], 'big')
        body = payload[24:]
        with self.lock:
            self._release()
            if self.armed_duration is not None and upstream and kind == 6 and messages(body, 1):
                self.block_until = time.monotonic()+self.armed_duration
                self.armed_duration = None
                self.fault['start_ns'] = now
            if upstream:
                self.clients[session] = source
                destination = self.upstream_udp
            else:
                destination = self.clients.get(session)
                if destination is None:
                    return
            event = {'time_ns': now, 'event': 'received', 'upstream': upstream,
                     'kind': kind, 'session': session, 'bytes': len(payload)}
            if kind == 2:
                self.players[session] = scalar(body, 1)
            if kind == 6:
                event['shots'] = [scalar(shot, 1) for shot in messages(body, 1)]
                event['ack'] = scalar(body, 2)
            if kind == 7:
                event['decisions'] = [decision(item) for item in messages(body, 2)]
                event['retired'] = scalar(body, 1)
            self.events.append(event)
            if self.mode == 'cross-life':
                if self.cross_session is None and kind == 7 and any(d.get('action_kind') == 1 and d['action_id'] == 2 and d['accepted'] for d in event.get('decisions', [])):
                    self.cross_session = session
                    self.cross_until = time.monotonic()+4.5
                    self.fault['start_ns'] = now
                if session == self.cross_session and self.cross_until is not None:
                    if kind == 7:
                        self.held.setdefault(('cross-life-result', session), (payload, destination, upstream))
                        self.events.append(dict(event, event='cross_life_result_held'))
                        return
                    if upstream and kind == 6 and 3 in event.get('shots', []):
                        # Keep every retry's immutable ID3/life1 withheld until
                        # life2. Later IDs still exercise a real receipt hole.
                        filtered = [(n,w,v) for n,w,v in fields(body) if not (n == 1 and w == 2 and scalar(v,1) == 3)]
                        self.events.append(dict(event, event='cross_life_request_held'))
                        if not any(n == 1 for n,_,_ in filtered) and not scalar(encoded(filtered),2):
                            return
                        payload = packet_payload(payload, encoded(filtered))
            if self.mode.startswith('network') and kind in (3, 4, 6, 7):
                if self.network_until is None:
                    self.network_until = time.monotonic()+4
                    self.fault['start_ns'] = now
                if time.monotonic() < self.network_until:
                    identity = (session, upstream)
                    generator = self.network_random.setdefault(identity, random.Random(0x47594F+len(self.network_random)))
                    rtt = int(self.mode.removeprefix('network'))
                    if generator.random() < .05:
                        self.network_counts['random_dropped'] += 1
                        self.network_counts['random_dropped_kind_'+str(kind)] += 1
                        self.events.append(dict(event, event='dropped_random'))
                        return
                    delay_ms = max(0, rtt/2+generator.uniform(-10, 10))
                    self.network_maximum_scheduled_delay_ms = max(self.network_maximum_scheduled_delay_ms, delay_ms)
                    self.network_order += 1
                    heapq.heappush(self.network_queue, (time.monotonic()+delay_ms/1000, self.network_order, payload, destination, upstream))
                    self.network_counts['delayed'] += 1
                    if len(self.network_queue) > 256:
                        raise ValueError('Network acceptance relay queue overflow')
                    return
            if self.mode == 'burst2' and upstream and kind == 6 and event.get('shots'):
                identity = ('burst', session)
                if self.network_counts[identity] < 2:
                    self.network_counts[identity] += 1
                    self.events.append(dict(event, event='dropped_burst'))
                    return
            if self.block_until is not None and (
                    self.block_layer == 'socket-path' or self.block_layer == ('upstream' if upstream else 'downstream')):
                # Retain the FIRST action/result to prove old requests age, while
                # keeping only latest state/command/keepalive per Session/type.
                key = (upstream, session, kind)
                if kind not in (6, 7) or key not in self.held:
                    self.held[key] = (payload, destination, upstream)
                self.fault['maximum_held_datagrams'] = max(self.fault['maximum_held_datagrams'], len(self.held))
                return
            loss = ('shot' if kind == 6 and event.get('shots') else
                    'ack' if kind == 6 and event.get('ack') else 'result' if kind == 7 and event.get('decisions') else None)
            # ACK may piggyback new shots; inspect its floor before choosing loss.
            if self.mode == 'loss-ack' and kind == 6 and event.get('ack'):
                loss = 'ack'
            if self.mode == 'loss-'+str(loss) and (session, loss) not in self.once:
                self.once.add((session, loss))
                self.events.append(dict(event, event='dropped_'+loss))
                return
            if self.mode == 'duplicate-reorder-conflict' and upstream and kind == 6 and event.get('shots') and (session, 'injected') not in self.once:
                self.once.add((session, 'injected'))
                self._send(payload, destination, upstream)
                self._send(payload, destination, upstream, 'duplicate_action')
                original = messages(body, 1)[0]
                conflict = [(n, w, v) for n, w, v in fields(original) if n != 3]
                conflict.append((3, 5, struct.pack('<f', .7)))
                later = [(n, w, scalar(original, 1)+31 if n == 1 else v) for n, w, v in fields(original)]
                malformed = encoded([(1, 2, encoded(conflict)), (1, 2, encoded(later))])
                self._send(packet_payload(payload, malformed), destination, upstream, 'conflicting_atomic_batch')
                for number, value, label in ((6, scalar(original, 6)+1, 'conflicting_life'), (5, 2, 'conflicting_kind')):
                    altered = [(n, w, v) for n, w, v in fields(original) if n != number and not (number == 5 and n in (3, 4))]
                    altered.append((number, 0, value))
                    self._send(packet_payload(payload, encoded([(1, 2, encoded(altered))])), destination, upstream, label)
                # A well-framed v5 datagram with malformed protobuf cannot become
                # a shot or damage event and must not disconnect a valid Session.
                self._send(packet_payload(payload, b'\x0a\x7f\x08'), destination, upstream, 'malformed_action')
                self.held[('reorder', session)] = (payload, destination, upstream)
                return
            if self.mode == 'duplicate-reorder-conflict' and kind == 6 and ('reorder', session) in self.held:
                self._send(payload, destination, upstream)
                old, peer, direction = self.held.pop(('reorder', session))
                self._send(old, peer, direction, 'reordered_old_action')
                return
            self._send(payload, destination, upstream)

    def _relay(self):
        try:
            while not self.stop.is_set():
                with self.lock:
                    self._release()
                readable, _, _ = select.select([self.udp], [], [], .002)
                if readable:
                    self._receive(*self.udp.recvfrom(65535))
        except Exception as failure:
            self._fail(str(failure))

    def evidence(self):
        with self.lock:
            return {'fault': dict(self.fault), 'events': list(self.events),
                    'players': dict(self.players), 'relay_error': self.error,
                    'network': {'configured_rtt_ms': int(self.mode[7:]) if self.mode.startswith('network') else 0,
                        'one_way_jitter_ms': 10 if self.mode.startswith('network') else 0,
                        'random_loss_probability': .05 if self.mode.startswith('network') else 0,
                        'maximum_scheduled_delay_ms': self.network_maximum_scheduled_delay_ms,
                        'random_dropped': self.network_counts['random_dropped'],
                        'random_dropped_by_kind': {str(kind): self.network_counts['random_dropped_kind_'+str(kind)] for kind in (3, 4, 6, 7)},
                        'delayed': self.network_counts['delayed'], 'remaining_queue': len(self.network_queue)}}


def maximum_window(timestamps, duration=1_000_000_000):
    pending = deque()
    maximum = 0
    for timestamp in sorted(timestamps):
        while pending and timestamp-pending[0] >= duration:
            pending.popleft()
        pending.append(timestamp)
        maximum = max(maximum, len(pending))
    return maximum


def analyze(output, relay, fault, mode):
    errors = []
    def check(condition, message):
        if not condition:
            errors.append(message)
    client = json.loads((output/'action-client.json').read_text())
    if client.get('gameplay_v5'):
        from gameplay_evidence import analyze as analyze_gameplay
        return analyze_gameplay(output, relay, fault, mode)
    events = list(records(output/'actions.jsonl', legacy=not (output/'action-frames.jsonl').exists()))
    frames = list(action_frames(output))
    if mode == 'burst2':
        dropped = [event for event in relay['events'] if event['event'] == 'dropped_burst']
        if dropped:
            # Fault release is exactly the last deliberately discarded packet;
            # derive it from retained wire evidence, including older probe runs.
            fault = dict(fault, start_ns=min(e['time_ns'] for e in dropped),
                         release_ns=max(e['time_ns'] for e in dropped))
    check(client['passed'], 'Client did not consume/retire every submitted action')
    check(relay['relay_error'] is None, 'Relay failed: '+str(relay['relay_error']))
    gateway_counters = relay.get('gateway_counters')
    rate_policy = 'zero rate-limit rejections; accepted fixed-window count <=120'
    if gateway_counters is not None:
        check(gateway_counters['max_session_window_packets'] <= 120, 'Gateway actual Session counting exceeded120')
        if mode == 'gateway':
            rate_policy = ('Intentionally stopped Gateway may reject queued arrival bursts; accepted fixed-window count <=120; '
                           'all original decisions, effects and1.5s recovery requirements remain enforced')
        else:
            check(gateway_counters['rate_limited_packets'] == 0, 'Gateway actual Session rate limiter discarded packets')
    check(max((e['bytes'] for e in relay['events']), default=0) <= 1200, 'UDP exceeds 1200 bytes')
    check(max(client['maximum_retained']) <= 32 and max(client['maximum_unconsumed']) <= 32, 'Action window exceeds32')
    if mode in ('upstream', 'downstream', 'socket-path', 'gateway', 'host-ipc', 'drain-stall'):
        check(fault['start_ns'] > 0 and fault['release_ns'] > fault['start_ns'], 'Requested fault was not applied/released')
    originals, repeated, accepted = {}, 0, 0
    total_damage = Counter()
    for event in relay['events']:
        if event['event'] != 'received' or event['kind'] != 7:
            continue
        check(len(event['decisions']) <= 8, 'Result batch exceeds8')
        for d in event['decisions']:
            identity = (str(event['session']), d['action_id'])
            if identity in originals:
                repeated += 1
                check(originals[identity] == d, 'Repeated result mutated original decision '+str(identity))
            else:
                originals[identity] = d
                if d['accepted']:
                    accepted += 1
                    total_damage[d['target_id']] += d['damage']
    for final in client['clients']:
        for combat in final['combat']:
            check(combat['hp'] == final['maximum_hp']-total_damage[combat['player_id']], 'HP differs from sum of unique decision effects')
    hp_observations = 0
    for frame in frames:
        for index, suffix in enumerate(('a', 'b')):
            tick = frame['snapshot_tick_'+suffix]
            for combat in frame['combat_'+suffix]:
                expected = client['clients'][index]['maximum_hp']-sum(d['damage'] for d in originals.values()
                    if d['resolved_tick'] <= tick and d['target_id'] == combat['player_id'])
                check(combat['hp'] == expected, 'Snapshot combat effect differs from unique original decisions at tick'+str(tick))
                hp_observations += 1
    check(accepted > 0 and sum(total_damage.values()) > 0, 'No authoritative damaging shots measured')
    check(len(originals) == sum(c['decisions'] for c in client['clients']), 'Wire decisions differ from delivered unique decisions')
    for event in events:
        if event['kind'] != 'decision':
            continue
        sessions = [s for s, player in relay['players'].items() if player == event['player_id']]
        check(len(sessions) == 1 and originals.get((str(sessions[0]), event['decision']['action_id'])) == event['decision'], 'Application decision differs from original wire result')
    rate = {}
    for session, player in relay['players'].items():
        sent = [e for e in relay['events'] if e['upstream'] and e['event'] != 'received' and not e['event'].startswith('dropped_') and str(e['session']) == str(session)]
        received = [e for e in relay['events'] if e['upstream'] and e['event'] == 'received' and str(e['session']) == str(session)]
        maximum = maximum_window([e['time_ns'] for e in sent])
        # Every possible fixed one-second limiter window is bounded by this
        # stricter sliding-window count; Hello and deliberate bad packets count.
        check(maximum <= 120, 'Authenticated traffic exceeds120 packets per second')
        counts = Counter(e['kind'] for e in received)
        check(counts[1] >= 4 and counts[3] >= 180 and counts[6] >= (10 if client.get('legal_shots') else 90), 'Concurrent Hello/movement/action traffic not exercised')
        action_max = maximum_window([e['time_ns'] for e in received if e['kind'] == 6])
        check(action_max <= 31, 'Client action/ACK schedule exceeds30Hz (one boundary packet allowed)')
        result_max = maximum_window([e['time_ns'] for e in relay['events'] if not e['upstream'] and e['event'] == 'received'
                                     and e['kind'] == 7 and str(e['session']) == str(session)])
        check(result_max <= 31, 'Gateway result schedule exceeds30Hz (one boundary packet allowed)')
        rate[str(player)] = {'maximum_any_1s_authenticated_packets': maximum,
                            'maximum_any_1s_action_ack_packets': action_max, 'maximum_any_1s_result_packets': result_max,
                            'received_kind_counts': dict(counts)}
    recovery = None
    fresh = {}
    expired = []
    outstanding_after_release = None
    if fault['start_ns'] and fault['release_ns']:
        try:
            recovery = analyze_recovery(output, fault)
        except (AssertionError, ValueError) as failure:
            errors.append('Movement/authority recovery: '+str(failure))
        submitted = {(e['player_id'], e['request']['action_id']): e for e in events if e['kind'] == 'submitted'}
        delivered = {(e['player_id'], e['decision']['action_id']): e['time_ns'] for e in events if e['kind'] == 'decision'}
        pending = [identity for identity, event in submitted.items() if event['time_ns'] <= fault['release_ns'] and
                   delivered.get(identity, 10**30) > fault['release_ns']]
        pending_delay = max(((delivered.get(identity, 10**30)-fault['release_ns'])/1e9 for identity in pending), default=0)
        check(pending_delay <= 1.5, 'Previously submitted decisions did not all converge within1.5s after release')
        outstanding_after_release = {'pending_at_release': len(pending), 'all_delivered_after_release_seconds': pending_delay}
        for player in client['player_ids']:
            choices = [e for e in events if e['kind'] == 'decision' and e['player_id'] == player and
                       submitted[(player, e['decision']['action_id'])]['time_ns'] >= fault['release_ns']]
            delay = (min((e['time_ns'] for e in choices), default=fault['release_ns']+10**12)-fault['release_ns'])/1e9
            fresh[str(player)] = delay
            check(delay <= 1.5, 'Fresh action decision did not resume within1.5s')
        for e in events:
            if e['kind'] == 'decision' and e['decision']['rejection'] in (1, 2):
                request = submitted[(e['player_id'], e['decision']['action_id'])]
                if fault['start_ns'] <= request['time_ns'] <= fault['release_ns']:
                    expired.append({'player_id': e['player_id'], 'action_id': e['decision']['action_id'], 'reason': e['decision']['rejection']})
        if mode in ('upstream', 'socket-path', 'gateway', 'host-ipc'):
            check(bool(expired), 'No blocked old shot expired/invalidated by original authority reference')
        stable_start = previous = None
        healthy_interval = None
        for f in frames:
            if f['time_ns'] < fault['release_ns']:
                continue
            healthy = all(f['retained_'+s] < 32 and not f['frozen_'+s] and f['pending_'+s] < 12 and f['remote_age_'+s] < .1 for s in ('a', 'b'))
            if not healthy or (previous is not None and f['time_ns']-previous >= 100_000_000):
                stable_start = None
            elif stable_start is None:
                stable_start = f['time_ns']
            if stable_start is not None and f['time_ns']-stable_start >= 250_000_000:
                healthy_interval = {'start_seconds': (stable_start-fault['release_ns'])/1e9,
                                    'stable_through_seconds': (f['time_ns']-fault['release_ns'])/1e9}
                break
            previous = f['time_ns']
        check(healthy_interval is not None and healthy_interval['start_seconds'] <= 1.5,
              'No continuously non-full action/prediction window with current timeline within1.5s for250ms')
        if recovery is not None:
            recovery['action_prediction_timeline_healthy_interval'] = healthy_interval
    if mode.startswith('loss-'):
        check(sum(e['event'] == 'dropped_'+mode[5:] for e in relay['events']) == 2, 'First loss not applied to both Sessions')
    if mode == 'burst2':
        check(sum(e['event'] == 'dropped_burst' for e in relay['events']) == 4, 'Two consecutive action packets not lost in both Sessions')
        counts = Counter(str(e['session']) for e in relay['events'] if e['event'] == 'dropped_burst')
        check(len(counts) == 2 and all(count == 2 for count in counts.values()), 'Burst loss was not exactly two packets per Session')
    if mode.startswith('network'):
        network = relay['network']
        check(network['random_dropped'] > 0 and network['delayed'] > 0, 'Network delay/loss was not exercised')
        check(network['maximum_scheduled_delay_ms'] <= network['configured_rtt_ms']/2+10,
              'Configured one-way jitter exceeds10ms')
        check(network['remaining_queue'] == 0, 'Network relay ended with unreported pending packets')
    if mode == 'duplicate-reorder-conflict':
        for label in ('duplicate_action', 'conflicting_atomic_batch', 'malformed_action', 'reordered_old_action'):
            check(sum(e['event'] == label for e in relay['events']) == 2, 'Missing '+label)
    if mode == 'drain-stall':
        check(client['maximum_retained'] == [32, 32] and min(client['blocked_submissions']) > 0, 'Drain stall did not reach/guard32 slots')
        check(min(client['maximum_unconsumed']) > 0, 'Drain stall retained no unconsumed decisions')
    movement_baseline = None
    if mode == 'baseline' and not client.get('legal_shots'):
        # A declared one-second warmup/tail leaves every frame inside the
        # four-second coexistence window, including any slow or failed frame.
        start, end = client['start_ns']+1_000_000_000, client['end_ns']-1_000_000_000
        measured = [frame for frame in frames if start <= frame['time_ns'] < end]
        (output/'timing.json').write_text(json.dumps({'start_ns': start, 'end_ns': end,
            'player_ids': client['player_ids'], 'fps': 60, 'injected': False,
            'gaps_100ms': sum(frame['frame_seconds'] >= .1 for frame in measured),
            'measurement_policy': 'fixed start+1s/end-1s warmup/tail; no slow-frame filtering'}, indent=2)+'\n')
        movement_baseline = analyze_commands(output, enforce=True)
        check(movement_baseline['passed'], 'Concurrent movement latency regression: '+str(movement_baseline['errors']))
        check(movement_baseline['production_60hz_passed'], 'Concurrent movement did not produce60Hz commands')
    legal = None
    if client.get('legal_shots'):
        legal = analyze_legal(output, injected=mode != 'baseline')
        check(legal['passed'], 'Legal-rate evidence failed: '+str(legal['errors']))
    return {'passed': not errors, 'errors': errors, 'mode': mode, 'fault': fault, 'rate': rate,
            'unique_wire_decisions': len(originals), 'identical_result_repeats': repeated,
            'accepted_unique_shots': accepted, 'unique_actual_damage': dict(total_damage),
            'combat_snapshot_hp_observations_matched_unique_decisions': hp_observations,
            'fresh_decision_seconds_after_release': fresh, 'expired_blocked_requests': expired,
            'outstanding_decisions_after_release': outstanding_after_release,
            'movement_authority_recovery': recovery, 'client': client,
            'gateway_actual_session_rate_counters': gateway_counters,
            'rate_limit_acceptance_policy': rate_policy,
            'movement_baseline_with_actions': movement_baseline,
            'legal_rate_evidence': legal,
            'kernel_socket_saturation_proven': False}


def run_case(args, mode, milliseconds=0):
    name = getattr(args, 'case_name', None) or mode+(f'-{milliseconds}ms' if milliseconds else '')
    output = args.output/name
    output.mkdir(parents=True, exist_ok=False)
    (output/'ready.json').unlink(missing_ok=True)
    artifacts = {name: {'path': str(getattr(args, name)),
                        'sha256': hashlib.sha256(getattr(args, name).read_bytes()).hexdigest()}
                 for name in ('match', 'gateway', 'probe', 'arena')}
    (output/'artifacts.json').write_text(json.dumps(artifacts, indent=2)+'\n')
    processes, logs = [], []
    relay = ipc_relay = gateway = None
    stopped = False
    def start(label, command):
        log = (output/(label+'.log')).open('w')
        logs.append(log)
        process = subprocess.Popen([str(part) for part in command], stdout=log, stderr=subprocess.STDOUT)
        processes.append(process)
        return process
    try:
        ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
        match = start('match', [args.match, '--arena', args.arena, '--listen', f'127.0.0.1:{ipc}', '--movement-trace', output/'match-commands.jsonl'])
        if mode == 'host-ipc':
            ipc_relay = IpcPause(ipc)
        gateway = start('gateway', [args.gateway, '--runtime', f'127.0.0.1:{ipc_relay.port if ipc_relay else ipc}', '--http', f'127.0.0.1:{http}', '--udp', f'127.0.0.1:{udp}', '--advertise-ip', '127.0.0.1'])
        until = time.monotonic()+10
        while True:
            if match.poll() is not None or gateway.poll() is not None:
                raise RuntimeError('Service exited during startup')
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{http}/rooms', timeout=.2):
                    break
            except OSError:
                if time.monotonic() >= until:
                    raise
                time.sleep(.01)
        relay = ActionRelay(http, udp, mode)
        command = [args.probe, '--gateway', relay.gateway, '--arena', args.arena, '--output', output, '--duration', 6]
        if getattr(args, 'gameplay_v5', False):
            command += ['--gameplay-v5', 'true', '--fps', getattr(args, 'fps', 60)]
        elif mode.startswith('network') or mode == 'burst2':
            command += ['--legal-shots', 'true']
        if mode == 'drain-stall':
            command += ['--drain-stall-ms', 1300]
        probe = start('probe', command)
        until = time.monotonic()+10
        while not (output/'ready.json').exists():
            if probe.poll() is not None or time.monotonic() >= until:
                raise RuntimeError('Probe did not join both real Sessions')
            time.sleep(.01)
        ready = json.loads((output/'ready.json').read_text())
        fault = {'start_ns': 0, 'release_ns': 0}
        if milliseconds:
            while time.monotonic_ns() < ready['start_ns']+int(getattr(args, 'fault_at', 1.5)*1e9):
                time.sleep(.005)
            if mode == 'gateway':
                fault['start_ns'] = time.monotonic_ns()
                gateway.send_signal(signal.SIGSTOP)
                stopped = True
                time.sleep(milliseconds/1000)
                gateway.send_signal(signal.SIGCONT)
                stopped = False
                fault['release_ns'] = time.monotonic_ns()
            elif mode == 'host-ipc':
                ipc_relay.arm(milliseconds/1000)
                if not ipc_relay.released.wait(milliseconds/1000+2):
                    raise RuntimeError('TCP fault did not release')
                fault = ipc_relay.evidence()
            else:
                relay.arm(mode, milliseconds/1000)
        if probe.wait(timeout=25):
            raise RuntimeError('Client action probe failed; see probe.log and client JSON')
        if mode == 'drain-stall':
            drain_at = 4 if getattr(args, 'gameplay_v5', False) else 1.5
            fault = {'start_ns': ready['start_ns']+int(drain_at*1e9), 'release_ns': ready['start_ns']+int((drain_at+1.3)*1e9)}
        evidence = relay.evidence()
        if mode in ('upstream', 'downstream', 'socket-path', 'cross-life') or mode.startswith('network'):
            fault = evidence['fault']
        (output/'relay.json').write_text(json.dumps(evidence, indent=2)+'\n')
        gateway.terminate();gateway.wait(timeout=5)
        match.terminate();match.wait(timeout=5)
        if match.returncode:
            raise RuntimeError('Match did not flush diagnostic trace cleanly')
        counters = re.search(r'rate_accepted_packets=(\d+) rate_limited_packets=(\d+) max_session_window_packets=(\d+)', (output/'gateway.log').read_text())
        if counters is None:
            raise RuntimeError('Missing actual Gateway Session limiter diagnostics')
        evidence['gateway_counters'] = dict(zip(('rate_accepted_packets', 'rate_limited_packets', 'max_session_window_packets'), map(int, counters.groups())))
        (output/'relay.json').write_text(json.dumps(evidence, indent=2)+'\n')
        result = analyze(output, evidence, fault, mode)
        result['artifacts'] = artifacts
        (output/'result.json').write_text(json.dumps(result, indent=2)+'\n')
        return result
    except Exception as failure:
        result = {'passed': False, 'mode': mode, 'requested_ms': milliseconds, 'errors': [str(failure)]}
        if relay:
            (output/'relay.json').write_text(json.dumps(relay.evidence(), indent=2)+'\n')
        (output/'result.json').write_text(json.dumps(result, indent=2)+'\n')
        return result
    finally:
        if stopped and gateway and gateway.poll() is None:
            gateway.send_signal(signal.SIGCONT)
        if relay:
            relay.close()
        if ipc_relay:
            ipc_relay.close()
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill();process.wait()
        for log in logs:
            log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('match', 'gateway', 'probe', 'arena', 'output'):
        parser.add_argument('--'+name, required=True, type=Path)
    parser.add_argument('--case', choices=('baseline', 'loss-shot', 'loss-result', 'loss-ack', 'duplicate-reorder-conflict', 'drain-stall', 'upstream', 'downstream', 'socket-path', 'gateway', 'host-ipc', 'network0', 'network20', 'network40', 'burst2'))
    parser.add_argument('--milliseconds', type=int, choices=(250, 1000), default=250)
    args = parser.parse_args()
    for name in ('match', 'gateway', 'probe', 'arena'):
        setattr(args, name, getattr(args, name).resolve())
        if not getattr(args, name).is_file():
            parser.error('Missing '+name)
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    block_modes = ('upstream', 'downstream', 'socket-path', 'gateway', 'host-ipc')
    cases = [(args.case, args.milliseconds if args.case in block_modes else 0)] if args.case else (
        [(mode, 0) for mode in ('baseline', 'loss-shot', 'loss-result', 'loss-ack', 'duplicate-reorder-conflict', 'drain-stall')]+
        [(mode, milliseconds) for mode in block_modes for milliseconds in (250, 1000)])
    results = []
    stop_after_case = False
    def finish_current_case(_signal, _frame):
        nonlocal stop_after_case
        stop_after_case = True
    signal.signal(signal.SIGINT, finish_current_case)
    signal.signal(signal.SIGTERM, finish_current_case)
    for mode, milliseconds in cases:
        result = run_case(args, mode, milliseconds)
        results.append(result)
        print(json.dumps({'case': mode, 'milliseconds': milliseconds, 'passed': result['passed'], 'errors': result['errors']}), flush=True)
        if stop_after_case:
            break
    summary = {'passed': len(results) == len(cases) and all(case['passed'] for case in results), 'cases': results,
               'requested_cases': len(cases), 'completed_cases': len(results), 'interrupted': stop_after_case,
               'scope': 'short headless localhost real-process faults; no GUI, long soak or kernel saturation claim'}
    (args.output/'action-matrix.json').write_text(json.dumps(summary, indent=2)+'\n')
    return 0 if summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
