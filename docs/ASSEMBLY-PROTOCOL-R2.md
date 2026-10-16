# ASSEMBLY-PROTOCOL-R2 — A0→C→E→D post-order stacking, r2 pins (design doc, no code)

Date: 2026-10-16 (R2 refresh of `ff-assembly-protocol/ASSEMBLY-PROTOCOL.md`; original untouched).
Branch copy: `ff/doc-b2f1` (F1 fix — appends `## Rule 5` only;
base text verbatim from the ff-combo-rerun scratch copy).
Copy: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-combo-rerun/ASSEMBLY-PROTOCOL-R2.md`
(TMPDIR there). Parent repos untouched, no branches, no commits, no pushes.
Supersedes the dry-run finding (RUNBOOK-DRYRUN top: 11/28 post-order conflicts)
with a conflict-free assembly procedure, validated by stacking in `wt/`.

## Tip verification (all `git rev-parse`, read-only)

| Series | Shape used | Tip | Verified |
|---|---|---|---|
| A0 | A0-submit (resquash) | c22b0fe | `master..A0-submit` = c22b0fe only |
| C | C-split-r2 9/9 (C1-C3 + C4'/C5' + C6 + W1-W3) | d37a7b8 | 9115609,b327617,ce7a9fc,2f8f1c1,efb75b7,b4a924f,ef9eaf2,21ff22e,d37a7b8 |
| E1 | E1-tsdesc-r2 | 98486f6 | 1 commit on master |
| E2 | E2-tsfate-r2 (on E1') | 09a1bd2 | 98486f6+09a1bd2 |
| D | D-split-r2 5/5 | fcc9dbe | 4ba9790,14c6b50,5cd1740,b0e64ff,fcc9dbe |

Order A0→C→E→D per SUBMIT-PLAN-V2:130 (`A0→…→C1→C2→C3→C4→E1→E2→D1→D2→D3`)
and :170 (`A→B→C→E→D`). Old branch tips (C-encoder e065111, D-parser 3b3ca20,
D-demux 9a98bea/d2a253d, D-mux 5244b5c, E-tsmux 3fdb20c) are SUPERSEDED for
post purposes — never stack them; their conflict hunks are documented only
as the reason for each rule.

## Rule 0 — what each series is (post set, 13 patches)

A0: c22b0fe. C: 9115609, b327617, ce7a9fc, 2f8f1c1, efb75b7,
b4a924f, ef9eaf2, 21ff22e, d37a7b8. E: 98486f6, 09a1bd2-REBASED.
D: 4ba9790, 14c6b50, 5cd1740, b0e64ff-REBASED, fcc9dbe-REBASED.
Exactly 3 patches need rebase (E2, D4, D5); the other 14
post as-is. Rebase = mechanical stanza move only, zero content change
(stanzas verified byte-exact, see Validation). R2 notes: C6 (b4a924f)
flips FATE_SAMPLES_AVCONV→FATE_FFMPEG (SAMPLES-unset); W3 (d37a7b8)
adds the 10-line fold8 block; W2 adds tests/ac4-neg-check.sh (not fate);
D5' drops one D-header line as part of its mak replacement (C header stays).

## Rule 1 — Changelog: C-only, anchor `version <next>:` + ` - ASTC…`

Claimants found: C `ce7a9fc:Changelog:5` (`+ native AC-4 audio encoder`),
old D-mux `16fc2b0` (`+ AC-4 audio muxing in MP4/MOV (ac-4/dac4)`),
old D-parser `70dc1b0` (`+ AC-4 parser`, self-dropped by d4e310f mid-series).
A0/E1/E2 carry no Changelog hunk (verified by diff-stat).

- C3 (ce7a9fc) KEEPS its line — sole Changelog claimant in the stack.
- D carries none: D-split D1/D3 already drop both old hunks per the
  d4e310f policy ("contributor patches leave the maintainer-curated
  Changelog alone", msg-D1/msg-D3). Do NOT resurrect them.
- Reconciles P5/D-split take-this-version: the old branch hunks stay dead.

## Rule 2 — tests/Makefile include: C creates, E2/D4 drop

All three hunks are the identical line `include $(SRC_PATH)/tests/fate/ac4.mak`
inserted after `include …/ac3.mak` (`master:tests/Makefile:156`) and before
`adpcm.mak` — add/add conflict, `git apply` fails for the 2nd and 3rd.

- C5' (efb75b7) KEEPS the hunk (creates the include).
- E2-REBASED and D4-REBASED DROP their `tests/Makefile` hunks entirely.

## Rule 3 — tests/fate/ac4.mak: C creates, E2/D4/D5 append before aggregate

Three creators (C efb75b7 43 lines → C-final d37a7b8 53 lines;
E2 09a1bd2 44 lines, blob-identical to old 39d4eab; D4 b0e64ff 13 lines,
blob-identical to old b51e2f5) plus D5' (appends onto D4's version;
D5'-final 22 lines = old 23 minus one D-header line, mp4 stanza content
identical, verified by diff). E-split precedent: scratch merge 4c65a7e
(C stanza verbatim + E2 stanza appended + merged aggregate).

- C5' creates the file verbatim (encoder stanza, 41 lines + blank +
  `fate-ac4: $(FATE_AC4_ENC-yes)` seed aggregate); C6 flips the FATE
  list line (SAMPLES-unset) and W3 inserts the 10-line fold8 block, so
  the C-final file is 51 lines + blank + seed aggregate (53 total).
- Each later fate patch inserts its stanza (own comment header + rules +
  own `FATE_* +=` lines, byte-verbatim, single-blank separators) immediately
  BEFORE the `fate-ac4:` aggregate line (unique anchor), and EXTENDS that
  line. Nobody keeps their own second `fate-ac4:` target (make would take
  the last and silently drop earlier tests from the aggregate).
- E2-REBASED append: 42-line TS stanza; aggregate gains
  `$(FATE_AC4_MPEGTS_PMT-yes) $(FATE_AC4_MPEGTS_PROBE-yes)`.
- D4-REBASED append: 12-line parser stanza (header + PARSE rules +
  `FATE-yes += $(FATE_AC4_PARSE-yes)` + `fate-ac4-parse:` sub-aggregate);
  aggregate gains `$(FATE_AC4_PARSE-yes)`.
- D5-REBASED, 2 edits: (a) insert 9-line mp4 rules stanza immediately before
  the `FATE-yes +=` line (unique anchor) and extend it with
  `$(FATE_AC4_MP4-yes)`; (b) insert `fate-ac4-mp4: $(FATE_AC4_MP4-yes)` after
  the `fate-ac4-parse:` line and extend the final aggregate with
  `$(FATE_AC4_MP4-yes)`. (D5's `tests/api/Makefile` hunk posts AS-IS —
  its context is D4's line, unaffected by the mak rebase.)
- Final aggregate (line 119 of validated r2 file):
  `fate-ac4: $(FATE_AC4_ENC-yes) $(FATE_AC4_MPEGTS_PMT-yes) $(FATE_AC4_MPEGTS_PROBE-yes) $(FATE_AC4_PARSE-yes) $(FATE_AC4_MP4-yes)`
- No `--- Series X ---` merge markers in posted patches (4c65a7e's marker
  was scratch-only); each stanza's own comment header is the section header.

## Rule 4 — verified disjoint, no action (file:line evidence)

- `configure`: C hunks @7957/@8593 vs D1 `ac4_demuxer_select` @3888.
- `libavcodec/Makefile`: C encoder OBJS @219 vs D1 parser OBJS @1256.
- Single-owner files: `parsers.c` D1; `allcodecs.c`+`version.h` C;
  `mov.c` D2; `movenc.c` D3; `mpegtsenc.c` E1; `ac4dec.c` A0;
  `demux.mak`+`seek.mak` A0 (M3 fate, never in ac4.mak);
  `doc/encoders.texi`+`doc/general_contents.texi` C;
  `tests/api/Makefile` D4(create-line)/D5(append-line, intra-D only).
- All new files have unique names (34 lavc `ac4_*`, `ac4_parser.c`,
  2 apitest tools, `ac4-pmt-section.sh`, 7 fate refs (adds fold8),
  `tests/ac4-neg-check.sh`). No APIchanges,
  no lavf Makefile/allformats touches anywhere in the stack.

## Validation (no commits: detached `wt/` @3e6cd5e + `git apply`, read-only sources)

Apply order with result (log: `results/apply.log`):

- A0 c22b0fe, C1–C5'+C6+W1-W3 (9), E1 98486f6: APPLIES-CLEAN as-is (11/11).
- E2' as-is: FAILS as predicted (`tests/Makefile:154`, `ac4.mak already
  exists`) — negative control. Protocol form (2 non-mak files as-is +
  Rule-3 append of 42-line stanza from 09a1bd2 blob, byte-exact): CLEAN.
- D1', D2', D3': APPLIES-CLEAN as-is (3/3).
- D4' as-is: FAILS as predicted — negative control. Protocol form
  (api/Makefile + apitest tool as-is + Rule-2 drop + Rule-3 append of
  12-line stanza from b0e64ff blob, byte-exact): CLEAN.
- D5' as-is: FAILS on mak only (`ac4.mak:1`, header hunk shifts the fail
  line; still mak-only) — negative control. Protocol form (api/Makefile
  + tool as-is + Rule-3b surgery with 8-line stanza from fcc9dbe blob
  lines 9–16, byte-exact): CLEAN.

Final-state checks, all pass: `grep -c ac4.mak tests/Makefile` = 1;
Changelog diff = C line only; all 9 fate tests present (5 ENC incl fold8);
single `fate-ac4:` aggregate with all 5 vars; C-51/E2-42/D4-rules-8/
D5-rules-9 blocks each byte-exact in place; `FATE-yes` extended;
sub-aggregates exact; FATE_FFMPEG (C6) present, zero FATE_SAMPLES_AVCONV;
final mak 119 lines (`logs/final-ac4.mak`); index empty, HEAD still
3e6cd5e, zero commits. Parents untouched (ffmpeg HEAD 3fdb20c clean;
outer repo still ff/dec-lib with pre-existing dirty set only).

## Rule 5 — B2 landing: append+merge, never copy (F1 fix, ff/doc-b2f1)

Source: `ff-B2-review/REVIEW-B2.md` F1 (medium). The staged B2 note
says "copy to tests/fate/ac4.mak; add include" — stale on both
counts: the file EXISTS (E-track TS-mux stanza, 44 lines) and the
include is already at tests/Makefile:157. A plain copy CLOBBERS
the E tests; worse, both files define a single-colon `fate-ac4:`
target, so even a concatenated file with two aggregates silently
drops one set (make keeps the last). No var-name collision
(`FATE_AC4_MPEGTS_*` vs `FATE_AC4/MD5/REMUX` — clean).

B2 lands exactly like a Rule-3 append (byte-verbatim stanza move,
zero content change), with the E-track tail as the anchor:

- Insert fate-b2/ac4.mak lines 151-369 (16 test stanzas + F6
  gates + F6 comment, byte-verbatim, single-blank separators)
  immediately BEFORE the `fate-ac4:` aggregate line. Do NOT land
  lines 1-150 (draft header, staging-only), 371-417 (parser
  staging block), or 422 (the staged file's own `fate-ac4:`).
- Land the B2 wiring with the other wiring lines, just above the
  aggregate: `# --- B2 wiring (decode/demux only; ENC terms
  re-added at C4) ---` + staged line 420 (`FATE_SAMPLES_AVCONV +=
  $(FATE_AC4-yes) $(FATE_AC4_MD5-yes) $(FATE_AC4_REMUX-yes)`).
- The ONE aggregate gains the B2 vars (E vars first):
  `fate-ac4: $(FATE_AC4_MPEGTS_PMT-yes)
  $(FATE_AC4_MPEGTS_PROBE-yes) $(FATE_AC4-yes)
  $(FATE_AC4_MD5-yes) $(FATE_AC4_REMUX-yes)`.

Validation (ff/doc-b2f1 dry-run): merged mak 267 lines; E region
+ B2 block each byte-exact; single `fate-ac4:`; parses under the
real tests/Makefile with zero `overriding recipe` warnings and
all 18 tests (2 E + 16 B2) expanding in the aggregate.
Negative control: the plain-copy form drops both E tests —
the F1 premise, confirmed.

Create-case note: if a from-scratch post-order stack reaches B2
with NO `tests/fate/ac4.mak` in tree, the B2 block lands as the
seed instead — but that re-opens the B/C creator boundary (C5'
also creates); do not improvise, re-stack per the assembly owner.

## Verdict: APPLIES-CLEAN — residual conflicts: NONE

Residual list: (none). The 3 rebases are mechanical (drop 2 duplicate
include hunks, convert 3 create-hunks to aggregate-anchored appends).
Not validated here (needs built tree, owned by other lanes): fate-test
execution on the assembled tree, per-patch build gates, patcheck pass.
