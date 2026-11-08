#!/bin/bash
# Builds the offline kits of the lag-l2 + K8 session (one folder and zip per
# machine; docs/object_fps_pvp/plans/v8/v8-product-l2-session-data-declaration.md).
#
#   make_kit.sh OUT WINDOWS_PKG MAC_PKG MAC_GATEWAY [names...]   (default names: win1 win2 win3)
#
# WINDOWS_PKG is the gyo-object_fps_pvp/ folder of the windows-x64 tarball and
# MAC_PKG / MAC_GATEWAY the Mac's local build, all from one commit. Each Windows
# kit holds the package, windows.ps1, start.cmd (double-click) and README.txt;
# the Mac kit holds its package, the Gateway, mac.sh and the judge. Every kit
# carries kit-manifest.txt (revision and SHA-256 of every file).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="$1"; windows_pkg="$2"; mac_pkg="$3"; mac_gateway="$4"; shift 4
names=("$@"); [ ${#names[@]} -gt 0 ] || names=(win1 win2 win3)
[ ! -e "$out" ] || { echo "$out exists: use a new folder" >&2; exit 2; }
revision() { python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["source_revision"])' "$1/build_metadata.json"; }
windows_revision="$(revision "$windows_pkg")"; mac_revision="$(revision "$mac_pkg")"
[ "$windows_revision" = "$mac_revision" ] || { echo "Windows $windows_revision and Mac $mac_revision differ" >&2; exit 2; }
manifest() {
    (cd "$1" && { echo "# source_revision=$windows_revision"; echo "# kit=$(basename "$1") built=$(date +%Y-%m-%dT%H:%M:%S)";
        echo "# sha256  bytes  path"
        find . -type f ! -name kit-manifest.txt | LC_ALL=C sort | while read -r f; do
            echo "$(shasum -a 256 "$f" | cut -d' ' -f1)  $(stat -f %z "$f")  $f"; done; } > kit-manifest.txt)
}
mkdir -p "$out"
for name in "${names[@]}"; do
    kit="$out/$name"
    mkdir -p "$kit"
    cp -R "$windows_pkg" "$kit/gyo-object_fps_pvp"
    cp "$here/windows.ps1" "$kit/"
    sed "s/@NAME@/$name/" "$here/start-windows.cmd" | sed 's/$/\r/' > "$kit/start.cmd"
    cat > "$kit/README.txt" <<TEXT
lag-l2 + K8 session kit: $name (commit $windows_revision)

0. Keep 2 GB free on this disk: one hour of play writes about 1.5-2 GB of logs here.
1. Copy this folder to a local disk (not a network drive).
2. Double-click start.cmd. Enter the Mac IP shown in the Mac terminal.
   - If Windows Firewall asks, allow the network the PC is on (often "Public" without internet).
   - If SmartScreen warns, choose "More info" -> "Run anyway".
3. Create or join the room and play. Do not close the black window.
4. At the end: stop shooting for 3 seconds, press Esc to leave, close the game window.
5. The window prints "Done": hand back lag-l2-$name*.zip from this folder.
TEXT
    sed -i '' 's/$/\r/' "$kit/README.txt"
    manifest "$kit"
    (cd "$out" && zip -q -r "lag-l2-kit-$name.zip" "$name")
done
kit="$out/mac"
mkdir -p "$kit/lag_l2_session"
cp -R "$mac_pkg" "$kit/gyo-object_fps_pvp"
cp "$mac_gateway" "$kit/gateway"
cp "$here/mac.sh" "$kit/lag_l2_session/"
cp "$here/../lag_l2_summary.py" "$here/../lan15_judge.py" "$kit/"
cat > "$kit/README.txt" <<TEXT
lag-l2 + K8 session kit: Mac (commit $mac_revision)

  ARENA_FILE=biome.json MOVEMENT_DIAG=1 lag_l2_session/mac.sh run SESSION gyo-object_fps_pvp gateway

SESSION is a new folder name. Type the Mac IP on the Windows network when asked.
Keys: s = s1-end after the continuous play, c = check, q = stop at the very end.
Then: lag_l2_session/mac.sh collect SESSION lag-l2-win1*.zip lag-l2-win2*.zip lag-l2-win3*.zip
Keep 2 GB free: one hour writes about 1-1.5 GB on the Mac.
TEXT
manifest "$kit"
echo "kits in $out:"; ls -1 "$out"
