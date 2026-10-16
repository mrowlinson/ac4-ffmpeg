#!/bin/sh
# F2 regression gate: a truncated tail must fail LOUD (rc!=0 + no-sync
# diagnostic), never rc=0 silent (B2EXEC-REAL F2: wrap emitted 8KB zeros
# rc=0 where the shim reports rc=3 no-sync, no output).
#
# Vector: lavc-dec/tests/gap-trunc.ac4 (500 B, sha256 e54faf83...) =
# head -c 500 of pack clip-stereo.ac4 (fate-b2 pins gap-trunc row;
# fate-ac4-gap-trunc slot). Demux shape: pkt0 clean + 148 B short-read
# tail pkt (CORRUPT-flagged), demux exit 0. The decoder emits the valid
# 1-frame prefix, then fails at EOF (INVALIDDATA).
#
# Usage: sh lavc-dec/tests/f2-gap-trunc.sh <ffmpeg-bin> [workdir]
# Exit 0 = gate PASS. The binary must be an in-tree build carrying
# lavc-dec (README recipe: wiring.patch + ac4dec.c + core/*).
set -u
T=$(dirname "$0")
VEC=$T/gap-trunc.ac4
FF=${1:-}
BLD=${2:-${TMPDIR:-/tmp}/f2-gap-$$}
AC4_PACK=${AC4_PACK:-/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-samples-pack}

if [ -z "$FF" ] || [ ! -x "$FF" ]; then
    echo "F2 GATE: missing ffmpeg binary (usage: $0 <ffmpeg-bin> [workdir])"
    exit 1
fi
if ! "$FF" -hide_banner -decoders 2>/dev/null | grep -q " ac4 "; then
    echo "F2 GATE: $FF has no ac4 decoder (not an lavc-dec build?)"
    exit 1
fi
WANT=e54faf833dcdfaa717748cdf007510a714dbdf7fd9d3b5fedda0f5121691f9b1
GOT=$(sha256sum "$VEC" | cut -d' ' -f1)
if [ "$GOT" != "$WANT" ]; then
    echo "F2 GATE: vector drift ($GOT, want $WANT)"
    exit 1
fi

rm -rf "$BLD"
mkdir -p "$BLD" || exit 1
fails=0

echo "--- F2: truncated tail fails loud ---"
"$FF" -y -v error -strict -2 -i "$VEC" -f s16le -acodec pcm_s16le \
    "$BLD/out.pcm" 2>"$BLD/stderr.txt"
rc=$?
sz=0
[ -f "$BLD/out.pcm" ] && sz=$(wc -c <"$BLD/out.pcm" | tr -d ' ')
echo "rc=$rc out_bytes=$sz"
if [ "$rc" -eq 0 ]; then
    echo "F2: FAIL (rc=0 silent-pass)"
    fails=$((fails + 1))
elif ! grep -q "no-sync" "$BLD/stderr.txt"; then
    echo "F2: FAIL (rc=$rc but no no-sync diagnostic)"
    cat "$BLD/stderr.txt"
    fails=$((fails + 1))
elif [ -s "$BLD/out.pcm" ]; then
    echo "F2: FAIL (rc=$rc but $sz PCM bytes written, want strict no-PCM)"
    fails=$((fails + 1))
else
    echo "F2: PASS (rc=$rc, no-sync loud, no PCM)"
fi

if [ -f "$AC4_PACK/clip-stereo.ac4" ]; then
    echo "--- control: full stereo still clean ---"
    "$FF" -y -v error -strict -2 -i "$AC4_PACK/clip-stereo.ac4" \
        -f s16le -acodec pcm_s16le "$BLD/stereo.pcm" 2>"$BLD/stereo-err.txt"
    rc=$?
    if [ "$rc" -ne 0 ]; then
        echo "CONTROL: FAIL (full stereo rc=$rc, must stay 0)"
        cat "$BLD/stereo-err.txt"
        fails=$((fails + 1))
    else
        echo "CONTROL: PASS (full stereo rc=0)"
    fi
else
    echo "--- pack absent ($AC4_PACK): control SKIPPED ---"
fi

if [ "$fails" -eq 0 ]; then
    echo "== F2 GATE PASS =="
else
    echo "== F2 GATE FAIL ($fails) =="
fi
exit "$fails"
