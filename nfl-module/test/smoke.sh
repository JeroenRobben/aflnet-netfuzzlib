#!/usr/bin/env bash
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$ROOT/nfl-module/build"
SUT="$BUILD/echo_server"
PORT=5555

# Build + instrument the SUT with afl-clang-fast (needs AFL_CC / PATH set up, see README).
if [ ! -x "$SUT" ]; then
    AFL_PATH="$ROOT" "$ROOT/afl-clang-fast" \
        "$ROOT/nfl-module/test/echo_server.c" -o "$SUT" || {
        echo "SMOKE FAIL: could not instrument SUT"; exit 1; }
fi

mkdir -p /tmp/nfl_in && printf 'HELLO\n' > /tmp/nfl_in/hello
rm -rf /tmp/nfl_out

AFL_PRELOAD="$BUILD/libnfl-aflnet.so" AFL_NO_AFFINITY=1 AFL_SKIP_CPUFREQ=1 \
timeout 25 "$ROOT/afl-fuzz" -i /tmp/nfl_in -o /tmp/nfl_out \
    -N tcp://127.0.0.1/$PORT -P HTTP -h 1 -m none -t 5000 -d \
    -- "$SUT" $PORT
STATUS=$?
echo "afl-fuzz exited with $STATUS (124 = timeout, expected for a bounded run)"

# Assert AFLNet actually executed the target through the forkserver:
# fuzzer_stats must exist and report a nonzero execs_done. (A populated
# queue alone is not enough: the initial seed is copied in before any run.)
if [ ! -s /tmp/nfl_out/fuzzer_stats ]; then
    echo "SMOKE FAIL: no fuzzer_stats (afl-fuzz aborted before fuzzing)"; exit 1
fi
grep -E 'execs_done|paths_total' /tmp/nfl_out/fuzzer_stats || true
EXECS=$(awk -F': *' '/^execs_done/{print $2}' /tmp/nfl_out/fuzzer_stats)
if [ -d /tmp/nfl_out/queue ] && ls /tmp/nfl_out/queue | grep -q 'id:' \
   && [ "${EXECS:-0}" -gt 0 ] ; then
    echo "SMOKE OK: queue populated, execs_done=$EXECS"
else
    echo "SMOKE FAIL: queue empty or execs_done=${EXECS:-0}"; exit 1
fi
