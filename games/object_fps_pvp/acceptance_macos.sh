#!/bin/bash
# The game flow's machine check on this Mac (game-flow declaration, the card's completion): every service on this
# machine (Gateway 127.0.0.1:7700, real Matches), and two Clients with profiles of their own ("a" and "b"), each with its
# own device id and so its own Gateway user. Builds what it runs in the worktree's test build; the logs and the
# services' data go to the main checkout's build/evidence/gyo2-pvp-game-flow/<time>/. Needs no network.
#
#   games/object_fps_pvp/acceptance_macos.sh            the services and two Clients (close both windows to end)
#   games/object_fps_pvp/acceptance_macos.sh --one      the services and one Client (single player, settings)
#
# The checklist (in either Client):
#   1. Single player: room -> 出撃 -> the loading screen, then the game; Esc -> 撤退する -> the menu says it was
#      recorded; the room shows the record. No socket is opened (the services' log shows no session for it).
#   2. Settings: 解像度 four sizes in turn, 適用 resizes the window; the next start opens at that size.
#   3. Multiplayer: Client a makes a room (モード, マップ, 作成), Client b joins it from the list; both 準備する;
#      a 開始; both show the loading screen until both are loaded, then play in one Match and see each other.
#   4. Esc -> 撤退する in both: back to the lobby; after a few seconds the record says one more game.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
main="$(cd "$(git -C "$root" rev-parse --git-common-dir)/.." && pwd)"
build="$root/build/target/_build/test"
cmake="$HOME/Code/Runtime/Builder/cmake-4.4.3/CMake.app/Contents/bin/cmake"
export PATH="$HOME/Code/Runtime/Builder/ninja-1.13.2:/usr/local/go/bin:$PATH"
stamp="$(date +%Y%m%d-%H%M%S)"
evidence="$main/build/evidence/gyo2-pvp-game-flow/$stamp"
mkdir -p "$evidence/data"

"$cmake" --build "$build" --target gyo_object_fps_pvp gyo_object_fps_pvp-match gyo_object_fps_pvp-services gyo_object_fps_pvp--content
bin="$root/build/target/object_fps_pvp/bin"
services="$root/build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-services"

echo "== the services (log: $evidence/services.log)"
"$services" all --gateway 127.0.0.1:7700 --data "$evidence/data" --match "$bin/gyo_object_fps_pvp-match" \
    --content "$root/assets/object_fps_pvp" >"$evidence/services.log" 2>&1 &
services_pid=$!
trap 'kill "$services_pid" 2>/dev/null || true' EXIT
sleep 1
if ! kill -0 "$services_pid" 2>/dev/null; then
    echo "the services did not start:"; cat "$evidence/services.log"; exit 1
fi

echo "== Client a (log: $evidence/client-a.log)"
"$bin/gyo_object_fps_pvp" --profile a --verbose >"$evidence/client-a.log" 2>&1 &
client_a=$!
if [[ "${1:-}" != "--one" ]]; then
    echo "== Client b (log: $evidence/client-b.log)"
    "$bin/gyo_object_fps_pvp" --profile b --verbose >"$evidence/client-b.log" 2>&1 &
    client_b=$!
    wait "$client_b" || echo "Client b exit=$?"
fi
wait "$client_a" || echo "Client a exit=$?"

echo "== the saves the Matches wrote (the save service's data)"
find "$evidence/data" -type f -print
echo "logs: $evidence"
