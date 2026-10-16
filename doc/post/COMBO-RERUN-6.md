# COMBO-RERUN-6 — full 24-patch decoder-first stack (2026-10-16)

Scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-combo-6-verify` (TMPDIR `tmp/`).
Validation only: detached worktree + `git am` onto pristine `3e6cd5e`, no
pushes, no parent merges. All builds out-of-tree
(`/Volumes/NVME/ac4-ffmpeg-ws/build/ff-combo-6`). No MBP/remote.
Parent repo untouched (master `f23777b` before/after identical).
Branch: `ff/doc-combo-6` (this record only). Caveman brief.

Stack (runbook order, §0 pins): A(2, `doc/post/series-A-decoder.mbox`)
-> A0(1, `ff-submit-fix/mbox/series-A0.mbox` cover skipped) -> B(2)
-> C(11) -> E(2) -> D(6) = **24 patches** over pristine `3e6cd5e`.
Assembly HEAD `a031022` + 2 combo6-resolution commits (`e34ba44`,
`89dcb7c`). Precedent: `ff-combo-rerun-5/COMBO-RERUN-5.md` (20/20,
A0->C->E->D, no decoder).

## Assembly verdicts per patch (plain first, then -3, then hand)

Plain `git am` is NOT clean past the B head: 14/24 clean, 10 need
resolution (2 auto via `-3`, 8 hand). Every failure logged verbatim
under `logs/am-*.log`; every hand merge verified byte-faithful below.

| # | Patch | Plain | Fix | Resolution (combo-tree only, never posted) |
|---|---|---|---|---|
| 1-2 | A1 A2 | ok | — | — |
| 3 | A0 | ok | — | cover `0001` skipped (empty), patch `0002` applied |
| 4-5 | B1 B2 | ok | — | — |
| 6 | C1 | FAIL Makefile:219 | -3 auto | A1 decoder block shifted context; both blocks intact |
| 7 | C2 | FAIL Makefile:221 | -3 auto | same class |
| 8 | C3 | FAIL Makefile+allcodecs | -3 + hand | Makefile+Changelog auto; allcodecs same-spot add -> dec line + enc line (A bytes stable) |
| 9 | C4 | FAIL general_contents:1307 | -3 + hand | same-spot row -> merged `@item AC-4 IX IX`; C4 "separate series" note dropped (stale, decoder landed) |
| 10 | C5 | FAIL Makefile:154 + mak exists | -3 + hand | Makefile hunk dropped (B2 identical line kept); mak add/add -> B-head + C-stanzas appended + SINGLE merged aggregate; refs as-is |
| 11 | C6 | FAIL mak:38 | hand | one-line flip applied (aggregate post-context differs); verified |
| 12-16 | C7-C11 | ok | — | — |
| 17 | E1 | ok | — | — |
| 18 | E2 | FAIL mak:50 | hand | stanza block inserted before aggregate, vars appended; 43/43 non-agg +lines byte-identical to patch |
| 19-21 | D1 D2 D3 | ok | — | configure/parser/mov/movenc disjoint |
| 22 | D4 | FAIL mak:93 | hand | 11/11 identical (resolver `-- ` trailer bug caught + fixed mid-run; commit amended, authorship kept) |
| 23 | D5 | FAIL mak:100 | hand | 11/11 identical (pure-insert alias bug caught + fixed; alias verified present) |
| 24 | D6 | FAIL mak:109 | hand | 10/10 identical |

Integrity: `git grep` zero conflict markers; single `tests/Makefile`
ac4.mak include; single `fate-ac4:` aggregate (9 vars); Changelog
carries encoder (top) + decoder (bottom); all 24 commits' file sets
match their mbox patch file-for-file (sole by-design delta: C5 drops
the already-applied Makefile hunk). Final mak 230 lines.

## Link finding L1 (BLOCKER-class for C post): A+C 34 duplicate symbols

First full build `ld` failed: 34 duplicate symbols, all Huffman table
globals (`AJOC_*`/`DE_*`/`DRC_*`) defined by BOTH A1 `huffman_tab.o`
and C1 `ac4_huffman_tables.o`. Table bytes are IDENTICAL (228-line
diff = identifier renames + comments only); the C-side M3 rename
prefixed `ASF_`/`ACPL_` but missed these 34. Encoder-side users of
the 34 exist only inside the encoder table files themselves
(definitions + in-file `ac4_huffman_books` registry; strings kept).
Fix (commit `e34ba44`, validation-only): `AC4_`-prefix the 34
identifiers in the 2 encoder files (68 + 34 sites), decoder untouched
(A lands first). C fate goldens below prove byte-neutrality. **C1/C2
need this AC4_ completion re-cut before C can post on landed A.**

## Probe finding P1 (E post): E2 probe assumed no decoder

`fate-ac4-mpegts-probe` printed EMPTY output: with Series A landed
(EXPERIMENTAL), ffprobe's decoder open fails and the stream entry
vanishes. Fix (commit `89dcb7c`, test-only): `-strict -2` on the
probe CMD (mirrors B2 precedent). Strip at E post if A drops
EXPERIMENTAL (same conditional as B-POST-CHECKLIST D1).

## Gates

- Build (configure `--disable-doc --disable-debug`, `make -j18`):
  exit 0. Total warnings 34 (macOS SDK/upstream baseline, same as
  combo-5); **ac4 0**.
- N5 pins: `fate-n5/validate.py` vs clean-archive dec-lib shim
  (built in scratch, master `f23777b` bytes): **14/14 PASS**.

## Per-test verdicts (`make fate-* SAMPLES=<local 12/12 sha-ok set>`)

| # | Test | Series | Verdict |
|---|---|---|---|
| 1-3 | ac4-demux, ac4-demux-51, seek-extra-ac4 | A0 | PASS |
| 4-5 | ac4-stereo, ac4-seek (pcm byte-exact vs suite refs) | B | PASS |
| 6 | ac4-gap-trunc (exit!=0 + no-sync) | B | PASS |
| 7 | ac4-probe-garbage (exit 234 + message) | B | PASS |
| 8-16 | 51, ims120, 714, stereo51, delay, crc-bad, copy-stereo, copy-51, seek-eof | B | FAIL stage-state: `reference file ... not found` — staged refs carry a `fate-` prefix nothing reads (default refs strip it; C/E prove the rule). Even the stubs are inert. Re-cut must rename refs prefix-less or add REF lines |
| 17 | ac4-crc-bad-note | B | FAIL stage-state: grep literal `crc1` never matches actual `skipping bad packet ...: bad packet` stderr. Re-cut must fix the literal |
| 18-22 | ac4-encode, encode-51, framecrc, fold8, tail | C | PASS, md5 = `86828e84ccecb4171d775806616e0e93` / `53a44acaa21853f91cfef7dbdcccb67e` (unchanged — rename byte-neutral) |
| 23-24 | ac4-mpegts-pmt, ac4-mpegts-probe | E | PASS (probe after P1 resolution; PMT REF hex unchanged) |
| 25-27 | ac4-parser, ac4-mp4-roundtrip, ac4-ts-demux | D | PASS (self-contained, null cmp) |
| 28 | fate-ac4 (aggregate, SAMPLES set) | all | 24 ran, 14 pass, 10 fail = exactly the B stage-state set above; zero C/E/D failure |
| 29 | fate-ac4 (SAMPLES unset) | all | 10/10 self-contained PASS + 14 samples-gated refusals (standard FATE_SAMPLES gate, by design) |
| 30 | fate-ac4-parse/mp4/ts (aliases) | D | PASS |
| 31 | ac4-neg-check.sh | C | PASS neg71 + neg514, `ac4-neg: 2/2 PASS` |

Decoder health beyond the 4 green B tests: all 6 suite clips decode
rc=0 with sane framemd5 output (direct `./ffmpeg` runs, logs
`tmp/dec-*.md5`); crc-bad rejects 1 packet + decodes the survivor,
exit 0. Zero combo regressions — all 10 B failures are pre-existing
stage-state (PENDING pack, suite 0/12 at round 8).

## Post-time re-cut list (for parent routing, all PROVEN above)

- F1 C1/C2: AC4_-prefix the 34 AJOC/DE/DRC table symbols (L1, link BLOCKER).
- F2 C1/C2/C3: Makefile hunks need A-context re-cut.
- F3 C3: allcodecs hunk needs A-context re-cut (dec+enc adjacency).
- F4 C4: general_contents hunk needs B-context re-cut (IX/IX; note stale).
- F5 C5: drop Makefile hunk, mak create->append, merged aggregate (B-context).
- F6 C6/E2/D4/D5/D6: aggregate anchors need B-context re-cut.
- F7 E2: probe CMD `-strict -2` while A experimental (P1).
- F8 B re-cut: rename 9 staged refs prefix-less (or add REF lines);
  crc-bad-note grep literal `crc1` -> actual wording.

## Verdict: CONDITIONAL PASS — stack validates with resolutions

24/24 patches + 2 mechanical resolutions assemble, build rc=0 with 0
ac4 warnings, 14/14 executable fate + 10/10 nosamples + aliases +
negatives green, N5 14/14 green, C goldens byte-identical. The 10 B
failures are stage-state (pending suite + staged-pack defects F8),
not regressions. First failure class: none unexpected — every halt
matched a runbook-disclosed residual (R-final) except L1, which is
new and the top post-time finding.
First failure: none (all halts predicted or stage-state).
