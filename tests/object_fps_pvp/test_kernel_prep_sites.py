"""Ratchet guard of the authority's wall-clock reads, global diagnostic entries and
off-thread touches (v8-product-kernel-prep-detect, D2 and the D3 writer site).

THE WHITELIST BELOW RECORDS TODAY'S SITES. IT PERMITS NOTHING. Every row is a
violation of spec G.4 (docs/architecture/plans/kernel/spec.zh-Hant.md) or an
audit finding (audit.zh-Hant.md section 3.4) that the kernel migration batches
will move; each batch that moves one deletes its row. A new row, a missing row
or a changed count fails this test: functional work is halted (2026-11-11), so
the table changes only by a migration batch's deliberate move. An unrelated
refactoring that changes a count stops and reports; it does not widen the table.

Three classes, each hit attributed to (file, function, call shape):
  W  wall-clock reads in the files that decide: now_(), the host's Now(), any
     <...Clock|clock>::now() (steady_clock, system_clock, high_resolution_clock,
     MonotonicClock and aliases), Engine::Time::Now*(), MovementTraceNowNs().
  G  global diagnostic entries at the StepMovement call sites and around them:
     TraceMovement(, MovementDiagEnabled(), RecordMovementDiag(.
  A  a thread other than the simulation role touching the authority:
     match_.<Method>( in the functions of MatchRuntimeHost.cpp that do not run
     on the role's thread (TICK_SIDE below lists the ones that do), and
     host.<Method>( in IpcHost.cpp for every host method that reaches match_
     synchronously on the caller's thread (HOST_TOUCHES).
The function of a hit is the outermost brace scope containing it whose header
has a parenthesis and is not a namespace/class/struct/enum/union/extern block;
lambdas and control blocks inside a function belong to it. Comments and string
literals are stripped first. The one hit that is a function's own name in its
definition header (MovementTraceNowNs) is not a call and is not counted; a hit
elsewhere in a header (a constructor's initializer list, a default argument) is.

Scope limits (declaration section 5): the scan reads source text and follows no
indirection, so a touch through a wrapper the file already has (IngressBucket,
IngressBucketId, RecordRejection, GetActionResults, TickCount), match_ passed as
an argument, or a reference `auto& m = match_` adds no row (review D2 S4); the
I/O-layer files (ClientConnection.cpp, IpcHost.cpp, PvpApplication.cpp) read the
clock legitimately and are not scanned for W. Not every A row is the IPC thread:
TakeIngressStatistics and TickCount are called by match_main on the main thread
(the former while the role runs, under the mutex); QueueActionAcknowledgement has
no production caller. Each such row says so (review D2 S5).
"""
from pathlib import Path
import re
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
SOURCE = REPOSITORY / 'apps/object_fps_pvp/src/Pvp'
INCLUDE = REPOSITORY / 'apps/object_fps_pvp/include/RetroFPS/Pvp'
SIM = REPOSITORY / 'apps/object_fps_pvp/src/Sim'  # v8-product-kernel-prep-zones: Movement.cpp lives here

# Functions of MatchRuntimeHost.cpp that run on the simulation role's thread
# (inside Advance/Step, or the role body itself); their match_ calls are the
# authority's own thread and are not off-thread touches. Step runs ClearState
# (reset), the tick lambda, TrackSlack and JudgeConnectionQuality;
# JudgeConnectionQuality and the tick lambda run RemovePlayerState; Body is the
# role loop calling Advance. ClearState is also reached from the public Reset()
# while the role is stopped (running_ guards it), never concurrently. Reviewed
# 2026-11-12 (Opus, xhigh); the self test pins this set.
TICK_SIDE = frozenset({'MatchRuntimeHost::Step', 'MatchRuntimeHost::Body', 'MatchRuntimeHost::TrackSlack',
                       'MatchRuntimeHost::JudgeConnectionQuality', 'MatchRuntimeHost::ClearState',
                       'MatchRuntimeHost::RemovePlayerState'})

# Every host method that reaches match_ before returning, on the caller's
# thread (see the A rows of MatchRuntimeHost.cpp): SubmitInput and
# SubmitActionBatch (spec G.4 items 4 and 5), GetActionResults (a read of the
# authority's results), NoteWireRejection and SubmitActions (IngressBucket ->
# IngressBucketId -> ContainsPlayer), QueueActionAcknowledgement, TickCount,
# TakeIngressStatistics, Reset (ClearState -> match_.Reset). IpcHost.cpp calls
# the first four today; the others are 0 rows and would appear as new ones.
HOST_TOUCHES = ('SubmitInput', 'SubmitActionBatch', 'GetActionResults', 'NoteWireRejection', 'SubmitActions',
                'QueueActionAcknowledgement', 'TickCount', 'TakeIngressStatistics', 'Reset')

WALL_CLOCK = re.compile(r'\bnow_\(\)|\bNow\(\)|\b\w*[Cc]lock::now\(\)|\bEngine::Time::Now\w*\(\)|'
                        r'\bMovementTraceNowNs\(\)')
GLOBAL_DIAGNOSTIC = re.compile(r'\bTraceMovement\(|\bMovementDiagEnabled\(\)|\bRecordMovementDiag\(')
MATCH_TOUCH = re.compile(r'\bmatch_\.(\w+)\(')
HOST_TOUCH = re.compile(r'\bhost\.(%s)\(' % '|'.join(HOST_TOUCHES))
FAILURES_WRITE = re.compile(r'\bconnectionQualityFailures\s*=[^=]')

W_FILES = [SOURCE / 'PvpMatch.cpp', SOURCE / 'MatchRuntimeHost.cpp', SOURCE / 'LocalPlayerPrediction.cpp',
           SIM / 'Movement.cpp', INCLUDE / 'MovementTrace.hpp', INCLUDE / 'MovementDiag.hpp']
G_FILES = [SOURCE / 'PvpMatch.cpp', SOURCE / 'MatchRuntimeHost.cpp', SOURCE / 'LocalPlayerPrediction.cpp',
           SIM / 'Movement.cpp']

# (class, file, function, call shape) -> count. Today's sites at 1e7625d9; the
# "audit: not listed" rows are off-thread reads audit section 3.4 did not list,
# reported to the architect in the batch's dev_log.
WHITELIST = {
    # W: spec G.4 items 1 (ShotReferenceAge and publishedAt by now_ in Step), 2 (SubmitInput's now_),
    # 3 (MovementTraceNowNs reads steady_clock); lockWaits and Body pacing are diagnostics on the wall clock.
    ('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::RecordRejection', 'MovementTraceNowNs()'): 1,
    ('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::SubmitInput', 'steady_clock::now()'): 2,
    ('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::SubmitInput', 'now_()'): 1,
    ('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::Step', 'steady_clock::now()'): 2,
    ('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::Step', 'now_()'): 2,
    ('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::Step', 'MovementTraceNowNs()'): 1,
    ('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::Body', 'steady_clock::now()'): 3,
    ('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::Now', 'now_()'): 1,
    ('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::TrackSlack', 'now_()'): 1,
    ('W', 'MovementTrace.hpp', 'MovementTraceNowNs', 'steady_clock::now()'): 1,
    ('W', 'MovementTrace.hpp', 'TraceMovement', 'MovementTraceNowNs()'): 1,
    ('W', 'MovementDiag.hpp', 'RecordMovementDiag', 'MovementTraceNowNs()'): 1,
    # G: spec G.4 item 3 (the globals are at the call sites; Movement.cpp is clean).
    ('G', 'PvpMatch.cpp', 'PvpMatch::ResetMovementEpoch', 'TraceMovement('): 1,
    ('G', 'PvpMatch.cpp', 'PvpMatch::Tick', 'MovementDiagEnabled()'): 1,
    ('G', 'PvpMatch.cpp', 'PvpMatch::Tick', 'RecordMovementDiag('): 1,
    ('G', 'PvpMatch.cpp', 'PvpMatch::Tick', 'TraceMovement('): 1,
    ('G', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::SubmitInput', 'TraceMovement('): 1,
    ('G', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::Step', 'TraceMovement('): 2,
    ('G', 'LocalPlayerPrediction.cpp', 'PredictStep', 'MovementDiagEnabled()'): 1,
    ('G', 'LocalPlayerPrediction.cpp', 'PredictStep', 'RecordMovementDiag('): 1,
    ('G', 'LocalPlayerPrediction.cpp', 'LocalPlayerPrediction::SeedLead', 'TraceMovement('): 1,
    ('G', 'LocalPlayerPrediction.cpp', 'LocalPlayerPrediction::Resume', 'TraceMovement('): 1,
    ('G', 'LocalPlayerPrediction.cpp', 'LocalPlayerPrediction::TracePhaseDecision', 'TraceMovement('): 1,
    ('G', 'LocalPlayerPrediction.cpp', 'LocalPlayerPrediction::Reconcile', 'TraceMovement('): 1,
    ('G', 'LocalPlayerPrediction.cpp', 'LocalPlayerPrediction::Advance', 'TraceMovement('): 2,
    # A: a thread other than the simulation role touching the authority (the IPC thread unless the row
    # says otherwise). Spec G.4 items 4 (RequestMovementResync) and 5 (SubmitInput's Snapshot,
    # SubmitActionBatch's validation, IpcHost 645/685); the rest "audit: not listed".
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::IngressBucketId', 'match_.ContainsPlayer('): 1,  # audit: not listed
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::RecordRejection', 'match_.ContainsPlayer('): 1,  # audit: not listed
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::RecordRejection', 'match_.Snapshot('): 1,  # audit: not listed
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::SubmitInput', 'match_.AdmitInput('): 1,
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::SubmitInput', 'match_.Snapshot('): 1,
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::SubmitInput', 'match_.RequestMovementResync('): 1,
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::SubmitInput', 'match_.TickCount('): 1,
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::SubmitActionBatch', 'match_.ContainsPlayer('): 1,
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::SubmitActionBatch', 'match_.CanAcknowledgeActions('): 1,
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::SubmitActionBatch', 'match_.CanSubmitActions('): 1,
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::SubmitActionBatch', 'match_.GetActionResults('): 1,
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::QueueActionAcknowledgement', 'match_.CanAcknowledgeActions('): 1,  # no production caller (tests only)
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::GetActionResults', 'match_.GetActionResults('): 1,
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::TickCount', 'match_.TickCount('): 1,  # audit: not listed; main thread after Stop (match_main)
    ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::TakeIngressStatistics', 'match_.Snapshot('): 1,  # audit: not listed; main thread while the role runs (match_main), under the mutex
    ('A', 'IpcHost.cpp', 'Receive', 'host.SubmitInput('): 1,
    ('A', 'IpcHost.cpp', 'Receive', 'host.SubmitActionBatch('): 1,
    ('A', 'IpcHost.cpp', 'Receive', 'host.GetActionResults('): 1,
    ('A', 'IpcHost.cpp', 'Receive', 'host.NoteWireRejection('): 2,
    ('A', 'IpcHost.cpp', 'Pump', 'host.GetActionResults('): 1,
    ('A', 'IpcHost.cpp', 'ScheduleWrite', 'host.GetActionResults('): 1,
    ('A', 'IpcHost.cpp', 'Write', 'host.GetActionResults('): 1,
}

# D3: the only writer of connectionQualityFailures in src/Pvp is the host, on
# the snapshot copy inside JudgeConnectionQuality; the Match has none.
FAILURES_WRITERS = {('MatchRuntimeHost.cpp', 'MatchRuntimeHost::JudgeConnectionQuality'): 1}

_COMMENT_OR_STRING = re.compile(
    r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])\'', re.S)
_NOT_A_FUNCTION = re.compile(r'^\s*(namespace|class|struct|enum|union|extern)\b')
_FUNCTION_NAME = re.compile(r'([A-Za-z_][\w:]*)\s*\(')


def strip(text):
    """Replace comments and string/char literals by spaces of the same length (newlines kept)."""
    def blank(match):
        return re.sub(r'[^\n]', ' ', match.group(0))
    return _COMMENT_OR_STRING.sub(blank, text)


def scopes(text):
    """Every brace scope of stripped text: (header_start, open, close, header text)."""
    result = []
    stack = []
    start = 0  # where the current header text begins
    for index, char in enumerate(text):
        if char == '{':
            stack.append((start, index))
            start = index + 1
        elif char == '}':
            if not stack:
                raise ValueError('unbalanced braces')
            header_start, open_at = stack.pop()
            result.append((header_start, open_at, index, text[header_start:open_at]))
            start = index + 1
        elif char == ';':
            if not stack or start > stack[-1][1]:
                start = index + 1
    if stack:
        raise ValueError('unbalanced braces')
    return sorted(result)


def function_of(text_scopes, position):
    """(function name, is_definition): the outermost function scope around
    position; is_definition when position is that function's own name in its header."""
    for header_start, open_at, close_at, header in text_scopes:
        if header_start <= position <= close_at and '(' in header and not _NOT_A_FUNCTION.match(header):
            name = _FUNCTION_NAME.search(header)
            if not name:
                return (header.strip(), False)
            return (name.group(1), header_start + name.start(1) <= position < header_start + name.end(1))
    return ('<file scope>', False)


def scan(text, pattern, label, shape=None):
    """Hits of pattern in text as {(label, function, shape): count}; a function's own definition is not a hit."""
    stripped = strip(text)
    text_scopes = scopes(stripped)
    found = {}
    for match in pattern.finditer(stripped):
        function, is_definition = function_of(text_scopes, match.start())
        if is_definition:
            continue
        key = (label, function, shape(match) if shape else match.group(0))
        found[key] = found.get(key, 0) + 1
    return found


def read_source(path):
    return path.read_text(encoding='utf-8')


def table(read=read_source):
    """The (class, file, function, shape) -> count table of the scanned files; read may substitute text."""
    found = {}
    for path in W_FILES:
        for (label, function, shape), count in scan(read(path), WALL_CLOCK, 'W').items():
            found[(label, path.name, function, shape)] = count
    for path in G_FILES:
        for (label, function, shape), count in scan(read(path), GLOBAL_DIAGNOSTIC, 'G').items():
            found[(label, path.name, function, shape)] = count
    for (label, function, shape), count in scan(read(SOURCE / 'MatchRuntimeHost.cpp'), MATCH_TOUCH, 'A').items():
        if function not in TICK_SIDE:
            found[(label, 'MatchRuntimeHost.cpp', function, shape)] = count
    for (label, function, shape), count in scan(read(SOURCE / 'IpcHost.cpp'), HOST_TOUCH, 'A').items():
        found[(label, 'IpcHost.cpp', function, shape)] = count
    return found


def differences(found, expected):
    lines = []
    for key in sorted(set(found) | set(expected)):
        if found.get(key) != expected.get(key):
            lines.append(f'{key}: whitelist {expected.get(key)}, found {found.get(key)}')
    return lines


def with_edit(path, old, new, count=1):
    """A reader that serves path with old replaced by new (old must occur exactly count times)."""
    def read(candidate):
        text = read_source(candidate)
        if candidate == path:
            assert text.count(old) == count, (old, text.count(old))
            text = text.replace(old, new)
        return text
    return read


class KernelPrepSitesTest(unittest.TestCase):
    HOST = SOURCE / 'MatchRuntimeHost.cpp'
    QUEUE_JOIN = 'bool MatchRuntimeHost::QueueJoin(std::uint64_t requestId, PlayerId playerId) {\n'

    def test_the_sites_equal_the_whitelist(self):
        found = table()
        self.assertEqual(differences(found, WHITELIST), [],
                         'authority wall-clock/diagnostic/off-thread sites changed; see the module docstring')
        # The whitelist is the positive example: every class is present.
        for label in 'WGA':
            self.assertTrue(any(key[0] == label for key in found), label)

    def test_the_tick_side_set_is_the_reviewed_one(self):
        self.assertEqual(TICK_SIDE, {'MatchRuntimeHost::Step', 'MatchRuntimeHost::Body', 'MatchRuntimeHost::TrackSlack',
                                     'MatchRuntimeHost::JudgeConnectionQuality', 'MatchRuntimeHost::ClearState',
                                     'MatchRuntimeHost::RemovePlayerState'})

    def test_connection_quality_failures_has_one_writer_on_the_host(self):
        found = {}
        # Every product source directory (Pvp, Sim, Present, App): the Match's Movement.cpp moved to Sim in
        # v8-product-kernel-prep-zones, and a writer there is exactly what D3 forbids (review 2026-11-12, must).
        for path in sorted(SOURCE.parent.rglob('*.cpp')):
            for (_, function, _), count in scan(read_source(path), FAILURES_WRITE, 'D3').items():
                found[(path.name, function)] = count
        self.assertEqual(found, FAILURES_WRITERS)
        self.assertNotIn('connectionQualityFailures', read_source(SOURCE / 'PvpMatch.cpp'))
        # An assignment is a write; a comparison is not.
        self.assertEqual(scan('void F() {\n    if (p.connectionQualityFailures == 0) return;\n'
                              '    p.connectionQualityFailures = 1;\n}\n', FAILURES_WRITE, 'D3'),
                         {('D3', 'F', 'connectionQualityFailures = '): 1})

    def test_a_synthetic_new_read_point_is_reported(self):
        # Through the real table: a read added to a function without one (QueueJoin)
        # is a new row of its class; the TICK_SIDE filter does not swallow it.
        self.assertEqual(read_source(self.HOST).count(self.QUEUE_JOIN), 1)
        for insertion, key in (('    static_cast<void>(now_());\n', ('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::QueueJoin', 'now_()')),
                               ('    static_cast<void>(Engine::Time::Now());\n', ('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::QueueJoin', 'Engine::Time::Now()')),
                               ('    static_cast<void>(match_.TickCount());\n', ('A', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::QueueJoin', 'match_.TickCount('))):
            with self.subTest(key=key):
                found = table(with_edit(self.HOST, self.QUEUE_JOIN, self.QUEUE_JOIN + insertion))
                self.assertEqual(differences(found, WHITELIST), [f'{key}: whitelist None, found 1'])

    def test_a_changed_count_and_a_removed_site_are_reported(self):
        # One more read in a function that has some: the count changes (architect 2026-11-12: a changed
        # count fails too). Now()'s one read removed: the row is missing (the ratchet holds both ways).
        more = table(with_edit(self.HOST, '    return QueueControl({requestId, playerId, ControlKind::Join});\n',
                               '    static_cast<void>(now_());\n    return QueueControl({requestId, playerId, ControlKind::Join});\n'))
        self.assertEqual(differences(more, WHITELIST),
                         [f"{('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::QueueJoin', 'now_()')}: whitelist None, found 1"])
        step_more = table(with_edit(self.HOST, '        if (!advancedAt) advancedAt = now_();\n',
                                    '        if (!advancedAt) advancedAt = now_();\n        static_cast<void>(now_());\n'))
        self.assertEqual(differences(step_more, WHITELIST),
                         [f"{('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::Step', 'now_()')}: whitelist 2, found 3"])
        gone = table(with_edit(self.HOST, 'std::chrono::steady_clock::time_point MatchRuntimeHost::Now() const {\n    return now_();\n',
                               'std::chrono::steady_clock::time_point MatchRuntimeHost::Now() const {\n    return {};\n'))
        self.assertEqual(differences(gone, WHITELIST),
                         [f"{('W', 'MatchRuntimeHost.cpp', 'MatchRuntimeHost::Now', 'now_()')}: whitelist 1, found None"])

    def test_a_touch_on_the_tick_side_is_not_an_off_thread_row(self):
        # The same match_ call inside Step (the role's thread) adds no A row: the filter is what makes
        # the A class "off-thread", so it must be exercised in the direction that excludes.
        anchor = '        auto state = match_.Snapshot();\n'
        found = table(with_edit(self.HOST, anchor, anchor + '        static_cast<void>(match_.TickCount());\n'))
        self.assertEqual(differences(found, WHITELIST), [])

    def test_the_scope_parser_on_known_shapes(self):
        text = ('namespace a::b {\n'
                'struct S final {\n    int x{1};\n};\n'
                'class Timer : public Base<sizeof(int)> {\n'  # a parenthesis in a class header is not a function
                'public:\n    void Read() { steady_clock::now(); }\n};\n'
                'inline std::int64_t Nowish() noexcept {\n    return steady_clock::now().time_since_epoch().count();\n}\n'
                'K::K(T t) : m_(std::move(t)), t0_(now_()) {}\n'  # an initializer list reads the clock: counted
                'bool K::Run(const T& t, Clock::time_point at = SteadyClock::now()) {\n'  # a default argument too
                '    const auto f = [&] { if (!r) r = now_(); return *r; };  // now_() in a comment\n'
                '    /* system_clock::now() in a block\n       comment */\n'
                '    const char* s = "now_()"; const char c = \'{\';\n'
                '    for (auto& x : xs) { TraceMovement({.kind = K::R}); }\n'
                '    return now_() == f() && system_clock::now() > Engine::Time::NowNs();\n}\n'
                '} // namespace\n')
        self.assertEqual(scan(text, WALL_CLOCK, 'W'), {
            ('W', 'Read', 'steady_clock::now()'): 1,  # in-class definitions carry no class qualifier
            ('W', 'Nowish', 'steady_clock::now()'): 1,
            ('W', 'K::K', 'now_()'): 1,
            ('W', 'K::Run', 'SteadyClock::now()'): 1,
            ('W', 'K::Run', 'now_()'): 2,
            ('W', 'K::Run', 'system_clock::now()'): 1,
            ('W', 'K::Run', 'Engine::Time::NowNs()'): 1,
        })
        self.assertEqual(scan(text, GLOBAL_DIAGNOSTIC, 'G'), {('G', 'K::Run', 'TraceMovement('): 1})
        # A definition's own name is not a call; a call of the same shape elsewhere is.
        definition = 'inline std::int64_t MovementTraceNowNs() noexcept {\n    return 1;\n}\nvoid G() { MovementTraceNowNs(); }\n'
        self.assertEqual(scan(definition, WALL_CLOCK, 'W'), {('W', 'G', 'MovementTraceNowNs()'): 1})


if __name__ == '__main__':
    unittest.main()
