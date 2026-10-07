"""Passive bots that fill the room of a two-role GUI probe run (gui_room.hpp, quad probe --passive).

Without bots nothing here runs and the GUI probes keep their two-player command lines. With N
bots the GUI probes get --room-players 2+N; the bots join once both GUIs are in the room, walk off
the line between the GUI spawns, mark themselves parked (the GUIs wait for that mark) and leave
cleanly when the runner writes the stop file after the GUIs finish.
"""
import json
from pathlib import Path
import subprocess
import urllib.request

from acceptance_capacity import MAX_PLAYERS

PARKED_FILE = 'bots-parked.txt'  # GuiRoom::BotsParkedFile
STOP_FILE = 'bots-stop.txt'
TRACE_FILE = 'bots-commands.jsonl'
MAXIMUM_SECONDS = 1800


def add_arguments(parser):
    parser.add_argument('--bots', type=int, default=0, help=f'passive bots in the room (0..{MAX_PLAYERS - 2})')
    parser.add_argument('--quad-probe', type=Path, help='quad probe for --bots')


def check_arguments(parser, args):
    if not 0 <= args.bots <= MAX_PLAYERS - 2:
        parser.error(f'--bots must be 0..{MAX_PLAYERS - 2}')
    if args.bots:
        if args.quad_probe is None or not args.quad_probe.resolve().is_file():
            parser.error('--bots needs --quad-probe')
        args.quad_probe = args.quad_probe.resolve()


class PassiveBots:
    def __init__(self, count, quad_probe, arena, directory):
        self.count, self.quad_probe, self.arena, self.directory = count, quad_probe, arena, Path(directory)
        self.process = None

    def gui_arguments(self):
        return ['--room-players', str(2 + self.count)] if self.count else []

    def poll(self, http, start):
        """Start the bots once the Gateway lists both GUI players in the room; True once started."""
        if not self.count or self.process:
            return bool(self.process) or not self.count
        try:
            with urllib.request.urlopen(f'http://127.0.0.1:{http}/rooms', timeout=.2) as response:
                rooms = json.loads(response.read()).get('rooms', [])
        except OSError:
            return False
        if not rooms or rooms[0].get('players', 0) < 2:
            return False
        command = [self.quad_probe, '--gateway', f'127.0.0.1:{http}', '--arena', self.arena,
                   '--output', self.directory / 'bots', '--clients', self.count, '--create', 'false',
                   '--expect-players', 2 + self.count, '--passive', 'true',
                   '--parked-file', self.directory / PARKED_FILE, '--stop-file', self.directory / STOP_FILE,
                   '--trace', self.directory / TRACE_FILE, '--duration', MAXIMUM_SECONDS]
        self.process = start('bots', [str(part) for part in command])
        return True

    def stop(self, timeout=15):
        """After the GUIs finish: the bots leave and close their trace; their exit code."""
        if not self.process:
            return None
        (self.directory / STOP_FILE).write_text('stop\n')
        try:
            return self.process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            return None

    def evidence(self):
        return {'count': self.count, 'started': bool(self.process),
                'exit_code': self.process.poll() if self.process else None,
                'parked': (self.directory / PARKED_FILE).exists()}
