"""Text scan of zone-C (cross-platform deterministic) source files: spec J-17 of
docs/architecture/plans/kernel/spec.zh-Hant.md, rules and judgement in
docs/architecture/plans/kernel/j17-scanner.zh-Hant.md, the list contract in
docs/architecture/target/zone-c-sources.zh-Hant.md.

Input: a list file (format "gyo.zone-c-sources", version 1) naming the .cpp files of one owner that
contain zone-C Systems. Every listed file is scanned, and so is every header it reaches through
quoted includes that resolve under the list's owner_root (transitive, each file once). Angle-bracket
includes are never expanded.

Comments, string and character literals are blanked first; no rule reads them. Rules:

  ZS1  include of a banned header (clock, random, thread, OS, SDL, file or stream effect): the name
       data is in zone_scan_rules.json, either include form.
  ZS2  mutable state at namespace scope or declared static / thread_local: anonymous-namespace or
       namespace-scope variables without const (`int counter;`, `inline int x;`, `extern int y;`,
       `Foo f(1);`, `struct S {...} s;`), and every `static` or `thread_local` declaration that is a
       variable (also inside functions and classes). Allowed: constexpr / consteval, const applied to
       the variable itself (not only to a pointee), functions. A parenthesised initializer whose
       arguments are not all type-shaped is a variable (fail closed); `Foo f(x);` with a lone
       identifier is read as a function declaration, as C++ would when x names a type (limit).
  ZS3  third-party global context: ImGui::, SDL_Get* / SDL_*State queries, TTF_*, Mix_* (data file).
  ZS4  clock alias: Time::Now, Time::NowNs, Clock::now, *_clock, SDL_GetTicks*,
       SDL_GetPerformanceCounter, QueryPerformanceCounter, clock_gettime, gettimeofday (data file).
  ZS5  candidates, reported but never a failure: a lambda capturing this / = / & stored into a
       member, a std::function member, a raw pointer or reference member named with a trailing
       underscore (a cross-tick pointer candidate).
  ZS6  randomness other than the <random> include: rand( / srand(, random_device, mt19937*,
       std::uniform_* (data file).
  ZS7  hashing: std::hash, unordered_* (data file).

Output: one line per finding, `<path>:<line>: <rule> <text>` (candidates as `ZS5?`), then one summary
line. `--report FILE` also writes a JSON report (counts, findings, candidates, expanded headers with
their include chain, unresolved quoted includes). Exit status 0 when clean (candidates do not count),
1 with findings, 2 on a contract or usage error (unknown list version or format, unknown field, a
path that is not relative / sorted / unique / under owner_root / present on disk). `--no-fail` turns
1 into 0 for the baseline report.

Usage: python3 zone_scan.py --list LIST.json [--root REPOSITORY] [--rules RULES.json]
                            [--report FILE] [--no-fail]
"""
import argparse
import json
import re
import sys
from collections import deque
from pathlib import Path, PurePosixPath

FORMAT = "gyo.zone-c-sources"
SUPPORTED_VERSIONS = (1,)
LIST_FIELDS = ("format", "version", "generated_by", "repo_commit", "owner_root", "include_roots", "files")
SOURCE_SUFFIXES = (".cpp", ".cc", ".cxx")
HEADER_SUFFIXES = (".hpp", ".h", ".hh", ".hxx", ".inl", ".ipp")
RULES = ("ZS1", "ZS2", "ZS3", "ZS4", "ZS5", "ZS6", "ZS7")
DEFAULT_RULES = Path(__file__).resolve().with_name("zone_scan_rules.json")

INCLUDE = re.compile(r"^[ \t]*#[ \t]*include[ \t]*([<\"])([^>\"]+)[>\"]")
RAW_STRING_START = re.compile(r'(?:u8|u|U|L)?R"')
LITERAL_PREFIX = re.compile(r"""(?:u8|u|U|L)(["'])""")
INCLUDE_LINE = re.compile(r"^[ \t]*#[ \t]*include\b")
STATIC = re.compile(r"\b(?:static|thread_local)\b")
CONST_WORDS = re.compile(r"\b(?:constexpr|consteval)\b")
TEMPLATE_HEAD = re.compile(r"^\s*template\s*<")
SKIP_HEAD = re.compile(r"^\s*(?:(?:inline\s+)?namespace|using|typedef|static_assert|friend|concept|requires|export|import|module|asm|goto|return|extern\s+template)\b")
TYPE_HEAD = re.compile(r"^\s*(?:struct|class|union|enum)\b")
TYPE_KEYWORDS = frozenset(("struct", "class", "union", "enum", "final", "alignas", "typename", "const", "volatile"))
FIRST_WORD = re.compile(r"\s*([A-Za-z_]\w*)")
IDENTIFIER = re.compile(r"[A-Za-z_]\w*")
ARGUMENT_WORDS = frozenset(("nullptr", "true", "false", "this", "sizeof", "alignof", "new", "delete"))
PARAMETER = re.compile(
    r"^\s*(?:(?:const|volatile|typename|struct|class|enum|union)\s+)*"
    r"(?:::)?[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*"            # a type name, possibly qualified
    r"(?:\s+(?:unsigned|signed|long|short|int|char|double|float|const|volatile)\b)*"  # multi-word builtin types
    r"(?:\s*(?:const|volatile|&&|&|\*|\.\.\.))*"          # cv, references, pointers, pack
    r"(?:\s*[A-Za-z_]\w*)?"                               # an optional parameter name (`&name` has no space)
    r"(?:\s*\[\s*\d*\s*\])*"                              # array bounds
    r"(?:\s*=\s*[^,]*)?\s*$")                             # a default argument (blank when it was a literal)
FUNCTION_POINTER_PARAMETER = re.compile(r"^\s*(?:const\s+)?[\w:\s*&]+\(\s*[*&]+\s*\w*\s*\)\s*(?:\(|\[)")
DECLTYPE_PARAMETER = re.compile(r"^\s*(?:const\s+)?decltype\s*\(")
LAMBDA_INTO_MEMBER = re.compile(r"(?:\bthis->\w+|\b\w+_)\s*(?:=|\.(?:push_back|emplace_back|emplace|insert|assign)\s*\()\s*\[(?:this|=|&)")
FUNCTION_MEMBER = re.compile(r"\bstd::(?:move_only_)?function\s*<.*>\s*\w+\s*(?:;|=|\{)")
POINTER_MEMBER = re.compile(r"^\s*(?:const\s+)?(?:std::)?[A-Z]\w*(?:::\w+)*(?:<[^;]*>)?\s*(?:const\s*)?[*&]\s*(?:const\s+)?[a-z]\w*_\s*(?:=|;|\{)")


class ContractError(Exception):
    """The list or the rules file breaks the contract; exit status 2."""


# ----------------------------------------------------------------------------------------------
# Masking

def blank_comments_and_literals(text):
    """Replaces comments, string and character literals with spaces, keeping every line break.

    Handles //, /* */, raw strings R"delim( )delim", ordinary and prefixed string literals,
    character literals (a ' after a digit or letter is a digit separator, 1'000)."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            end = text.find("\n", i)
            while 0 <= end and text[:end].rstrip("\r").endswith("\\"):  # a line comment continued by a backslash
                end = text.find("\n", end + 1)
            end = n if end < 0 else end
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        elif c in "RLuU" and (prefix := RAW_STRING_START.match(text, i)) and not identifier_before(text, i):
            quote = prefix.end() - 1
            delimiter_end = text.find("(", quote + 1)
            if delimiter_end < 0:
                out.append(c); i += 1
                continue
            closing = ")" + text[quote + 1:delimiter_end] + '"'
            end = text.find(closing, delimiter_end)
            end = n if end < 0 else end + len(closing)
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        elif (c in "\"'" and not (c == "'" and identifier_before(text, i))) or \
                (c in "LuU" and LITERAL_PREFIX.match(text, i) and not identifier_before(text, i)):
            if c not in "\"'":  # a prefixed literal: L"..", u8"..", u'..'
                c = LITERAL_PREFIX.match(text, i).group(1)
                out.append(" " * (LITERAL_PREFIX.match(text, i).end() - 1 - i)); i = LITERAL_PREFIX.match(text, i).end() - 1
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            end = min(j + 1, n)
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        else:
            out.append(c); i += 1
    return "".join(out)


def identifier_before(text, i):
    return i > 0 and (text[i - 1].isalnum() or text[i - 1] == "_")


def blank_preprocessor_lines(code):
    """Blanks every preprocessor line (with its backslash continuations), keeping line breaks."""
    lines = code.split("\n")
    i = 0
    while i < len(lines):
        if re.match(r"^[ \t]*#", lines[i]):
            while True:
                continued = lines[i].rstrip().endswith("\\")
                lines[i] = " " * len(lines[i])  # same length: offsets stay aligned with the raw text
                i += 1
                if not continued or i >= len(lines):
                    break
        else:
            i += 1
    return "\n".join(lines)


def line_of(text, offset):
    return text.count("\n", 0, offset) + 1


def squash(text, limit=96):
    text = " ".join(text.split())
    return text if len(text) <= limit else text[:limit - 3] + "..."


# ----------------------------------------------------------------------------------------------
# ZS2: declaration classification

def strip_template_arguments(text, with_map=False):
    """Removes every balanced <...> group so const / parentheses inside template arguments are
    not read as the declaration's own. With with_map, also returns the index in text of every
    kept character."""
    out, kept, depth = [], [], 0
    for i, c in enumerate(text):
        if c == "<":
            depth += 1
        elif c == ">" and depth > 0:
            depth -= 1
        elif depth == 0:
            out.append(c); kept.append(i)
    return ("".join(out), kept) if with_map else "".join(out)


def first_parentheses(text, raw):
    """(inside, raw_inside) of the first parentheses of text once template arguments are
    stripped; raw is the unblanked text at the same offsets. (None, None) without parentheses."""
    flat, kept = strip_template_arguments(text, with_map=True)
    open_paren = flat.find("(")
    if open_paren < 0:
        return None, None
    depth, close_paren = 0, -1
    for j in range(open_paren, len(flat)):
        if flat[j] == "(":
            depth += 1
        elif flat[j] == ")":
            depth -= 1
            if depth == 0:
                close_paren = j
                break
    if close_paren < 0:
        return flat[open_paren + 1:], raw[kept[open_paren] + 1:]
    return flat[open_paren + 1:close_paren], raw[kept[open_paren] + 1:kept[close_paren]]


def strip_template_head(text):
    """Removes leading `template <...>` heads (balanced) and attributes [[...]], alignas(...)."""
    while True:
        text = re.sub(r"^\s*\[\[[^\]]*\]\]", "", text)
        text = re.sub(r"^\s*alignas\s*\([^)]*\)", "", text)
        match = TEMPLATE_HEAD.match(text)
        if not match:
            return text
        depth, i = 0, match.end() - 1
        while i < len(text):
            if text[i] == "<":
                depth += 1
            elif text[i] == ">":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        text = text[i + 1:]


def is_immutable(declaration):
    """True when the declared object itself cannot change: constexpr, consteval, or a const that
    applies to the variable (after the last * or &, or with no pointer / reference at all)."""
    flat = strip_template_arguments(declaration)
    assignment = first_top_level_assignment(flat)
    if assignment >= 0:
        flat = flat[:assignment]  # a const inside the initializer belongs to the initializer
    cut = flat.find("(")
    if cut >= 0 and not re.match(r"^\s*(?:const\s+)?decltype\b", flat):
        flat = flat[:cut]
    if CONST_WORDS.search(flat):
        return True
    consts = [m.start() for m in re.finditer(r"\bconst\b", flat)]
    if not consts:
        return False
    pointer = max((m.start() for m in re.finditer(r"[*&]", flat)), default=-1)
    return consts[-1] > pointer


def split_top_level(text, separator=","):
    parts, depth, start = [], 0, 0
    for i, c in enumerate(text):
        if c in "([{<":
            depth += 1
        elif c in ")]}>":
            depth -= 1
        elif c == separator and depth == 0:
            parts.append(text[start:i]); start = i + 1
    parts.append(text[start:])
    return parts


def parenthesised_is_parameter_list(inside, raw_inside):
    """True when the text inside the first parentheses reads as a parameter list (so the
    declaration is a function), False when it reads as constructor arguments (a variable)."""
    if inside.strip() == "":
        # Blank once literals are blanked: an empty parameter list, unless the raw text held a
        # string or character literal (`Foo f("x");` is a variable).
        return not ('"' in raw_inside or "'" in raw_inside)
    if inside.strip() == "void":
        return True
    for item in split_top_level(inside):
        item = item.strip()
        if not item:
            return False
        first = FIRST_WORD.match(item)
        if not first or first.group(1) in ARGUMENT_WORDS or item[0].isdigit() or item[0] in "-+!~*&(":
            return False
        if FUNCTION_POINTER_PARAMETER.match(item) or DECLTYPE_PARAMETER.match(item):
            continue  # `int (*cb)(int)`, `const char (&s)[N]`, `decltype(x) y`
        if not PARAMETER.match(item):
            return False
    return True


def first_top_level_assignment(text):
    """Offset of the first `=` that is an assignment (not ==, !=, <=, >=, and not inside
    parentheses / brackets / template arguments), or -1."""
    depth = 0
    for i, c in enumerate(text):
        if c in "([{<":
            depth += 1
        elif c in ")]}>":
            depth -= 1
        elif c == "=" and depth == 0:
            before = text[i - 1] if i > 0 else ""
            after = text[i + 1] if i + 1 < len(text) else ""
            if before not in "=!<>" and after != "=":
                return i
    return -1


def declaration_end(code, start):
    """Offset of the first `;`, or of a `{` or assignment `=` outside parentheses, after start."""
    depth = 0
    for i in range(start, len(code)):
        c = code[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth = max(0, depth - 1)
        elif c == ";":
            return i
        elif depth == 0 and (c == "{" or (c == "=" and code[i - 1] not in "=!<>" and code[i + 1:i + 2] != "=")):
            return i
    return len(code)


def static_declaration_is_variable(code, start, raw):
    """True when the static / thread_local declaration starting at start declares a variable."""
    end = declaration_end(code, start)
    declaration = code[start:end]
    if is_immutable(declaration):
        return False
    inside, raw_inside = first_parentheses(declaration, raw[start:end])
    if inside is None:
        return True
    return not parenthesised_is_parameter_list(inside, raw_inside)


def namespace_statement_kind(head, raw_head):
    """Classifies a statement at namespace scope. Returns ("variable", text), ("namespace", None),
    ("class", None), ("other", None) or ("skip", None). head is the masked text before the
    terminator (; or {); raw_head the same span of the original text."""
    leading = len(head) - len(head.lstrip())  # blanked comments and blank lines before the statement
    head, raw_head = head[leading:], raw_head[leading:]
    text = strip_template_head(head)
    raw_text = raw_head[len(raw_head) - len(text):]  # the head strip only removes a prefix
    stripped = text.strip()
    if not stripped:
        return "skip", None
    if SKIP_HEAD.match(text):
        return ("namespace" if re.match(r"(?:inline\s+)?namespace\b", stripped) else "skip"), None
    if stripped == "extern":  # extern "C" { ... } with the literal blanked
        return "namespace", None
    if TYPE_HEAD.match(text):
        # `struct S {` opens a class scope; `struct S;` is a forward declaration; `struct S s;` is a
        # variable (two identifiers after the keyword, no base clause).
        words = [w for w in IDENTIFIER.findall(strip_template_arguments(re.sub(r"\[\[[^\]]*\]\]", "", stripped)))
                 if w not in TYPE_KEYWORDS]
        if ":" in stripped or len(words) <= 1 or any(re.fullmatch(r"[A-Z][A-Z0-9_]+", w) for w in words):
            return "class", None  # an ALL_CAPS word is an export macro, not a declarator
        return "variable", squash(raw_text)
    if STATIC.search(text):
        return "skip", None  # reported by the static rule
    flat = strip_template_arguments(text)
    if re.search(r"\boperator\b", flat):
        return "other", None
    assignment = first_top_level_assignment(flat)
    open_paren = flat.find("(")
    if open_paren >= 0 and (assignment < 0 or open_paren < assignment):
        inside, raw_inside = first_parentheses(text, raw_text)
        if parenthesised_is_parameter_list(inside, raw_inside):
            return "other", None  # function declaration or definition
        if is_immutable(text):
            return "other", None
        return "variable", squash(raw_text)
    if not IDENTIFIER.search(flat):
        return "skip", None
    if is_immutable(text):
        return "other", None
    return "variable", squash(raw_text)


def declarators_only(trailer):
    """True when the text after a closing brace holds only declarators (`s`, `*p, q[4]`), not a
    statement with its own type (`int afterwards`)."""
    flat = strip_template_arguments(trailer)
    if not IDENTIFIER.search(flat):
        return True
    return re.search(r"[A-Za-z_]\w*\s+[A-Za-z_]\w*", flat) is None


def namespace_scope_variables(code, raw):
    """Yields (line, text) for mutable variables declared at namespace scope. code has comments,
    literals and preprocessor lines blanked; raw is the original text (same offsets)."""
    scopes = ["namespace"]  # kinds of the enclosing braces, file scope first
    statement_start = 0
    depth = 0  # parentheses and brackets inside the current statement
    i, n = 0, len(code)
    trailer_start = None  # after `}` at namespace scope: text until `;` (declarators only after a class)
    trailer_is_declarator = False
    while i < n:
        c = code[i]
        if c in "([":
            depth += 1
        elif c in ")]":
            depth = max(0, depth - 1)
        elif c == "{" and depth == 0:
            if scopes[-1] == "namespace":
                head = code[statement_start:i]
                kind, text = namespace_statement_kind(head, raw[statement_start:i])
                if kind == "variable":
                    yield line_of(code, statement_start + (len(head) - len(head.lstrip()))), text
                    scopes.append("other")
                else:
                    scopes.append(kind if kind in ("namespace", "class") else "other")
                    if kind == "class" and re.match(r"^\s*typedef\b", head):
                        scopes[-1] = "other"
            else:
                scopes.append("other")
            statement_start = i + 1
            trailer_start = None
        elif c == "}" and depth == 0:
            closed = scopes.pop() if len(scopes) > 1 else "namespace"
            statement_start = i + 1
            trailer_start = i + 1 if scopes[-1] == "namespace" else None
            trailer_is_declarator = closed == "class"
        elif c == ";" and depth == 0:
            if scopes[-1] == "namespace":
                if trailer_start is not None and declarators_only(code[trailer_start:i]):
                    # `struct S {...} s;` declares a variable; `typedef struct {...} Foo;` and a function
                    # body's trailing `;` do not.
                    trailer = code[trailer_start:i]
                    if trailer_is_declarator and IDENTIFIER.search(trailer) and not is_immutable(trailer):
                        yield line_of(code, trailer_start + (len(trailer) - len(trailer.lstrip()))), squash(raw[trailer_start:i])
                else:
                    head = code[statement_start:i]
                    kind, text = namespace_statement_kind(head, raw[statement_start:i])
                    if kind == "variable":
                        yield line_of(code, statement_start + (len(head) - len(head.lstrip()))), text
            statement_start = i + 1
            trailer_start = None
        i += 1


# ----------------------------------------------------------------------------------------------
# Rules

class Rules:
    def __init__(self, data):
        try:
            if data.get("version") != 1:
                raise ContractError(f"rules: unsupported rules version {data.get('version')!r} (supported: 1)")
            self.zs1_headers = frozenset(h.lower() for h in data["ZS1"]["headers"])
            self.zs1_prefixes = tuple(p.lower() for p in data["ZS1"]["header_prefixes"])
            self.patterns = {rule: [re.compile(p) for p in data[rule]["patterns"]] for rule in ("ZS3", "ZS4", "ZS6", "ZS7")}
        except (KeyError, TypeError, re.error) as error:
            raise ContractError(f"rules: malformed rules file ({error})")

    def banned_header(self, target):
        lowered = target.lower()  # include names are case-insensitive on Windows (<Windows.h>)
        return lowered in self.zs1_headers or lowered.startswith(self.zs1_prefixes)


def load_rules(path):
    try:
        return Rules(json.loads(Path(path).read_text(encoding="utf-8")))
    except (OSError, json.JSONDecodeError) as error:
        raise ContractError(f"rules: cannot read {path} ({error})")


def scan_text(relative, raw, rules):
    """Returns (findings, candidates, includes): findings and candidates are (line, rule, text);
    includes are (line, kind, target) for every include line."""
    findings, candidates, includes = [], [], []
    code = blank_comments_and_literals(raw)
    raw_lines, code_lines = raw.split("\n"), code.split("\n")
    for number, (raw_line, code_line) in enumerate(zip(raw_lines, code_lines), start=1):
        if not INCLUDE_LINE.match(code_line):
            continue
        include = INCLUDE.match(raw_line)
        if not include:
            continue
        kind, target = include.groups()
        includes.append((number, kind, target))
        if rules.banned_header(target):
            findings.append((number, "ZS1", kind + target + (">" if kind == "<" else '"')))
    for rule in ("ZS3", "ZS4", "ZS6", "ZS7"):
        for pattern in rules.patterns[rule]:
            for match in pattern.finditer(code):
                findings.append((line_of(code, match.start()), rule, match.group(0).rstrip("( ")))
    previous_end = -1
    for match in STATIC.finditer(code):
        adjacent = previous_end >= 0 and code[previous_end:match.start()].strip() == ""
        previous_end = match.end()
        if adjacent:
            continue  # `static thread_local`: one declaration, one finding
        if static_declaration_is_variable(code, match.start(), raw):
            findings.append((line_of(code, match.start()), "ZS2", squash(code[match.start():declaration_end(code, match.start())])))
    structural = blank_preprocessor_lines(code)
    for line, text in namespace_scope_variables(structural, raw):
        findings.append((line, "ZS2", text))
    for number, code_line in enumerate(code_lines, start=1):
        if LAMBDA_INTO_MEMBER.search(code_line):
            candidates.append((number, "ZS5", "lambda capturing this/=/& stored into a member: " + squash(code_line)))
        if FUNCTION_MEMBER.search(code_line):
            candidates.append((number, "ZS5", "std::function member: " + squash(code_line)))
        if POINTER_MEMBER.match(code_line):
            candidates.append((number, "ZS5", "raw pointer or reference member: " + squash(code_line)))
    findings = sorted(set(findings))
    candidates = sorted(set(candidates))
    return findings, candidates, includes


# ----------------------------------------------------------------------------------------------
# List contract

def validate_path(label, value, owner_root=None):
    if not isinstance(value, str) or not value:
        raise ContractError(f"list: {label} must be a non-empty string, got {value!r}")
    if "\\" in value:
        raise ContractError(f"list: {label} {value!r} uses a backslash; paths are forward-slash, repo-relative")
    parts = value.split("/")
    if value.startswith("/") or any(p in ("", ".", "..") for p in parts):
        raise ContractError(f"list: {label} {value!r} must be repo-relative and canonical (no leading or trailing slash, no '//', '.' or '..')")
    if owner_root is not None and not (value == owner_root or value.startswith(owner_root + "/")):
        raise ContractError(f"list: {label} {value!r} is not under owner_root {owner_root!r}")


def load_list(path, root):
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ContractError(f"list: cannot read {path} ({error})")
    if not isinstance(data, dict):
        raise ContractError(f"list: {path} is not a JSON object")
    if data.get("format") != FORMAT:
        raise ContractError(f"list: unknown format {data.get('format')!r} in {path} (expected {FORMAT!r})")
    version = data.get("version")
    if not isinstance(version, int) or isinstance(version, bool) or version not in SUPPORTED_VERSIONS:
        supported = ", ".join(str(v) for v in SUPPORTED_VERSIONS)
        raise ContractError(f"zone_scan: unsupported list version {version!r} in {path} (supported: {supported}); refusing to scan")
    errors = []
    unknown = sorted(set(data) - set(LIST_FIELDS))
    if unknown:
        errors.append(f"list: unknown field(s) {unknown} (version {version} allows {list(LIST_FIELDS)})")
    missing = [f for f in LIST_FIELDS if f not in data]
    if missing:
        errors.append(f"list: missing field(s) {missing}")
    if errors:
        raise ContractError("\n".join(errors))
    if not isinstance(data["generated_by"], str) or not data["generated_by"].strip():
        errors.append("list: generated_by must be a non-empty string")
    if not isinstance(data["repo_commit"], str) or not re.fullmatch(r"[0-9a-fA-F]{7,40}", data["repo_commit"]):
        errors.append(f"list: repo_commit {data['repo_commit']!r} must be 7 to 40 hexadecimal digits")
    try:
        validate_path("owner_root", data["owner_root"])
    except ContractError as error:
        errors.append(str(error))
    owner_root = data["owner_root"] if isinstance(data["owner_root"], str) else ""
    if owner_root and not errors and not (Path(root) / owner_root).is_dir():
        errors.append(f"list: owner_root {owner_root!r} is not a directory under {root}")
    if not isinstance(data["include_roots"], list):
        errors.append("list: include_roots must be an array")
    elif len(set(map(str, data["include_roots"]))) != len(data["include_roots"]):
        errors.append("list: include_roots must not repeat")
    else:
        for item in data["include_roots"]:
            try:
                validate_path("include_roots item", item, owner_root)
            except ContractError as error:
                errors.append(str(error))
            else:
                if not (Path(root) / item).is_dir():
                    errors.append(f"list: include_roots item {item!r} is not a directory")
    files = data["files"]
    if not isinstance(files, list):
        errors.append("list: files must be an array")
        files = []
    for item in files:
        try:
            validate_path("files item", item, owner_root)
        except ContractError as error:
            errors.append(str(error))
            continue
        if not item.endswith(SOURCE_SUFFIXES):
            errors.append(f"list: files item {item!r} is not a source file ({', '.join(SOURCE_SUFFIXES)}); headers are expanded, not listed")
        if not (Path(root) / item).is_file():
            errors.append(f"list: files item {item!r} does not exist under {root}")
    if files != sorted(files):
        errors.append("list: files must be sorted (byte order)")
    if len(set(files)) != len(files):
        errors.append("list: files must not repeat")
    if errors:
        raise ContractError("\n".join(errors))
    return data


# ----------------------------------------------------------------------------------------------
# Expansion and the whole scan

def resolve_quoted_include(root, including, target, include_roots, owner_root):
    """Repo-relative posix path of a quoted include, or None when it does not resolve under
    owner_root. Order: the including file's directory, then include_roots."""
    if "\\" in target or target.startswith("/"):
        return None
    candidates = [PurePosixPath(including).parent / target] + [PurePosixPath(r) / target for r in include_roots]
    for candidate in candidates:
        value = normalize_posix(candidate.as_posix())
        if value is None or not (value == owner_root or value.startswith(owner_root + "/")):
            continue
        if (Path(root) / value).is_file():
            return value
    return None


def normalize_posix(value):
    """Collapses `.` and `..` segments of a relative posix path; None when it climbs above the root."""
    parts = []
    for part in value.split("/"):
        if part in ("", "."):
            continue
        if part == "..":
            if not parts:
                return None
            parts.pop()
        else:
            parts.append(part)
    return "/".join(parts)


def read_source(root, relative):
    return (Path(root) / relative).read_text(encoding="utf-8", errors="replace")


def scan(root, list_path, rules):
    """Runs the scan. Returns a report dict (see the module docstring)."""
    root = Path(root)
    listing = load_list(list_path, root)
    owner_root, include_roots = listing["owner_root"], listing["include_roots"]
    report = {
        "list": {k: listing[k] for k in ("format", "version", "generated_by", "repo_commit", "owner_root")},
        "list_path": str(list_path),
        "files_listed": len(listing["files"]), "files_scanned": 0, "lines": 0,
        "findings": [], "candidates": [], "per_rule": {rule: 0 for rule in RULES},
        "expanded": [], "unresolved_includes": [],
    }
    queue = deque((f, [f]) for f in listing["files"])
    seen = set(listing["files"])
    while queue:
        relative, chain = queue.popleft()
        raw = read_source(root, relative)
        report["files_scanned"] += 1
        report["lines"] += raw.count("\n") + (1 if raw and not raw.endswith("\n") else 0)
        findings, candidates, includes = scan_text(relative, raw, rules)
        for line, rule, text in findings:
            report["findings"].append({"file": relative, "line": line, "rule": rule, "text": text})
            report["per_rule"][rule] += 1
        for line, rule, text in candidates:
            report["candidates"].append({"file": relative, "line": line, "rule": rule, "text": text})
            report["per_rule"]["ZS5"] += 1
        for line, kind, target in includes:
            if kind != '"':
                continue
            resolved = resolve_quoted_include(root, relative, target, include_roots, owner_root)
            if resolved is None:
                report["unresolved_includes"].append({"file": relative, "line": line, "include": target})
            elif resolved not in seen:
                seen.add(resolved)
                report["expanded"].append({"file": resolved, "via": chain + [resolved]})
                queue.append((resolved, chain + [resolved]))
    report["findings"].sort(key=lambda f: (f["file"], f["line"], f["rule"], f["text"]))
    report["candidates"].sort(key=lambda f: (f["file"], f["line"], f["rule"], f["text"]))
    report["expanded"].sort(key=lambda e: e["file"])
    report["unresolved_includes"].sort(key=lambda u: (u["file"], u["line"], u["include"]))
    return report


def format_lines(report):
    lines = [f"{f['file']}:{f['line']}: {f['rule']} {f['text']}" for f in report["findings"]]
    lines += [f"{c['file']}:{c['line']}: ZS5? {c['text']}" for c in report["candidates"]]
    expanded = report["files_scanned"] - report["files_listed"]
    lines.append(f"zone_scan: {report['files_listed']} file(s) listed, {report['files_scanned']} scanned "
                 f"({expanded} expanded headers), {report['lines']} lines, {len(report['findings'])} finding(s), "
                 f"{len(report['candidates'])} candidate(s)")
    return lines


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--list", required=True, help="the gyo.zone-c-sources list file")
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[3]), help="repository root")
    parser.add_argument("--rules", default=str(DEFAULT_RULES))
    parser.add_argument("--report", help="write the JSON report here")
    parser.add_argument("--no-fail", action="store_true", help="exit 0 even with findings (baseline report)")
    args = parser.parse_args(argv)
    try:
        rules = load_rules(args.rules)
        report = scan(args.root, args.list, rules)
    except ContractError as error:
        print(str(error), file=sys.stderr)
        return 2
    for line in format_lines(report):
        print(line)
    if args.report:
        Path(args.report).parent.mkdir(parents=True, exist_ok=True)
        Path(args.report).write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    if report["findings"] and not args.no_fail:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
