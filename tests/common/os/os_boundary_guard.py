"""Guard for the Design Invariants of engine/os (docs/architecture/os.md).

Outside engine/os/src, C/C++ sources include no OS header (R1), branch on no OS macro in a preprocessor
condition (R2) and call no listed OS or CRT-split API (R3). Runtime code outside engine/os opens no native file
directly (R4: fstream, fopen, std::filesystem operations that touch the file system). engine/os itself includes
no other Engine module, SDL, asio or other library (R5).

Existing sites are frozen in ratchet allowlists named os_boundary_allowlist.txt, one per owner directory. Each
line is "path rule count # reason": the file must exist and its finding count for that rule must equal count, so
a new site fails and a removed one asks for the entry to shrink. The Engine allowlist (tests/common/os) may list
engine/ and tests/common/; any other allowlist only files under its own directory.

Usage: python3 os_boundary_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import posixpath
import re
import sys
from collections import Counter
from pathlib import Path

# Top-level roots scanned for R1-R3, and those of them that are runtime code (R4). os-guard-wide extends both to
# apps, tools, build/acceptance and the product test directories.
ROOTS = ("engine", "tests/common")
RUNTIME_ROOTS = ("engine",)
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".mm"}
# Never scanned: the OS layer's own tests (they may assert per OS), vendored code and build output. engine/os/src is
# scanned for R5 only; engine/os/include for everything, since public headers must not leak the OS either.
EXCLUDED = ("tests/common/os/", "third_party/", "build/target/")
OS_SOURCES = "engine/os/src/"
# Runtime code that R4 does not cover: the OS layer itself and the separately configured host shader tool.
R4_EXCLUDED = ("engine/os/", "engine/render/shaders/pipeline/")
ENGINE_ALLOWLIST_DIRECTORY = "tests/common/os"
ENGINE_ALLOWLIST_SCOPE = ("engine/", "tests/common/")
ALLOWLIST_NAME = "os_boundary_allowlist.txt"
RULES = ("R1", "R2", "R3", "R4", "R5")

OS_HEADERS = (
    # Windows SDK and CRT
    r"windows\.h", r"winsock2?\.h", r"ws2tcpip\.h", r"mswsock\.h", r"winternl\.h", r"io\.h", r"direct\.h",
    r"process\.h", r"conio\.h", r"crtdbg\.h", r"shellapi\.h", r"shlobj\.h", r"processthreadsapi\.h",
    r"synchapi\.h", r"avrt\.h", r"timeapi\.h", r"mmsystem\.h", r"psapi\.h",
    # POSIX and Apple
    r"unistd\.h", r"pthread\.h", r"sched\.h", r"dirent\.h", r"poll\.h", r"spawn\.h", r"termios\.h", r"dlfcn\.h",
    r"fcntl\.h", r"signal\.h", r"csignal", r"pwd\.h", r"grp\.h", r"syslog\.h", r"netdb\.h", r"execinfo\.h",
    r"ifaddrs\.h", r"libproc\.h", r"TargetConditionals\.h", r"sys/[^>\"]+", r"mach/[^>\"]+", r"mach-o/[^>\"]+",
    r"arpa/[^>\"]+", r"netinet/[^>\"]+", r"dispatch/[^>\"]+", r"CoreFoundation/[^>\"]+", r"os/[^>\"]+",
)
OS_HEADER = r"(?:" + "|".join(OS_HEADERS) + r")"
OS_MACROS = (
    "_WIN32", "_WIN64", "__APPLE__", "__MACH__", r"TARGET_OS_\w+", "__linux__", "__linux", "__unix__", "__unix",
    "__ANDROID__", "__FreeBSD__", "__OpenBSD__", "__NetBSD__", "__MINGW32__", "__MINGW64__", "__CYGWIN__",
    "__EMSCRIPTEN__", "_POSIX_VERSION", "WINAPI_FAMILY", r"SDL_PLATFORM_\w+",
)
OS_APIS = (
    "localtime_s", "localtime_r", "gmtime_s", "gmtime_r", "getrusage", "clock_gettime", "sigaction", r"std::signal",
    r"pthread_\w+", "kqueue", "kevent", "GetLastError", "GetModuleFileNameW", "GetModuleFileNameA", "MoveFileExW",
    "SetThreadDescription", "GetThreadDescription", "SetThreadPriority", "SetConsoleCtrlHandler",
    "SetConsoleOutputCP", "GetCommandLineW", "CommandLineToArgvW", "MultiByteToWideChar", "WideCharToMultiByte",
    "GetFullPathNameW", "CreateFileW", "GetProcessTimes", "GetThreadTimes", "timeBeginPeriod", "_NSGetExecutablePath",
    "_wfopen", "_wfsopen",
)
FILESYSTEM_OPERATIONS = (
    "exists", "is_regular_file", "is_directory", "is_symlink", "is_empty", "is_other", "is_block_file",
    "is_character_file", "is_fifo", "is_socket", "file_size", "last_write_time", "status", "symlink_status",
    "create_directory", "create_directories", "create_symlink", "create_directory_symlink", "create_hard_link",
    "remove", "remove_all", "rename", "copy", "copy_file", "copy_symlink", "resize_file", "permissions", "absolute",
    "canonical", "weakly_canonical", "read_symlink", "relative", "proximate", "equivalent", "hard_link_count",
    "space", "current_path", "temp_directory_path", "directory_iterator", "recursive_directory_iterator",
)

INCLUDE = re.compile(r"^\s*#\s*include\s*([<\"])([^>\"]+)[>\"]")
R1_INCLUDE = re.compile(r"^" + OS_HEADER + r"$")
CONDITIONAL = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef|elif|elifdef|elifndef)\b(.*)$")
R2_PATTERNS = (
    re.compile(r"\b(?:" + "|".join(OS_MACROS) + r")\b"),
    re.compile(r"__has_include\s*\(\s*[<\"]" + OS_HEADER + r"[>\"]\s*\)"),
)
R3_PATTERN = re.compile(r"(?<![\w.>])(?:" + "|".join(OS_APIS) + r")\b|(?<![\w.>:])signal\s*\(")
R4_PATTERNS = (
    re.compile(r"\b(?:std::)?(?:basic_)?w?[io]?fstream\b"),
    re.compile(r"\b(?:std::)?w?filebuf\b"),
    re.compile(r"(?<![\w.>])(?:std::)?(?:fopen|_wfopen|freopen)\s*\("),
    re.compile(r"\b(?:std::filesystem|fs)::(?:" + "|".join(FILESYSTEM_OPERATIONS) + r")\b"),
)
# engine/os may include the standard library, OS headers, engine/base and its own headers only.
R5_ALLOWED_QUOTED = re.compile(r"^(?:engine/base/|engine/os/)")
R5_FORBIDDEN_ANGLED = re.compile(r"^(?:engine/|SDL3?/|asio|nlohmann/|doctest/|imgui|httplib|ufbx)")


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


def file_findings(relative, text):
    """Yields (line, rule, match) for one file whose repository-relative path is relative."""
    in_os = relative.startswith("engine/os/")
    os_source = relative.startswith(OS_SOURCES)
    runtime = relative.startswith(tuple(r + "/" for r in RUNTIME_ROOTS)) and not relative.startswith(R4_EXCLUDED)
    raw_lines = text.splitlines()
    for number, line in enumerate(raw_lines, 1):
        include = INCLUDE.match(line)
        if not include:
            continue
        kind, header = include.groups()
        if not os_source and R1_INCLUDE.match(header):
            yield number, "R1", header
        if in_os:
            if kind == '"' and header.startswith(("./", "../")):
                # A relative include is judged by the file it names: it must stay inside engine/os or engine/base.
                target = posixpath.normpath(posixpath.join(posixpath.dirname(relative), header))
                forbidden = not target.startswith(("engine/os/", "engine/base/"))
            else:
                forbidden = (kind == '"' and "/" in header and not R5_ALLOWED_QUOTED.match(header)) or (
                    R5_FORBIDDEN_ANGLED.match(header) and not R5_ALLOWED_QUOTED.match(header))
            if forbidden:
                yield number, "R5", header
    if os_source:
        return
    code = strip_comments_and_strings(text).splitlines()
    for number, line in enumerate(code, 1):
        conditional = CONDITIONAL.match(line)
        if conditional:
            for pattern in R2_PATTERNS:
                for match in pattern.finditer(conditional.group(1)):
                    yield number, "R2", match.group(0)
            continue
        if INCLUDE.match(raw_lines[number - 1] if number <= len(raw_lines) else ""):
            continue
        for match in R3_PATTERN.finditer(line):
            yield number, "R3", match.group(0).rstrip("( ")
        if runtime:
            for pattern in R4_PATTERNS:
                for match in pattern.finditer(line):
                    yield number, "R4", match.group(0).rstrip("( ")


def scanned_files(root, roots):
    for top in roots:
        base = root / top
        if not base.is_dir():
            continue
        count = 0
        for path in sorted(base.rglob("*")):
            relative = path.relative_to(root).as_posix()
            if path.suffix not in SUFFIXES or not path.is_file() or relative.startswith(EXCLUDED):
                continue
            count += 1
            yield top, relative, path
        if count == 0:
            yield top, None, None


def load_allowlists(root, roots):
    """Returns ({(path, rule): (count, allowlist line)}, [problem lines])."""
    entries, problems = {}, []
    sources = []
    for top in roots:
        base = root / top
        if base.is_dir():
            sources.extend(sorted(base.rglob(ALLOWLIST_NAME)))
    for allowlist in sources:
        directory = allowlist.parent.relative_to(root).as_posix()
        scope = ENGINE_ALLOWLIST_SCOPE if directory == ENGINE_ALLOWLIST_DIRECTORY else (directory + "/",)
        for number, raw in enumerate(allowlist.read_text(encoding="utf-8").splitlines(), 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            where = f"{directory}/{ALLOWLIST_NAME}:{number}"
            fields = line.split()
            if len(fields) != 3 or fields[1] not in RULES or not fields[2].isdigit() or int(fields[2]) == 0:
                problems.append(f"{where}: expected 'path rule count', got '{line}'")
                continue
            path, rule, count = fields[0], fields[1], int(fields[2])
            if not path.startswith(scope):
                problems.append(f"{where}: {path} is outside this allowlist's directory ({', '.join(scope)})")
                continue
            if not (root / path).is_file():
                problems.append(f"{where}: {path} does not exist; remove the entry")
                continue
            if (path, rule) in entries:
                problems.append(f"{where}: duplicate entry for {path} {rule}")
                continue
            entries[(path, rule)] = (count, where)
    return entries, problems


def check(root, roots=ROOTS):
    """Returns the list of problem lines (empty when the tree satisfies the invariants)."""
    problems, found = [], {}
    for top, relative, path in scanned_files(root, roots):
        if relative is None:
            problems.append(f"{top}: no C/C++ source scanned; is the root right?")
            continue
        for number, rule, match in file_findings(relative, path.read_text(encoding="utf-8", errors="replace")):
            found.setdefault((relative, rule), []).append(f"{relative}:{number}: {rule} {match}")
    entries, allowlist_problems = load_allowlists(root, roots)
    problems.extend(allowlist_problems)
    for key, lines in sorted(found.items()):
        allowed = entries.get(key)
        if allowed is None:
            problems.extend(lines)
        elif len(lines) > allowed[0]:
            problems.extend(lines)
            problems.append(f"{allowed[1]}: {key[0]} {key[1]} has {len(lines)} findings, allowlisted {allowed[0]}")
    for key, (count, where) in sorted(entries.items()):
        actual = len(found.get(key, []))
        if actual < count:
            problems.append(f"{where}: {key[0]} {key[1]} has {actual} findings, allowlisted {count}; "
                            f"shrink the allowlist")
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    args = parser.parse_args(argv)
    problems = check(args.root.resolve())
    for line in problems:
        print(f"OS boundary guard: {line}")
    if problems:
        print(f"OS boundary guard: {len(problems)} problems (see docs/architecture/os.md)")
        return 1
    print("OS boundary guard: no OS knowledge outside engine/os beyond the allowlists")
    return 0


if __name__ == "__main__":
    sys.exit(main())
