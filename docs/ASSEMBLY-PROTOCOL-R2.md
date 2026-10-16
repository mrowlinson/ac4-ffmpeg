# ASSEMBLY-PROTOCOL-R2 — A0→C→E→D post-order stacking, r4 pins (design doc, no code)

Date: 2026-10-16 (R2 refresh of `ff-assembly-protocol/ASSEMBLY-PROTOCOL.md`; original untouched).
Branch copy: `ff/doc-b2f1` (F1 fix — appends `## Rule 5` only;
base text verbatim from the ff-combo-rerun scratch copy).
R3 refresh: 2026-10-16, branch `ff/doc-runbook-r3` (C series r2→r3,
E/D pins unchanged; r2 SHAs survive only in explicitly-marked
historical notes; Rules 0-5 text otherwise intact).
R4 refresh: 2026-10-16, branch `ff/doc-runbook-r4` (full sweep to the
rebased 20-commit stack: A0-submit-r2, C-split-r4, E-ts-r3, D-split-r3
with D6 in-series; r2/r3 SHAs survive only in explicitly-marked
historical notes; protocol form now pre-applied in-branch, zero
assembly rebases).
Copy: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-combo-rerun/ASSEMBLY-PROTOCOL-R2.md`
(TMPDIR there). Parent repos untouched, no branches, no commits, no pushes.
Supersedes the dry-run finding (RUNBOOK-DRYRUN top: 11/28 post-order conflicts)
with a conflict-free assembly procedure, validated by stacking in `wt/`
(r2: ff-combo-rerun; r3: `ff-combo-rerun-4/COMBO-RERUN-4.md`;
r4: `ff-combo-rerun-5/COMBO-RERUN-5.md`, `git am` 20/20).

## Tip verification (all `git rev-parse`, read-only; r4 stack in the
`ff-submit-fix/ffmpeg` scratch clone, linear over pristine 3e6cd5e)

| Series | Shape used | Tip | Verified |
|---|---|---|---|
| A0 | A0-submit-r2 (resquash + ASCII recut) | fc4866b | `master..A0-submit-r2` = fc4866b only, parent 3e6cd5e |
| C | C-split-r4 11/11 (C1-C3 + C4'/C5' + C6 + W1-W3 + C7/C8 DSI) | 1ccd10a | 282ac3b,acc1bb8,0b3f890,3631fcf,b3f3a1f,38a19be,f36f407,f07df7f,e6f99ec,e017d9b,1ccd10a |
| E | E-ts-r3 2/2 on the C' tip (E2 already protocol-form) | a2485a9 | 5e2d5d5+a2485a9 |
| D | D-split-r3 6/6 on the E' tip (D4/D5/D6 append-form) | 08b4f00 | 083c676,c01ad3f,499892d,dd69611,0747972,08b4f00 |

Order A0→C→E→D per SUBMIT-PLAN-V2:130 (`A0→…→C1→C2→C3→C4→E1→E2→D1→D2→D3`)
and :170 (`A→B→C→E→D`). Old branch tips (C-encoder e065111, D-parser 3b3ca20,
D-demux 9a98bea/d2a253d, D-mux 5244b5c, E-tsmux 3fdb20c) are SUPERSEDED for
post purposes — never stack them; their conflict hunks are documented only
as the reason for each rule. HISTORICAL r2 C line (C-split-r2@d37a7b8 9/9:
9115609,b327617,ce7a9fc,2f8f1c1,efb75b7,b4a924f,ef9eaf2,21ff22e,d37a7b8),
HISTORICAL r3 C line (C-split-r3@065976d 11/11:
a6f5544,3c8cafc,607e9fa,ed26bdd,ed73469,be72f52,c747e4f,41e00f5,f03e143,89adb71,065976d),
HISTORICAL E1'/E2' (@98486f6/09a1bd2, create-form) and HISTORICAL
D-split-r2 (@fcc9dbe 5/5: 4ba9790,14c6b50,5cd1740,b0e64ff,fcc9dbe)
likewise superseded — post from the r4 lines.

## Rule 0 — what each series is (post set, 20 patches, zero rebases)

A0: fc4866b. C: 282ac3b, acc1bb8, 0b3f890, 3631fcf, b3f3a1f,
38a19be, f36f407, f07df7f, e6f99ec, e017d9b, 1ccd10a. E: 5e2d5d5,
a2485a9 (protocol-form in-branch). D: 083c676, c01ad3f, 499892d,
dd69611, 0747972, 08b4f00 (all protocol-form in-branch).
Zero patches need rebase: E2/D4/D5/D6 already carry the Rules 2-3 form
(no tests/Makefile hunks, mak stanzas aggregate-anchored), so all 20
post as-is (`git am` 20/20, see Validation). R4 notes: C6 (38a19be)
flips FATE_SAMPLES_AVCONV→FATE_FFMPEG (SAMPLES-unset); W3 (e6f99ec)
adds the 10-line fold8 block; W2 adds tests/ac4-neg-check.sh (not fate);
C7/C8 ride inside C as-is (lavc + fate refs only — no mak, Makefile-block,
Changelog, or include impact); C-final mak 53 lines byte-identical to r3;
D6 appends the ts-demux stanza + `FATE_AC4_TSDEMUX-yes` (E1 in-history,
no out-of-series dep).

## Rule 1 — Changelog: C-only, anchor `version <next>:` + ` - ASTC…`

Claimants found: C `0b3f890:Changelog` (`+ native AC-4 audio encoder`;
HISTORICAL r3 C3 was 607e9fa, r2 C3 was ce7a9fc, same line),
old D-mux `16fc2b0` (`+ AC-4 audio muxing in MP4/MOV (ac-4/dac4)`),
old D-parser `70dc1b0` (`+ AC-4 parser`, self-dropped by d4e310f mid-series).
A0/E1/E2 carry no Changelog hunk (verified by diff-stat).

- C3 (0b3f890) KEEPS its line — sole Changelog claimant in the stack.
- D carries none: D-split D1/D3 already drop both old hunks per the
  d4e310f policy ("contributor patches leave the maintainer-curated
  Changelog alone", msg-D1/msg-D3). Do NOT resurrect them.
- Reconciles P5/D-split take-this-version: the old branch hunks stay dead.

## Rule 2 — tests/Makefile include: C creates, E2/D4 already dropped in-branch

The r4 E2/D4 commits carry NO `tests/Makefile` hunk (verified by
show: a2485a9 touches ac4.mak + script + ref only; dd69611/0747972/
08b4f00 likewise mak/api-only) — the Rule-2 drop is pre-applied, not an
assembly step. For the record, the r2/r3-era conflict was the identical
line `include $(SRC_PATH)/tests/fate/ac4.mak` inserted after
`include …/ac3.mak` (`master:tests/Makefile:156`) and before
`adpcm.mak` (add/add, `git apply` failed for the 2nd and 3rd).

- C5' (b3f3a1f; HISTORICAL r3 C5' was ed73469, r2 was efb75b7) KEEPS the
  hunk (creates the include).

## Rule 3 — tests/fate/ac4.mak: C creates, E2/D4/D5/D6 append before aggregate

One creator (C b3f3a1f 43 lines → C-final 1ccd10a 53 lines,
byte-identical to r3) plus four in-branch appends (E2 a2485a9 42-line TS
stanza; D4 dd69611 12-line parser stanza; D5 0747972 mp4 stanza via the
Rule-3b 2-edit surgery; D6 08b4f00 ts-demux stanza). Mak progression
53→96→107→117→126. E-split precedent: scratch merge 4c65a7e
(C stanza verbatim + E2 stanza appended + merged aggregate).

- C5' creates the file verbatim (encoder stanza, 41 lines + blank +
  `fate-ac4: $(FATE_AC4_ENC-yes)` seed aggregate); C6 flips the FATE
  list line (SAMPLES-unset) and W3 inserts the 10-line fold8 block, so
  the C-final file is 51 lines + blank + seed aggregate (53 total —
  byte-identical to r3-final; C7/C8 touch lavc + fate refs only, zero
  mak impact).
- Each later fate patch inserts its stanza (own comment header + rules +
  own `FATE_* +=` lines, byte-verbatim, single-blank separators) immediately
  BEFORE the `fate-ac4:` aggregate line (unique anchor), and EXTENDS that
  line. Nobody keeps their own second `fate-ac4:` target (make would take
  the last and silently drop earlier tests from the aggregate).
- E2 append: 42-line TS stanza; aggregate gains
  `$(FATE_AC4_MPEGTS_PMT-yes) $(FATE_AC4_MPEGTS_PROBE-yes)`.
- D4 append: 12-line parser stanza (header + PARSE rules +
  `FATE-yes += $(FATE_AC4_PARSE-yes)` + `fate-ac4-parse:` sub-aggregate);
  aggregate gains `$(FATE_AC4_PARSE-yes)`.
- D5, 2 edits: (a) insert 9-line mp4 rules stanza immediately before
  the `FATE-yes +=` line (unique anchor) and extend it with
  `$(FATE_AC4_MP4-yes)`; (b) insert `fate-ac4-mp4: $(FATE_AC4_MP4-yes)` after
  the `fate-ac4-parse:` line and extend the final aggregate with
  `$(FATE_AC4_MP4-yes)`. (D5's `tests/api/Makefile` hunk posts AS-IS —
  its context is D4's line, unaffected by the mak rebase.)
- D6 append: ts-demux rules stanza + `FATE_AC4_TSDEMUX-yes` wiring with
  `fate-ac4-ts:` sub-aggregate; final aggregate gains
  `$(FATE_AC4_TSDEMUX-yes)`.
- Final aggregate (line 126 of validated r4 file):
  `fate-ac4: $(FATE_AC4_ENC-yes) $(FATE_AC4_MPEGTS_PMT-yes) $(FATE_AC4_MPEGTS_PROBE-yes) $(FATE_AC4_PARSE-yes) $(FATE_AC4_MP4-yes) $(FATE_AC4_TSDEMUX-yes)`
- No `--- Series X ---` merge markers in posted patches (4c65a7e's marker
  was scratch-only); each stanza's own comment header is the section header.

## Rule 4 — verified disjoint, no action (file:line evidence at r4 tip 08b4f00)

- `configure`: C probe block @7962–7972 + `CFLAGS-ac4enc_float` @8597
  vs D1 `ac4_demuxer_select` @3891.
- `libavcodec/Makefile`: C encoder OBJS block @222–234 (DSI line rides
  inside the C block; `CFLAGS +=` guard @250) vs D1 parser OBJS @1288.
- Single-owner files: `parsers.c` D1; `allcodecs.c`+`version.h` C;
  `mov.c` D2; `movenc.c` D3; `mpegtsenc.c` E1; `ac4dec.c` A0;
  `demux.mak`+`seek.mak` A0 (M3 fate, never in ac4.mak);
  `doc/encoders.texi`+`doc/general_contents.texi` C;
  `tests/api/Makefile` D4(create-line)/D5(append-line)/D6(append 2-line,
  intra-D only).
- All new files have unique names (34 lavc `ac4_*` + 5 DSI `ac4dsi*`,
  `ac4_parser.c`, 3 apitest tools incl `api-ac4-ts-demux-test.c`,
  `ac4-pmt-section.sh`, 7 fate refs, `tests/ac4-neg-check.sh`).
  No APIchanges, no lavf Makefile/allformats touches anywhere in the stack.
- Disjointness proven operationally: `git am` 20/20 clean onto pristine
  3e6cd5e (combo-5), applied tree == tip 08b4f00.

## Validation (`git am` 20/20 onto pristine 3e6cd5e, read-only sources;
r4 record `ff-combo-rerun-5/COMBO-RERUN-5.md` — HISTORICAL r3 record was
`ff-combo-rerun-4/COMBO-RERUN-4.md`, r2 `results/apply.log` +
`logs/final-ac4.mak` in ff-combo-rerun)

Apply order with result (20 patches `0001-` … `0020-`, `logs/am.log`):

- A0 fc4866b, C1–C5'+C6+W1-W3+C7/C8 (11), E1 5e2d5d5, E2 a2485a9,
  D1–D3, D4 dd69611, D5 0747972, D6 08b4f00: `git am` 20/20 OK, exit 0.
  Applied HEAD tree ba3a1e8 == tip 08b4f00 tree, diff EMPTY. Zero hand
  rebases (protocol form pre-applied in-branch).
- Raw-patch negative controls 3/3 (HISTORICAL r2-era create-forms vs
  rebased intermediate trees): N1 raw E2 (`14-09a1bd2`) vs C'-tip FAILS
  (`tests/Makefile:154`, `ac4.mak already exists`); N2 raw D4
  (`18-b0e64ff`) vs E'-tip FAILS (same two); N3 raw D5 (`19-fcc9dbe`)
  vs D4'-tip FAILS on mak only (`ac4.mak:1`).

Final-state checks, all pass: `grep -c ac4.mak tests/Makefile` = 1;
Changelog diff = C line only; 13/13 fate + `fate-ac4` aggregate green
(both SAMPLES modes, 10/10 TEST lines) + D alias aggregates +
neg-gate 2/2; single `fate-ac4:` aggregate with all 6 vars (line 126);
combo build exit 0, 34 baseline warnings, 0 ac4; encoder bytes
identical to combo-4 binary both layouts (stereo 86828e84…, 5.1
53a44aca…); PMT section = REF hex; .o 24/26 IDENT (ac4enc cstring-only
`--` fix, movenc mov-imm-only); index empty, HEAD still 3e6cd5e in the
validating clone, zero commits. Parents untouched.

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
  lines 1-150 (draft header, staging-only), 371-420 (parser
  staging block), or 425 (the staged file's own `fate-ac4:`).
- Land the B2 wiring with the other wiring lines, just above the
  aggregate: `# --- B2 wiring (decode/demux only; ENC terms
  re-added at C4) ---` + staged line 423 (`FATE_SAMPLES_AVCONV +=
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

r3 cross-check (ff/doc-runbook-r3): Rule 5 text above is unchanged from
the F1 fix — COMBO-RERUN-4 re-audited the premise (B2-F1 caution: the
r3 stack contains no B2 mak at any step, every mak merge a Rule-3 stanza
append + `fate-ac4` prereq extension, no wholesale mak copy anywhere),
so append+merge stands as the B2 landing procedure.

r4 cross-check (ff/doc-runbook-r4): Rule 5 text still unchanged —
COMBO-RERUN-5 re-audited the premise on the 20-stack (no B2 mak at any
step, `git am` 20/20, every mak merge a Rule-3 append + aggregate
extension incl D6), so append+merge stands as the B2 landing procedure.

## Verdict: APPLIES-CLEAN — residual conflicts: NONE

Residual list: (none). Zero assembly rebases (protocol form pre-applied
in-branch: E2/D4/D5/D6 carry no create/include hunks).
r4 protocol deviations: NONE (same Rules, rebased series; all 3 negative
controls reproduced verbatim with HISTORICAL raw patches). Not validated
here (needs built tree, owned by other lanes): per-patch build gates,
patcheck pass. (Fate-test execution on the assembled tree: covered by
COMBO-RERUN-5, 13/13 green.)
