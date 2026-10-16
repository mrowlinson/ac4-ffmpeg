#!/bin/sh
# FUZZ-DEC-1 regression gate: br_pres_v1 cfg==6 left ngi/frf unset
# (uninit ngis[i] -> UBSan OOB at the gis row scan, dec_lib.c:2327).
# Fix: init outputs at br_pres_v1 head + fail loud when the selected
# presentation carries no groups.
#
# Builds dec-lib + vendored fuzzer with ASan+UBSan (out-of-tree) and
# replays every crashing repro from the FUZZ-DEC report. All must exit 0.
# The fuzz corpus is an external asset (ff-fuzz-dec scratch); the pinned
# crash input (fuzzfix-cfg6.bin) always runs. Usage:
#   sh dec-lib/tests/fuzzfix-gate.sh [build-dir]
set -u
T=$(dirname "$0")
SRC=$T/../src
BLD=${1:-${TMPDIR:-/tmp}/fuzzfix-gate-$$}
FZ=/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-fuzz-dec
GRIND_ITERS=${GRIND_ITERS:-6000}
CC=${CC:-cc}
SAN="-fsanitize=address,undefined -fno-sanitize-recover=all"
export ASAN_OPTIONS=detect_leaks=0
export UBSAN_OPTIONS=print_stacktrace=1,halt_on_error=1

rm -rf "$BLD"
mkdir -p "$BLD" || exit 1
echo "gate build: $BLD"
for f in "$SRC"/*.c; do
    b=$(basename "$f" .c)
    # shellcheck disable=SC2086
    $CC -std=c11 -O1 $SAN -DAC4D_LIB_ONLY -I "$SRC" -c "$f" \
        -o "$BLD/lib_$b.o" || exit 1
done
# shellcheck disable=SC2086
$CC -std=c11 -O1 $SAN -I "$SRC" -c "$T/fuzzfix-harness.c" \
    -o "$BLD/harness.o" || exit 1
# shellcheck disable=SC2086
$CC $SAN "$BLD"/lib_*.o "$BLD/harness.o" -o "$BLD/fzharness" -lm \
    || exit 1
# shellcheck disable=SC2086
$CC -std=c11 -O1 $SAN -I "$SRC" -c "$T/fuzzfix-pin.c" \
    -o "$BLD/pin.o" || exit 1
# shellcheck disable=SC2086
$CC $SAN "$BLD"/lib_*.o "$BLD/pin.o" -o "$BLD/pin" -lm || exit 1
echo "build OK"

fails=0
echo "--- pinned input (always-on behavior pin) ---"
if "$BLD/pin" "$T/fuzzfix-cfg6.bin"; then
    echo "pin: PASS"
else
    echo "pin: FAIL"
    fails=$((fails + 1))
fi

if [ -d "$FZ/corpus" ] && [ -d "$FZ/corpus2" ] && [ -d "$FZ/corpus41" ]; then
    C="$FZ/corpus $FZ/corpus2 $FZ/corpus41"
    echo "--- pinpoint repros (each aborted pre-fix) ---"
    # shellcheck disable=SC2086
    for spec in "dnsplice 200 6165" "dnsplice 201 324" "dnsplice 202 790" \
                "dnsplice 203 261" "dnsplice 204 43" "opt 301 11255"; do
        # shellcheck disable=SC2086
        set -- $spec
        rm -rf "$BLD/dump-$1-$2-$3"
        if "$BLD/fzharness" "$1" "$2" 0 $C --repro "$3" \
                --dump "$BLD/dump-$1-$2-$3" >"$BLD/repro-$1-$2-$3.log" 2>&1; then
            echo "$1 seed=$2 repro=$3: PASS"
        else
            echo "$1 seed=$2 repro=$3: FAIL (rc=$?)"
            tail -n 3 "$BLD/repro-$1-$2-$3.log"
            fails=$((fails + 1))
        fi
    done
    echo "--- whole-seed grinds over cfg6 shapes ($GRIND_ITERS iters) ---"
    # shellcheck disable=SC2086
    for SEED in 205 206 207 208 209 210; do
        rm -rf "$BLD/dump-grind-$SEED"
        if "$BLD/fzharness" dnsplice "$SEED" "$GRIND_ITERS" $C \
                --dump "$BLD/dump-grind-$SEED" \
                >"$BLD/grind-$SEED.log" 2>&1; then
            F=$(grep -o 'feeds=[0-9]*' "$BLD/grind-$SEED.log" | tail -1)
            echo "seed=$SEED: PASS ($F)"
        else
            echo "seed=$SEED: FAIL (rc=$?)"
            grep -m1 -o 'dec_lib.c:[0-9]*' "$BLD/grind-$SEED.log"
            fails=$((fails + 1))
        fi
    done
else
    echo "--- corpus absent ($FZ): repro/grind legs SKIPPED ---"
fi

if [ "$fails" -eq 0 ]; then
    echo "== FUZZFIX GATE PASS =="
else
    echo "== FUZZFIX GATE FAIL ($fails) =="
fi
exit "$fails"
