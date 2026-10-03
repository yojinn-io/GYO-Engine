#!/usr/bin/env python3
"""Syntax-only check of object_fps_pvp sources that no target compiles.

Uses the compile flags of an app_support source (PlayerPresentation.cpp) from a
configured build's compile_commands.json, so the result reflects the include
paths and definitions the product would use. Run from the repository root.

usage: syntax_check.py <compile_commands.json> <list.txt> <out_dir>
  list.txt  one path per line, relative to apps/object_fps_pvp (or absolute)
  out_dir   receives one .log per file and summary.tsv (PASS/FAIL, error count, path)
"""
import json
import pathlib
import shlex
import subprocess
import sys

TEMPLATE_SOURCE = 'apps/object_fps_pvp/src/Pvp/PlayerPresentation.cpp'


def main() -> int:
    cc_path, list_path, out = sys.argv[1:4]
    app = pathlib.Path.cwd() / 'apps/object_fps_pvp'
    commands = json.load(open(cc_path))
    template = next(c for c in commands if c['file'].endswith(TEMPLATE_SOURCE))
    args = template.get('arguments') or shlex.split(template['command'])
    flags, skip_next = [], False
    for arg in args[1:]:
        if skip_next:
            skip_next = False
            continue
        if arg in ('-o', '-MF', '-MT', '-MQ'):
            skip_next = True
            continue
        if arg in ('-c', '-MD', '-MMD') or arg.endswith('PlayerPresentation.cpp'):
            continue
        flags.append(arg)
    out_dir = pathlib.Path(out)
    out_dir.mkdir(parents=True, exist_ok=True)
    summary, failed = [], 0
    for rel in [line.strip() for line in open(list_path) if line.strip()]:
        source = pathlib.Path(rel) if rel.startswith('/') else app / rel
        result = subprocess.run([args[0], *flags, '-fsyntax-only', str(source)],
                                cwd=template['directory'], capture_output=True, text=True)
        (out_dir / (rel.strip('/').replace('/', '__') + '.log')).write_text(result.stderr)
        status = 'PASS' if result.returncode == 0 else 'FAIL'
        failed += status == 'FAIL'
        summary.append(f"{status}\t{result.stderr.count(' error: ')}\t{rel}")
        print(summary[-1], flush=True)
    (out_dir / 'summary.tsv').write_text('\n'.join(summary) + '\n')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
