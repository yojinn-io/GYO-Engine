"""Same-host two-tree comparison of the collision corpus (FF-1, used by FF-9).

Build gyo_collision_corpus (tests/common/collision) in a base tree and a branch
tree on the same host, then pass both binaries. Each runner writes its records;
records are compared line by line. The report counts differing records per
query and shows the first difference of each query.

Exit status: 0 when every record matches, 1 when any differs, 2 on a usage or
runner error (including different record counts).
"""
import argparse
import collections
import json
from pathlib import Path
import subprocess
import sys


def records(runner, output):
    subprocess.run([str(runner), '--out', str(output)], check=True)
    return output.read_text(encoding='utf-8').splitlines()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True, help='gyo_collision_corpus built in the base tree')
    parser.add_argument('--branch', type=Path, required=True, help='gyo_collision_corpus built in the branch tree')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
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
    differing = collections.Counter()
    first = {}
    for left, right in zip(base, branch):
        if left != right:
            query = left.split(' ', 1)[0]
            differing[query] += 1
            first.setdefault(query, {'base': left, 'branch': right})
    report = {'records': len(base), 'differing': dict(differing), 'first_difference': first}
    (args.output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'{len(base)} records, {sum(differing.values())} differing')
    for query, count in sorted(differing.items()):
        print(f'  {query}: {count}\n    base:   {first[query]["base"]}\n    branch: {first[query]["branch"]}')
    return 1 if differing else 0


if __name__ == '__main__':
    raise SystemExit(main())
