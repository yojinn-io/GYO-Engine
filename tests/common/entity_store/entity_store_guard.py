"""Text guard for the Engine entity store (docs/architecture/entity-store.md, Design Invariants 8 and 9).

Scans every C/C++ file and CMakeLists.txt under engine/entity_store/ for:

  W1  a product, tool or connection-role name, in code or comments: object_fps, RetroFPS, fps::pvp, Pvp/pvp,
      Player/player, Gateway, and the capitalized word Match (the lowercase English verb "match" is allowed).
  W2  an unordered container or a hash: unordered_, std::hash.
  W3  a clock, random source, thread or atomic: an include of <chrono>, <random>, <thread>, <atomic>, <mutex>,
      <condition_variable>, <future>, <shared_mutex>, <stop_token>, <semaphore>, <latch>, <barrier>, <ctime>,
      a call rand( or srand(, or a name std::chrono, std::random_device, std::mt19937*, std::rand, std::srand,
      std::thread, std::jthread, std::this_thread, std::atomic*, std::mutex.
  W4  a static or thread_local variable. In code (comments and literals blanked) the word "static" or
      "thread_local" starts a declaration that runs to the first ";", "{" or "=". It is allowed when
        - the declaration contains constexpr or consteval (static constexpr data or functions), or
        - it declares a function: a "(" comes before the end, and the parenthesized text is empty or begins
          with a parameter declaration "Type name" (a type token, then whitespace, & or *, then a name).
      static_assert and static_cast never match (the word is followed by "_"). A variable direct-initialized
      with parentheses and a single argument ("static int x(0);") is reported (fail closed); a static variable
      written so that it looks like a function declaration with a named parameter is left to review.
  W5  an include of a header that is neither the standard library nor engine/base or engine/entity_store.

Literals and comments are blanked before W2-W5 (W1 also reads comments: a product name in a comment is product
knowledge too). Engine-owned: names no product's path and reads nothing outside engine/entity_store.

Usage: python3 entity_store_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

SCANNED = "engine/entity_store"
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl"}  # CMakeLists.txt: W1 only

W1 = re.compile(r"object_fps|RetroFPS|fps::pvp|\b[Pp]vp|[Pp]layer|Gateway|\bMatch\b")
W2 = re.compile(r"\bunordered_\w*|\bstd::hash\b")
W3_HEADERS = ("chrono", "random", "thread", "atomic", "mutex", "condition_variable", "future", "shared_mutex",
              "stop_token", "semaphore", "latch", "barrier", "ctime", "time.h")
W3 = re.compile(r"\bstd::(?:chrono|random_device|mt19937\w*|s?rand|thread|jthread|this_thread|atomic\w*|mutex)\b"
                r"|(?<![\w:.])s?rand\s*\(")
INCLUDE = re.compile(r"^[ \t]*#[ \t]*include[ \t]*([<\"])([^>\"]+)[>\"]", re.MULTILINE)
ALLOWED_PROJECT_INCLUDES = ("engine/base/", "engine/entity_store/")
STATIC = re.compile(r"\b(?:static|thread_local)\b")
PARAMETER = re.compile(r"^\s*(?:const\s+|volatile\s+)*[\w:<>]+[\s&*]+[&*\s]*\w+")


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


def static_variable(code, start):
    """True when the static or thread_local declaration starting at start declares a variable."""
    end = len(code)
    for terminator in (";", "{", "="):
        found = code.find(terminator, start)
        if 0 <= found < end:
            end = found
    declaration = code[start:end]
    if re.search(r"\b(?:constexpr|consteval)\b", declaration):
        return False
    open_paren = declaration.find("(")
    if open_paren < 0:
        return True
    close_paren = declaration.find(")", open_paren)
    inside = declaration[open_paren + 1:close_paren if close_paren >= 0 else len(declaration)]
    return not (inside.strip() == "" or PARAMETER.match(inside))


def file_findings(relative, raw):
    """(line, rule, text) for one file."""
    for match in W1.finditer(raw):
        yield line_of(raw, match.start()), "W1", match.group(0)
    if Path(relative).suffix not in SUFFIXES:
        return
    code = strip_comments_and_strings(raw)
    # Include targets are string literals, which the stripping blanks: read them from the raw text of the lines
    # that are includes in the code.
    raw_lines, code_lines = raw.splitlines(), code.splitlines()
    for number, (raw_line, code_line) in enumerate(zip(raw_lines, code_lines), start=1):
        if not re.match(r"^[ \t]*#[ \t]*include\b", code_line):
            continue
        include = INCLUDE.match(raw_line)
        if not include:
            continue
        kind, target = include.groups()
        if kind == "<" and target in W3_HEADERS:
            yield number, "W3", "<" + target + ">"
        if "/" in target or kind == '"':
            if not target.startswith(ALLOWED_PROJECT_INCLUDES):
                yield number, "W5", target
    for match in W2.finditer(code):
        yield line_of(code, match.start()), "W2", match.group(0)
    for match in W3.finditer(code):
        yield line_of(code, match.start()), "W3", match.group(0).rstrip("( ")
    for match in STATIC.finditer(code):
        if static_variable(code, match.start()):
            yield line_of(code, match.start()), "W4", match.group(0)


def scanned_files(root):
    base = root / SCANNED
    if not base.is_dir():
        return []
    files = [p for p in base.rglob("*") if p.is_file() and (p.suffix in SUFFIXES or p.name == "CMakeLists.txt")]
    return sorted(files)


def check(root):
    root = Path(root)
    files = scanned_files(root)
    if not files:
        return [f"{SCANNED}: no source to scan (moved or renamed? update this guard)"]
    problems = []
    for path in files:
        relative = path.relative_to(root).as_posix()
        raw = path.read_text(encoding="utf-8")
        for number, rule, text in file_findings(relative, raw):
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
        print(f"entity store guard: {len(problems)} finding(s) (docs/architecture/entity-store.md, Design "
              f"Invariants 8 and 9)")
        return 1
    print("entity store guard: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
