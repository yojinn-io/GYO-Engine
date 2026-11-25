#!/bin/bash
# The machine checks of the window's mode and VSync on macOS (window-mode-vsync declaration, section 5). Builds the
# example in the worktree's test build, runs the automatic pass, then the interactive one; both logs go to the main
# checkout's build/evidence/gyo2-window-mode-vsync/. Needs no network (the build directory is configured already).
#   examples/window_mode/acceptance_macos.sh            automatic pass, then the interactive one
#   examples/window_mode/acceptance_macos.sh --auto     the automatic pass only
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
main="$(cd "$(git -C "$root" rev-parse --git-common-dir)/.." && pwd)"
build="$root/build/target/_build/test"
evidence="$main/build/evidence/gyo2-window-mode-vsync"
cmake="$HOME/Code/Runtime/Builder/cmake-4.4.3/CMake.app/Contents/bin/cmake"
export PATH="$HOME/Code/Runtime/Builder/ninja-1.13.2:$PATH"
mkdir -p "$evidence"
stamp="$(date +%Y%m%d-%H%M%S)"
"$cmake" --build "$build" --target gyo_example_window_mode
exe="$build/examples/window_mode/gyo_example_window_mode"

echo "== automatic pass (about 4 seconds; the window goes fullscreen and back by itself)"
"$exe" --frames 200 2>&1 | tee "$evidence/macos-auto-$stamp.log"
echo "exit=${PIPESTATUS[0]}" | tee -a "$evidence/macos-auto-$stamp.log"
[[ "${1:-}" == "--auto" ]] && exit 0

echo
echo "== interactive pass: F fullscreen, W windowed, 1-4 sizes, V VSync, Escape closes (checklist: the example's README section)"
"$exe" 2>&1 | tee "$evidence/macos-interactive-$stamp.log"
echo "exit=${PIPESTATUS[0]}" | tee -a "$evidence/macos-interactive-$stamp.log"
echo "logs: $evidence"
