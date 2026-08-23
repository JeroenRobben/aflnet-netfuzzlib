#!/usr/bin/env bash
# Fuzz the stateful FTP-like server through netfuzzlib and find the
# sequence-dependent crash.  Usage: run.sh tcp|udp [seconds]
set -uo pipefail
PROTO="${1:-tcp}"
SECS="${2:-180}"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HERE="$ROOT/nfl-module/test/stateful"
BUILD="$ROOT/nfl-module/build"
PORT=5599
SUT="$HERE/build/stateful_server"

mkdir -p "$HERE/build"
# Instrument the SUT with AddressSanitizer; it flags the STOR overflow.
AFL_PATH="$ROOT" AFL_USE_ASAN=1 "$ROOT/afl-clang-fast" \
    "$HERE/server.c" -o "$SUT" || { echo "build failed"; exit 1; }

IN="$HERE/in-$PROTO"
OUT="$HERE/out-$PROTO"
rm -rf "$OUT"


echo "[*] Fuzzing $PROTO (ASan) for up to ${SECS}s ..."
AFL_PRELOAD="$BUILD/libnfl-aflnet.so" AFL_NO_AFFINITY=1 AFL_SKIP_CPUFREQ=1 \
timeout "$SECS" "$ROOT/afl-fuzz" -i "$IN" -o "$OUT" \
    -N "$PROTO://127.0.0.1/$PORT" -P FTP -E -m none -t 1000 -d \
    -- "$SUT" "$PROTO" "$PORT" >/dev/null 2>&1

CR="$OUT/replayable-crashes"
[ -d "$CR" ] || CR="$OUT/crashes"
N=$(ls "$CR" 2>/dev/null | grep -c 'id:' || true)
echo "[*] crashes found in $CR : $N"
if [ "$N" -gt 0 ]; then
    FIRST=$(ls "$CR"/id:* 2>/dev/null | head -1)
    echo "[+] BUG FOUND ($PROTO). First crashing sequence:"
    xxd "$FIRST" | head -8
    echo "CRASH_OK"
else
    echo "CRASH_NOT_FOUND"
fi
