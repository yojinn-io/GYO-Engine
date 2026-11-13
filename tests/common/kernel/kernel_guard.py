"""Text guard for the Engine kernel (spec J-9).

Sources of the rules: docs/architecture/target/kernel.zh-Hant.md DI-KS 3 (order comes only from ids and the
Plan) and DI-KS 15 (the kernel is neutral, owns no thread, reads no clock or random source);
docs/architecture/target/naming.zh-Hant.md DI 4 (Engine names are neutral); docs/architecture/target/
error-handling.md DI 2 (the Executor's handler is the only place a kernel error is logged). Declared in
docs/architecture/plans/kernel/kernel-scaffold-declaration.zh-Hant.md section 3.1.

Scans every C/C++ file and CMakeLists.txt under engine/kernel/ for:

  K1  a product or tool name, or a connection-role word, in code or comments (CMakeLists.txt included):
        (i)   the ids registered in engine/config/projects.csv and engine/config/tools.csv, as written and in
              CamelCase, ignoring case. Adding a product needs no change here; a removed product's id is no
              longer banned once its registration row is gone.
        (ii)  PRODUCT_ALIASES: names of registered products that the registry id does not spell.
        (iii) ROLE_WORDS: connection-role words (entity_store W1).
      The registry must be readable: a missing file or a missing "name" column is a finding (fail closed).
  K2  an unordered container or a hash: unordered_, std::hash.
  K3  a clock, random source, thread or atomic: an include of <chrono>, <random>, <thread>, <atomic>, <mutex>,
      <condition_variable>, <future>, <shared_mutex>, <stop_token>, <semaphore>, <latch>, <barrier>, <ctime>,
      <time.h>, a call rand( or srand(, or a name std::chrono, std::random_device, std::mt19937*, std::rand,
      std::srand, std::thread, std::jthread, std::this_thread, std::atomic*, std::mutex. There is no Parallel
      module exception: v1 has only the Serial executor (LIM-2); the batch that adds Parallel changes this file.
  K4  a static or thread_local variable that is not constexpr. ALLOWED_STATICS names exactly two symbols
      (spec A.6); the self-test fails when a third is added. A declaration runs from the keyword to the first
      ";", "{" or "="; it is a function, not a variable, when a "(" comes before the end and the parenthesized
      text is empty or begins with "Type name". A variable direct-initialized with parentheses
      ("static int x(0);") is reported (fail closed).
  K5  an include outside an allowlist: a C++ standard library header (STANDARD_HEADERS) in angle brackets, or a
      project include under engine/base/ or engine/kernel/. No capability module, no platform header.
  K6  a log call outside the Executor: Log(, SetLogHandler, an include of engine/base/Log.hpp, <iostream> or
      <cstdio>, std::cout, std::cerr, std::clog, printf, fprintf, vprintf, vfprintf, puts, fputs, SDL_Log*.
      A file whose name starts with "Executor" is exempt from K6 only.
  K7  the identifiers far and near (empty macros in windef.h; PRs #152 and #163 each broke the MSVC build).

Comments and literals are blanked before K2-K7. The scan root engine/kernel must exist and hold files: an
empty or missing root is a finding, so the guard never passes by scanning nothing.

Usage: python3 kernel_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import csv
import re
import sys
from pathlib import Path

SCANNED = "engine/kernel"
REGISTRIES = ("engine/config/projects.csv", "engine/config/tools.csv")
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl"}  # CMakeLists.txt: K1 only

# (ii) Product names the registry id does not spell. One line each: which product, which kind of name, and why
# the id does not give it.
PRODUCT_ALIASES = (
    r"RetroFPS",     # object_fps_pvp: include directory of its sources (apps/object_fps_pvp/src/RetroFPS); not derived from the id
    r"fps::pvp",     # object_fps_pvp: C++ namespace of its code; the id splits as object/fps/pvp, not fps::pvp
    r"\b[Pp]vp",     # object_fps_pvp: the short product word used in its type names (PvpMatch); only the id's last part
    r"Gyo::Tools",   # the four tools (tools.csv): their shared C++ namespace root; not part of any tool id
)
# (iii) Connection-role words; source: entity_store guard W1 (tests/common/entity_store/entity_store_guard.py).
# "Match" only as a whole capitalized word, so "matches", "Matcher" and "rematch" stay allowed.
ROLE_WORDS = (r"\bMatch\b", r"[Pp]layer", r"Gateway")

K2 = re.compile(r"\bunordered_\w*|\bstd::hash\b")
K3_HEADERS = ("chrono", "random", "thread", "atomic", "mutex", "condition_variable", "future", "shared_mutex",
              "stop_token", "semaphore", "latch", "barrier", "ctime", "time.h")
K3 = re.compile(r"\bstd::(?:chrono|random_device|mt19937\w*|s?rand|thread|jthread|this_thread|atomic\w*|mutex)\b"
                r"|(?<![\w:.])s?rand\s*\(")
LOG_HEADERS = ("iostream", "cstdio", "engine/base/Log.hpp")
K6 = re.compile(r"\bLog\s*\(|\bSetLogHandler\b|\bstd::c(?:out|err|log)\b|\bSDL_Log\w*"
                r"|(?<![\w:.])(?:std::)?(?:v?f?printf|f?puts)\s*\(")
K7 = re.compile(r"\b(?:far|near)\b")
INCLUDE = re.compile(r"^[ \t]*#[ \t]*include[ \t]*([<\"])([^>\"]+)[>\"]")
ALLOWED_PROJECT_INCLUDES = ("engine/base/", "engine/kernel/")
STANDARD_HEADERS = frozenset("""
    algorithm any array atomic barrier bit bitset charconv chrono compare complex concepts condition_variable
    coroutine deque exception execution expected filesystem flat_map flat_set format forward_list fstream
    functional future generator initializer_list iomanip ios iosfwd iostream istream iterator latch limits list
    locale map mdspan memory memory_resource mutex new numbers numeric optional ostream print queue random ranges
    ratio regex scoped_allocator semaphore set shared_mutex source_location span spanstream sstream stack
    stacktrace stdexcept stop_token streambuf string string_view syncstream system_error thread tuple type_traits
    typeindex typeinfo unordered_map unordered_set utility valarray variant vector version
    cassert cctype cerrno cfenv cfloat cinttypes climits clocale cmath csetjmp csignal cstdarg cstddef cstdint
    cstdio cstdlib cstring ctime cuchar cwchar cwctype
""".split())
STATIC = re.compile(r"\b(?:static|thread_local)\b")
PARAMETER = re.compile(r"^\s*(?:const\s+|volatile\s+)*[\w:<>]+[\s&*]+[&*\s]*\w+")
DECLARED_NAME = re.compile(r"(\w+)\s*(?:\[[^\]]*\]\s*)*$")

# Spec A.6: the only two non-constexpr static or thread_local variables the kernel may declare. Each entry is
# symbol name -> (word the declaration must contain, reason). Adding a symbol is a change to this file, which
# review sees; the self-test pins the set to exactly these two names.
ALLOWED_STATICS = {
    # Access verification module: read only while verification is on; takes no part in ordering or results.
    "tlCurrent": ("thread_local", "current-System context of access verification (spec A.6)"),
    # TypeKey<T>'s static inline char: only its address is used, as a lookup key (W-10 second sentence).
    # Provisional name; the registry-schedule batch fixes it (declaration LIM-5).
    "typeKeyTag": ("char", "address-only type key of TypeKey<T> (spec A.6)"),
}


def strip_comments_and_strings(text):
    """Replaces comments, string and character literals with spaces, keeping line breaks."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            end = text.find("\n", i)
            end = n if end < 0 else end
            out.append(" " * (end - i)); i = end
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        elif c == "R" and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_")):
            delimiter_end = text.find("(", i + 2)
            closing = ")" + text[i + 2:delimiter_end] + '"'
            end = text.find(closing, delimiter_end)
            end = n if end < 0 else end + len(closing)
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        elif c in "\"'" and not (c == "'" and i > 0 and text[i - 1].isalnum()):
            # A ' after a digit or letter is a digit separator (1'000), not a character literal.
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            end = min(j + 1, n)
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        else:
            out.append(c); i += 1
    return "".join(out)


def line_of(text, offset):
    return text.count("\n", 0, offset) + 1


def camel_case(identifier):
    return "".join(part[:1].upper() + part[1:] for part in identifier.split("_") if part)


def registered_names(root):
    """(patterns, problems): K1 patterns for every registered id, and findings for an unreadable registry."""
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
                for spelling in {identifier, camel_case(identifier)}:
                    patterns.append(re.compile(re.escape(spelling), re.IGNORECASE))
    return patterns, problems


def static_declaration(code, start):
    """(end, is_variable, name) for the static or thread_local declaration starting at start."""
    end = len(code)
    for terminator in (";", "{", "="):
        found = code.find(terminator, start)
        if 0 <= found < end:
            end = found
    declaration = code[start:end]
    name_match = DECLARED_NAME.search(declaration)
    name = name_match.group(1) if name_match else None
    if re.search(r"\b(?:constexpr|consteval)\b", declaration):
        return end, False, name
    open_paren = declaration.find("(")
    if open_paren < 0:
        return end, True, name
    close_paren = declaration.find(")", open_paren)
    inside = declaration[open_paren + 1:close_paren if close_paren >= 0 else len(declaration)]
    is_function = inside.strip() == "" or bool(PARAMETER.match(inside))
    # A variable direct-initialized with parentheses is never matched against the allowlist (fail closed).
    return end, not is_function, None


def allowed_static(declaration, name):
    if name not in ALLOWED_STATICS:
        return False
    required_word, _reason = ALLOWED_STATICS[name]
    return re.search(r"\b" + re.escape(required_word) + r"\b", declaration) is not None


def file_findings(relative, raw, name_patterns):
    """(line, rule, text) for one file."""
    for pattern in name_patterns + [re.compile(p) for p in PRODUCT_ALIASES + ROLE_WORDS]:
        for match in pattern.finditer(raw):
            yield line_of(raw, match.start()), "K1", match.group(0)
    if Path(relative).suffix not in SUFFIXES:
        return
    executor_file = Path(relative).name.startswith("Executor")
    code = strip_comments_and_strings(raw)
    # Include targets are string literals, which the stripping blanks: read them from the raw text of the lines
    # that are includes in the code.
    for number, (raw_line, code_line) in enumerate(zip(raw.splitlines(), code.splitlines()), start=1):
        if not re.match(r"^[ \t]*#[ \t]*include\b", code_line):
            continue
        include = INCLUDE.match(raw_line)
        if not include:
            continue
        kind, target = include.groups()
        if kind == "<" and target in K3_HEADERS:
            yield number, "K3", "<" + target + ">"
        if target in LOG_HEADERS and not executor_file:
            yield number, "K6", target
        if not (target.startswith(ALLOWED_PROJECT_INCLUDES) or (kind == "<" and target in STANDARD_HEADERS)):
            yield number, "K5", target
    for match in K2.finditer(code):
        yield line_of(code, match.start()), "K2", match.group(0)
    for match in K3.finditer(code):
        yield line_of(code, match.start()), "K3", match.group(0).rstrip("( ")
    covered = -1
    for match in STATIC.finditer(code):
        if match.start() < covered:
            continue  # "static thread_local x;" is one declaration
        end, is_variable, name = static_declaration(code, match.start())
        covered = end
        if is_variable and not allowed_static(code[match.start():end], name):
            yield line_of(code, match.start()), "K4", f"{match.group(0)} {name or '(unnamed)'}"
    if not executor_file:
        for match in K6.finditer(code):
            yield line_of(code, match.start()), "K6", match.group(0).rstrip("( ")
    for match in K7.finditer(code):
        yield line_of(code, match.start()), "K7", match.group(0)


def scanned_files(root):
    base = Path(root) / SCANNED
    if not base.is_dir():
        return []
    return sorted(p for p in base.rglob("*") if p.is_file() and (p.suffix in SUFFIXES or p.name == "CMakeLists.txt"))


def check(root):
    root = Path(root)
    files = scanned_files(root)
    if not files:
        return [f"{SCANNED}: no source to scan (moved or renamed? update this guard)"]
    name_patterns, problems = registered_names(root)
    for path in files:
        relative = path.relative_to(root).as_posix()
        raw = path.read_text(encoding="utf-8")
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
