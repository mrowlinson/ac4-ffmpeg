# COMBO-RERUN-8 — MUST-fixed 23-patch Final assembly (2026-10-17)

Scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-combo-8-final`
(TMPDIR `tmp/`). Validation only: own pristine-`3e6cd5e` clone +
sequential `git am` in STACK-DAG order, no pushes, no parent merges.
All builds out-of-tree (`/Volumes/NVME/ac4-ffmpeg-ws/build/ff-combo-8-final`).
Parent repo untouched (master `2def950` before/after identical).
Branch: `ff/doc-combo-8` (this record only). Caveman brief.

Stack (DAG order, ALL SIX MUST-fixed): A0(1, `series-A0-head.mbox`
43263) -> A(2, `series-A-decoder.mbox` r6 507373)
-> B(2, `series-B-fate.mbox` 40708) -> C(10, `series-C-encoder.mbox`
650974, C08 dropped) -> E(2, `series-E-ts.mbox` 12693)
-> D(6, `series-D-mp4.mbox` 162643) = **23 patches** over pristine
`3e6cd5e`. Sizes match SEND-SITTING expected bytes exactly.
Assembly HEAD `2f067f6` + 2 combo-tree resolution commits
(`aadbdc6` B-encneg-strict, RT-GEN-strict).
Precedent: `doc/post/COMBO-RERUN-7.md` (24/24 CONDITIONAL, F1 B-714).

## Assembly verdicts per patch (plain first, then -3, then hand)

17/23 plain-clean, 0 via -3 (fake-ancestor unbuildable, A-r6
comment drift), 6 hand. Every failure logged verbatim under
`logs/am-*.log`; every hand merge byte-faithful (plus-line diffs).

| # | Patch | Plain | Fix | Resolution (combo-tree only, never posted) |
|---|---|---|---|---|
| 1 | A0 | ok | — | single-patch mbox, DAG head |
| 2-3 | A1 A2 (r6) | ok | — | — |
| 4-5 | B1 B2 | ok | — | — |
| 6-8 | C1 C2 C3 | FAIL Makefile | hand | A-r6 rewrote the decoder comment block C refit against; anchor (pre-AC3_MF) identical, trailing context drifted. C1 2 lines, C2 9 lines, C3 17 lines inserted verbatim (C3 diff-verified byte-identical); C3 allcodecs/configure/Changelog parts applied plain |
| 9-15 | C4-C10 | ok | — | clean once C1-C3 landed (docs row, mak appendix, fold8, DSI x2) |
| 16 | E1 | ok | — | — |
| 17 | E2 | FAIL mak:218 | hand + fwd-fix | union-C HO fix carried: context tail keeps tree `-strict -2`; GEN rule gains `-strict -2` (EXPERIMENTAL encoder, rc88 class). 43/43 block lines = mbox + 1 strict token |
| 18 | D1 | FAIL toc exists | hand (seam) | predicted B-A0/D seam: A0 walker header already in tree. D1's 678-line create dropped; headers differ ONLY in 3 comment lines (D1 "converges later" stale vs A0 "shared"); parser `#include "ac4_toc.h"` kept, code identical |
| 19-23 | D2-D6 | ok | — | mov, movenc, parser/mp4/ts tests incl T3 roundtrip |

Integrity: zero conflict markers; single `fate-ac4:` aggregate
(11 vars); `show --check` 0 errors all 23; subjects 1:1;
signoffs 23/23; mbox non-ASCII 0. Resolution commits (test-only):
B encneg x2 `-strict -2` (union-C HO proven), D RT GEN `-strict -2`
(E2-GEN same class, found this seat: GEN rc88 without it).

## Gates

- Build (out-of-tree `configure --disable-doc --disable-debug`,
  `make -j18`): exit 0. Total warnings 34 (macOS SDK/upstream
  baseline, same class as combo-7); **ac4 0**; duplicate symbols 0
  (link + `nm` sweep). `DEA.L. ac4` + encoder/decoder regs;
  `ff_ac4_parser` in parser_list.
- N5 pins: `fate-n5/validate.py` (master bytes) vs fresh-built
  dec-lib shim (master `2def950` bytes): **14/14 PASS** (rc=0).
  Tie: `out_sr = base_fs x sf_mult` + s5.11 converter verified
  in-tree (`libavcodec/dec_lib.c`).
- Live suite: fresh `rsync rsync://fate-suite.ffmpeg.org/fate-suite/ac4/`
  rc=0 + `shasum -c EXPECTED.sha256` -> **12/12 OK** (own mirror
  `samples/ac4/` used for SAMPLES).
- TB send link `169.254.103.231` absent this seat (no sends made;
  assembly only).

## Per-test verdicts (`make fate-* SAMPLES=<live 12/12 set>`)

| # | Test | Series | Verdict |
|---|---|---|---|
| 1-11 | stereo, seek, 51, ims120, **714**, stereo51, delay, frac, skip-bad, skip-bad-note, gap-trunc | B | PASS (combo-7 F1 CLOSED: 714 golden now N5-current on A-r6) |
| 12-15 | copy-stereo, copy-51, frac-copy, seek-eof | B | FAIL = G1 (predicted D-repin drift; bytes == d-repin exact, see below) |
| 16 | probe-garbage | B | PASS |
| 17-18 | enc-neg71, enc-neg514 | B/C | PASS (with carried `-strict -2`; without it exit-234 pins fail — union-C proof) |
| 19-23 | encode, encode-51, framecrc, fold8, tail | C | PASS, md5 `86828e84...`/`53a44aca...` unchanged (union-C byte-neutral) |
| 24-25 | mpegts-pmt, mpegts-probe | E | PASS (probe `-strict -2` in-mbox; GEN strict carried) |
| 26-28 | parser, mp4-roundtrip, ts-demux | D | PASS (self-contained) |
| 29 | roundtrip (T3) | D | PASS after RT-GEN strict resolution (was GEN Error 88); golden deterministic |
| 30-32 | demux, demux-51, seek-extra-ac4 | A0 | FAIL = G1 (F-suffix x2, 695->348 x9 — all predicted-exact) |
| 33 | fate-ac4 (aggregate) | all | 29 ran, 25 pass, 4 fail = exactly G1; zero other failure |
| 34 | no-samples shape | all | 13/13 self-contained PASS + 16 samples-gated fail (standard) |
| 35 | rerun | all | deterministic: same 29/25/4 |
| 36 | ac4-neg-check.sh | C | absent (C08 dropped); layout negatives ride encneg FATE |

## G1 (sole red class): 7 goldens need the D re-pin re-cut

All 7 failures are the B-A0 HO predicted post-D parser drift, and
every observed byte is `cmp`-identical to the predicted re-pin
bytes in `scratch/ff-union-B-A0/d-repin/` (7/7 PREDICTED-EXACT):
copy pair last-line `, F=0x0`; frac-copy 116/116 rows F-only (0
other pairs); seek-eof 1-line -> 2-line `-copyts` true stamps
+2048/+4096; demux pair F-suffix; seek-extra 9x 695->348.
Reswap proof (validation-only, reverted after, tree verified
clean): with the 7 d-repin bytes swapped in, `make -k fate-ac4
fate-ac4-demux fate-ac4-demux-51 fate-seek-extra-ac4` = **32/32
PASS rc=0**. Union-D shipped "zero golden churn" (cut pre-walker);
the walker-form A0 on master makes the parser refine keyflags, so
D must carry these 7 re-pins (normal stack practice, no A0 change).

## Verdict: HOLD (1 content re-cut + 4 mechanical re-cuts, all bytes proven)

Assembly 23/23 + build green + N5 14/14 + live 12/12 + reswap
32/32. Nothing red except G1. Before posting, cut:

1. **D re-pin re-cut** (content): carry the 7 d-repin files into
   the D series goldens (paths: `tests/ref/fate/ac4-{copy-stereo,
   copy-51,frac-copy,seek-eof,demux,demux-51}` + `tests/ref/seek/
   extra-ac4`). Proven flips 32/32.
2. **E2 strict recut** (mechanical): context tail + GEN `-strict
   -2` (union-C HO item 1; this seat's E2 resolution bytes).
3. **B encneg strict** (mechanical): both CMDs `-strict -2`
   (union-C HO item 2; this seat's `aadbdc6` bytes).
4. **D RT GEN strict** (mechanical): `-strict -2` (this seat; E2-GEN
   same class; without it T3 GEN dies rc88).
5. **C01-C03 Makefile context** (mechanical): refit trailing
   context to A-r6 comment bytes (anchor unchanged; this seat's
   C1-C3 resolution bytes).

No code changes needed anywhere; no re-validation of A/A0/B/C/E
content implied. Next: one re-cut seat per series (or a single
stack re-cut), then combo-9 closer (or parent rules the reswap
proof sufficient for the D bytes + mechanicals).
