# COMBO-RERUN-9 — MUST-final 23-patch full-stack CLOSE (2026-10-18)

Scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-combo-9-base`
(TMPDIR `tmp/`). Validation only: own pristine-`3e6cd5e` clone +
sequential plain `git am` in STACK-DAG order, no pushes, no parent
merges. All builds out-of-tree (`/Volumes/NVME/ac4-ffmpeg-ws/build/
ff-combo-9-base`). Parent repo untouched. Branch: `ff/doc-combo-9`
(this record only). Caveman brief. Venue: MBP-TB `169.254.103.231`
DOWN first-hand (ssh timeout/rc255, both users; TB addrs local-only)
→ LOCAL-ONLY pure-FFmpeg; zero Dolby-binary invocation;
Dolby-dependent checks UNRUN (none in scope). `10.0.2.163` never
touched.

Stack (DAG order, MUST-final fixed bytes): A0(1, `series-A0-head.mbox`
43263) -> A(2, `series-A-decoder.mbox` 507373) -> B(2,
`series-B-fate.mbox` 40729) -> C(10, `series-C-encoder.mbox` 650794)
-> E(2, `series-E-ts.mbox` 12774) -> D(6, `series-D-mp4.mbox` 163076)
= **23 patches** over pristine `3e6cd5e`. Assembly HEAD `f8b62af`,
tree `1601eddc…` == `ASSEMBLY-DEBTS-R2.md` proof tree byte-exact.
Base master `fc5573a`; merged tip `d045728` is DOCS-ONLY
(`diff fc5573a..d045728 -- doc/post/series-*.mbox` EMPTY) → every
gate below is valid on the merged tip. Precedent:
`doc/post/COMBO-RERUN-8.md` (HOLD: G1 + 4 mechanicals).

## Assembly: 23/23 plain, zero hand

6/6 mbox `git am --whitespace=error` rc=0 (A0 1/1, A 2/2, B 2/2,
C 10/10, E 2/2, D 6/6), 23 commits, zero hand, zero resolution
commits. All 5 combo-8 cut items are IN the shipped bytes (debts
1–6, `20bb917`): C01–C03 r6 context, E2 strict, B encneg strict,
D RT-GEN strict, D toc-drop + 7 re-pins.

Integrity: `show --check` 0 errors all 23; zero conflict markers;
single `fate-ac4:` aggregate; single `tests/Makefile` ac4.mak
include; subjects 1:1 mbox↔am (C 07/10 + 10/10 fold in BOTH files,
wrap-insensitive match); signoffs 23/23; non-ASCII lines 0;
mailsplit 23/23; bare `-c:a ac4` 0 across all 6 mboxes (debt 4=7).

## Gates

- Build (out-of-tree `configure --disable-doc --disable-debug`,
  `make -j18`): rc=0/rc=0. Warnings 29 lines, all upstream/macOS-SDK
  classes (NSOpenGL/SDK deprecations, adpcm unused, 1 fallthrough);
  **ac4 0**; duplicate symbols 0. `DEA.L. ac4` + `codec_list`
  dec/enc + `ff_ac4_parser` in parser_list.
- N5 pins: `fate-n5/validate.py` (master bytes) vs fresh-built
  dec-lib shim (own `n5/` copy, `diff -r` identical, `make` rc=0):
  **14/14 PASS** rc=0. Tie: `out_sr = base_fs x sf_mult` + s5.11
  converter verified in-tree (`libavcodec/dec_lib.c`).
- Live suite: fresh `rsync rsync://fate-suite.ffmpeg.org/fate-suite/
  ac4/` rc=0 (16 files; +4 vs combo-8: `audio_id_2ch_2997fps.ac4`,
  `audio_id_514ch_25fps.mp4`, `audio_id_ims_25fps.ts`,
  `sync-frames.ts`) + `shasum -c EXPECTED.sha256` -> **12/12 OK**
  @ 2026-10-18T00:58Z. This is the post-expiry live re-confirm D
  re-verify owed (clock ended 2026-10-17T22:23:09Z). `samples/ac4/`
  mirror = 13 files (12 + audio_id_2ch).

## Per-test verdicts (`make fate-* SAMPLES=<13-file mirror>`)

| # | Test | Series | Verdict |
|---|---|---|---|
| 1-11 | stereo, seek, 51, ims120, **714**, stereo51, delay, frac, skip-bad, skip-bad-note, gap-trunc | B | PASS (714 golden N5-current on A-r6) |
| 12-15 | copy-stereo, copy-51, frac-copy, seek-eof | B | PASS — G1 CLOSED (debt-6 re-pins in D 1/6) |
| 16 | probe-garbage | B | PASS |
| 17-18 | enc-neg71, enc-neg514 | B/C | PASS (in-mbox `-strict -2`, debt 5; exit-234 pins) |
| 19-23 | encode, encode-51, framecrc, fold8, tail | C | PASS, md5 `86828e84ccecb417…`/`53a44acaa21853f9…` in-mak pinned (union-C byte-neutral) |
| 24-25 | mpegts-pmt, mpegts-probe | E | PASS (probe `-strict -2` in-mbox; GEN strict in-mbox, debt 2) |
| 26-29 | parser, mp4-roundtrip, ts-demux, roundtrip (T3) | D | PASS (RT-GEN `-strict -2` in-mbox, debt 4=7; T3 GEN md5 `86828e84…`, byte-identical x2 runs) |
| 30-32 | demux, demux-51, seek-extra-ac4 | A0 | PASS — G1 CLOSED (F-suffix + 695->348 re-pins in D 1/6) |
| 33 | fate-ac4 (aggregate) | all | **29/29 rc=0, zero fail/error** |
| 34 | no-samples shape | all | 13/13 self-contained TEST + 16 samples-gated Error-1 (standard) |
| 35 | rerun | all | deterministic: identical 29/29 TEST set |
| 36 | ac4-neg-check.sh | C | absent (C08 dropped); layout negatives ride encneg FATE |

## Red-gate re-run on the merged tip (former reds, explicit)

`make -k` 12 targets rc=0, 12/12 TEST: enc-neg71, enc-neg514 (C-tip
red class), encode, encode-51 (md5 pins), copy-stereo, copy-51,
frac-copy, seek-eof, demux, demux-51, seek-extra-ac4 (G1 7),
roundtrip (T3 GEN rc88 class). T3 GEN file md5
`86828e84ccecb4171d775806616e0e93` == D-R2-VERIFY pin; GEN
byte-identical across delete+rebuild (`cmp`).

## Phase-2 fold (all 3 reverifies landed, docs-only tip)

| Input | Commit | Verdict folded |
|---|---|---|
| ff/doc-C-reverify (`C-R3-VERIFY.md`) | `ef3efbe` / merge `0aa46e6` | POST-OK: 15/15 A0ABC am, debt-1 6-line-context proof, build 0-ac4, Cenc 5/5 md5s, C-tip 23/23 (encneg flipped), statics 10/10. Carry N1 (send-steps 650974 size) FIXED @ tip (`579bad2`: now 650794). |
| ff/doc-E-reverify (`E-R2-VERIFY.md`) | `c7e2839` / merge `1517e71` | POST-OK: 17/17 am, E 2/2 + rerun, M22 TRUE (code+empirical), Changelog L5/L6/L24, E-tip 25/25, statics + send-steps refreshed IN-commit (12774 B). No fresh debts. |
| ff/doc-D-reverify (`D-R2-VERIFY.md`) | `d01d86b` / merge `8c9ba74` | POST-OK: 23/23 full am, toc-drop (2 refs, tree `cmp`-EQ), T3 strict (0 bare), 7/7 re-pins `cmp`-MATCH vs d-repin oracle, build 0-ac4, 29/29 + trio 3/3, T3 md5 pin. Fresh N1 (cosmetic): `COVER-D.txt:42` drops ` test` from 5/6 subject — 1 word at send, zero gate impact. |
| ff/doc-cover-size-sweep-r2 | `579bad2` / merge `6a194c3` | Size/count claims re-swept vs MUST-final bytes (closes C N1). |
| ff/doc-B2-post-ready | `131fa93` / merge `d045728` | B2 post-gate readiness ALL READY @ `fc5573a`. |

Cross-checks vs this seat: debt-1 6-line proof consistent with our
plain C 10/10; debt-2 GEN strict consistent with our 0-bare +
E 2/2; D 7/7 oracle match consistent with our G1-CLOSED 29/29 +
3/3; T3 md5 identical first-hand both seats; Cenc md5s identical
in-mak pins. No conflicts, no deltas outstanding.

## Verdict: POST-OK — combo-8 HOLD fully closed, stack green

Assembly 23/23 plain + build rc0/0-ac4 + fate-ac4 29/29 + A0D 3/3
+ N5 14/14 + live 12/12 + red-gate 12/12 + T3 determinism. All 5
combo-8 cut items proven IN the shipped bytes; all 3 reverifies
POST-OK with zero fresh HOLDs. Remaining at send (both cosmetic,
zero gate impact): D N1 cover word (`...roundtrip` → `...roundtrip
test`, `COVER-D.txt:42`); suite live re-confirm DONE this round
(2026-10-18T00:58Z 12/12). NOT verified here: actual `git
send-email` wire bytes (send seats' scope); Dolby-binary oracles
(venue: MBP down, none in scope).

## Evidence map (`scratch/ff-combo-9-base/`)

`mbox-series-*.mbox` (6 exports), `ff/` (assembly tip `f8b62af`),
`logs/am-{A0-head,A-decoder,B-fate,C-encoder,E-ts,D-mp4}.log`
(6/6 rc0), `logs/show-check.log` (0), `logs/configure.log`,
`logs/make.log`, `logs/suite-shasum.log` (12/12),
`logs/fate-ac4.log` (29/29), `logs/fate-A0D.log` (3/3),
`logs/fate-nosamp.log` (13+16), `logs/fate-rerun.log` (29/29),
`logs/fate-redgates.log` (12/12), `logs/fate-T3-run2.log` +
`tmp/rt1.ac4` (T3 determinism), `logs/n5-make.log`,
`logs/n5-validate.log` (14/14), `n5/` (dec-lib copy + shim +
pins-out), `samples/ac4/` (13), `suite-live/` (16 + EXPECTED),
`wt/` (own clone, branch `ff/doc-combo-9`), `HEARTBEAT.md`.
Build: `/Volumes/NVME/ac4-ffmpeg-ws/build/ff-combo-9-base` (kept).
