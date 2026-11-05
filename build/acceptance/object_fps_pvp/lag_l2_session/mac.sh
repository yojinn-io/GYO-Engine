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
#
# PKG is the package folder gyo-object_fps_pvp/ from the macos-x64 tarball (it holds
# build_metadata.json and bin/); GATEWAY the
# Gateway built from the same commit. Nothing here needs the network beyond the LAN.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
command="${1:-}"
shift || true

arena_of() {
    local found
    found="$(find "$1" -name pvp_corners.json -path '*object_fps_pvp*')"
    [ -n "$found" ] || { echo "no pvp_corners.json under $1" >&2; exit 2; }
    [ "$(echo "$found" | wc -l)" -eq 1 ] || { echo "more than one pvp_corners.json under $1: $found" >&2; exit 2; }
    echo "$found"
}

case "$command" in
prepare)
    session="$1"; pkg="$2"; gateway="$3"
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
    cat "$session/marks.txt"
    echo "This Mac's addresses (pick the one on the Windows machines' network):"
    ifconfig | awk '/^[a-z]/{name=$1} /inet /{print "  " name " " $2}'
    ;;
match)
    session="$1"; pkg="$2"
    # caffeinate: an idle Mac must not sleep during the session (nobody touches it for minutes).
    arena="$(arena_of "$pkg")"
    exec caffeinate -i "$pkg/bin/gyo_object_fps_pvp-match" --arena "$arena" --listen 127.0.0.1:27016 \
        --log "$session/mac/match.log" --movement-trace "$session/mac/match-commands.jsonl"
    ;;
gateway)
    session="$1"; gateway="$2"; mac_ip="$3"
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
*)
    sed -n '2,13p' "$0" >&2
    exit 2
    ;;
esac
