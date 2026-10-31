"""Same-host two-tree authority comparison: the formal "authority unchanged" proof.

Build the authority digest runner (gyo_object_fps_pvp_authority_digest) in two trees on the same
host, a base tree and a branch tree, then pass both binaries here. Every scenario of both runners
is compared record by record. For each differing scenario the first differing record and its first
differing field are named, from the runners' own --dump output.

A unit change is compared with --scaled and --added (v8-product-hit-rules-permille): every scenario is
then dumped in both trees and compared field by field. A --scaled field must equal base x factor, an
--added field is ignored but must appear somewhere in the branch dumps, and every other field must be
identical, record for record. The digests themselves differ in that mode by construction.

Exit status: 0 when every scenario matches, 1 when any differs, 2 on a usage or runner error.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys


def run(runner, output, scale):
    subprocess.run([str(runner), '--run', '--ticks', '--scale', str(scale), '--out', str(output)], check=True)
    return json.loads(output.read_text(encoding='utf-8'))


def first_difference(base_dump, branch_dump):
    """(record index, field, base value, branch value) of the first differing field."""
    base_lines = base_dump.read_text(encoding='utf-8').splitlines()
    branch_lines = branch_dump.read_text(encoding='utf-8').splitlines()
    for index, (left, right) in enumerate(zip(base_lines, branch_lines)):
        if left == right:
            continue
        left_fields, right_fields = left.split(' ')[1:], right.split(' ')[1:]
        for a, b in zip(left_fields, right_fields):
            if a != b:
                name = a.split('=', 1)[0]
                return {'record': index, 'field': name, 'base': a.split('=', 1)[-1], 'branch': b.split('=', 1)[-1],
                        'base_record': left, 'branch_record': right}
        return {'record': index, 'field': '<record length>', 'base': len(left_fields), 'branch': len(right_fields)}
    if len(base_lines) != len(branch_lines):
        return {'record': min(len(base_lines), len(branch_lines)), 'field': '<record count>',
                'base': len(base_lines), 'branch': len(branch_lines)}
    return None


def parse_scaled(text):
    """'hp,damage=1000' -> {'hp': 1000, 'damage': 1000}."""
    names, _, factor = text.partition('=')
    if not names or not factor.isdigit() or int(factor) < 2:
        raise ValueError(f'--scaled needs NAME[,NAME]=FACTOR with FACTOR >= 2: {text}')
    return {name: int(factor) for name in names.split(',') if name}


def fields(line):
    return [token.split('=', 1) for token in line.split(' ')[1:]]


def scaled_difference(base_lines, branch_lines, scaled, added):
    """First violation of 'only the scaled fields changed, by their factor', or None.

    Also returns the number of occurrences of each added field in the branch lines."""
    seen = {name: 0 for name in added}
    if len(base_lines) != len(branch_lines):
        return ({'record': min(len(base_lines), len(branch_lines)), 'field': '<record count>',
                 'base': len(base_lines), 'branch': len(branch_lines)}, seen)
    for index, (left, right) in enumerate(zip(base_lines, branch_lines)):
        right_fields = []
        for name, value in fields(right):
            if name in seen:
                seen[name] += 1
            else:
                right_fields.append((name, value))
        left_fields = fields(left)
        if len(left_fields) != len(right_fields):
            return ({'record': index, 'field': '<record length>', 'base': len(left_fields),
                     'branch': len(right_fields)}, seen)
        for (name, a), (other, b) in zip(left_fields, right_fields):
            if name != other:
                return ({'record': index, 'field': '<field name>', 'base': name, 'branch': other}, seen)
            factor = scaled.get(name)
            expected = str(int(a) * factor) if factor is not None and a.isdigit() else a
            if b != expected:
                return ({'record': index, 'field': name, 'base': a, 'branch': b, 'expected': expected}, seen)
    return None, seen


def compare_scaled(base_runner, branch_runner, output, scale, scaled, added):
    output.mkdir(parents=True, exist_ok=True)
    base = run(base_runner, output / 'base.json', scale)
    branch = run(branch_runner, output / 'branch.json', scale)
    report = {'scale': scale, 'base_digest_version': base['digest_version'],
              'branch_digest_version': branch['digest_version'], 'scaled': scaled, 'added': sorted(added),
              'scenarios': [], 'differing': []}
    base_names = {s['name'] for s in base['scenarios']}
    branch_names = {s['name'] for s in branch['scenarios']}
    added_total = {name: 0 for name in added}
    for name in sorted(base_names | branch_names):
        entry = {'name': name}
        if name not in base_names or name not in branch_names:
            entry['same'] = False
            entry['missing_in'] = 'base' if name not in base_names else 'branch'
        else:
            safe = name.replace('/', '_')
            dumps = output / f'{safe}.base.txt', output / f'{safe}.branch.txt'
            for runner, dump in zip((base_runner, branch_runner), dumps):
                subprocess.run([str(runner), '--dump', name, '--scale', str(scale), '--out', str(dump)], check=True)
            lines = [dump.read_text(encoding='utf-8').splitlines() for dump in dumps]
            difference, seen = scaled_difference(*lines, scaled, added)
            for field, count in seen.items():
                added_total[field] += count
            entry['records'] = len(lines[0])
            entry['same'] = difference is None
            if difference is not None:
                entry['first_difference'] = difference
        if not entry['same']:
            report['differing'].append(name)
        report['scenarios'].append(entry)
    report['added_occurrences'] = added_total
    report['absent_added'] = sorted(field for field, count in added_total.items() if count == 0)
    report['all_same'] = not report['differing'] and not report['absent_added']
    (output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def compare(base_runner, branch_runner, output, scale=1):
    output.mkdir(parents=True, exist_ok=True)
    base = run(base_runner, output / 'base.json', scale)
    branch = run(branch_runner, output / 'branch.json', scale)
    report = {'scale': scale, 'base_digest_version': base['digest_version'],
              'branch_digest_version': branch['digest_version'], 'scenarios': [], 'differing': []}
    base_by_name = {s['name']: s for s in base['scenarios']}
    branch_by_name = {s['name']: s for s in branch['scenarios']}
    for name in sorted(set(base_by_name) | set(branch_by_name)):
        left, right = base_by_name.get(name), branch_by_name.get(name)
        entry = {'name': name, 'same': bool(left and right and left['digest'] == right['digest'])}
        if left is None or right is None:
            entry['missing_in'] = 'base' if left is None else 'branch'
        elif not entry['same']:
            prefixes = zip(left['record_prefixes'], right['record_prefixes'])
            entry['first_differing_record'] = next((i for i, (a, b) in enumerate(prefixes) if a != b),
                                                   min(left['records'], right['records']))
            safe = name.replace('/', '_')
            dumps = output / f'{safe}.base.txt', output / f'{safe}.branch.txt'
            for runner, dump in zip((base_runner, branch_runner), dumps):
                subprocess.run([str(runner), '--dump', name, '--scale', str(scale), '--out', str(dump)], check=True)
            entry['first_difference'] = first_difference(*dumps)
            report['differing'].append(name)
        report['scenarios'].append(entry)
    report['all_same'] = not report['differing']
    (output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True, help='authority digest runner built in the base tree')
    parser.add_argument('--branch', type=Path, required=True, help='authority digest runner built in the branch tree')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scale', type=int, default=1, choices=range(1, 11))
    parser.add_argument('--scaled', help='NAME[,NAME]=FACTOR: these fields must equal base x FACTOR')
    parser.add_argument('--added', default='', help='NAME[,NAME]: fields only the branch has; must appear')
    args = parser.parse_args()
    added = {name for name in args.added.split(',') if name}
    try:
        scaled = parse_scaled(args.scaled) if args.scaled else {}
    except ValueError as error:
        parser.error(str(error))
    if added and not scaled:
        parser.error('--added needs --scaled')
    for runner in (args.base, args.branch):
        if not runner.is_file():
            parser.error(f'Missing runner: {runner}')
    try:
        if scaled:
            report = compare_scaled(args.base.resolve(), args.branch.resolve(), args.output.resolve(), args.scale,
                                    scaled, added)
        else:
            report = compare(args.base.resolve(), args.branch.resolve(), args.output.resolve(), args.scale)
    except (subprocess.CalledProcessError, OSError, ValueError) as error:
        print(f'authority comparison failed: {error}', file=sys.stderr)
        return 2
    print(f"{len(report['scenarios'])} scenarios, {len(report['differing'])} differing")
    if report.get('absent_added'):
        print(f"  added fields never present in the branch: {', '.join(report['absent_added'])}")
    for entry in report['scenarios']:
        if not entry['same']:
            difference = entry.get('first_difference') or {}
            print(f"  {entry['name']}: first differing record {entry.get('first_differing_record')}, "
                  f"field {difference.get('field')} ({difference.get('base')} -> {difference.get('branch')})")
    return 0 if report['all_same'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
