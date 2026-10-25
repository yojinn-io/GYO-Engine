"""Record only (drop-report): the reports the Match read against the reports the Gateway wrote.

  python3 -I -B compare_reports.py RUN_DIR...

Every directory holding gateway.log and match.log is one run. For each run: the Gateway's
"drop report <body>" lines and the Match's "gw_drop report <body>" lines, keyed by (player,
report_sequence), must be equal one for one (the L2 statement to declare, pv7 §4); the Match's
violation lines are listed; every stream with content should end with a final.
"""
import json
import re
import sys
from pathlib import Path

LINE = re.compile(r'(?:gw_drop|drop) report (player=(\d+) report_sequence=(\d+) final=(\d) .*)$')


def reports(path, prefix):
    out = {}
    for line in path.read_text(errors='replace').splitlines():
        if prefix not in line:
            continue
        match = LINE.search(line)
        if match:
            out[(int(match[2]), int(match[3]))] = match[1]
    return out


def compare(run):
    gateway = reports(run / 'gateway.log', 'drop report ')
    gateway = {k: v for k, v in gateway.items()}
    match = reports(run / 'match.log', 'gw_drop report ')
    violations = [line.split('gw_drop violation ', 1)[1] for line in (run / 'match.log').read_text(errors='replace').splitlines()
                  if 'gw_drop violation ' in line]
    streams = {player for player, _ in gateway}
    finals = {player for (player, _), body in gateway.items() if ' final=1 ' in body + ' '}
    return {'run': str(run), 'gateway_reports': len(gateway), 'match_reports': len(match),
            'equal': gateway == match,
            'only_gateway': sorted(set(gateway) - set(match))[:10], 'only_match': sorted(set(match) - set(gateway))[:10],
            'different': sorted(k for k in set(gateway) & set(match) if gateway[k] != match[k])[:10],
            'violations': violations[:10], 'streams': sorted(streams), 'streams_without_final': sorted(streams - finals)}


def main(roots):
    results = []
    for root in roots:
        for log in sorted(Path(root).rglob('gateway.log')):
            if (log.parent / 'match.log').exists():
                results.append(compare(log.parent))
    print(json.dumps(results, indent=1))
    return 0 if all(r['equal'] and not r['violations'] for r in results) else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
