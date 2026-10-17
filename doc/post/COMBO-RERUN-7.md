# COMBO-RERUN-7 — full 24-patch FINAL stack (2026-10-16)

Scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-combo-7-verify` (TMPDIR `tmp/`).
Validation only: detached worktree + sequential `git am` onto pristine
`3e6cd5e`, no pushes, no parent merges. All builds out-of-tree
(`/Volumes/NVME/ac4-ffmpeg-ws/build/ff-combo-7`, control `ff-combo-7-aab`).
Parent repo untouched (master `ef93aa9` before/after identical).
Branch: `ff/doc-combo-7` (this record only). Caveman brief.

Stack (runbook order, ALL SIX final): A(2, `series-A-decoder.mbox` r4)
-> A0(1, `series-A0-head.mbox`) -> B(2, `series-B-fate.mbox` FINAL)
-> C(11, `series-C-encoder.mbox` r5) -> E(2, `series-E-ts.mbox`)
-> D(6, `series-D-mp4.mbox`) = **24 patches** over pristine `3e6cd5e`.
Assembly HEAD `8e4f1e7` + 1 combo7-resolution commit (`15ab964`, P1 probe).
Precedent: `doc/post/COMBO-RERUN-6.md` (24/24 CONDITIONAL PASS, B PENDING).

## Assembly verdicts per patch (plain first, then -3, then hand)

13/24 plain-clean, 2 auto via `-3`, 9 hand. Every failure logged verbatim
under `logs/am-*.log`; every hand merge verified byte-faithful below.

| # | Patch | Plain | Fix | Resolution (combo-tree only, never posted) |
|---|---|---|---|---|
| 1-2 | A1 A2 | ok | — | — |
| 3 | A0 | ok | — | single-patch mbox, no cover to skip |
| 4-5 | B1 B2 | ok | — | — |
| 6 | C1 | FAIL Makefile:219 | -3 auto | A1 decoder block shifted context; both blocks intact |
| 7 | C2 | FAIL Makefile:221 | -3 auto | same class |
| 8 | C3 | FAIL Makefile+allcodecs | -3 + hand | Makefile+Changelog+configure auto; allcodecs same-spot add -> dec line + enc line (A bytes stable) |
| 9 | C4 | FAIL general_contents:1307 | -3 + hand | same-spot row -> merged `@item AC-4 IX IX`; C4 "separate series" note dropped (stale, decoder landed) |
| 10 | C5 | FAIL Makefile:154 + mak exists | hand | Makefile hunk dropped (B2 identical line kept); mak add/add -> B-head + C-stanzas appended + SINGLE merged aggregate; 35/35 C mak lines present; refs as-is |
| 11 | C6 | FAIL mak:38 | hand | one-line flip applied (`FATE_FFMPEG += $(FATE_AC4_ENC-yes)` split out of the merged aggregate) |
| 12 | W1 | ok | — | (fails only on wrong base; clean sequential after C4) |
| 13 | W2 | ok | — | — |
| 14 | fold8 | ok | — | (fails only on wrong base; clean sequential after C5+C6) |
| 15-16 | C7 C8 | ok | — | — |
| 17 | E1 | ok | — | — |
| 18 | E2 | FAIL mak:50 | hand | stanza block inserted before aggregate, vars appended; 37/37 non-blank non-agg +lines byte-identical |
| 19-21 | D1 D2 D3 | ok | — | configure/parser/mov/movenc disjoint |
| 22 | D4 | FAIL mak:93 | hand | 8/8 identical; api/Makefile + test .c plain |
| 23 | D5 | FAIL mak (+api on wrong base) | hand | mak 10/10 identical; alias `fate-ac4-mp4:` verified present (combo-6 bug class checked) |
| 24 | D6 | FAIL mak (+api on wrong base) | hand | mak 10/10 identical |

Integrity: zero conflict markers in tree files (only pre-existing upstream
hits: one ref-file content line + `tools/murge`); single `tests/Makefile`
ac4.mak include; single `fate-ac4:` aggregate (9 vars); Changelog carries
encoder (top) + decoder; all 24 commits' file sets match their mbox patch
file-for-file (sole by-design delta: C5 drops the already-applied Makefile
hunk). Final mak 238 lines. One in-tree `configure` misfire mid-run was
fully reverted (`distclean` + `checkout -- .`, tree verified clean, HEAD
intact); the gated build is out-of-tree.

## L1 CLOSED: zero duplicate symbols, link clean

Combo-6 blocker L1 (A+C 34 duplicate Huffman globals) is gone: C-r5 carries
the `AC4_`-prefix completion, full build links first try with zero
duplicate-symbol errors. No L1-class resolution commit needed this round.

## Finding F1 (NEW, BLOCKER-class for B post as-is, pre-send): B 714 golden stale under A-r4

`fate-ac4-714` FAILs: ref pins 51200 Hz / 2048 spf, tree produces
48000 Hz / 1920 spf (bytes `6f5b3a..`/`6d73c3..`/`89090903..`, deterministic
across runs). Root cause is NOT C/E/D: an A+A0+B-final-only control stack
(`wt-aab` @ `66be8ff`, own out-of-tree build) produces byte-identical
48000/1920 output — C/E/D proven innocent. Root cause is A-r4: B-final
gated on A-r3 bytes (branch base `df3f2de` predates the A-r4 merge
`2018c89`), while A-r4 embeds the post-N5 core. R3-vs-R4 mbox diff is the
smoking gun: r3 rated via the `e4_sr[]` grid-label table (this clip's fri
row -> 51200); r4 rates via N5 `out_sr = base_fs x sf_mult` (48000x1).
The 48000 output is N5-CORRECT (spec Table E.4; N5 pins 14/14 green below,
N5 DSP verbatim in-tree); the B golden is stale. Proof: swapping the
re-recorded golden in (validation-only, reverted after) makes
`fate-ac4-714` PASS rc=0. B needs a re-cut (re-record 714 golden +
`51200 Hz clip reality` mak comment) before it can post; A is correct,
nothing in A/C/E/D needs to change. B unsent (A/A0-unlanded preconditions),
so caught pre-send.

Re-recorded 714 golden (for the B re-cut seat; `#software` header omitted
per committed-ref convention, fate-stripped):
```
#tb 0: 1/48000 / #sample_rate 0: 48000
0, 0, 0, 1920, 30720, 6f5b3a6ab5f16a4190de2baa79ebd1f9
0, 1920, 1920, 1920, 30720, 6d73c3fef3e44831a4f348f9817664da
0, 3840, 3840, 1920, 30720, 89090903b8a5939523044a75651df977
```
Full bytes: scratch `tmp/ac4-714-newgolden` (cmp-exact vs fresh decode
modulo the fate-stripped `#software` line).

## Finding P1 carried (E post, combo-6 known): probe needs -strict -2

`fate-ac4-mpegts-probe` printed EMPTY output on first run — exact P1
recurrence (Series A EXPERIMENTAL; ffprobe stream entry vanishes without
`-strict -2`). Resolution commit `15ab964` (test-only, combo-tree): `-strict
-2` on the probe CMD (mirrors combo-6 `89dcb7c` + B2 precedent). Probe PASS
after. Strip at E post if A drops EXPERIMENTAL (same conditional as
B-POST-CHECKLIST D1).

## Gates

- Build (out-of-tree `configure --disable-doc --disable-debug`, `make
  -j18`): exit 0. Total warnings 34 (macOS SDK/upstream baseline: CoreImage
  + SecureTransport deprecations, same class as combo-5/6); **ac4 0**;
  duplicate symbols 0.
- N5 pins: `fate-n5/validate.py` (master bytes) vs fresh-built dec-lib shim
  (master `ef93aa9` bytes): **14/14 PASS** (`== PASS 14/14 ==`, rc=0).
  Tie: N5 DSP (sf_mult latch + `out_sr = base_fs x sf_mult`) verified
  verbatim in the in-tree `libavcodec/dec_lib.c` (lavc copy = dec-lib minus
  CLI/shim + LGPL header per vendor note).
- Live suite: fresh `rsync rsync://fate-suite.ffmpeg.org/fate-suite/ac4/`
  rc=0 + `sha256sum -c EXPECTED.sha256` -> **12/12 OK** (own mirror copy
  used for SAMPLES, same pins).

## Per-test verdicts (`make fate-* SAMPLES=<live 12/12 set>`)

| # | Test | Series | Verdict |
|---|---|---|---|
| 1-3 | ac4-demux, ac4-demux-51, seek-extra-ac4 | A0 | PASS |
| 4-16 | 13 B decode/remux legs (stereo, seek, 51, ims120, stereo51, delay, crc-bad, crc-bad-note, gap-trunc, copy-stereo, copy-51, seek-eof, probe-garbage) | B | PASS (B-final F8/P3 fixes hold on live suite) |
| 17 | ac4-714 | B | FAIL = F1 (stale 51200 golden; tree 48000 N5-correct; reswap PASS) |
| 18-22 | ac4-encode, encode-51, framecrc, fold8, tail | C | PASS, md5 = `86828e84ccecb4171d775806616e0e93` / `53a44acaa21853f91cfef7dbdcccb67e` (unchanged — r5 rename byte-neutral) |
| 23-24 | ac4-mpegts-pmt, ac4-mpegts-probe | E | PASS (probe after P1 resolution; PMT REF hex unchanged) |
| 25-27 | ac4-parser, ac4-mp4-roundtrip, ac4-ts-demux | D | PASS (self-contained, null cmp) |
| 28 | fate-ac4 (aggregate, SAMPLES set) | all | 24 ran, 23 pass, 1 fail = exactly F1; zero C/E/D/A/A0 failure |
| 29 | fate-ac4 (SAMPLES=/nonexistent) | all | 10/10 self-contained PASS + exactly the 14 samples-gated legs failing (standard missing-samples shape, by design) |
| 30 | fate-ac4-parse/mp4/ts (aliases) | D | PASS 3/3 |
| 31 | ac4-neg-check.sh | C | PASS neg71 + neg514, `ac4-neg: 2/2 PASS` |

B score on live suite: 13/14 (sole gap F1). C/E/D/A/A0: zero failures.

## Verdict: CONDITIONAL PASS (1 pre-send blocker)

Assembly 24/24 + build green + L1 closed + N5 14/14 + live 12/12 + C/E/D/A/A0
fully green. FULL fate-ac4 green is one B re-cut away: re-record the 714
golden on A-r4 bytes (bytes above), then B posts. No code changes needed
anywhere; no re-validation of A/C/E/D implied. Next: B re-cut seat, then a
combo-8 closer (or a B-only re-gate if parent rules the reswap proof
sufficient).
