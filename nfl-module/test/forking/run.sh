#!/usr/bin/env bash
# Fuzz the accept-and-fork-per-connection server through netfuzzlib. Confirms
# the child-reap + crash-propagation path: the bug fires in the forked handler
# child, and AFLNet still finds it. Usage: run.sh [seconds]
set -uo pipefail
SECS="${1:-180}"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HERE="$ROOT/nfl-module/test/forking"
BUILD="$ROOT/nfl-module/build"
PORT=5599
SUT="$HERE/build/forking_server"

mkdir -p "$HERE/build"
AFL_PATH="$ROOT" AFL_USE_ASAN=1 "$ROOT/afl-clang-fast" \
    "$HERE/forking_server.c" -o "$SUT" || { echo "build failed"; exit 1; }

OUT="$HERE/out-tcp"
rm -rf "$OUT"


echo "[*] Fuzzing forking server (ASan) for up to ${SECS}s ..."
AFL_PRELOAD="$BUILD/libnfl-aflnet.so" AFL_NO_AFFINITY=1 AFL_SKIP_CPUFREQ=1 \
timeout "$SECS" "$ROOT/afl-fuzz" -i "$HERE/in-tcp" -o "$OUT" \
    -N "tcp://127.0.0.1/$PORT" -P FTP -E -m none -t 1000 -d \
    -- "$SUT" "$PORT" >/dev/null 2>&1

CR="$OUT/replayable-crashes"
[ -d "$CR" ] || CR="$OUT/crashes"
N=$(ls "$CR" 2>/dev/null | grep -c 'id:' || true)
echo "[*] crashes found in $CR : $N"
echo "[*] lingering handler processes: $(pgrep -c forking_server || echo 0) (want 0)"
if [ "$N" -gt 0 ]; then
    echo "[+] BUG FOUND in the forked handler child, reaped+propagated correctly."
    xxd "$(ls "$CR"/id:* | head -1)" | head -6
    echo "CRASH_OK"
else
    echo "CRASH_NOT_FOUND"
fi
