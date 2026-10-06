"""Same-host two-tree comparison of the collision corpus (FF-1, used by FF-9).

Build gyo_collision_corpus (tests/common/collision) in a base tree and a branch
tree on the same host, then pass both binaries. Each runner writes its records;
records are compared line by line. The report counts differing records per
query, splits raycast differences into hit flips and distance ULP buckets, and
shows the first difference of each query.

Checks (--check):
  report  (default) exit 0 when every record matches, 1 otherwise.
  equal   the same as report: every one of the records must be bit-identical.
  ff9     the FF-9 pre-declaration (HANDOFF, FF-9 recorder):
          (a) every branch RaycastAabb record equals the exact reference: a
              rational slab test where only an exactly zero direction component
              is parallel, the distance correctly rounded to float;
          (b) every branch .../vertical record equals the base .../general
              record of the same key (the VerticalCapsule overloads become thin
              wrappers of the Math::Capsule overloads);
          (c) every other record is bit-identical to base.
          Exit 0 only when (a), (b) and (c) hold. Run with the base binary as
          both trees, it must fail with exactly the declared base deviations.

Exit status: 1 when a check fails, 2 on a usage or runner error (including
different record counts or keys).
"""
import argparse
import collections
from fractions import Fraction
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys

ULP_BUCKETS = ((0, 0, '0'), (1, 1, '1'), (2, 4, '2-4'), (5, 16, '5-16'), (17, 256, '17-256'), (257, None, '>256'))
WRAPPED = {'RaycastCapsule/vertical': 'RaycastCapsule/general',
           'SweepSphereAgainstCapsule/vertical': 'SweepSphereAgainstCapsule/general'}


def records(runner, output):
    subprocess.run([str(runner), '--out', str(output)], check=True)
    return output.read_text(encoding='utf-8').splitlines()


def parse(line):
    """(query, key, inputs, result) of one record: 'query key tag | inputs | result'."""
    head, inputs, result = line.split(' | ')
    query, key, _ = head.split(' ', 2)
    return query, key, inputs, result


def float_of(hex_bits):
    return struct.unpack('>f', bytes.fromhex(hex_bits))[0]


def bits_of(value):
    return struct.unpack('>I', struct.pack('>f', value))[0]


def ordered(bits):
    """Integer order of float bit patterns, so that adjacent floats differ by 1."""
    return bits if bits < 0x80000000 else -(bits - 0x80000000)


def raycast_hit(result):
    """Distance bits of a raycast result, or None for a miss; ValueError otherwise."""
    if result == 'miss':
        return None
    if result.startswith('hit='):
        return int(result[4:], 16)
    raise ValueError(result)


def ulp_bucket(distance):
    for low, high, name in ULP_BUCKETS:
        if distance >= low and (high is None or distance <= high):
            return name
    raise AssertionError(distance)


def round_sqrt_to_float(square):
    """Correctly rounded float of sqrt(square) for a rational square >= 0 (ties to even)."""
    if square == 0:
        return 0.0
    bits = bits_of(struct.unpack('>f', struct.pack('>f', math.sqrt(float(square))))[0])
    while True:
        value = struct.unpack('>f', struct.pack('>I', bits))[0]
        up = struct.unpack('>f', struct.pack('>I', bits + 1))[0]
        down = struct.unpack('>f', struct.pack('>I', bits - 1))[0] if bits > 0 else 0.0
        upper = (Fraction(value) + Fraction(up)) / 2
        lower = (Fraction(value) + Fraction(down)) / 2
        if square > upper * upper or (square == upper * upper and bits & 1):
            bits += 1
        elif square < lower * lower or (square == lower * lower and bits & 1 and bits > 0):
            bits -= 1
        else:
            return value


RAYCAST_AABB_INPUT = re.compile(r'([0-9a-f,]+)/([0-9a-f,]+) o=([0-9a-f,]+) d=([0-9a-f,]+) max=([0-9a-f]+)')


def exact_raycast_aabb(inputs):
    """Exact reference result string of one RaycastAabb record: 'miss' or 'hit=<float bits>'."""
    match = RAYCAST_AABB_INPUT.match(inputs)
    if not match:
        raise ValueError(inputs)
    vector = lambda text: [Fraction(float_of(part)) for part in text.split(',')]
    low, high, origin, direction = (vector(match.group(i)) for i in range(1, 5))
    maximum = Fraction(float_of(match.group(5)))
    entry, exit_ = Fraction(0), None
    for axis in range(3):
        if direction[axis] == 0:  # Only an exactly zero component is parallel.
            if origin[axis] < low[axis] or origin[axis] > high[axis]:
                return 'miss'
            continue
        near = (low[axis] - origin[axis]) / direction[axis]
        far = (high[axis] - origin[axis]) / direction[axis]
        if near > far:
            near, far = far, near
        entry = max(entry, near)
        exit_ = far if exit_ is None else min(exit_, far)
    length_squared = sum(component * component for component in direction)
    if exit_ is not None and (entry > exit_ or exit_ < 0):
        return 'miss'
    distance_squared = entry * entry * length_squared
    if distance_squared > maximum * maximum:
        return 'miss'
    return f'hit={bits_of(round_sqrt_to_float(distance_squared)):08x}'


def describe(old, new):
    """Classify a raycast difference (old -> new): flip direction or ULP distance; None if not a raycast."""
    try:
        a, b = raycast_hit(old), raycast_hit(new)
    except ValueError:
        return None
    if (a is None) != (b is None):
        return 'hit->miss' if b is None else 'miss->hit'
    return ulp_bucket(abs(ordered(a) - ordered(b)))


def summarize(pairs):
    """{query: {'differing': n, 'kinds': {...}, 'max_ulp': m, 'first': {...}}} for (query, key, old, new)."""
    report = {}
    for query, key, old, new in pairs:
        entry = report.setdefault(query, {'differing': 0, 'kinds': collections.Counter(), 'max_ulp': 0, 'first': None})
        entry['differing'] += 1
        kind = describe(old, new)
        entry['kinds'][kind or 'other'] += 1
        if kind and '->' not in kind:
            entry['max_ulp'] = max(entry['max_ulp'], abs(ordered(raycast_hit(old)) - ordered(raycast_hit(new))))
        if entry['first'] is None:
            entry['first'] = {'key': key, 'old': old, 'new': new}
    for entry in report.values():
        entry['kinds'] = dict(sorted(entry['kinds'].items()))
    return report


def ff9_checks(base, branch):
    """The three FF-9 declared checks; each is a list of (query, key, expected, actual)."""
    general = {(query, key): result for query, key, _, result in map(parse, base)}
    exact, wrapped, others = [], [], []
    for base_line, branch_line in zip(base, branch):
        query, key, inputs, result = parse(branch_line)
        if query == 'RaycastAabb':
            expected = exact_raycast_aabb(inputs)
            if result != expected:
                exact.append((query, key, result, expected))
        elif query in WRAPPED:
            expected = general[(WRAPPED[query], key)]
            if result != expected:
                wrapped.append((query, key, result, expected))
        elif base_line != branch_line:
            others.append((query, key, parse(base_line)[3], result))
    return {'raycast_aabb_vs_exact': exact, 'vertical_vs_base_general': wrapped, 'others_vs_base': others}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--base', type=Path, required=True, help='gyo_collision_corpus built in the base tree')
    parser.add_argument('--branch', type=Path, required=True, help='gyo_collision_corpus built in the branch tree')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--check', choices=('report', 'equal', 'ff9'), default='report')
    args = parser.parse_args(argv)
    args.output.mkdir(parents=True, exist_ok=True)
    try:
        base = records(args.base.resolve(), args.output / 'base.txt')
        branch = records(args.branch.resolve(), args.output / 'branch.txt')
    except (subprocess.CalledProcessError, OSError) as error:
        print(f'collision corpus comparison failed: {error}', file=sys.stderr)
        return 2
    if len(base) != len(branch):
        print(f'record counts differ: base {len(base)}, branch {len(branch)}', file=sys.stderr)
        return 2
    if [parse(line)[:2] for line in base] != [parse(line)[:2] for line in branch]:
        print('record keys differ between base and branch', file=sys.stderr)
        return 2
    changes = summarize((parse(a)[0], parse(a)[1], parse(a)[3], parse(b)[3])
                        for a, b in zip(base, branch) if a != b)
    report = {'records': len(base), 'check': args.check, 'base_to_branch': changes}
    print(f'{len(base)} records, {sum(c["differing"] for c in changes.values())} differing (base -> branch)')
    for query, entry in sorted(changes.items()):
        print(f'  {query}: {entry["differing"]} {entry["kinds"]} max_ulp={entry["max_ulp"]}')
    failed = bool(changes)
    if args.check == 'ff9':
        checks = ff9_checks(base, branch)
        report['ff9'] = {name: summarize(items) for name, items in checks.items()}
        report['ff9_keys'] = {name: [f'{q} {k}' for q, k, _, _ in items] for name, items in checks.items()}
        for name, items in checks.items():
            print(f'ff9 {name}: {len(items)}')
            for query, entry in sorted(report['ff9'][name].items()):
                print(f'  {query}: {entry["differing"]} {entry["kinds"]} max_ulp={entry["max_ulp"]}')
        failed = any(checks.values())
    (args.output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
