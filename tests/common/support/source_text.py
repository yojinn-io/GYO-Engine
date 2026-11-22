"""How the text guards read C/C++ sources: the file suffixes, comment and literal blanking, include directives,
the C++ standard library headers, and the CamelCase spelling of a snake_case name.

Shared by tests/common/kernel/kernel_guard.py, tests/common/orchestration/orchestration_guard.py and
tests/common/naming/naming_guard.py (v8-arch-gyo2-naming-cleanup: moved out of kernel_guard, which the other two
imported from). It holds no rule: each guard decides what to report. A guard imports it with
sys.path.insert(0, <tests/common/support>).
"""
import re

# The C/C++ files the guards read.
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp", ".tpp", ".inc", ".ixx"}
DIRECTIVE = re.compile(r"^[ \t]*(?:#|%:)[ \t]*(?:include_next|include|import)\b")
TARGET = re.compile(r"[ \t]*([<\"])([^>\"]+)[>\"]")
# The C++ standard library headers and the <cxxx> C wrappers (no .h C header, no platform header).
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


def camel_case(identifier, acronyms=None):
    """identifier split at "_", each part capitalized; a part in acronyms is written as listed there.

    One conversion for the guards: kernel_guard K1 spells a registered product id (no acronym table: a product
    spells its own names), naming_guard derives a unit's namespace from its directory (with rebuild-naming N17's
    acronym table, naming_guard.ACRONYMS).
    """
    acronyms = acronyms or {}
    return "".join(acronyms.get(part, part[:1].upper() + part[1:]) for part in identifier.split("_") if part)
