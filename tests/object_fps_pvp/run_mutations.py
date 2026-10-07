"""Product-owned mutation runner for object_fps_pvp.

Each mutant in mutations.json replaces one exact text in one source file,
rebuilds the declared targets, runs the declared check, and counts as killed
only when the check fails AND its output matches the mutant's expected message.
Any other outcome is reported on its own: survived (the check passed), stale
(the text is not found exactly once), build_failed, wrong_failure (the check
failed for another reason, e.g. a missing tool) or timeout. The unmutated tree
must pass every used check first, and every file is restored afterwards.

Python only, with subprocess timeouts and no shell tools, so it runs the same
on Windows, Linux and macOS. C++ checks run through ctest, which knows each
test's executable path on every platform.

Usage (from the repository root):
  python3 tests/object_fps_pvp/run_mutations.py --build-dir build/target/_build/test
      [--only ID ...] [--batch NN] [--report FILE] [--cmake PATH] [--ctest PATH] [--go PATH]
Exit status: 0 when every selected mutant is killed, 1 otherwise, 2 on a usage
or baseline error.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

REPOSITORY = Path(__file__).resolve().parents[2]
CATALOG = Path(__file__).resolve().with_name('mutations.json')
CHECK_TIMEOUT_SECONDS = 600
BUILD_TIMEOUT_SECONDS = 1800


class Tools:
    def __init__(self, build_dir, cmake='cmake', ctest='ctest', go='go'):
        self.build_dir, self.cmake, self.ctest, self.go = Path(build_dir), cmake, ctest, go


def execute(command, cwd, timeout):
    """(status, output): status is 'passed', 'failed', 'timeout' or 'error'."""
    try:
        completed = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, errors='replace', timeout=timeout)
    except subprocess.TimeoutExpired as expired:
        return 'timeout', (expired.output or '') if isinstance(expired.output, str) else ''
    except OSError as error:  # Missing or unrunnable tool: never a kill.
        return 'error', str(error)
    return ('passed' if completed.returncode == 0 else 'failed'), completed.stdout


def check_command(check, tools):
    if 'ctest' in check:
        return [tools.ctest, '--test-dir', str(tools.build_dir), '-R', '^' + re.escape(check['ctest']) + '$',
                '--output-on-failure', '--no-tests=error'], REPOSITORY
    command = [tools.go if part == '{go}' else part.replace('{python}', sys.executable) for part in check['command']]
    return command, REPOSITORY / check.get('cwd', '.')


def build(targets, tools):
    if not targets:
        return 'passed', ''
    return execute([tools.cmake, '--build', str(tools.build_dir), '--target', *targets], REPOSITORY,
                   BUILD_TIMEOUT_SECONDS)


def run_check(check, tools):
    status, output = build(check.get('build', []), tools)
    if status != 'passed':
        return 'build_failed', output
    command, cwd = check_command(check, tools)
    return execute(command, cwd, check.get('timeout', CHECK_TIMEOUT_SECONDS))


def run_mutant(mutant, checks, tools, root=REPOSITORY):
    """Apply one mutant, run its check, restore the file, and classify the result."""
    path = root / mutant['file']
    original = path.read_bytes()
    text = original.decode('utf-8')
    if text.count(mutant['find']) != 1:
        return {'id': mutant['id'], 'status': 'stale', 'detail': f"text found {text.count(mutant['find'])} times"}
    try:
        path.write_bytes(text.replace(mutant['find'], mutant['replace']).encode('utf-8'))
        status, output = run_check(checks[mutant['check']], tools)
    finally:
        path.write_bytes(original)
    if status == 'passed':
        result = 'survived'
    elif status == 'failed' and re.search(mutant['expect'], output):
        result = 'killed'
    elif status == 'failed':
        result = 'wrong_failure'
    else:
        result = status  # build_failed, timeout or error
    detail = '' if result == 'killed' else output[-2000:]
    return {'id': mutant['id'], 'status': result, 'detail': detail}


def changed_files(files):
    """Catalog files that differ from HEAD; a crash must never leave a mutant unnoticed."""
    completed = subprocess.run(['git', 'status', '--porcelain', '--', *files], cwd=REPOSITORY,
                               stdout=subprocess.PIPE, text=True, check=True)
    return [line[3:] for line in completed.stdout.splitlines()]


def select(catalog, only, batch):
    mutants = catalog['mutants']
    if only:
        unknown = set(only) - {m['id'] for m in mutants}
        if unknown:
            raise SystemExit(f'unknown mutant ids: {sorted(unknown)}')
        mutants = [m for m in mutants if m['id'] in only]
    if batch:
        mutants = [m for m in mutants if m.get('batch') == batch]
    return mutants


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--build-dir', required=True)
    parser.add_argument('--only', nargs='*', default=[])
    parser.add_argument('--batch')
    parser.add_argument('--report')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--ctest', default='ctest')
    parser.add_argument('--go', default='go')
    parser.add_argument('--allow-dirty', action='store_true', help='run although catalog files differ from HEAD')
    args = parser.parse_args(argv)
    tools = Tools(REPOSITORY / args.build_dir, args.cmake, args.ctest, args.go)
    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    mutants = select(catalog, args.only, args.batch)
    if not mutants:
        print('no mutants selected', file=sys.stderr)
        return 2
    dirty = changed_files(sorted({m['file'] for m in mutants}))
    if dirty and not args.allow_dirty:
        print(f'catalog files differ from HEAD (commit or pass --allow-dirty): {dirty}', file=sys.stderr)
        return 2
    used = sorted({m['check'] for m in mutants})
    results = []
    try:
        for name in used:  # The unmutated tree must pass every check it uses.
            status, output = run_check(catalog['checks'][name], tools)
            print(f'baseline {name}: {status}', flush=True)
            if status != 'passed':
                print(output[-2000:], file=sys.stderr)
                return 2
        for mutant in mutants:
            result = run_mutant(mutant, catalog['checks'], tools)
            results.append(result)
            print(f"{result['id']}: {result['status']}", flush=True)
    finally:
        # Leave every binary built from the restored sources.
        targets = sorted({t for name in used for t in catalog['checks'][name].get('build', [])})
        build(targets, tools)
    killed = sum(r['status'] == 'killed' for r in results)
    print(f'{killed}/{len(results)} killed')
    if args.report:
        Path(args.report).parent.mkdir(parents=True, exist_ok=True)
        Path(args.report).write_text(json.dumps({'results': results}, indent=2, ensure_ascii=False) + '\n',
                                     encoding='utf-8')
    return 0 if killed == len(results) else 1


if __name__ == '__main__':
    sys.exit(main())
