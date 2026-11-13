#!/usr/bin/env bash
# v8-product-pvp-baseline: the same measurements inside the Linux toolchain
# container (build/ci/linux-toolchain/Dockerfile). Owner object_fps_pvp; run by
# hand, never by CI. Inside the container:
#   build    SOURCE_TAR=/in/src.tar  -> /work/src (a git archive), the test preset
#            configuration, the measured targets and a local Gateway
#   measure  OUT=/out [REPS=5] [RUNS=5] [DURATION=600] -> the declaration's M1..M7 into OUT
# /work is a Docker volume, so a measure run sees the build run's tree.
set -euo pipefail
phase=${1:?build or measure}
src=/work/src
build=$src/build/target/_build/test
case "$phase" in
build)
    rm -rf "$src" && mkdir -p "$src" && tar -xf "${SOURCE_TAR:?}" -C "$src"
    cd "$src"
    cmake --preset test
    cmake --build "$build" --parallel "${JOBS:-6}" --target gyo_object_fps_pvp-match gyo_object_fps_pvp_quad_probe \
        gyo_object_fps_pvp_baseline_record gyo_object_fps_pvp_authority_digest gyo_object_fps_pvp_authority_digest_o0
    (cd apps/object_fps_pvp && go build -o "$build/pvp-baseline/gateway" ./gateway/cmd)
    ctest --test-dir "$build" -R '^object_fps_pvp\.authority_digest$' --output-on-failure
    ;;
measure)
    out=${OUT:?}
    cd "$src"
    record=$build/tests/object_fps_pvp/gyo_object_fps_pvp_baseline_record
    match=build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match
    arena=$src/assets/object_fps_pvp/maps/pvp_arena.json
    mkdir -p "$out/replay" "$out/load" "$out/real"
    {
        echo "date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"; uname -a; nproc; free -k | head -2
        echo "cgroup_cpu_max=$(cat /sys/fs/cgroup/cpu.max 2>/dev/null)"
        echo "cgroup_memory_max=$(cat /sys/fs/cgroup/memory.max 2>/dev/null)"
        g++-14 --version | head -1; cmake --version | head -1; ninja --version; python3 --version; go version
        grep -E 'CMAKE_BUILD_TYPE:|CMAKE_CXX_COMPILER:' "$build/CMakeCache.txt"
        sha256sum "$record" "$match" "$build/pvp-baseline/gateway" \
            "$build/acceptance/object_fps_pvp/gyo_object_fps_pvp_quad_probe" "$arena"
    } > "$out/machine.txt" 2>&1
    for i in $(seq 0 $(( ${REPS:-5} - 1 ))); do
        echo "rep $i before: $(cat /proc/loadavg)" >> "$out/replay/load.txt"
        ps -eo pcpu,comm --sort=-pcpu | head -11 >> "$out/replay/load.txt"
        "$record" --replay --out "$out/replay/replay-$i.csv" --samples "$out/replay/samples-$i.csv"
        sleep 5
    done
    for i in 0 1 2 3 4 5 6 7 8 9; do "$record" --load --out "$out/load/load-$i.csv"; sleep 1; done
    python3 tests/object_fps_pvp/pvp_baseline_session.py start --match "$match" --arena "$arena" \
        --output "$out/real" --cold-starts 10
    (while true; do echo "$(date +%H:%M:%S) $(cat /proc/loadavg)"; sleep 60; done) > "$out/real/loadavg-sessions.txt" &
    sampler=$!
    python3 tests/object_fps_pvp/pvp_baseline_session.py session --match "$match" --arena "$arena" --output "$out/real" \
        --gateway "$build/pvp-baseline/gateway" --probe "$build/acceptance/object_fps_pvp/gyo_object_fps_pvp_quad_probe" \
        --runs "${RUNS:-5}" --duration "${DURATION:-600}" | tee "$out/real/sessions.out"
    kill "$sampler"
    ;;
*) echo "unknown phase $phase" >&2; exit 2 ;;
esac
