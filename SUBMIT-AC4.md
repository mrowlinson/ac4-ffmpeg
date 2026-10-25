# SUBMIT-AC4: Forgejo PR readiness (submit1, 23-patch)

Lane: `ff/submit1` (base `ff/dec-lib` b1a1582).
Status: SUBMIT-READY. No push, no PR created by the lane --
everything below is prepared for the owner to send.

This supersedes the 2026-10-18 email send (29 wire messages:
23 patches + 6 covers across series A0/A/B/C/E/D, Gmail-accepted,
no list traction). The series is re-cut onto fresh upstream tip;
7 of 23 patches carry mechanical 7-day-drift re-cuts, zero
behavior edits.

## 1. Series contents

`upstream/submit1/`: `submit-0001.patch`..`submit-0023.patch`
(`git format-patch`, subject prefix `PATCH`, unified `01/23`
numbering) + `cover-letter-submit.txt` + `SHA256SUMS-submit.txt`.

Old-series map (sent subjects preserved inside each patch body):

- 01/23: A0 raw-demux params (codecpar + timestamps from first frame).
- 02/23: A decoder core (bit reader, entropy, IMDCT, QMF, A-SPX, ACPL, tables as `*_tab.*`).
- 03/23: A decoder wiring (`ff_ac4_decoder`, EXPERIMENTAL, pres_sel only).
- 04/23: B decoder docs (texi + Changelog). RE-CUT (Changelog/9.1, x2 -- see cover letter).
- 05/23: B decoder FATE (suite-clip tests + negatives).
- 06/23: C encoder static tables.
- 07/23: C encoder core + lib adapter.
- 08/23: C encoder wrapper + configure/sincos + Changelog. RE-CUT (Changelog/9.1).
- 09/23: C encoder docs.
- 10/23: C encoder FATE (asynth md5/framecrc).
- 11/23: C FATE without SAMPLES (1-line re-home).
- 12/23: C silent auto-fold doc note.
- 13/23: C fold8 pin.
- 14/23: C DSI extradata via putbits serializer.
- 15/23: C DSI layout from lavc-mask.
- 16/23: E mpegtsenc descriptors + Changelog. RE-CUT (Changelog/9.1).
- 17/23: E TS mux FATE (pmt-section + probe).
- 18/23: D `ff_ac4_parser` + Changelog. RE-CUT (Changelog/9.1).
- 19/23: D mov dac4 box read support.
- 20/23: D movenc ac-4/dac4 muxing + Changelog. RE-CUT (Changelog/9.1).
- 21/23: D self-contained parser FATE. RE-CUT (api/Makefile anchor).
- 22/23: D self-contained mp4 roundtrip FATE. RE-CUT (api/Makefile anchor).
- 23/23: D self-contained ts demux FATE.

Re-cut 7/23, clean 16/23. Full per-patch notes + drift-port
record: the cover letter. Hashes of every file: `SHA256SUMS-submit.txt`.

## 2. Base pin

`b593998340c76a8269515448b565c93d060ae8b4` (tests/fate: add
tests for jpeg2000 and apv frame timestamps), fetched 2026-10-25
from https://git.ffmpeg.org/ffmpeg.git `master`. Series rebased
(not carried): From-hashes are the new commits (stack applies
with plain `git am` in 01-23 order only); subjects/authors kept,
single `Signed-off-by: Michael Rowlinson <mrowlinson@gmail.com>`
per patch (committer identity identical, no dup trailer).

## 3. Gate evidence (summary)

Two pristine legs at the pinned tip, same tree
(`f97dba10c07f5c57e0d43bba597ab95220e8c623`):

- `git am --signoff submit-0001..0023`: legA 23/23 with the 7
  re-cuts resolved inline (all other hunks byte-clean first
  contact, incl. decoder/encoder/parser code and every test
  ref); legB 23/23 clean off the sealed set; tip trees
  byte-identical.
- `./configure --disable-doc --disable-debug` rc=0 x2.
- `make -j16` exit-0 x2, zero warnings in AC-4 files (first full
  builds: only upstream/macOS-SDK lines in tls_securetransport,
  adpcm, nspdec; final incremental build: zero warnings total).
- FATE `fate-ac4` aggregate = 29: 13/13 self-contained PASS,
  rc=0, both legs, same tree (enc-neg71, enc-neg514, encode,
  encode-51, framecrc, fold8, tail, mpegts-pmt, mpegts-probe,
  parser, mp4-roundtrip, ts-demux, roundtrip). The 16 suite-clip
  legs need the `ac4/` fate-suite clips (not bundled; samples
  mail + suite upload remain owner-gated as in the Oct-18
  send): they fail only with the standard "requires external
  samples" shape, zero other failures. Decoder coverage without
  samples rides fate-ac4-roundtrip (native encode -> raw -> native
  decode framemd5) plus the three api binary tests.

Caps: one full leg-1 (am + cfg + build + FATE) + one Changelog
attribution fix with a 05-23 replay + one fresh leg-2 (am + cfg +
build + FATE). Zero flakes, zero retries consumed.

## 4. PR split plan (recommended)

Upstream accepts Forgejo PRs or the mailing list: one Forgejo PR
with all 23 commits (keeps the tested order and FATE green at
every prefix), with the cover letter as the PR description. If
reviewers ask for splits, fall back to 3 stacked PRs, each
green at its tip for the self-contained subset:

- PR-1 (carriage + decoder, 01-05): raw demux params, decoder
  core + wiring, docs, decoder FATE (suite-clip legs pending
  the samples upload).
- PR-2 (encoder, 06-15): tables, core, wrapper, docs, 5 FATE +
  fold8 + negatives wiring.
- PR-3 (mux + parser, 16-23): TS descriptors + FATE, parser,
  mov/movenc, 4 self-contained FATE.

PR-3 can further split (TS 16-17 vs MP4/parser 18-23) if asked.
The 16 suite-clip goldens need `ac4/` in fate-suite; if upstream
wants them moved, attach per the PR template's `fate-samples`
block. No sample bytes are bundled in this PR.

## 5. Exact send-steps for the owner

Prereqs: an account on https://code.ffmpeg.org, and a personal
fork (or branch-push rights) -- the lane did NOT create any
remote or PR.

```sh
# 0. Fresh shell; scratch on NVME only.
export TMPDIR=~/tmp/ac4/submit1; mkdir -p $TMPDIR

# 1. Pristine base + apply the sealed set (reproduces the gate).
rm -rf $TMPDIR/send && mkdir -p $TMPDIR/send
git clone https://git.ffmpeg.org/ffmpeg.git $TMPDIR/send
git -C $TMPDIR/send checkout -q b593998340c76a8269515448b565c93d060ae8b4
git -C $TMPDIR/send am --signoff \
  /Users/mrowlinson/Projects/ac4-ffmpeg/upstream/submit1/submit-*.patch
# expect: 23/23, tip tree f97dba10c07f5c57e0d43bba597ab95220e8c623
git -C $TMPDIR/send log --format='%T' -1
cd /Users/mrowlinson/Projects/ac4-ffmpeg/upstream/submit1 && sha256sum -c SHA256SUMS-submit.txt

# 2. Smoke: configure + the 13 self-contained FATEs (fast, no network).
cd $TMPDIR/send && mkdir -p build && cd build &&
  ../configure --disable-doc --disable-debug && make -j$(sysctl -n hw.ncpu)
make fate-ac4-enc-neg71 fate-ac4-enc-neg514 fate-ac4-encode \
  fate-ac4-encode-51 fate-ac4-framecrc fate-ac4-fold8 fate-ac4-tail \
  fate-ac4-mpegts-pmt fate-ac4-mpegts-probe fate-ac4-parser \
  fate-ac4-mp4-roundtrip fate-ac4-ts-demux fate-ac4-roundtrip

# 3. Push to YOUR fork as one branch (replace <you>).
git remote add submit1 https://code.ffmpeg.org/<you>/FFmpeg.git
git checkout -b ac4-submit1
git push -u submit1 ac4-submit1

# 4. Open ONE pull request FFmpeg/FFmpeg <= <you>/FFmpeg:ac4-submit1
#    at https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls :
#    title = cover letter subject line; body =
#    upstream/submit1/cover-letter-submit.txt
#    (Subject line stripped). Base branch: master.
```

Mailing-list alternative (if Forgejo is unavailable):
`git send-email --to=ffmpeg-devel@ffmpeg.org
upstream/submit1/submit-*.patch` (23 files, in order; note in
the 00/23 thread that this supersedes the Oct-18 email series).

Do NOT push to git.ffmpeg.org (no rights, and the lane must never
push). Do NOT merge this lane into master or ff/dec-lib -- submit
via Forgejo only. Do NOT mail anything from this lane.
