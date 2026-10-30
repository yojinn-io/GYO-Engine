"""Report for v8-tool-ui-ime-observe: what happened to ':' in each text input session of a PvP client log.

The client writes the input backend's text diagnostics with --text-input-trace (category "input.text"). A trial is
one text input session: from "set_text_input enabled=1" to "set_text_input enabled=0" (or the end of the log). For
each trial the report gives the committed text (text_input lines the frame took), the compositions seen, and one
class for the colon, fixed before any Windows log was read:

- ascii      a committed text_input holds ':' (U+003A)
- fullwidth  no ':' was committed, but a full-width colon (U+FF1A) was; the product's address filter drops it
- held       a composition (text_editing) held a colon, and no colon was committed in the trial
- missing    no colon was composed or committed
"composed_first" says whether a composition held the colon before it was committed, and keys_until_commit counts
the text keys between that composition and the commit.

Usage: python3 ime_observe_report.py CLIENT_LOG
Exit status 0 with one line per trial; 2 when the log has no text diagnostics.
"""
import re
import sys
from pathlib import Path

LINE = re.compile(r"\] input\.text (?P<event>\w+) (?P<fields>.*)$")
FIELD = re.compile(r'(\w+)=("(?:[^"\\]|\\.)*"|\S+)')
ESCAPE = re.compile(r"\\u\{([0-9A-F]+)\}|\\x\{([0-9A-F]{2})\}")
COLONS = {":": "ascii", "\uff1a": "fullwidth"}


def decode(quoted: str) -> str:
    if not quoted.startswith('"'):
        return ""
    return ESCAPE.sub(lambda m: chr(int(m.group(1), 16)) if m.group(1) else "\ufffd", quoted[1:-1])


def parse(path: Path) -> list[tuple[str, dict[str, str]]]:
    events = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = LINE.search(line)
        if match:
            events.append((match["event"], dict(FIELD.findall(match["fields"]))))
    return events


def trials(events):
    current = None
    for event, fields in events:
        if event == "set_text_input" and fields.get("enabled") == "1":
            if current:
                yield current
            current = []
        elif event == "set_text_input" and fields.get("enabled") == "0":
            if current is not None:
                yield current
            current = None
        elif current is not None:
            current.append((event, fields))
    if current:
        yield current


def classify(trial) -> dict:
    committed = ""
    compositions = 0
    composed_at = None
    composed_first = False
    keys = 0
    keys_until_commit = None
    colon = None
    for event, fields in trial:
        if event == "text_key" and fields.get("own_window") == "1":
            keys += 1
        elif event == "text_editing" and fields.get("own_window") == "1":
            text = decode(fields.get("text", ""))
            if text:
                compositions += 1
            if any(mark in text for mark in COLONS) and composed_at is None and colon is None:
                composed_at = keys
        elif event == "text_input" and fields.get("taken") == "1":
            text = decode(fields.get("text", ""))
            committed += text
            for mark, name in COLONS.items():
                if mark in text and (colon is None or (colon == "fullwidth" and name == "ascii")):
                    if colon is None and composed_at is not None:
                        composed_first = True
                        keys_until_commit = keys - composed_at
                    colon = name
    if colon is None:
        colon = "held" if composed_at is not None else "missing"
    return {"committed": committed, "colon": colon, "compositions": compositions,
            "composed_first": composed_first, "keys_until_commit": keys_until_commit}


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__.strip().splitlines()[-2])
        return 2
    events = parse(Path(sys.argv[1]))
    if not events:
        print("no input.text lines: was the client started with --text-input-trace?")
        return 2
    header = next((fields for event, fields in events if event == "diagnostics"), {})
    print(f"diagnostics ime_ui={header.get('ime_ui', '?')} events={len(events)}")
    for index, trial in enumerate(trials(events), start=1):
        result = classify(trial)
        print(f"trial {index}: colon={result['colon']} committed={result['committed']!r} "
              f"compositions={result['compositions']} composed_first={int(result['composed_first'])} "
              f"keys_until_commit={result['keys_until_commit']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
