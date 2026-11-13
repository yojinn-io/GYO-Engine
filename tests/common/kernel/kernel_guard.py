"""Text guard for the Engine kernel (spec J-9).

Sources of the rules: docs/architecture/target/kernel.zh-Hant.md DI-KS 3 (order comes only from ids and the
Plan) and DI-KS 15 (the kernel is neutral, owns no thread, reads no clock or random source);
docs/architecture/target/naming.zh-Hant.md DI 4 (Engine names are neutral); docs/architecture/target/
error-handling.md DI 2 (the Executor's handler is the only place a kernel error is logged). Declared in
docs/architecture/plans/kernel/kernel-scaffold-declaration.zh-Hant.md section 3.1.

Scans every C/C++ file and CMakeLists.txt under engine/kernel/ for:

  K1  a product or tool name, or a connection-role word, in code or comments (CMakeLists.txt included):
        (i)   the ids registered in engine/config/projects.csv and engine/config/tools.csv, as words:
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
        (ii)  PRODUCT_ALIASES: names of registered products that the registry id does not spell.
        (iii) ROLE_WORDS: connection-role words (entity_store W1).
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
      declaration shape it must have; the self-test fails when a third is added. Rule limit: only declarations
      with the keyword are seen (spec J-9 is keyword-based); a namespace-scope "inline int g;" is left to review.
  K5  an include outside an allowlist: a C++ standard library header (STANDARD_HEADERS) in angle brackets, or a
      project include under engine/base/ or engine/kernel/ with no ".." in it. No capability module, no platform
      header. An include directive (#include, #include_next, #import, %:include) whose target cannot be read
      as <...> or "..." (a macro, for example) is reported too (fail closed).
  K6  a log call outside the Executor: Log(, SetLogHandler, an include of engine/base/Log.hpp, <iostream>,
      <cstdio> or <print>, std::cout, std::cerr, std::clog, std::print, std::println, printf, fprintf, vprintf,
      vfprintf, puts, fputs, putchar, perror, fwrite, SDL_Log*. A file whose name starts with "Executor" is exempt
      from K6 only; every other rule applies to it.
  K7  the identifiers far, near, FAR and NEAR (macros in windef.h; PRs #152 and #163 each broke the MSVC build).
  K0  a file under engine/kernel/ that this guard cannot scan: anything but SUFFIXES and CMakeLists.txt. A file
      type the guard does not read would otherwise pass every rule unseen.

Comments and literals are blanked before K2-K7, keeping every line and column, so an include target is read from
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

SCANNED = "engine/kernel"
REGISTRIES = ("engine/config/projects.csv", "engine/config/tools.csv")
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp", ".tpp", ".inc", ".ixx"}
# CMakeLists.txt: K1 only. Any other file under engine/kernel/ is K0.

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
DIRECTIVE = re.compile(r"^[ \t]*(?:#|%:)[ \t]*(?:include_next|include|import)\b")
TARGET = re.compile(r"[ \t]*([<\"])([^>\"]+)[>\"]")
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
    # TypeKey<T>'s static inline char: only its address is used, as a lookup key (W-10 second sentence).
    # Provisional name; the registry-schedule batch fixes it (declaration LIM-5).
    "typeKeyTag": r"^static\s+inline\s+char\s+typeKeyTag\s*$",
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
    for pattern in [re.compile(p) for p in PRODUCT_ALIASES + ROLE_WORDS]:
        for match in pattern.finditer(raw):
            yield line_of(raw, match.start()), "K1", match.group(0)
    if Path(relative).suffix not in SUFFIXES:
        return
    executor_file = Path(relative).name.startswith("Executor")
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
        if target in LOG_HEADERS and not executor_file:
            yield number, "K6", target
        project = target.startswith(ALLOWED_PROJECT_INCLUDES) and ".." not in target.split("/")
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
    if not executor_file:
        for match in K6.finditer(code):
            yield line_of(code, match.start()), "K6", match.group(0).rstrip("( ")
    for match in K7.finditer(code):
        yield line_of(code, match.start()), "K7", match.group(0)


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
