"""Text scan of host source files for spec H-7 (target/kernel.zh-Hant.md DI-KS 15: a host does not order
Systems; a migrated host calls only Run, Rewind / Replay and the exchange points). Declaration and
judgement: docs/architecture/plans/kernel/h7-scanner-declaration.zh-Hant.md; the review changes are in
docs/dev_logs/2026_11_13_arch_h7_scanner.zh-Hant.md.

Input: one list file per owner (zero or more --list). With no list the scan prints
`h7_scan: nothing to scan` and exits 0: the common layer assumes no product (AGENTS.md section 7).

List contract, format "gyo.h7-hosts", version 1 (owned here; there is no target/ page for it):

  { "format": "gyo.h7-hosts", "version": 1, "generated_by": "manual", "repo_commit": "<7-40 hex>",
    "owner_root": "<repo-relative dir>", "include_roots": ["<dir under owner_root>", ...],
    "files": ["<repo-relative source or header under owner_root>", ...] }

  Forward slashes, repo-relative, canonical; files sorted (byte order), unique, existing, at least one;
  a header may be listed when it holds host code (inline). No other field: in particular an owner cannot
  add whitelist entries. An unknown format, an unknown version, an unknown or missing field, a field of
  the wrong type is a contract error (exit 2); the scanner never guesses.

Comments, string and character literals are blanked first (the masking of the zone-C scanner,
tests/common/zone_scan/zone_scan.py, imported, not copied; when that module or one of the three
functions is missing the scan fails with exit 2). Sources are read as UTF-8 (a BOM is dropped).
Rules (names in h7_scan_rules.json):

  H7-a  an #include whose last path segment is a banned header (IRuntimeClient.hpp, RuntimeLoop.hpp),
        either form, case-insensitive, also with comments between the tokens. Scanned in the listed
        files and in every header they reach through includes that resolve under owner_root (quoted:
        the including directory, then include_roots; angle: include_roots only), transitively, each
        once; `via` is the chain. Not seen: #include MACRO, #include_next, #import, a backslash-continued
        directive.
  H7-b  a member call `<receiver> (. | ->) [template] [Qualifier::] <Method> [<template args>] (` in a
        listed file, Method one of Tick, Advance, Update, Step, Run, unless the receiver's type is on the
        whitelist for that method. Not counted: `this->` calls, calls without a receiver, qualified free
        calls (`Foo::Run(`). A qualified member call (`x.Base::Tick(`) is always reported.

Whitelist by type, approximated without a C++ parser; uncertainty is reported (fail closed):
  1. The receiver is the last identifier before the operator (`session_.clock.Advance` -> clock). A
     receiver ending in `)`, `]` or `}` has no knowable type: reported, type `?`.
  2. The receiver is reported (type `?`) when its name stands anywhere in the listed file or its
     expanded owner headers in a declaration position this scan cannot type: after a comma
     (`Product other, clock;`), before a single colon (range-for, bit-field), or inside brackets
     (structured binding, lambda capture).
  3. Declarations `<Type> [*&const] name` followed by ; { = ( , ) or [ are collected from the listed
     file and its expanded owner headers; a declaration ending in `,` or `)` (a parameter) counts only
     in the listed file itself. `auto name = other;` is followed one hop to the bare identifier `other`;
     a second hop, an auto with any other initializer, or no declaration: reported, type `?`.
  4. Wrappers in the rule data (std::unique_ptr, ...) and cv / pointer / reference are peeled.
  5. A declared type D matches a whitelist type Q when D == Q (a leading :: allowed), or D is a proper
     suffix of Q and the listed file itself has `using namespace <the rest of Q's namespace>;`, or D is
     Q's last segment and the listed file has `using Q;`. The method must be on that entry.
  6. Every declaration found must match; any other: reported with the declared types.
  Known gap (limit L4 of the dev_log): a same-name whitelisted variable or member visible in the
  closure while the receiver's real declaration is outside it (a base class of another owner) or in a
  form step 3 does not read (a std::function member, a macro, a function returning the type) passes.

Output: one line per finding `<path>:<line>: <rule> <text>`, then
`h7_scan: <N> file(s) listed, <M> scanned (<E> expanded headers), <H> finding(s), <W> whitelisted` per
list. `--report FILE` writes JSON {"reports": [per list: list, list_path, files_listed, files_scanned,
findings, whitelisted, this_calls, per_rule, expanded, unresolved_includes], "per_rule": totals}; the
receiver's types are in each H7-b text as `[type=...]`.
`--expect-finding "<path>|<rule>|<regex>"` (repeatable): exit 1 unless a finding of that file and rule
has a text matching the regex, even with --no-fail (an owner pins sites it knows must be reported).
Exit status: 0 clean or nothing to scan, 1 findings (0 with --no-fail) or a missing expected finding,
2 contract or usage error (also an unreadable or non-UTF-8 source file and an unwritable report).

Usage: python3 h7_scan.py [--list LIST.json ...] [--root REPOSITORY] [--rules RULES.json]
                          [--report FILE] [--no-fail] [--expect-finding SPEC ...]
"""
import argparse
import importlib
import json
import re
import sys
from collections import deque
from pathlib import Path, PurePosixPath

DIRECTORY = Path(__file__).resolve().parent
ZONE_SCAN_DIRECTORY = DIRECTORY.parent / "zone_scan"
DEFAULT_RULES = DIRECTORY / "h7_scan_rules.json"
FORMAT = "gyo.h7-hosts"
SUPPORTED_VERSIONS = (1,)
LIST_FIELDS = ("format", "version", "generated_by", "repo_commit", "owner_root", "include_roots", "files")
LISTABLE_SUFFIXES = (".cpp", ".cc", ".cxx", ".hpp", ".h", ".hh", ".hxx", ".inl", ".ipp")
RULES = ("H7-a", "H7-b")
ZONE_SCAN_NAMES = ("blank_comments_and_literals", "resolve_quoted_include", "validate_path")

DIRECTIVE = re.compile(r"^[ \t]*#[ \t]*include\b")
IDENT = r"[A-Za-z_]\w*"
TEMPLATE_ARGS = r"<(?:[^<>;{}()]|<[^<>;{}()]*>)*>"
QUALIFIED_TYPE = rf"(?:::\s*)?{IDENT}(?:\s*{TEMPLATE_ARGS})?(?:\s*::\s*{IDENT}(?:\s*{TEMPLATE_ARGS})?)*"
NOT_A_TYPE = frozenset((
    "return", "else", "case", "new", "delete", "throw", "co_return", "co_yield", "co_await", "goto",
    "sizeof", "alignof", "typeid", "operator", "using", "namespace", "if", "while", "for", "switch",
    "do", "const", "volatile", "static", "mutable", "inline", "extern", "constexpr", "struct", "class",
    "typename", "decltype", "and", "or", "not", "public", "private", "protected", "friend", "virtual",
))
WRAPPED = re.compile(rf"^(?P<outer>{IDENT}(?:::{IDENT})*)<(?P<inner>.*)>$")


class ContractError(Exception):
    """The list, the rules or the environment breaks the contract; exit status 2."""


def zone_scan_functions(directory=None):
    """The three functions shared with the zone-C scanner. Missing module or name: ContractError."""
    directory = Path(directory) if directory else ZONE_SCAN_DIRECTORY
    if not (directory / "zone_scan.py").is_file():
        raise ContractError(f"h7_scan: the shared zone-C scanner {directory / 'zone_scan.py'} is missing; "
                            f"h7_scan needs its {', '.join(ZONE_SCAN_NAMES)}")
    sys.path.insert(0, str(directory))
    try:
        module = importlib.import_module("zone_scan")
    except Exception as error:  # an import that fails for any reason must not degrade silently
        raise ContractError(f"h7_scan: cannot import the zone-C scanner from {directory} ({error})")
    finally:
        sys.path.remove(str(directory))
    missing = [name for name in ZONE_SCAN_NAMES if not callable(getattr(module, name, None))]
    if missing:
        raise ContractError(f"h7_scan: the zone-C scanner {module.__file__} lacks {missing}")
    return module


# ----------------------------------------------------------------------------------------------
# Rules

def strings(value, label):
    if not isinstance(value, list) or not value or not all(isinstance(v, str) and v for v in value):
        raise ContractError(f"rules: {label} must be a non-empty array of non-empty strings")
    return value


class Rules:
    def __init__(self, data):
        if not isinstance(data, dict) or data.get("version") != 1:
            raise ContractError(f"rules: unsupported version {data.get('version') if isinstance(data, dict) else data!r}")
        try:
            self.headers = frozenset(h.lower() for h in strings(data["H7a"]["headers"], "H7a.headers"))
            self.methods = tuple(strings(data["H7b"]["methods"], "H7b.methods"))
            self.whitelist = [(tuple(split_qualified(e["type"])), frozenset(strings(e["methods"], "whitelist methods")))
                              for e in data["whitelist"]]
            self.unwrap = frozenset(strings(data["unwrap"], "unwrap"))
        except (KeyError, TypeError, AttributeError) as error:
            raise ContractError(f"rules: malformed rule data ({error!r})")
        alternatives = "|".join(re.escape(m) for m in self.methods)
        self.call = re.compile(
            rf"(?P<receiver>{IDENT}|[)\]}}])\s*(?P<op>\.|->)\s*(?:template\s+)?(?P<qualifier>(?:{IDENT}\s*::\s*)*)"
            rf"(?P<method>{alternatives})\b\s*(?:<[^;{{}}]*?>\s*)?\(")


def load_rules(path):
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ContractError(f"rules: cannot read {path} ({error})")
    return Rules(data)


def split_qualified(text):
    if not isinstance(text, str) or not text:
        raise TypeError(f"type name {text!r}")
    text = re.sub(r"\s+", "", text)
    if text.startswith("::"):
        text = text[2:]
    return text.split("::")


def spaced(qualified):
    """A regular expression for a qualified name with optional spaces around each ::."""
    return r"\s*::\s*".join(re.escape(part) for part in qualified)


# ----------------------------------------------------------------------------------------------
# List contract

def load_list(path, root, zs):
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
        raise ContractError(f"h7_scan: unsupported list version {version!r} in {path}; refusing to scan")
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
    owner_root = data["owner_root"]
    try:
        zs.validate_path("owner_root", owner_root)
        if not (Path(root) / owner_root).is_dir():
            errors.append(f"list: owner_root {owner_root!r} is not a directory under {root}")
    except Exception as error:  # zone_scan.ContractError
        errors.append(str(error))
        owner_root = None
    roots = data["include_roots"]
    if not isinstance(roots, list) or not all(isinstance(r, str) for r in roots) or len(set(roots)) != len(roots):
        errors.append("list: include_roots must be an array of strings without repeats")
    elif owner_root:
        for item in roots:
            try:
                zs.validate_path("include_roots item", item, owner_root)
            except Exception as error:
                errors.append(str(error))
                continue
            if not (Path(root) / item).is_dir():
                errors.append(f"list: include_roots item {item!r} is not a directory")
    files = data["files"]
    if not isinstance(files, list) or not all(isinstance(f, str) for f in files):
        errors.append("list: files must be an array of strings")
        raise ContractError("\n".join(errors))
    if not files:
        errors.append("list: files must name at least one host file")
    for item in files:
        try:
            zs.validate_path("files item", item, owner_root)
        except Exception as error:
            errors.append(str(error))
            continue
        if not item.endswith(LISTABLE_SUFFIXES):
            errors.append(f"list: files item {item!r} is not a C++ source or header ({', '.join(LISTABLE_SUFFIXES)})")
        elif not (Path(root) / item).is_file():
            errors.append(f"list: files item {item!r} does not exist under {root}")
    if files != sorted(files):
        errors.append("list: files must be sorted (byte order)")
    if len(set(files)) != len(files):
        errors.append("list: files must not repeat")
    if errors:
        raise ContractError("\n".join(errors))
    return data


def read_source(root, relative):
    try:
        return (Path(root) / relative).read_text(encoding="utf-8-sig")
    except (OSError, UnicodeDecodeError) as error:
        raise ContractError(f"h7_scan: cannot read {relative} as UTF-8 ({error})")


# ----------------------------------------------------------------------------------------------
# Type resolution (the whitelist by type)

def declarations(name, texts):
    """[(type text, ends like a parameter, auto alias or None)] of every declaration of `name`."""
    pattern = re.compile(
        rf"(?<![\w.:>)])(?P<type>{QUALIFIED_TYPE})(?P<decor>(?:\s*(?:\*|&|&&|\bconst\b|\bvolatile\b))*)"
        rf"\s*\b{re.escape(name)}\s*(?P<after>[;{{=(,)\[])")
    found = []
    for text in texts:
        for match in pattern.finditer(text):
            type_text = re.sub(r"\s+", " ", match.group("type")).strip()
            if split_qualified(type_text)[0] in NOT_A_TYPE:
                continue
            alias = None
            if type_text == "auto":
                hop = re.match(rf"\s*({IDENT})\s*;", text[match.end("after"):]) if match.group("after") == "=" else None
                alias = hop.group(1) if hop else ""
            found.append((type_text, match.group("after") in ",)", alias))
    return found


def untyped_position(name, texts):
    """True when `name` stands in a declaration position this scan cannot type (step 2)."""
    word = re.escape(name)
    patterns = (rf",\s*[*&]*\s*\b{word}\s*[;={{(\[,)]", rf"\b{word}\s*:(?!:)", rf"\[[^\[\]]*\b{word}\b[^\[\]]*\]")
    return any(re.search(p, text) for p in patterns for text in texts)


def peel(type_text, unwrap):
    """The declared type without cv, pointers, references and the listed wrappers."""
    text = type_text.strip()
    while True:
        text = re.sub(r"\s*(::|<|>|,|\*|&)\s*", r"\1", text)
        text = re.sub(r"^(?:(?:const|volatile)\s+)+", "", text)
        text = text[2:] if text.startswith("::") else text
        wrapped = WRAPPED.match(text)
        if wrapped and wrapped.group("outer") in unwrap:
            text = wrapped.group("inner")
            continue
        return text


def matches_whitelist(type_text, method, rules, listed_text):
    declared = split_qualified(peel(type_text, rules.unwrap))
    for qualified, methods in rules.whitelist:
        if method not in methods:
            continue
        if declared == list(qualified):
            return True
        n = len(declared)
        if 0 < n < len(qualified) and list(qualified[-n:]) == declared:
            evidence = [rf"\busing\s+namespace\s+(?:::\s*)?{spaced(qualified[:-n])}\s*;"]
            if n == 1:
                evidence.append(rf"\busing\s+(?:::\s*)?{spaced(qualified)}\s*;")
            if any(re.search(e, listed_text) for e in evidence):
                return True
    return False


def resolve_receiver(receiver, method, rules, texts, listed_text):
    """(whitelisted?, description of the declared types). texts[0] is the listed file."""
    if receiver in (")", "]", "}") or untyped_position(receiver, texts):
        return False, "?"
    found = [(t, a) for i, text in enumerate(texts) for t, parameter, a in declarations(receiver, [text])
             if i == 0 or not parameter]
    if not found:
        return False, "?"
    types = []
    for type_text, alias in found:
        if alias is None:
            types.append(type_text)
            continue
        if not alias or untyped_position(alias, texts):
            return False, "?"
        hop_all = [(t, a) for i, text in enumerate(texts) for t, parameter, a in declarations(alias, [text])
                   if i == 0 or not parameter]
        hop = [t for t, a in hop_all if a is None]
        if not hop or len(hop) != len(hop_all):
            return False, "?"
        types.extend(hop)
    unique = sorted(set(types))
    return all(matches_whitelist(t, method, rules, listed_text) for t in unique), ",".join(unique)


# ----------------------------------------------------------------------------------------------
# The scan

def line_of(text, offset):
    return text.count("\n", 0, offset) + 1


def includes_of(raw, masked):
    """[(line, target, form)] of the #include directives that are code, read through comments."""
    found = []
    for number, (raw_line, masked_line) in enumerate(zip(raw.split("\n"), masked.split("\n")), start=1):
        directive = DIRECTIVE.match(masked_line)
        if not directive:
            continue
        rest = masked_line[directive.end():]
        angle = re.match(r"\s*<([^>]+)>", rest)
        if angle:
            found.append((number, angle.group(1).strip(), "<"))
            continue
        offset = directive.end()
        for j in range(offset, len(raw_line)):  # the quoted path is blanked in the masked line: read it raw
            if raw_line[j] == '"' and not masked_line[offset:j].strip():
                end = raw_line.find('"', j + 1)
                if end > j + 1:
                    found.append((number, raw_line[j + 1:end].strip(), '"'))
                break
    return found


def resolve_angle_include(root, target, include_roots, owner_root):
    for include_root in include_roots:
        value = PurePosixPath(include_root, target).as_posix()
        if ".." in value.split("/") or not value.startswith(owner_root + "/"):
            continue
        if (Path(root) / value).is_file():
            return value
    return None


def scan(root, list_path, rules, zs):
    root = Path(root)
    listing = load_list(list_path, root, zs)
    owner_root, include_roots = listing["owner_root"], listing["include_roots"]
    report = {
        "list": {k: listing[k] for k in ("format", "version", "generated_by", "repo_commit", "owner_root")},
        "list_path": Path(list_path).as_posix(),
        "files_listed": len(listing["files"]), "files_scanned": 0,
        "findings": [], "whitelisted": [], "this_calls": [], "per_rule": {rule: 0 for rule in RULES},
        "expanded": [], "unresolved_includes": [],
    }
    masked_of, chain_of, closure_of = {}, {}, {}
    for listed in listing["files"]:
        closure, queue = [], deque([(listed, [listed])])
        visited = {listed}
        while queue:
            relative, chain = queue.popleft()
            if relative not in masked_of:
                raw = read_source(root, relative)
                masked_of[relative] = (raw, zs.blank_comments_and_literals(raw))
                chain_of[relative] = chain
                report["files_scanned"] += 1
                if relative not in listing["files"]:
                    report["expanded"].append({"file": relative, "via": chain})
            closure.append(relative)
            raw, masked = masked_of[relative]
            for line, target, form in includes_of(raw, masked):
                if form == '"':
                    resolved = zs.resolve_quoted_include(str(root), relative, target, include_roots, owner_root)
                    if resolved is None:
                        entry = {"file": relative, "line": line, "include": target}
                        if entry not in report["unresolved_includes"]:
                            report["unresolved_includes"].append(entry)
                else:
                    resolved = resolve_angle_include(root, target, include_roots, owner_root)
                if resolved is not None and resolved not in visited:
                    visited.add(resolved)
                    queue.append((resolved, chain + [resolved]))
        closure_of[listed] = closure
    for relative, (raw, masked) in masked_of.items():
        for line, target, _ in includes_of(raw, masked):
            if target.replace("\\", "/").split("/")[-1].lower() in rules.headers:
                finding = {"file": relative, "line": line, "rule": "H7-a", "text": target}
                if relative not in listing["files"]:
                    finding["via"] = chain_of[relative]
                report["findings"].append(finding)
    for listed in listing["files"]:
        masked = masked_of[listed][1]
        texts = [masked_of[f][1] for f in closure_of[listed]]
        for match in rules.call.finditer(masked):
            receiver, method = match.group("receiver"), match.group("method")
            line = line_of(masked, match.start("op"))
            qualifier = re.sub(r"\s+", "", match.group("qualifier"))
            text = f"{receiver}{match.group('op')}{qualifier}{method}"
            if receiver == "this":
                report["this_calls"].append({"file": listed, "line": line, "text": text})
                continue
            if qualifier:
                allowed, types = False, "?"
            else:
                allowed, types = resolve_receiver(receiver, method, rules, texts, masked)
            entry = {"file": listed, "line": line, "rule": "H7-b", "text": f"{text} [type={types}]"}
            (report["whitelisted"] if allowed else report["findings"]).append(entry)
    for finding in report["findings"]:
        report["per_rule"][finding["rule"]] += 1
    key = lambda f: (f["file"], f["line"], f.get("rule", ""), f["text"])
    report["findings"].sort(key=key)
    report["whitelisted"].sort(key=key)
    report["this_calls"].sort(key=key)
    report["expanded"].sort(key=lambda e: e["file"])
    report["unresolved_includes"].sort(key=lambda u: (u["file"], u["line"], u["include"]))
    return report


def format_lines(report):
    lines = [f"{f['file']}:{f['line']}: {f['rule']} {f['text']}" for f in report["findings"]]
    expanded = report["files_scanned"] - report["files_listed"]
    lines.append(f"h7_scan: {report['files_listed']} file(s) listed, {report['files_scanned']} scanned "
                 f"({expanded} expanded headers), {len(report['findings'])} finding(s), "
                 f"{len(report['whitelisted'])} whitelisted")
    return lines


def parse_expectation(spec):
    parts = spec.split("|", 2)
    if len(parts) != 3 or parts[1] not in RULES or not parts[0]:
        raise ContractError(f"--expect-finding {spec!r} must be '<path>|<rule>|<regex>' with rule in {RULES}")
    try:
        return parts[0], parts[1], re.compile(parts[2])
    except re.error as error:
        raise ContractError(f"--expect-finding {spec!r}: bad regex ({error})")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--list", action="append", default=[], help="a gyo.h7-hosts list file (repeatable)")
    parser.add_argument("--root", default=str(DIRECTORY.parents[2]), help="repository root")
    parser.add_argument("--rules", default=str(DEFAULT_RULES))
    parser.add_argument("--report", help="write the JSON report here")
    parser.add_argument("--no-fail", action="store_true", help="exit 0 even with findings (baseline report)")
    parser.add_argument("--expect-finding", action="append", default=[], help="'<path>|<rule>|<regex>'")
    args = parser.parse_args(argv)
    try:
        expectations = [parse_expectation(spec) for spec in args.expect_finding]
        if not args.list:
            if expectations:
                raise ContractError("--expect-finding needs at least one --list")
            print("h7_scan: nothing to scan")
            return 0
        zs = zone_scan_functions()
        rules = load_rules(args.rules)
        reports = [scan(args.root, path, rules, zs) for path in args.list]
        findings = [f for report in reports for f in report["findings"]]
        if args.report:
            totals = {rule: sum(r["per_rule"][rule] for r in reports) for rule in RULES}
            try:
                Path(args.report).parent.mkdir(parents=True, exist_ok=True)
                Path(args.report).write_text(json.dumps({"reports": reports, "per_rule": totals}, indent=2,
                                                        ensure_ascii=False) + "\n", encoding="utf-8")
            except OSError as error:
                raise ContractError(f"h7_scan: cannot write the report {args.report} ({error})")
    except ContractError as error:
        print(str(error), file=sys.stderr)
        return 2
    for report in reports:
        for line in format_lines(report):
            print(line)
    absent = [spec for spec, (path, rule, regex) in zip(args.expect_finding, expectations)
              if not any(f["file"] == path and f["rule"] == rule and regex.search(f["text"]) for f in findings)]
    for spec in absent:
        print(f"h7_scan: expected finding absent: {spec}", file=sys.stderr)
    if absent:
        return 1
    if findings and not args.no_fail:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
