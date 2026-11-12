#!/bin/bash
# lag-l2 + K8 L2 session, the Mac side (docs/object_fps_pvp/plans/v8/v8-product-hitbox-lag-l2-session.md).
# The Mac runs the Match and the Gateway only; the players are on Windows.
#
#   mac.sh prepare  SESSION PKG GATEWAY   new session folder, hashes, revision, this Mac's addresses
#   mac.sh match    SESSION PKG           the Match (foreground; Ctrl+C at the very end)
#   mac.sh gateway  SESSION GATEWAY MAC_IP  the Gateway (foreground; Ctrl+C before the Match)
#   mac.sh check    MAC_IP                the Gateway answers on MAC_IP:8080
#   mac.sh s1-end   SESSION               writes s1_end=<now> to marks.txt (end of the continuous play)
#   mac.sh judge    SESSION               lag_l2_summary.py on the collected session folder
#   mac.sh run      SESSION PKG GATEWAY   the whole Mac side in one terminal: prepare, Match and
#                                         Gateway in the background, memory samples, then keys:
#                                         s = s1-end, c = check, q = stop (Gateway, then Match)
#   mac.sh collect  SESSION ZIP...        unzip each Windows lag-l2-<name>*.zip into
#                                         clients/<name>/, judge, and zip the whole session
#
# MOVEMENT_DIAG=1 also writes the Match's movement diagnostics (v8-product-pm-detect-diag;
# a package from before it refuses the option).
# ARENA_FILE picks the arena file in the package (default pvp_corners.json; the
# 11/9 session uses biome.json, lag-l2 declaration revision 8): set it the same
# for prepare and match.
# PKG is the package folder gyo-object_fps_pvp/ from the macos-x64 tarball (it holds
# build_metadata.json and bin/); GATEWAY the
# Gateway built from the same commit. Nothing here needs the network beyond the LAN.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
arena_file="${ARENA_FILE:-pvp_corners.json}"
command="${1:-}"
shift || true

# An absolute path: a bare name such as "gateway" would otherwise be looked up
# in PATH by caffeinate and exec (the 2026-11-08 Windows rehearsal: the Gateway
# never started).
absolute() { (cd "$(dirname "$1")" && echo "$PWD/$(basename "$1")"); }

arena_of() {
    local found
    found="$(find "$1" -name "$arena_file" -path '*object_fps_pvp*')"
    [ -n "$found" ] || { echo "no $arena_file under $1" >&2; exit 2; }
    [ "$(echo "$found" | wc -l)" -eq 1 ] || { echo "more than one $arena_file under $1: $found" >&2; exit 2; }
    echo "$found"
}

case "$command" in
prepare)
    session="$1"; pkg="$(absolute "$2")"; gateway="$(absolute "$3")"
    [ ! -e "$session" ] || { echo "$session exists: use a new folder" >&2; exit 2; }
    # Everything is read before the folder is made, so a wrong PKG leaves nothing behind.
    match="$pkg/bin/gyo_object_fps_pvp-match"
    revision="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["source_revision"])' "$pkg/build_metadata.json")"
    match_hash="$(shasum -a 256 "$match" | cut -d' ' -f1)"
    gateway_hash="$(shasum -a 256 "$gateway" | cut -d' ' -f1)"
    arena="$(arena_of "$pkg")"
    [ -n "$revision" ] && [ -n "$match_hash" ] && [ -n "$gateway_hash" ] || { echo "missing revision or hash" >&2; exit 2; }
    mkdir -p "$session/mac" "$session/clients"
    {
        echo "# source_revision=$revision"
        echo "# match sha256=$match_hash"
        echo "# gateway sha256=$gateway_hash"
        echo "# arena=$arena"
    } > "$session/marks.txt"
    # Session facts (field-session C1): this Mac.
    {
        echo "start=$(date +%Y-%m-%dT%H:%M:%S%z)"
        sw_vers | sed 's/^/os_/; s/:[[:space:]]*/=/'
        echo "cpu=$(sysctl -n machdep.cpu.brand_string)"
        echo "ram_bytes=$(sysctl -n hw.memsize)"
        system_profiler SPDisplaysDataType 2>/dev/null | awk -F': ' '/Chipset Model/{print "gpu=" $2}'
        echo "arena_file=$arena_file movement_diag=${MOVEMENT_DIAG:-0} artificial_delay=none"
    } > "$session/mac/machine.txt"
    cat "$session/marks.txt"
    echo "This Mac's addresses (pick the one on the Windows machines' network):"
    ifconfig | awk '/^[a-z]/{name=$1} /inet /{print "  " name " " $2}'
    ;;
match)
    session="$1"; pkg="$(absolute "$2")"
    # caffeinate: an idle Mac must not sleep during the session (nobody touches it for minutes).
    arena="$(arena_of "$pkg")"
    exec caffeinate -i "$pkg/bin/gyo_object_fps_pvp-match" --arena "$arena" --listen 127.0.0.1:27016 \
        --log "$session/mac/match.log" --movement-trace "$session/mac/match-commands.jsonl" \
        ${MOVEMENT_DIAG:+--movement-diag "$session/mac/match-movement-diag.jsonl"}
    ;;
gateway)
    session="$1"; gateway="$(absolute "$2")"; mac_ip="$3"
    exec caffeinate -i "$gateway" -runtime 127.0.0.1:27016 -http 0.0.0.0:8080 -udp 0.0.0.0:27015 \
        -advertise-ip "$mac_ip" -log "$session/mac/gateway.log"
    ;;
check)
    curl -sS --max-time 3 "http://$1:8080/rooms"; echo
    ;;
s1-end)
    session="$1"
    line="s1_end=$(date +%Y-%m-%dT%H:%M:%S)"
    echo "$line" >> "$session/marks.txt"
    echo "$line"
    ;;
judge)
    session="$1"
    python3 "$here/../lag_l2_summary.py" "$session" --output "$session/lag_l2_summary.json"
    ;;
run)
    session="$1"; pkg="$(absolute "$2")"; gateway="$(absolute "$3")"
    "$0" prepare "$session" "$pkg" "$gateway"
    mac_ip="${MAC_IP:-}"
    if [ -z "$mac_ip" ]; then
        read -r -p "MAC_IP (the address on the Windows machines' network): " mac_ip
    fi
    echo "# mac_ip=$mac_ip" >> "$session/marks.txt"
    "$0" match "$session" "$pkg" > "$session/mac/match.out" 2>&1 &
    match_pid=$!
    for _ in $(seq 60); do grep -q "Match ready" "$session/mac/match.log" 2>/dev/null && break; sleep 0.5; done
    grep -q "Match ready" "$session/mac/match.log" || { echo "the Match did not become ready (see $session/mac/)" >&2; kill -INT "$match_pid"; exit 1; }
    "$0" gateway "$session" "$gateway" "$mac_ip" > "$session/mac/gateway.out" 2>&1 &
    gateway_pid=$!
    for _ in $(seq 20); do curl -s --max-time 1 "http://$mac_ip:8080/rooms" > /dev/null && break; sleep 0.5; done
    # Memory of the Match and the Gateway every 10 s (exploratory data).
    (
        echo "time,process,rss_kb"
        while kill -0 "$match_pid" 2>/dev/null; do
            now="$(date +%Y-%m-%dT%H:%M:%S)"
            for name in gyo_object_fps_pvp-match "$(basename "$gateway")"; do
                pid="$(pgrep -nx "$name" || true)"
                [ -n "$pid" ] && echo "$now,$name,$(ps -o rss= -p "$pid" | tr -d ' ')"
            done
            sleep 10
        done
    ) > "$session/mac/memory.csv" &
    if ! "$0" check "$mac_ip"; then
        echo "The Gateway does not answer on $mac_ip:8080 (see $session/mac/gateway.out). Press q, y to stop." >&2
    fi
    echo "Running. Keys: s = s1-end (after the continuous play), c = check, q = stop (only at the very end)."
    while true; do
        read -r -n 1 -s key || key=q
        case "$key" in
        s) "$0" s1-end "$session" ;;
        c) "$0" check "$mac_ip" || true ;;
        q)
            read -r -p "Stop the Gateway and the Match now? (y/N) " answer
            [ "$answer" = "y" ] || continue
            # caffeinate does not pass SIGINT on: signal the programs themselves (as Ctrl+C would).
            pkill -INT -f "^$gateway -runtime" || true; wait "$gateway_pid" 2>/dev/null || true
            pkill -INT -x gyo_object_fps_pvp-match || true; wait "$match_pid" 2>/dev/null || true
            echo "# end=$(date +%Y-%m-%dT%H:%M:%S%z)" >> "$session/marks.txt"
            tail -1 "$session/mac/match-shots.jsonl" 2>/dev/null || true
            echo "Stopped. Next: mac.sh collect $session <the Windows zips>"
            break ;;
        esac
    done
    ;;
collect)
    session="$1"; shift
    for zip in "$@"; do
        name="$(basename "$zip" .zip | sed -E 's/^lag-l2-([^-]+).*/\1/')"
        mkdir -p "$session/clients/$name"
        unzip -o -q "$zip" -d "$session/clients/$name"
        # A zip that passed through a cloud drive wraps the files in one folder:
        # move them up so the judge finds clients/<name>/*.log.
        inner="$(find "$session/clients/$name" -mindepth 1 -maxdepth 1 -type d)"
        if [ -z "$(find "$session/clients/$name" -maxdepth 1 -name '*.log')" ] && [ "$(echo "$inner" | grep -c .)" -eq 1 ]; then
            mv "$inner"/* "$session/clients/$name/" && rmdir "$inner"
        fi
        echo "unzipped $zip -> clients/$name"
    done
    "$0" judge "$session" || echo "judge exit $? (the summary, if any, is in $session/lag_l2_summary.json)"
    archive="$session.zip"
    (cd "$(dirname "$session")" && zip -q -r "$(basename "$archive")" "$(basename "$session")")
    echo "The whole session: $archive"
    ;;
*)
    sed -n '2,23p' "$0" >&2
    exit 2
    ;;
esac
