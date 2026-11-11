#!/usr/bin/env python3
"""Static scan for the kernel-line audit (v8-arch-kernel-audit).

For every module directory under the given roots (depth 1), count source lines and
list hits for the four audit questions of spec I.3:
  Q1 lifecycle / update-loop entry points and threading primitives
  Q2 cross-tick pointers or references (heuristic: member pointers/references to
     engine/product state; reported as candidates, judged by reading)
  Q3 global mutable state, clocks and randomness
  Q4 threads started by the module itself
Output: one CSV row per hit (module, question, category, file, line, text) plus a
summary CSV (module, lines, hits per category). Deterministic: sorted paths, no
hashing. Read-only. Usage:
  audit_scan.py --repo <checkout> --out <dir> [--roots engine framework apps/object_fps_pvp/src tools third_party]
"""
from __future__ import annotations
import argparse, csv, os, re, sys
from pathlib import Path

SOURCE_EXT = {".cpp", ".hpp", ".h", ".cc", ".cxx", ".inl", ".mm"}
PATTERNS = {
    # Q1 lifecycle / update loop
    ("Q1", "update-entry"): re.compile(r"\b(BeginFrame|EndFrame|Update|Tick|Advance|Pump(Events)?|Poll|Step|Run)\s*\("),
    ("Q1", "lifecycle"): re.compile(r"\b(Create|Start|Stop|Init(ialize)?|Shutdown|Destroy|Open|Close)\s*\("),
    ("Q1", "thread-primitive"): re.compile(r"\b(std::mutex|std::recursive_mutex|std::shared_mutex|std::atomic|std::condition_variable|RoleThread|std::lock_guard|std::unique_lock)\b"),
    # Q2 cross-tick pointer/reference candidates (members only; heuristic)
    ("Q2", "member-pointer-or-ref"): re.compile(r"^\s*(?:const\s+)?[A-Z][A-Za-z0-9_:<>]*\s*[*&]\s*[a-z][A-Za-z0-9_]*_\s*(=|;|\{)"),
    ("Q2", "callback-member"): re.compile(r"\b(std::function|std::move_only_function)\s*<"),
    # Q3 global mutable state, clock, random
    ("Q3", "static-mutable"): re.compile(r"^\s*(?:inline\s+)?static\s+(?!constexpr\b|const\b|inline\s+constexpr\b|void\b|bool\b\s*\w+\s*\()(?!.*\bconstexpr\b)[A-Za-z_][A-Za-z0-9_:<>,\s*&]*\s+[A-Za-z_][A-Za-z0-9_]*\s*(=|;|\{)"),
    ("Q3", "thread_local"): re.compile(r"\bthread_local\b"),
    ("Q3", "inline-variable"): re.compile(r"^\s*inline\s+(?!constexpr\b|const\b)(?:constinit\s+)?[A-Za-z_][A-Za-z0-9_:<>]*\s+[A-Za-z_][A-Za-z0-9_]*\s*(=|;|\{)"),
    ("Q3", "clock"): re.compile(r"(<chrono>|steady_clock|system_clock|high_resolution_clock|SDL_GetTicks|SDL_GetPerformanceCounter|QueryPerformanceCounter|clock_gettime|mach_absolute_time)"),
    ("Q3", "random"): re.compile(r"(<random>|random_device|\brand\s*\(|\bsrand\s*\(|mt19937|std::uniform_)"),
    ("Q3", "singleton"): re.compile(r"\b(Instance|GetInstance|Singleton)\s*\("),
    # Q4 threads started here
    ("Q4", "thread-start"): re.compile(r"\b(std::thread|std::jthread|RoleThread::Start|SDL_CreateThread|CreateThread|pthread_create|std::async)\b"),
}
SKIP_DIRS = {"build", ".git", "node_modules", "_deps", "CMakeFiles"}

def iter_sources(dir: Path):
    for root, dirs, files in os.walk(dir):
        dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS)
        for f in sorted(files):
            if Path(f).suffix in SOURCE_EXT:
                yield Path(root) / f

def modules_under(repo: Path, root: str):
    base = repo / root
    if not base.is_dir():
        return []
    subs = sorted(p for p in base.iterdir() if p.is_dir() and p.name not in SKIP_DIRS)
    # a root with sources directly in it (e.g. apps/x/src) counts as one module
    direct = [p for p in base.iterdir() if p.is_file() and p.suffix in SOURCE_EXT]
    mods = [(f"{root}/{p.name}", p) for p in subs]
    if direct:
        mods.append((f"{root}/.", base))
    return mods

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--roots", nargs="+", default=["engine", "framework", "apps/object_fps_pvp/src", "tools", "third_party"])
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    hits_path, summary_path = a.out / "audit_hits.csv", a.out / "audit_summary.csv"
    cats = sorted(PATTERNS.keys())
    with hits_path.open("w", newline="", encoding="utf-8") as hf, summary_path.open("w", newline="", encoding="utf-8") as sf:
        hw, sw = csv.writer(hf), csv.writer(sf)
        hw.writerow(["module", "question", "category", "file", "line", "text"])
        sw.writerow(["module", "files", "lines"] + [f"{q}:{c}" for q, c in cats])
        total_mods = 0
        for root in a.roots:
            for name, path in modules_under(a.repo, root):
                total_mods += 1
                files = list(iter_sources(path)) if name.endswith("/.") is False else [p for p in path.iterdir() if p.is_file() and p.suffix in SOURCE_EXT]
                counts = {k: 0 for k in cats}
                nlines = 0
                for f in files:
                    try:
                        text = f.read_text(encoding="utf-8", errors="replace").splitlines()
                    except OSError:
                        continue
                    nlines += len(text)
                    for i, line in enumerate(text, 1):
                        s = line.strip()
                        if s.startswith("//") or s.startswith("*") or s.startswith("/*"):
                            continue
                        for key, rx in PATTERNS.items():
                            if rx.search(line):
                                counts[key] += 1
                                hw.writerow([name, key[0], key[1], str(f.relative_to(a.repo)), i, s[:160]])
                sw.writerow([name, len(files), nlines] + [counts[k] for k in cats])
        print(f"modules={total_mods} hits_csv={hits_path} summary_csv={summary_path}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
