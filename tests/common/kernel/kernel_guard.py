"""Text guard for the Engine kernel (spec J-9).

Sources of the rules: docs/architecture/target/kernel.zh-Hant.md DI-KS 3 (order comes only from ids and the
Plan) and DI-KS 15 (the kernel is neutral, owns no thread, reads no clock or random source);
docs/architecture/target/naming.zh-Hant.md DI 4 (Engine names are neutral); docs/architecture/target/
error-handling.md DI 2 (the kernel has no error handler and no log call; error levels D1 removed the
Executor's exemption). Declared in
docs/architecture/plans/kernel/kernel-scaffold-declaration.zh-Hant.md section 3.1.

Scans every C/C++ file and CMakeLists.txt under engine/kernel/ for:

  K1  a product or tool name, or a connection-role word, in code or comments (CMakeLists.txt included):
        (i)   the ids registered in config/games.csv and config/tools.csv, as words:
              - as written (snake form), ignoring case, bounded by the start or end of the text or by a character
                that is not a letter or digit ("_" is a boundary): id arena matches arena, ARENA_SIZE, k_arena;
              - in CamelCase, case-sensitive, from the start of a word (no uppercase letter before it, so an
                identifier start or a lowercase-to-uppercase turn) to the end of a word (no lowercase letter
                after it): Arena matches FrameArena and ArenaHost, not Marenal, parenage or Arenas.
              A product name used as a prefix (ObjectFpsPvpHost) is still caught; matching whole identifiers only
              would miss it. A hit has two readings, and the message names both: the kernel uses a product's
              name (change the kernel), or a newly registered product id is an Engine generic word (change the
              product's registered id). The cost of a collision falls on the new product's name, not on the
              common layer (AGENTS 7, 12). Adding a product needs no change here; a removed product's id is no
              longer banned once its registration row is gone.
        (ii)  ROLE_WORDS: connection-role words (entity_store W1).
      No alias list for names the registry id does not spell: the four aliases of the old products on master
      (RetroFPS, fps::pvp, Pvp, Gyo::Tools) were dropped (user 2026-11-18, v8-arch-gyo2-mutation-prune).
      The registry must be readable: a missing file or a missing "name" column is a finding (fail closed).
  K2  an unordered container or a hash: unordered_, std::hash, and hash< (std::hash under using namespace std).
  K3  a clock, random source, thread or atomic: an include of <chrono>, <random>, <thread>, <atomic>, <mutex>,
      <condition_variable>, <future>, <shared_mutex>, <stop_token>, <semaphore>, <latch>, <barrier>, <ctime>,
      <time.h>, a call rand( or srand(, or a name std::chrono, std::random_device, std::mt19937*, std::rand,
      std::srand, std::thread, std::jthread, std::this_thread, std::atomic*, std::mutex, std::execution,
      std::clock, std::time, a call ::rand(, time( or clock(, and an include of <execution>. There is no Parallel
      module exception: v1 has only the Serial executor (LIM-2); the batch that adds Parallel changes this file.
  K4  a static or thread_local variable that is not constexpr (constexpr or consteval anywhere in the
      declaration, before or after the keyword). A declaration runs from the keyword to the first ";", "{" or
      "="; it declares a function when a "(" comes before the end and the parenthesized text is empty or starts
      like a parameter list (a type keyword, an attribute, or a name followed by more than ")"). A single bare
      name or a literal in the parentheses reads as direct initialization ("static int x(0);", "static T x(y);")
      and is reported (fail closed). ALLOWED_STATICS names exactly two symbols (spec A.6), each with the exact
      declaration shape it must have; the self-test fails when a third is added.
      Also an "inline" variable without static / thread_local (a namespace-scope "inline int g = 0;"): it has
      static storage like a static one, and when no kernel source odr-uses it, the library's symbol table (the
      writable-symbol guard kernel_symbols.py) never sees it (review B8; architect 2026-11-13). Allowed: constexpr
      or consteval, const applied to the variable (not only to a pointee), inline functions and inline namespaces.
  K5  an include outside an allowlist: a C++ standard library header (STANDARD_HEADERS) in angle brackets, or a
      project include under engine/base/, engine/entity_store/ (Delta-3, world-exec) or engine/kernel/ with no ".."
      in it. No capability module, no platform
      header. An include directive (#include, #include_next, #import, %:include) whose target cannot be read
      as <...> or "..." (a macro, for example) is reported too (fail closed).
  K6  a log call anywhere in the kernel: Log(, SetLogHandler, an include of engine/base/Log.hpp, <iostream>,
      <cstdio> or <print>, std::cout, std::cerr, std::clog, std::print, std::println, printf, fprintf, vprintf,
      vfprintf, puts, fputs, putchar, perror, fwrite, SDL_Log*. No file is exempt (the Executor's exemption ended
      with error levels D1: the outermost Run returns the Err).
  K7  the identifiers far, near, FAR and NEAR (macros in windef.h; PRs #152 and #163 each broke the MSVC build).
  K8  GYO::Base's issuer of numbers in the kernel: the identifier IdIssuer, or Base's thread serial state
      threadSerials / threadSerial. The kernel asks which thread runs through GYO::Engine::Base::CurrentThreadSerial()
      only (DI-KS 14; user 2026-11-18, v8-arch-gyo2-kernel-subtract-structure): an IdIssuer is an atomic counter,
      cross-thread state that K3 keeps out of the kernel but does not name. Read in every scanned file, the
      Executor's included, and in CMakeLists.txt outside "#" comments (a target_compile_definitions alias). An
      include line is not read (its target names the header the kernel needs for CurrentThreadSerial). The kernel's
      own names may not be these three either. Limits (text level, the rest is review, like the scaffold
      declaration's LIM-7): a name built by token pasting (##) or split by a line continuation, an asm label or a
      compiler builtin such as __atomic_fetch_add; and the comment and literal blanking shared by K2-K8 does not
      read a prefixed character literal (L'"') or a prefixed raw string (LR"(...)") right, so text after one on
      the same line can hide a name (an existing gap of J-9, recorded apart).
  K9  a name of what moved out of the kernel to the orchestration layer: the registration units (Plugin,
      FinishContext, InstallRunner, Requires(, the plugin list), game initialization (the startup schedule and its
      checks) and the runner owners (DI-KS 15: the kernel knows no game initialization, registration unit or
      runner). Read in the raw text of every scanned C/C++ file (comments and string literals included, so the old
      rule codes "H-12"/"H-13" are seen). Kept from the one-time scan G-3 of the orchestration-skeleton
      declaration (section 11), made standing by v8-arch-gyo2-time-events step 1 (that declaration's section 7.4).
      StartupCode and StartupReport are the kernel's startup errors and stay allowed.
      Since v8-arch-gyo2-authority-rollback step 1 also the clock that moved out to the time Module (K9_CLOCK: the
      old clock domain, CurrentTick, the tick-to-stamp table, the time Module's type names), written as identifiers
      so the comments saying the kernel keeps no clock stay allowed.
      Since v8-arch-gyo2-exchange step 0 also the names of what the kernel no longer has (K9_REMOVED: the zones,
      the write policy and the exclusive-writer check, the rollback flags and roles, the ingress flag, the digest
      marks, the C fingerprint and the zone rule codes; DI-KS 15: the kernel knows no zone, authority or rollback
      strategy), the one-time scan of authority-rollback-declaration section 13 (judgment 4) made standing as its
      section 9.5 deferred. Written as identifiers, so lower-case prose in comments saying the kernel keeps no zone
      or digest stays allowed; a capitalized identifier (Zone..., Rollback::, "Z1") counts in a comment too.
  K0  a file under engine/kernel/ that this guard cannot scan: anything but SUFFIXES and CMakeLists.txt. A file
      type the guard does not read would otherwise pass every rule unseen.

Comments and literals are blanked before K2-K8, keeping every line and column, so an include target is read from
the raw text at the column where the code has the directive. Files are read as UTF-8 with an optional BOM. The scan root engine/kernel must exist and hold files: an
empty or missing root is a finding, so the guard never passes by scanning nothing.

Usage: python3 kernel_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import csv
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "support"))
from source_text import (  # noqa: E402  (the C/C++ text reading shared by the guards)
    DIRECTIVE, STANDARD_HEADERS, SUFFIXES, TARGET, camel_case, line_of, strip_comments_and_strings)

SCANNED = "engine/ecs/kernel"
REGISTRIES = ("config/games.csv", "config/tools.csv")
# CMakeLists.txt: K1 only. Any other file under engine/kernel/ is K0.

# (ii) Connection-role words; source: entity_store guard W1 (tests/common/entity_store/entity_store_guard.py).
# "Match" only as a whole capitalized word, so "matches", "Matcher" and "rematch" stay allowed.
ROLE_WORDS = (r"\bMatch\b", r"[Pp]layer", r"Gateway")

K2 = re.compile(r"\bunordered_\w*|\bstd::hash\b|(?<![\w:])hash\s*<")
K3_HEADERS = ("chrono", "random", "thread", "atomic", "mutex", "condition_variable", "future", "shared_mutex",
              "stop_token", "semaphore", "latch", "barrier", "ctime", "time.h", "execution")
K3 = re.compile(r"\bstd::(?:chrono|random_device|mt19937\w*|s?rand|thread|jthread|this_thread|atomic\w*|mutex|execution"
                r"|clock|time)\b"
                r"|(?<![\w.:])(?:::)?(?:s?rand|time|clock)\s*\(")
LOG_HEADERS = ("iostream", "cstdio", "print", "engine/base/Log.hpp")
K6 = re.compile(r"\bLog\s*\(|\bSetLogHandler\b|\bstd::(?:c(?:out|err|log)|println|print)\b|\bSDL_Log\w*"
                r"|(?<![\w:.])(?:std::)?(?:v?f?printf|f?puts|putchar|perror|fwrite)\s*\(")
K7 = re.compile(r"\b(?:far|near|FAR|NEAR)\b")
K8 = re.compile(r"\b(?:IdIssuer|threadSerials?)\b")
K9_MOVED = (
    r"\bclass\s+Plugin\b", r"\bFinishContext\b", r"\bInstallRunner\b", r"\bRequires\s*\(",
    r"std::vector<Plugin\s*\*>", r"Registry::Finish\b", r"\.startup\b", r"\bstartup(?:_|Ran_|Running_|Schedule)\b",
    r"\bStartupScope\b", r"\bRunStartup\b", r"\bStartupNotNestable\b", r"\bStartupRunning\b", r"\bDE21\b",
    r"\bRE[136]\b", r'"H-1[23]"', r"\bPluginRecord\b", r"\bCheckPluginList\b", r"\bCheckRunners\b",
    r"\brunnerOwners\b")
# The clock that moved out to the time Module (v8-arch-gyo2-clock-out-of-kernel; added by
# v8-arch-gyo2-authority-rollback step 1, the guard time-events-declaration 6.3 deferred to batch 4): the kernel's old
# clock domain, its tick, the tick-to-stamp table and the time Module's type names. Written as identifiers only, so
# the kernel's comments saying it keeps no clock ("clock", "tick") stay allowed.
K9_CLOCK = (
    r"\bDomainTick\w*", r"\bCurrentTick\b", r"\bTickStamp\w*", r"\btickStamps_?\b", r"\bkTickStampSlots\b",
    r"\bDomainName\b", r"\benum\s+class\s+Domain\b", r"\bDomain::\w+", r"\brejectTick\b", r"\btickOfSchedule\b",
    r"\boptions\.clock\b", r"\bbool\s+clock\b",
    r"\b(?:VirtualTime|FixedTime|RealTime|ActiveTime|TimeControl|Resimulation|TickWriters|TickAccumulator)\b")
# What the kernel no longer has (v8-arch-gyo2-authority-rollback took it out; made standing by
# v8-arch-gyo2-exchange step 0, the guard authority-rollback-declaration 9.5 deferred). Source: the patterns of
# that declaration's judgment 4, written as identifiers (an identifier holding Zone, zoneX, .zone, .external,
# ingress...) so lower-case prose in comments ("the kernel keeps no zone or digest") stays allowed; a capitalized
# identifier (Zone..., Rollback::, "Z1") counts in a comment too. The digest itself is the authority Module's
# (DI-A 12): only the kernel's old digest marks are named here. The old startup codes (DE2, DE9, XE1, ...) are not
# listed: the StartupCode set is pinned by "DI-KS 15: the StartupCode values" (RegistryTests.cpp).
K9_REMOVED = (
    r"\b\w*Zone\w*", r"\bzone[A-Z]\w*", r"\.zone\b", r"\b[Ww]ritePolicy\b", r"\bCheckExclusiveWriters\b",
    r"\bFunnel\w*", r"\bRollback::", r"\benum\s+class\s+(?:Role|Rollback)\b", r"\bRole::(?:Input|State)\b",
    r"\brollback(?:Enabled|Safe|Active)\b", r"\bnotInRollback\b", r"\bingress\w*", r"\bbool\s+external\b",
    r"\.external\b", r"\b\w*DigestSet\b", r"\bdigestMarks\b", r"\bk?CFingerprint\w*", r"\bparallelGuardTest\b",
    r'"Z[0-9]"')
K9 = tuple(re.compile(p) for p in K9_MOVED + K9_CLOCK + K9_REMOVED)
# engine/entity_store/: Delta-3 of the migration plan (user 2026-11-13, world-exec): the World's columns are kept by
# entity_store's algorithms (target/world section 6, layout 2). The link side is pinned by
# gyo_expect_links(gyo_kernel GYO::Base GYO::EntityStore) in tests/common/kernel/CMakeLists.txt.
ALLOWED_PROJECT_INCLUDES = ("engine/base/", "engine/entity_store/", "engine/kernel/")
STATIC = re.compile(r"\b(?:static|thread_local)\b")
INLINE = re.compile(r"\binline\b")
TYPE_KEYWORDS = frozenset("void bool char wchar_t char8_t char16_t char32_t short int long float double signed "
                           "unsigned auto const volatile decltype typename struct class enum".split())
DECLARED_NAME = re.compile(r"(\w+)\s*(?:\[[^\]]*\]\s*)*$")

# Spec A.6: the only two non-constexpr static or thread_local variables the kernel may declare. Each entry is
# symbol name -> (word the declaration must contain, reason). Adding a symbol is a change to this file, which
# review sees; the self-test pins the set to exactly these two names.
ALLOWED_STATICS = {
    # Access verification module: a thread_local pointer to the current System's context, read only while
    # verification is on; takes no part in ordering or results.
    "tlCurrent": r"^(?:static\s+|inline\s+)*thread_local\s+(?:const\s+)?[\w:<>]+\s*\*\s*tlCurrent\s*$",
    # TypeKey<T>'s static inline char (engine/ecs/kernel/include/engine/kernel/TypeKey.hpp, Detail::TypeKeyOf<T>):
    # only its address is used, as a lookup key (W-10 second sentence). Named by the registry-schedule batch.
    "typeKeyAnchor": r"^static\s+inline\s+char\s+typeKeyAnchor\s*$",
}


def registered_patterns(identifier):
    """The two K1 word patterns for one registered id (see K1 (i))."""
    snake = re.compile(r"(?<![A-Za-z0-9])" + re.escape(identifier) + r"(?![A-Za-z0-9])", re.IGNORECASE)
    camel = re.compile(r"(?<![A-Z])" + re.escape(camel_case(identifier)) + r"(?![a-z])")
    return [snake, camel]


def registered_names(root):
    """(patterns, problems): (pattern, id) for every registered id, and findings for an unreadable registry."""
    patterns, problems = [], []
    for relative in REGISTRIES:
        path = Path(root) / relative
        if not path.is_file():
            problems.append(f"{relative}: K1 registry missing (product names cannot be checked)")
            continue
        with path.open(encoding="utf-8", newline="") as stream:
            reader = csv.DictReader(stream)
            if not reader.fieldnames or "name" not in reader.fieldnames:
                problems.append(f"{relative}: K1 registry has no name column")
                continue
            for row in reader:
                identifier = (row.get("name") or "").strip()
                if not identifier:
                    continue
                for pattern in registered_patterns(identifier):
                    patterns.append((pattern, identifier))
    return patterns, problems


def parameter_list(inside):
    """True when the text inside the first parentheses of a static declaration starts like a parameter list."""
    text = inside.strip()
    if text == "" or text.startswith("[["):
        return True
    first = re.match(r"(?:::)?\w+(?:::\w+)*", text)
    if not first:
        return False  # a literal, an address-of or another expression: direct initialization
    if first.group(0).split("::")[-1] in TYPE_KEYWORDS or first.group(0).startswith("std::"):
        return True
    rest = text[first.end():].lstrip()
    return rest != "" and not rest.startswith((",", "+", "-", "/", "%", "|", "^", "?", "."))


def static_declaration(code, start):
    """(end, is_variable, declaration) for the static or thread_local declaration starting at start."""
    end = len(code)
    for terminator in (";", "{", "="):
        found = code.find(terminator, start)
        if 0 <= found < end:
            end = found
    statement_start = max(code.rfind(";", 0, start), code.rfind("{", 0, start), code.rfind("}", 0, start)) + 1
    prefix = code[statement_start:start]
    declaration = code[start:end]
    if re.search(r"\b(?:constexpr|consteval)\b", prefix + declaration):
        return end, False, declaration
    open_paren = declaration.find("(")
    if open_paren < 0:
        return end, True, (prefix + declaration).strip()
    close_paren = declaration.rfind(")")
    inside = declaration[open_paren + 1:close_paren if close_paren > open_paren else len(declaration)]
    return end, not parameter_list(inside), (prefix + declaration).strip()


def inline_variable(code, start):
    """The variable name when the "inline" at start declares a mutable variable that K4's static rule does not
    cover; None otherwise (an inline function or namespace, constexpr, const, or a static / thread_local one)."""
    if re.match(r"inline\s+namespace\b", code[start:]):
        return None
    statement_start = max(code.rfind(";", 0, start), code.rfind("{", 0, start), code.rfind("}", 0, start)) + 1
    prefix = code[statement_start:start]
    if re.search(r"\b(?:static|thread_local|constexpr|consteval)\b", prefix):
        return None  # static / thread_local: the static rule; constexpr before inline: immutable
    end = len(code)
    for terminator in (";", "{", "="):
        found = code.find(terminator, start)
        if 0 <= found < end:
            end = found
    declaration = code[start:end]
    if re.search(r"\b(?:static|thread_local)\b", declaration):
        return None  # "inline static ...": the static rule sees it
    if re.search(r"\b(?:constexpr|consteval)\b", declaration):
        return None
    open_paren = declaration.find("(")
    if open_paren >= 0:
        close_paren = declaration.rfind(")")
        inside = declaration[open_paren + 1:close_paren if close_paren > open_paren else len(declaration)]
        if parameter_list(inside):
            return None  # a function
    name = DECLARED_NAME.search(declaration.split("(")[0].strip())
    declarator = declaration.split("(")[0]
    # const on the variable itself: "inline const int x", "inline int const x", "inline T* const p"; not a pointee only.
    pointer_part = declarator.rsplit("*", 1)
    if "*" in declarator:
        if re.search(r"\bconst\b", pointer_part[1]):
            return None
    elif re.search(r"\bconst\b", declarator):
        return None
    return name.group(1) if name else "(unnamed)"


def allowed_static(declaration):
    """True when the declaration has exactly one of the allowlisted shapes (one declarator, the exact type)."""
    shape = re.sub(r"\s+", " ", declaration)
    return any(re.match(pattern, shape) for pattern in ALLOWED_STATICS.values())


def file_findings(relative, raw, name_patterns):
    """(line, rule, text) for one file."""
    for pattern, identifier in name_patterns:
        for match in pattern.finditer(raw):
            yield line_of(raw, match.start()), "K1", (
                f"{match.group(0)} (registered id {identifier}: either the kernel uses a product's name, so change "
                f"the kernel, or a newly registered product id is an Engine generic word, so change that "
                f"product's registered id)")
    for pattern in [re.compile(p) for p in ROLE_WORDS]:
        for match in pattern.finditer(raw):
            yield line_of(raw, match.start()), "K1", match.group(0)
    if Path(relative).name == "CMakeLists.txt":
        for number, line in enumerate(raw.split("\n"), start=1):
            for match in K8.finditer(line.split("#", 1)[0]):
                yield number, "K8", match.group(0)
    if Path(relative).suffix not in SUFFIXES:
        return
    for pattern in K9:
        for match in pattern.finditer(raw):
            yield line_of(raw, match.start()), "K9", match.group(0)
    code = strip_comments_and_strings(raw)
    # Include targets in quotes are string literals, which the stripping blanks. The stripping keeps every line and
    # column, so the target is read from the raw text right after the directive the code shows. split("\n"), not
    # splitlines(): a form feed or U+2028 in a comment must not shift the raw lines against the code lines.
    for number, (raw_line, code_line) in enumerate(zip(raw.split("\n"), code.split("\n")), start=1):
        directive = DIRECTIVE.match(code_line)
        if not directive:
            continue
        include = TARGET.match(raw_line, directive.end())
        if not include:
            yield number, "K5", "unreadable include target: " + raw_line.strip()
            continue
        kind, target = include.groups()
        if kind == "<" and target in K3_HEADERS:
            yield number, "K3", "<" + target + ">"
        if target in LOG_HEADERS:
            yield number, "K6", target
        # ".." between "/" or "\\" separators (a backslash path reaches outside the allowlist on Windows too).
        project = target.startswith(ALLOWED_PROJECT_INCLUDES) and ".." not in re.split(r"[/\\]", target)
        if not (project or (kind == "<" and target in STANDARD_HEADERS)):
            yield number, "K5", target
    for match in K2.finditer(code):
        yield line_of(code, match.start()), "K2", match.group(0)
    for match in K3.finditer(code):
        yield line_of(code, match.start()), "K3", match.group(0).rstrip("( ")
    covered = -1
    for match in STATIC.finditer(code):
        if match.start() < covered:
            continue  # "static thread_local x;" is one declaration
        end, is_variable, declaration = static_declaration(code, match.start())
        covered = end
        if is_variable and not allowed_static(declaration):
            name = re.search(r"(\w+)\W*$", declaration)
            yield line_of(code, match.start()), "K4", f"{match.group(0)} {name.group(1) if name else '(unnamed)'}"
    for match in INLINE.finditer(code):
        finding = inline_variable(code, match.start())
        if finding:
            yield line_of(code, match.start()), "K4", "inline " + finding
    for match in K6.finditer(code):
        yield line_of(code, match.start()), "K6", match.group(0).rstrip("( ")
    for match in K7.finditer(code):
        yield line_of(code, match.start()), "K7", match.group(0)
    code_lines = code.split("\n")
    for match in K8.finditer(code):
        number = line_of(code, match.start())
        if not DIRECTIVE.match(code_lines[number - 1]):
            yield number, "K8", match.group(0)


def all_files(root):
    base = Path(root) / SCANNED
    return sorted(p for p in base.rglob("*") if p.is_file()) if base.is_dir() else []


def scannable(path):
    return path.suffix in SUFFIXES or path.name == "CMakeLists.txt"


def check(root):
    root = Path(root)
    files = all_files(root)
    if not any(scannable(path) for path in files):
        return [f"{SCANNED}: no source to scan (moved or renamed? update this guard)"]
    name_patterns, problems = registered_names(root)
    for path in files:
        relative = path.relative_to(root).as_posix()
        if not scannable(path):
            problems.append(f"{relative}:1: K0 file type this guard does not scan ({path.suffix or path.name})")
            continue
        raw = path.read_text(encoding="utf-8-sig")
        for number, rule, text in file_findings(relative, raw, name_patterns):
            problems.append(f"{relative}:{number}: {rule} {text}")
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[3]))
    args = parser.parse_args(argv)
    problems = check(args.root)
    for line in problems:
        print(line)
    if problems:
        print(f"kernel guard: {len(problems)} finding(s) (spec J-9; target/kernel DI-KS 3 and 15)")
        return 1
    print("kernel guard: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
