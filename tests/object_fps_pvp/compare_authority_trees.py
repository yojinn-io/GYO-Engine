"""Same-host two-tree authority comparison: the formal "authority unchanged" proof.

Build the authority digest runner (gyo_object_fps_pvp_authority_digest) in two trees on the same
host, a base tree and a branch tree, then pass both binaries here. Every scenario of both runners
is compared record by record. For each differing scenario the first differing record and its first
differing field are named, from the runners' own --dump output.

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
    args = parser.parse_args()
    for runner in (args.base, args.branch):
        if not runner.is_file():
            parser.error(f'Missing runner: {runner}')
    try:
        report = compare(args.base.resolve(), args.branch.resolve(), args.output.resolve(), args.scale)
    except (subprocess.CalledProcessError, OSError, ValueError) as error:
        print(f'authority comparison failed: {error}', file=sys.stderr)
        return 2
    print(f"{len(report['scenarios'])} scenarios, {len(report['differing'])} differing")
    for entry in report['scenarios']:
        if not entry['same']:
            difference = entry.get('first_difference') or {}
            print(f"  {entry['name']}: first differing record {entry.get('first_differing_record')}, "
                  f"field {difference.get('field')} ({difference.get('base')} -> {difference.get('branch')})")
    return 0 if report['all_same'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
