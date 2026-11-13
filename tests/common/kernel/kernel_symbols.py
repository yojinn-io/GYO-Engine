"""Writable data symbols of the kernel library (registry-schedule declaration section 3 item 1, judgement 9).

kernel_guard.py (spec J-9, K4) is keyword-based: it sees "static" and "thread_local" but not a namespace-scope
variable written without them ("inline int g = 0;", a variable in an anonymous namespace, "extern int g;"). The
compiler sees every one of them: each is a symbol in a writable data section of the library. This guard lists
those symbols with nm and allows only:

  KERNEL_ALLOWED   the kernel's two (spec A.6, J-9): TypeKey's anchor (Detail::TypeKeyOf<T>::typeKeyAnchor,
                   engine/kernel/include/engine/kernel/TypeKey.hpp) and tlCurrent (the access verification's
                   thread_local, which arrives with world-exec);
  FOREIGN          symbols the kernel's object files carry because they include another module's header that
                   defines an inline variable. They belong to that module, not to the kernel: GYO::Base's
                   assertion handler and its thread_local reentry flag (engine/base/include/engine/base/Assert.hpp;
                   spec B.5 names both). They are reported on their own line, never silently;
  TOOLCHAIN        data the compiler emits for exception handling (GCC's DW.ref.__gxx_personality_v0), reported
                   the same way.
--require NAME makes an allowed name mandatory (a positive control: an empty result cannot pass).

Writable sections: on ELF (Linux) .data*, .bss*, .tdata*, .tbss*, not .data.rel.ro* (read-only after relocation);
on Mach-O (macOS) the __DATA segment's __data, __bss, __common and __thread_* sections, not __const. A symbol's
"$tlv$init" companion (Mach-O thread-local storage) is reported under its variable's name.

The report lists, for each allowed name, whether it is present. Exit status 0 when every writable symbol is
allowed or foreign; 1 with one line per other symbol; 2 when nm fails or prints nothing readable.
Windows is not covered (MSVC has no nm; dumpbin prints another shape): declared limit.

On macOS the nm -m output is demangled with c++filt.

Usage: python3 kernel_symbols.py --nm NM (--library LIBRARY | --library-newest DIR) [--format auto|sysv|darwin]
"""
import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

KERNEL_ALLOWED = {
    "TypeKey anchor": re.compile(r"^GYO::Kernel::Detail::TypeKeyOf<.+>::typeKeyAnchor$"),
    "tlCurrent": re.compile(r"^GYO::Kernel::(?:\w+::)*tlCurrent$"),
}
FOREIGN = {
    "GYO::Base assertion handler": re.compile(r"^Engine::Base::Detail::assertionHandler$"),
    "GYO::Base assertion reentry flag": re.compile(r"^Engine::Base::Detail::reportingAssertion$"),
}
# Data the compiler emits for exception handling, not a variable of any source: GCC's indirect reference to the
# C++ personality routine (DW.ref.__gxx_personality_v0, in .data.rel.local on ELF).
TOOLCHAIN = {
    "GCC personality reference": re.compile(r"^DW\.ref\.__gxx_personality_v0$"),
}
ELF_WRITABLE = re.compile(r"^\.(?:data|bss|tdata|tbss)(?:\.|$)")
ELF_READ_ONLY = re.compile(r"^\.data\.rel\.ro(?:\.|$)")
DARWIN_WRITABLE = {"__data", "__bss", "__common", "__thread_vars", "__thread_data", "__thread_bss"}
DARWIN_LINE = re.compile(r"^[0-9a-fA-F]+\s+\((\w+),(\w+)\)\s+(.*)$")


def parse_sysv(text):
    """Writable defined symbols from `nm -C --format=sysv` output (GNU or LLVM nm, ELF)."""
    symbols = set()
    for line in text.splitlines():
        parts = [part.strip() for part in line.split("|")]
        if len(parts) < 7 or parts[0] in ("", "Name"):
            continue
        name, value, section = parts[0], parts[1], parts[6]
        if not value or not ELF_WRITABLE.match(section) or ELF_READ_ONLY.match(section):
            continue
        symbols.add(name)
    return symbols


def parse_darwin(text):
    """Writable defined symbols from `nm -m -C` output (Mach-O)."""
    symbols = set()
    for line in text.splitlines():
        match = DARWIN_LINE.match(line.strip())
        if not match:
            continue
        segment, section, rest = match.groups()
        if segment != "__DATA" or section not in DARWIN_WRITABLE:
            continue
        name = re.sub(r"^(?:weak\s+|private\s+)?(?:non-)?external\s+(?:automatically hidden\s+)?", "", rest).strip()
        name = re.sub(r"\$tlv\$init$", "", name)
        symbols.add(name)
    return symbols


def classify(symbols):
    """(present allowed names, foreign symbols, findings)."""
    present, foreign, findings = set(), [], []
    for name in sorted(symbols):
        allowed = [label for label, pattern in KERNEL_ALLOWED.items() if pattern.match(name)]
        if allowed:
            present.update(allowed)
            continue
        if any(pattern.match(name) for pattern in FOREIGN.values()) or \
                any(pattern.match(name) for pattern in TOOLCHAIN.values()):
            foreign.append(name)
            continue
        findings.append(name)
    return present, foreign, findings


def run(command, stdin=None):
    done = subprocess.run(command, input=stdin, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                          errors="replace", timeout=120)
    if done.returncode != 0:
        raise RuntimeError(f"{' '.join(command)} exited {done.returncode}: {done.stderr.strip()}")
    return done.stdout


def run_nm(nm, library, kind):
    if kind != "darwin":
        return run([nm, "-C", "--format=sysv", library])
    # Apple's nm does not demangle together with -m; c++filt does (Mach-O names carry one extra "_").
    cxxfilt = shutil.which("c++filt")
    if cxxfilt is None:
        raise RuntimeError("c++filt not found (needed to demangle nm -m output on macOS)")
    # A thread-local's "$tlv$init" companion would stop c++filt from demangling it; drop the suffix first.
    raw = re.sub(r"\$tlv\$init$", "", run([nm, "-m", library]), flags=re.MULTILINE)
    return run([cxxfilt], stdin=raw)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--nm", required=True)
    library = parser.add_mutually_exclusive_group(required=True)
    library.add_argument("--library")
    # For the mutation catalog, whose commands cannot name a build directory: the most recently built kernel
    # library under DIR (the one the mutation runner just rebuilt).
    library.add_argument("--library-newest", metavar="DIR")
    parser.add_argument("--format", choices=("auto", "sysv", "darwin"), default="auto")
    # Allowed names that must be present: a positive control, so an unreadable symbol table or a wrong section
    # list (an empty result) cannot pass (architect 2026-11-13, judgement 9 condition (i)).
    parser.add_argument("--require", action="append", default=[], choices=sorted(KERNEL_ALLOWED))
    args = parser.parse_args(argv)
    if args.library_newest:
        found = sorted(Path(args.library_newest).rglob("libgyo_kernel.a"), key=lambda p: p.stat().st_mtime)
        if not found:
            print(f"kernel symbols: no libgyo_kernel.a under {args.library_newest}")
            return 2
        args.library = str(found[-1])
    kind = args.format if args.format != "auto" else ("darwin" if sys.platform == "darwin" else "sysv")
    try:
        text = run_nm(args.nm, args.library, kind)
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"kernel symbols: nm failed: {error}")
        return 2
    if not text.strip():
        print("kernel symbols: nm printed nothing for the library")
        return 2
    symbols = parse_darwin(text) if kind == "darwin" else parse_sysv(text)
    present, foreign, findings = classify(symbols)
    for label in KERNEL_ALLOWED:
        print(f"kernel symbols: allowed {label}: {'present' if label in present else 'absent'}")
    for name in foreign:
        print(f"kernel symbols: foreign (another module's inline variable, or toolchain data): {name}")
    for name in findings:
        print(f"kernel symbols: writable data symbol not allowed: {name}")
    missing = [label for label in args.require if label not in present]
    for label in missing:
        print(f"kernel symbols: required allowed symbol missing: {label} (is the symbol table read, and the "
              f"section list right?)")
    if findings or missing:
        print(f"kernel symbols: {len(findings) + len(missing)} finding(s) (declaration judgement 9; spec J-9, A.6)")
        return 1
    print(f"kernel symbols: clean ({len(symbols)} writable symbol(s), {nm_kind(kind)})")
    return 0


def nm_kind(kind):
    return "Mach-O, nm -m" if kind == "darwin" else "ELF, nm --format=sysv"


if __name__ == "__main__":
    sys.exit(main())
