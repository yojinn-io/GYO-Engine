"""Text scan of host source files for spec H-7 (target/kernel.zh-Hant.md DI-KS 15: a host does not order
Systems; a migrated host calls only Run, Rewind / Replay and the exchange points). Declaration and
judgement: docs/architecture/plans/kernel/h7-scanner-declaration.zh-Hant.md.

Input: one list file per owner (zero or more --list). With no list the scan prints
`h7_scan: nothing to scan` and exits 0: the common layer assumes no product (AGENTS.md section 7).

List contract, format "gyo.h7-hosts", version 1 (owned here; there is no target/ page for it):

  { "format": "gyo.h7-hosts", "version": 1, "generated_by": "manual", "repo_commit": "<7-40 hex>",
    "owner_root": "<repo-relative dir>", "include_roots": ["<dir under owner_root>", ...],
    "files": ["<repo-relative .cpp under owner_root>", ...] }

  Forward slashes, repo-relative, canonical; files sorted (byte order), unique, existing, source files
  only. No other field: in particular an owner cannot add whitelist entries. An unknown format, an
  unknown version, an unknown or missing field is a contract error (exit 2); the scanner never guesses.

Comments, string and character literals are blanked first (the masking of the zone-C scanner,
tests/common/zone_scan/zone_scan.py, imported, not copied; when that module or one of the three
functions is missing the scan fails with exit 2). Rules (names in h7_scan_rules.json):

  H7-a  an #include whose last path segment is a banned header (IRuntimeClient.hpp, RuntimeLoop.hpp),
        either form, case-insensitive. Scanned in the listed files and in every header they reach
        through quoted includes resolving under owner_root (transitive, each once; `via` is the chain).
  H7-b  a member call `<receiver> (. | ->) <Method> [<template args>] (` in a listed file, Method one of
        Tick, Advance, Update, Step, Run, unless the receiver's type is on the whitelist for that method.
        Not counted: `this->` calls, calls without a receiver, qualified calls (`Foo::Run(`).

Whitelist by type, approximated without a C++ parser; every uncertainty is reported (fail closed):
  1. The receiver is the last identifier before the operator (`session_.clock.Advance` -> clock). A
     receiver ending in `)` or `]` has no knowable type: reported, type `?`.
  2. Its declarations are searched in the listed file and its expanded owner headers: `<Type> [*&] name`
     followed by ; { = ( , ) or [. `auto name = other;` is followed one hop to `other`; a second hop, an
     auto with any other initializer, or no declaration at all: reported, type `?`.
  3. Wrappers in the rule data (std::unique_ptr, ...) and cv / pointer / reference are peeled.
  4. A declared type D matches a whitelist type Q when D == Q (a leading :: allowed), or D is a proper
     suffix of Q and the scanned text has `using namespace <the rest of Q's namespace>;`, or D is Q's
     last segment and the text has `using Q;`. The method must be on that entry.
  5. Every declaration found must match; any other: reported with the declared types.

Output: one line per finding `<path>:<line>: <rule> <text>`, then
`h7_scan: <N> file(s) listed, <M> scanned (<E> expanded headers), <H> finding(s), <W> whitelisted` per
list. `--report FILE` writes JSON {"reports": [per list: list, files_listed, files_scanned, findings,
whitelisted, this_calls, per_rule, expanded, unresolved_includes], "per_rule": totals}.
`--expect-finding "<path>|<rule>|<regex>"` (repeatable): exit 1 unless a finding of that file and rule
has a text matching the regex, even with --no-fail (an owner pins sites it knows must be reported).
Exit status: 0 clean or nothing to scan, 1 findings (0 with --no-fail) or a missing expected finding,
2 contract or usage error (also an unreadable or non-UTF-8 source file).

Usage: python3 h7_scan.py [--list LIST.json ...] [--root REPOSITORY] [--rules RULES.json]
                          [--report FILE] [--no-fail] [--expect-finding SPEC ...]
"""
import argparse
import importlib
import json
import re
import sys
from collections import deque
from pathlib import Path

DIRECTORY = Path(__file__).resolve().parent
ZONE_SCAN_DIRECTORY = DIRECTORY.parent / "zone_scan"
DEFAULT_RULES = DIRECTORY / "h7_scan_rules.json"
FORMAT = "gyo.h7-hosts"
SUPPORTED_VERSIONS = (1,)
LIST_FIELDS = ("format", "version", "generated_by", "repo_commit", "owner_root", "include_roots", "files")
SOURCE_SUFFIXES = (".cpp", ".cc", ".cxx")
RULES = ("H7-a", "H7-b")
ZONE_SCAN_NAMES = ("blank_comments_and_literals", "resolve_quoted_include", "validate_path")

INCLUDE = re.compile(r"^[ \t]*#[ \t]*include[ \t]*([<\"])([^>\"]+)[>\"]")
MASKED_INCLUDE = re.compile(r"^[ \t]*#[ \t]*include\b")
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

class Rules:
    def __init__(self, data):
        try:
            if data.get("version") != 1:
                raise ContractError(f"rules: unsupported version {data.get('version')!r}")
            self.headers = frozenset(h.lower() for h in data["H7a"]["headers"])
            self.methods = tuple(data["H7b"]["methods"])
            self.whitelist = [(tuple(split_qualified(e["type"])), frozenset(e["methods"])) for e in data["whitelist"]]
            self.unwrap = frozenset(data["unwrap"])
        except (KeyError, TypeError, AttributeError) as error:
            raise ContractError(f"rules: malformed rule data ({error!r})")
        if not self.headers or not self.methods:
            raise ContractError("rules: H7a.headers and H7b.methods must not be empty")
        alternatives = "|".join(re.escape(m) for m in self.methods)
        self.call = re.compile(
            rf"(?P<receiver>{IDENT}|[)\]])\s*(?P<op>\.|->)\s*(?P<method>{alternatives})\b\s*(?:{TEMPLATE_ARGS}\s*)?\(")


def load_rules(path):
    try:
        return Rules(json.loads(Path(path).read_text(encoding="utf-8")))
    except (OSError, json.JSONDecodeError) as error:
        raise ContractError(f"rules: cannot read {path} ({error})")


def split_qualified(text):
    text = re.sub(r"\s+", "", text)
    if text.startswith("::"):
        text = text[2:]
    return [part for part in text.split("::")]


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
    except Exception as error:  # zone_scan.ContractError; any failure is a contract error here
        errors.append(str(error))
        owner_root = None
    if not isinstance(data["include_roots"], list) or len(set(map(str, data["include_roots"]))) != len(data["include_roots"]):
        errors.append("list: include_roots must be an array without repeats")
    elif owner_root:
        for item in data["include_roots"]:
            try:
                zs.validate_path("include_roots item", item, owner_root)
            except Exception as error:  # zone_scan.ContractError; any failure is a contract error here
                errors.append(str(error))
                continue
            if not (Path(root) / item).is_dir():
                errors.append(f"list: include_roots item {item!r} is not a directory")
    files = data["files"] if isinstance(data["files"], list) else None
    if files is None:
        errors.append("list: files must be an array")
        files = []
    if not files:
        errors.append("list: files must name at least one host source")
    for item in files:
        try:
            zs.validate_path("files item", item, owner_root)
        except Exception as error:  # zone_scan.ContractError; any failure is a contract error here
            errors.append(str(error))
            continue
        if not item.endswith(SOURCE_SUFFIXES):
            errors.append(f"list: files item {item!r} is not a source file ({', '.join(SOURCE_SUFFIXES)})")
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
        return (Path(root) / relative).read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise ContractError(f"h7_scan: cannot read {relative} as UTF-8 ({error})")


# ----------------------------------------------------------------------------------------------
# Type resolution (the whitelist by type)

def declarations(name, texts):
    """[(type text, initializer identifier or None)] of every declaration of `name` in the masked texts."""
    pattern = re.compile(
        rf"(?<![\w.:>])(?<!->)(?P<type>{QUALIFIED_TYPE})(?P<decor>(?:\s*(?:\*|&|&&|\bconst\b|\bvolatile\b))*)"
        rf"\s*\b{re.escape(name)}\s*(?P<after>[;{{=(,)\[])")
    found = []
    for text in texts:
        for match in pattern.finditer(text):
            type_text = re.sub(r"\s+", "", match.group("type"))
            if split_qualified(type_text)[0] in NOT_A_TYPE:
                continue
            alias = None
            if type_text == "auto":
                rest = text[match.end("after"):]
                hop = re.match(rf"\s*(?:[\w.]*(?:\.|->))?({IDENT})\s*;", rest) if match.group("after") == "=" else None
                alias = hop.group(1) if hop else ""
            found.append((type_text, alias))
    return found


def peel(type_text, unwrap):
    """The declared type without cv, pointers, references and the listed wrappers."""
    text = re.sub(r"\s+", "", type_text)
    while True:
        text = re.sub(r"^(?:const|volatile)(?=[A-Za-z_:])", "", text)
        text = text[2:] if text.startswith("::") else text
        wrapped = WRAPPED.match(text)
        if wrapped and wrapped.group("outer") in unwrap:
            text = wrapped.group("inner").rstrip("*&")
            continue
        return text


def matches_whitelist(type_text, method, rules, texts):
    declared = split_qualified(peel(type_text, rules.unwrap))
    for qualified, methods in rules.whitelist:
        if method not in methods:
            continue
        if declared == list(qualified):
            return True
        n = len(declared)
        if 0 < n < len(qualified) and list(qualified[-n:]) == declared:
            prefix = "::".join(qualified[:-n])
            evidence = [rf"\busing\s+namespace\s+(?:::)?{re.escape(prefix).replace('::', r'\s*::\s*')}\s*;"]
            if n == 1:
                evidence.append(rf"\busing\s+(?:::)?{re.escape('::'.join(qualified)).replace('::', r'\s*::\s*')}\s*;")
            if any(re.search(e, text) for e in evidence for text in texts):
                return True
    return False


def resolve_receiver(receiver, method, rules, texts):
    """(whitelisted?, description of the declared types)."""
    if receiver in (")", "]"):
        return False, "?"
    found = declarations(receiver, texts)
    if not found:
        return False, "?"
    types = []
    for type_text, alias in found:
        if alias is None:
            types.append(type_text)
            continue
        hop = [t for t, a in declarations(alias, texts) if a is None] if alias else []
        if not alias or not hop or len(hop) != len(declarations(alias, texts)):
            return False, "?"
        types.extend(hop)
    unique = sorted(set(types))
    return all(matches_whitelist(t, method, rules, texts) for t in unique), ",".join(unique)


# ----------------------------------------------------------------------------------------------
# The scan

def line_of(text, offset):
    return text.count("\n", 0, offset) + 1


def includes_of(raw, masked):
    """[(line, target, form)] of the #include lines that are code, not comment text."""
    found = []
    raw_lines, masked_lines = raw.split("\n"), masked.split("\n")
    for number, (raw_line, masked_line) in enumerate(zip(raw_lines, masked_lines), start=1):
        if MASKED_INCLUDE.match(masked_line):
            match = INCLUDE.match(raw_line)
            if match:
                found.append((number, match.group(2).strip(), match.group(1)))
    return found


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
                if relative != listed and relative not in listing["files"]:
                    report["expanded"].append({"file": relative, "via": chain})
            closure.append(relative)
            raw, masked = masked_of[relative]
            for line, target, form in includes_of(raw, masked):
                if form != '"':
                    continue
                resolved = zs.resolve_quoted_include(str(root), relative, target, include_roots, owner_root)
                if resolved is None:
                    entry = {"file": relative, "line": line, "include": target}
                    if entry not in report["unresolved_includes"]:
                        report["unresolved_includes"].append(entry)
                elif resolved not in visited:
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
            text = f"{receiver}{match.group('op')}{method}"
            if receiver == "this":
                report["this_calls"].append({"file": listed, "line": line, "text": text})
                continue
            allowed, types = resolve_receiver(receiver, method, rules, texts)
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
    except ContractError as error:
        print(str(error), file=sys.stderr)
        return 2
    for report in reports:
        for line in format_lines(report):
            print(line)
    findings = [f for report in reports for f in report["findings"]]
    absent = [spec for spec, (path, rule, regex) in zip(args.expect_finding, expectations)
              if not any(f["file"] == path and f["rule"] == rule and regex.search(f["text"]) for f in findings)]
    for spec in absent:
        print(f"h7_scan: expected finding absent: {spec}", file=sys.stderr)
    if args.report:
        totals = {rule: sum(r["per_rule"][rule] for r in reports) for rule in RULES}
        Path(args.report).parent.mkdir(parents=True, exist_ok=True)
        Path(args.report).write_text(json.dumps({"reports": reports, "per_rule": totals}, indent=2, ensure_ascii=False) + "\n",
                                     encoding="utf-8")
    if absent:
        return 1
    if findings and not args.no_fail:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
