#!/bin/sh
# FINDING-1 (== FUZZ-DEC-1) regression gate: br_pres_v1 cfg==6 left
# ngi/frf unset (uninit ngis[i] -> SBO READ at the gis row scan,
# dec_lib.c toc_parse_bv2). Fix: init outputs at br_pres_v1 head +
# bound ngis[i] vs E4_MAXGRP + fail loud when the selected
# presentation carries no groups.
#
# Builds dec-lib + pin with ASan+UBSan (out-of-tree) and replays the
# pinned crash input (ngis-bounds.bin: wrap-harness corner seed
# 2654435697 iter 75, TOC-flip mutant; single feed aborted pre-fix).
# Exit 0 = gate PASS. Usage: sh dec-lib/tests/ngis-gate.sh [build-dir]
set -u
T=$(dirname "$0")
SRC=$T/../src
BLD=${1:-${TMPDIR:-/tmp}/ngis-gate-$$}
HARN=/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-fuzz-wrap-harness
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
$CC -std=c11 -O1 $SAN -I "$SRC" -c "$T/ngis-pin.c" \
    -o "$BLD/pin.o" || exit 1
# shellcheck disable=SC2086
$CC $SAN "$BLD"/lib_*.o "$BLD/pin.o" -o "$BLD/pin" -lm || exit 1
echo "build OK"

fails=0
echo "--- pinned input (always-on behavior pin) ---"
if "$BLD/pin" "$T/ngis-bounds.bin"; then
    echo "pin: PASS"
else
    echo "pin: FAIL"
    fails=$((fails + 1))
fi

if [ -f "$HARN/wrapharness.c" ] && [ -d "$HARN/corpus" ]; then
    echo "--- wrap-harness repro leg (corner 2654435697) ---"
    # shellcheck disable=SC2086
    $CC -std=c11 -O1 $SAN -DAC4D_LIB_ONLY -I "$SRC" -I "$HARN" \
        -c "$HARN/wrapapi.c" -o "$BLD/wrapapi.o" || exit 1
    # shellcheck disable=SC2086
    $CC -std=c11 -O1 $SAN -DAC4D_LIB_ONLY -I "$SRC" -I "$HARN" \
        -c "$HARN/wrapharness.c" -o "$BLD/wrapharness.o" || exit 1
    # shellcheck disable=SC2086
    $CC $SAN "$BLD"/lib_*.o "$BLD/wrapapi.o" "$BLD/wrapharness.o" \
        -o "$BLD/wrapharness" -lm -lpthread || exit 1
    UP=/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-fuzz-dec/corpus
    C="$HARN/corpus"
    [ -d "$UP" ] && C="$C $UP"
    # shellcheck disable=SC2086
    if "$BLD/wrapharness" corner 2654435697 100 $C --repro 75 \
            >"$BLD/repro75.log" 2>&1; then
        echo "repro75: PASS"
    else
        echo "repro75: FAIL (rc=$?)"
        grep -m1 -o 'dec_lib.c:[0-9]*' "$BLD/repro75.log"
        fails=$((fails + 1))
    fi
    # shellcheck disable=SC2086
    if "$BLD/wrapharness" corner 2654435697 100 $C \
            >"$BLD/corner100.log" 2>&1; then
        echo "corner100: PASS"
    else
        echo "corner100: FAIL (rc=$?)"
        grep -m1 -o 'dec_lib.c:[0-9]*' "$BLD/corner100.log"
        fails=$((fails + 1))
    fi
else
    echo "--- harness scratch absent ($HARN): repro legs SKIPPED ---"
fi

if [ "$fails" -eq 0 ]; then
    echo "== NGIS GATE PASS =="
else
    echo "== NGIS GATE FAIL ($fails) =="
fi
exit "$fails"
