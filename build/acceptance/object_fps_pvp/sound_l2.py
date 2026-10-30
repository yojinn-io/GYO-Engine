"""The L2 of v8-product-sfx: local shot sounds follow the press, not the 30 FPS frame grid.

Declared before any result (docs/dev_logs/2026_10_30_pvp_v8_product_sfx.zh-Hant.md, "L2 宣告",
approved by the user on 2026-10-30): per round, a real Match and Gateway on localhost and two GUI
probes (--sound-shots --fps 30). The actor's clicks are injected from a thread of their own at
times off the frame grid; its sound output is SDL's disk driver (480-frame blocks, timescale 1)
with the output anchors logged. Three rounds, no retries; any round failing 1-4 fails the L2; a
round failing 5 is invalid, and the run stops there.

  1. Balance: submitted == on time + late + queue full + no voice, the drops are 0, and the local
     shot cues equal the shots the client submitted.
  2. Every local shot onset lies at the frame rebuilt from the anchor that took its request.
  3. The local shot onsets' intervals in Engine time differ from the presses' intervals by less
     than one frame (20.8 us).
  4. No local shot starts late.
  5. Validity: at least half of the press timestamps are more than 2 ms from the nearest frame start.

Onsets are found by exact sample values: one voice of gun_shoot.wav at the local shot volume gives
output samples the mixer's integer arithmetic fixes bit for bit, so its first frames are searched
for in the disk file.

Usage (from the repository root):
  python3 build/acceptance/object_fps_pvp/sound_l2.py --match <match> --gateway <gateway>
      --probe <gyo_object_fps_pvp_gui_probe> --assets <deployed assets/object_fps_pvp> --output <dir> [--rounds 3]
Exit status: 0 when the L2 holds, 1 when it does not or a round is invalid, 2 on a usage error.
"""
import argparse
import bisect
import csv
import json
import os
import socket
import struct
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

from run_network import free_port, wait_for_match_ready

RATE = 48000
FRAME_NS = 1e9 / RATE
PATTERN_FRAMES = 48


def frames_from_ns(ns):
    # The mixer's rounding: floor(ns * 48000 / 10^9 + 1/2).
    return (ns * 6 + 62500) // 125000


def read_wav_pcm16(path):
    data = Path(path).read_bytes()
    offset, fmt, samples = 12, None, None
    while offset + 8 <= len(data):
        chunk, size = data[offset:offset + 4], struct.unpack_from('<I', data, offset + 4)[0]
        body = data[offset + 8:offset + 8 + size]
        if chunk == b'fmt ':
            fmt = struct.unpack_from('<HHIIHH', body)
        elif chunk == b'data':
            samples = struct.unpack_from(f'<{len(body) // 2}h', body)
        offset += 8 + size + (size & 1)
    tag, channels, rate, _, _, bits = fmt
    if (tag, rate, bits) != (1, RATE, 16):
        raise ValueError(f'{path}: not 48 kHz PCM16')
    return channels, samples


def expected_output(asset_root, volume):
    """The mixer's output for one voice of gun_shoot.wav, as int16 stereo pairs (first frames)."""
    channels, samples = read_wav_pcm16(asset_root / 'audio' / 'gun_shoot.wav')
    gain = round(volume * 65536)  # std::lround of a positive value
    master = 65536
    out = []
    for frame in range(PATTERN_FRAMES):
        left = samples[frame * channels]
        right = samples[frame * channels + (1 if channels == 2 else 0)]
        for value in (left, right):
            mixed = (value * gain * master + (1 << 31)) >> 32
            out.append(max(-32768, min(32767, mixed)))
    return struct.pack(f'<{len(out)}h', *out)


def find_onsets(raw, pattern):
    onsets, start = [], 0
    while True:
        found = raw.find(pattern, start)
        if found < 0:
            return onsets
        if found % 4 == 0:
            onsets.append(found // 4)
        start = found + 2


def read_csv(path):
    with open(path, newline='') as handle:
        return list(csv.DictReader(handle))


def judge_round(directory, asset_root):
    summary = json.loads((directory / 'sound-summary.json').read_text())
    cues = [c for c in read_csv(directory / 'sound-cues.csv') if c['cue'] == 'local_shot']
    anchors = [(int(a['anchor_ns']), int(a['first_frame'])) for a in read_csv(directory / 'sound-anchors.csv')]
    frames = [int(f['frame_start_ns']) for f in read_csv(directory / 'sound-frames.csv')]
    settings = json.loads((asset_root / 'audio' / 'sfx.json').read_text())
    volume = settings['cues']['local_shot']['volume']
    raw = (directory / 'disk-audio.raw').read_bytes()
    onsets = find_onsets(raw, expected_output(asset_root, volume))
    onset_set = set(onsets)
    delay = frames_from_ns(int(summary['schedule_delay_ns']))
    anchor_times = [a for a, _ in anchors]
    result = {'cues': len(cues), 'onsets_found': len(onsets), 'anchors': len(anchors),
              'anchors_dropped': summary['anchors_dropped'], 'driver': summary['driver']}

    # 1. Balance.
    drops = summary['queue_full'] + summary['no_voice']
    balance = (summary['submitted'] == summary['on_time'] + summary['late'] + drops and drops == 0 and
               len(cues) == summary['shots_submitted'])
    result['1_balance'] = {'passed': balance, 'submitted': summary['submitted'], 'on_time': summary['on_time'],
                           'late': summary['late'], 'drops': drops, 'local_shot_cues': len(cues),
                           'shots_submitted': summary['shots_submitted']}

    # 2. Each onset at its rebuilt frame; 4. none late.
    matched, late, misses, engine_onsets = 0, 0, [], []
    for cue in sorted(cues, key=lambda c: int(c['played_ns'])):
        at, played = int(cue['at_ns']), int(cue['played_ns'])
        k = bisect.bisect_left(anchor_times, played)
        hit = None
        for i in (k, k - 1, k + 1):
            if not 0 <= i < len(anchors):
                continue
            anchor_ns, first = anchors[i]
            unclamped = first + frames_from_ns(at - anchor_ns) + delay
            start = max(first, unclamped)
            if start in onset_set:
                hit = (anchor_ns, first, start, unclamped < first)
                break
        if hit is None:
            misses.append(at)
            continue
        matched += 1
        late += 1 if hit[3] else 0
        engine_onsets.append((at, hit[0] + (hit[2] - hit[1]) * FRAME_NS))
    result['2_rebuilt_frames'] = {'passed': matched == len(cues) and len(cues) > 0, 'matched': matched,
                                  'of': len(cues), 'extra_onsets': len(onsets) - matched}
    result['4_local_late'] = {'passed': late == 0 and matched == len(cues), 'late': late}

    # 3. Intervals in Engine time against the presses.
    errors = [abs((b[1] - a[1]) - (b[0] - a[0])) for a, b in zip(engine_onsets, engine_onsets[1:])]
    result['3_intervals'] = {'passed': bool(errors) and max(errors) < FRAME_NS and len(engine_onsets) == len(cues),
                             'intervals': len(errors), 'max_error_ns': max(errors) if errors else None,
                             'limit_ns': FRAME_NS}

    # 5. Validity: presses off the frame grid.
    off = 0
    for cue in cues:
        at = int(cue['at_ns'])
        i = bisect.bisect_left(frames, at)
        nearest = min(abs(at - frames[j]) for j in (i - 1, i) if 0 <= j < len(frames))
        off += 1 if nearest > 2_000_000 else 0
    result['5_validity'] = {'passed': len(cues) > 0 and off * 2 >= len(cues), 'off_grid': off, 'of': len(cues)}
    result['valid'] = result['5_validity']['passed']
    result['passed'] = all(result[k]['passed'] for k in ('1_balance', '2_rebuilt_frames', '3_intervals', '4_local_late'))
    return result


def run_round(args, directory):
    directory.mkdir(parents=True, exist_ok=True)
    handles, processes = [], []

    def start(name, command, env=None):
        log = (directory / (name + '.log')).open('w')
        handles.append(log)
        process = subprocess.Popen([str(v) for v in command], stdout=log, stderr=subprocess.STDOUT,
                                   env={**os.environ, **(env or {})})
        processes.append(process)
        return process

    ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
    try:
        match = start('match', [args.match, '--arena', args.assets / 'pvp_arena.json', '--listen', f'127.0.0.1:{ipc}'])
        wait_for_match_ready(match, directory / 'match.log', f'127.0.0.1:{ipc}')
        start('gateway', [args.gateway, '--runtime', f'127.0.0.1:{ipc}', '--http', f'127.0.0.1:{http}',
                          '--udp', f'127.0.0.1:{udp}', '--advertise-ip', '127.0.0.1'])
        deadline = time.monotonic() + 20
        while True:
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{http}/rooms', timeout=.3):
                    break
            except OSError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(.05)
        common = ['--sound-shots', '--fps', 30, '--arena-root', args.assets, '--gateway', f'127.0.0.1:{http}',
                  '--output', directory]
        disk = {'SDL_AUDIO_DRIVER': 'disk', 'SDL_AUDIO_DISK_OUTPUT_FILE': str(directory / 'disk-audio.raw'),
                'SDL_AUDIO_DISK_TIMESCALE': '1', 'SDL_AUDIO_DEVICE_SAMPLE_FRAMES': '480',
                'SDL_AUDIO_FORMAT': 'S16', 'SDL_AUDIO_FREQUENCY': '48000', 'SDL_AUDIO_CHANNELS': '2'}
        actor = start('create', [args.probe, '--role', 'create', *common], disk)
        joiner = start('join', [args.probe, '--role', 'join', *common], {'SDL_AUDIO_DRIVER': 'dummy'})
        for client in (actor, joiner):
            if client.wait(timeout=180):
                raise RuntimeError(f'probe failed; see {directory}')
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
        for handle in handles:
            handle.close()


def main():
    parser = argparse.ArgumentParser()
    for key in ('match', 'gateway', 'probe', 'assets', 'output'):
        parser.add_argument('--' + key, required=True, type=Path)
    parser.add_argument('--rounds', type=int, default=3)
    args = parser.parse_args()
    for key in ('match', 'gateway', 'probe', 'assets', 'output'):
        setattr(args, key, getattr(args, key).resolve())
    args.output.mkdir(parents=True, exist_ok=True)
    rounds, status = [], 0
    for number in range(1, args.rounds + 1):
        directory = args.output / f'round-{number}'
        try:
            run_round(args, directory)
            result = judge_round(directory, args.assets)
        except Exception as failure:  # a round that cannot run is recorded, never retried
            result = {'passed': False, 'valid': False, 'error': str(failure)}
        result['round'] = number
        rounds.append(result)
        print(json.dumps(result), flush=True)
        if not result.get('valid'):
            status = 1
            break
        if not result['passed']:
            status = 1
    verdict = {'passed': status == 0 and len(rounds) == args.rounds and all(r['passed'] for r in rounds),
               'rounds': rounds}
    (args.output / 'sound-l2.json').write_text(json.dumps(verdict, indent=2) + '\n')
    print(json.dumps({'passed': verdict['passed']}))
    return 0 if verdict['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
