# ASSEMBLY-PROTOCOL-R2 — A→A0→B→C→E→D post-order stacking, r5 pins + Final send packs (design doc, no code)

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
R-final refresh: 2026-10-16, branch `ff/doc-runbook-final` (decoder-first
order A→A0→B→C→E→D: Series-A pre-step + Series-B creator-flip recorded;
validated A0→C→E→D 20-stack core stands as combo-5 left it; B-first
re-validation is a disclosed residual, not done here).
R-recut refresh: 2026-10-17, branch `ff/doc-runbook-refresh-2` (post-recut
pins: A-r4/A0/B-final/C-r5/E/D Final packs all on master;
combo-6 executed the full 24-stack with resolutions — residuals (a)/(b)
closed for staged bytes, re-owed as combo-7 on B-final + C-r5 bytes;
r4/re-cut-3/staged-B SHAs survive only in explicitly-marked historical
notes; Rules 0-5 mechanism text otherwise intact).
Copy: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-combo-rerun/ASSEMBLY-PROTOCOL-R2.md`
(TMPDIR there). Parent repos untouched, no branches, no commits, no pushes.
Supersedes the dry-run finding (RUNBOOK-DRYRUN top: 11/28 post-order conflicts)
with a conflict-free assembly procedure, validated by stacking in `wt/`
(r2: ff-combo-rerun; r3: `ff-combo-rerun-4/COMBO-RERUN-4.md`;
r4: `ff-combo-rerun-5/COMBO-RERUN-5.md`, `git am` 20/20;
recut: `doc/post/COMBO-RERUN-6.md`, 24/24 with resolutions).

## Tip verification (all `git rev-parse`, read-only; C-r5 line in the
`ff-C-post-r2-retry/ffmpeg` scratch clone; E/D lines UNCHANGED in the
`ff-submit-fix/ffmpeg` clone; all six send packs on parent master,
file pins)

| Series | Shape used | Tip | Verified |
|---|---|---|---|
| A | Series-A decoder re-cut 4 (FINAL, post-N5, post-ready) | mbox 620505 B @ 2018c89 | staging 7955a1e/6144274, `git am` 6f026cb/f6a620e ZERO warnings on pristine 3e6cd5e |
| A0 | A0-submit-r2 (resquash + ASCII recut) + Final pack | fc4866b / mbox 35400 B @ c53d99e | UNCHANGED (fresh `-1` byte-identical per A0 preflight) |
| B | Series-B doc + decoder-FATE FINAL (merged) | mbox 19113 B @ ef93aa9 | staging 0a49864/42d53d9, `git am` clean pristine+A0; F8 fixed; suite 12/12 live, clock ends 2026-10-17T22:23:09Z |
| C | C-split-r5 11/11 (C1-C3 + C4'/C5' + C6 + W1-W3 + C7/C8 DSI) | fd578ca | 6c07969,065c5e5,2cd9378,bd7a122,1985788,c8c74e1,1f76f88,1ef16bd,6a4733d,383a1e9,fd578ca (01/11 L1 fix, 02-11 byte-identical to r4) |
| E | E-ts-r3 2/2 still-clean (E2 protocol-form) + Final pack | a2485a9 / mbox 11605 B @ f521c36 | UNCHANGED (fresh format-patch byte-identical per ED-STILL-CLEAN) |
| D | D-split-r3 6/6 still-clean (D4/D5/D6 append-form) + Final pack | 08b4f00 / mbox 128977 B @ 96a0bee | UNCHANGED (fresh format-patch byte-identical per ED-STILL-CLEAN) |

Order A→A0→B→C→E→D (decoder-first flip APPROVED, P3; HISTORICAL order
A0→C→E→D per SUBMIT-PLAN-V2:130 and :170 survives only as the validated
20-stack core shape). Old branch tips (C-encoder e065111, D-parser 3b3ca20,
D-demux 9a98bea/d2a253d, D-mux 5244b5c, E-tsmux 3fdb20c) are SUPERSEDED for
post purposes — never stack them; their conflict hunks are documented only
as the reason for each rule. HISTORICAL r2 C line (C-split-r2@d37a7b8 9/9:
9115609,b327617,ce7a9fc,2f8f1c1,efb75b7,b4a924f,ef9eaf2,21ff22e,d37a7b8),
HISTORICAL r3 C line (C-split-r3@065976d 11/11:
a6f5544,3c8cafc,607e9fa,ed26bdd,ed73469,be72f52,c747e4f,41e00f5,f03e143,89adb71,065976d),
HISTORICAL r4 C line (C-split-r4@1ccd10a 11/11:
282ac3b,acc1bb8,0b3f890,3631fcf,b3f3a1f,38a19be,f36f407,f07df7f,e6f99ec,e017d9b,1ccd10a;
02/11..11/11 byte-identical to r5),
HISTORICAL E1'/E2' (@98486f6/09a1bd2, create-form) and HISTORICAL
D-split-r2 (@fcc9dbe 5/5: 4ba9790,14c6b50,5cd1740,b0e64ff,fcc9dbe)
likewise superseded — post the r5 packs. HISTORICAL Series-A
re-cut 3 (606385 B @ f827e56: PRE-N5, STALE), re-cut 2 (606436 B @
a89c3b1) and re-cut 1 (605740 B) superseded by re-cut 4 (620505 B @
2018c89: post-N5 core). HISTORICAL Series-B stage (02bc75b/af19c5e,
13067 B @ 1645e4e, PENDING stubs) superseded by the B-final re-cut
(0a49864/42d53d9, 19113 B @ ef93aa9, merged).

## Rule 0 — what each series is (post set, 24 patches: A→A0→B→C→E→D)

A: 7955a1e (1/2 decoder core, 25 files, post-N5) + 6144274 (2/2 wire,
2 files). A0: fc4866b. B: 0a49864 (1/2 doc, 3 files) + 42d53d9 (2/2
fate, 11 files, F8 fixed — FINAL re-cut, merged ef93aa9).
C: 6c07969, 065c5e5, 2cd9378, bd7a122, 1985788,
c8c74e1, 1f76f88, 1ef16bd, 6a4733d, 383a1e9, fd578ca. E: 5e2d5d5,
a2485a9 (protocol-form in-branch). D: 083c676, c01ad3f, 499892d,
dd69611, 0747972, 08b4f00 (all protocol-form in-branch).
Validated stack (combo-6, staged-B bytes): A→A0→B→C→E→D assembles
24/24 — 14/24 plain-clean, 10 resolved (2 `-3`-auto, 8 hand,
byte-faithful) + 2 resolution commits; build rc=0, 0 ac4 warnings;
14/14 executable + 10/10 nosamples fate + aliases + neg 2/2; N5
14/14; mak 230 lines, single aggregate 9 vars (see Validation).
HISTORICAL core: A0→C→E→D 20/20 as-is, zero rebases (combo-5).
Re-validation owed as combo-7 on B-final + C-r5 master bytes
(now runnable — Verdict). R5 notes: C6 (c8c74e1)
flips FATE_SAMPLES_AVCONV→FATE_FFMPEG (SAMPLES-unset); W3 (6a4733d)
adds the 10-line fold8 block; W2 adds tests/ac4-neg-check.sh (not fate);
C1 (6c07969) folds the L1 AC4_ completion (34 symbols, decoder untouched);
C7/C8 ride inside C as-is (lavc + fate refs only — no mak, Makefile-block,
Changelog, or include impact); C-final mak 53 lines;
D6 appends the ts-demux stanza + `FATE_AC4_TSDEMUX-yes` (E1 in-history,
no out-of-series dep).

## Rule 1 — Changelog: B appends, C prepends, no conflict (anchor `version <next>:`)

Claimants found: B1 `0a49864:Changelog` (`+ native AC-4 audio decoder`,
APPENDED at the `<next>` chapter end, oldest→youngest; HISTORICAL
staged B1 was 02bc75b, same line); C
`2cd9378:Changelog` (`+ native AC-4 audio encoder`, PREPENDED;
HISTORICAL r4 C3 was 0b3f890, r3 607e9fa, r2 ce7a9fc, same line),
old D-mux `16fc2b0` (`+ AC-4 audio muxing in MP4/MOV (ac-4/dac4)`),
old D-parser `70dc1b0` (`+ AC-4 parser`, self-dropped by d4e310f mid-series).
A/A0/E1/E2 carry no Changelog hunk (A: decoder-only code, no Changelog
in 1/2 or 2/2; A0/E1/E2 verified by diff-stat).

- B1 APPENDS its decoder line; C3 (2cd9378) KEEPS its encoder line —
  different lines, opposite ends of the chapter, no textual conflict
  (B-POST-CHECKLIST D6; HISTORICAL "C-only" was the pre-B 20-stack core;
  combo-6 confirms: encoder top + decoder bottom, both present).
- D carries none: D-split D1/D3 already drop both old hunks per the
  d4e310f policy ("contributor patches leave the maintainer-curated
  Changelog alone", msg-D1/msg-D3). Do NOT resurrect them.
- Reconciles P5/D-split take-this-version: the old branch hunks stay dead.

## Rule 2 — tests/Makefile include: B creates, C5'/E2/D4 carry none

B2 (42d53d9, FINAL re-cut — HISTORICAL staged B2 was af19c5e) CREATES
the include line `include $(SRC_PATH)/tests/fate/ac4.mak` at
tests/Makefile:157 (between ac3/adpcm) — B-first creator
(B-POST-CHECKLIST D5, am-clean on pristine 3e6cd5e). HISTORICAL
20-stack-core form: C5' created it and E2/D4 drops were pre-applied
in-branch (r4 E2/D4 carry NO tests/Makefile hunk — verified by show:
a2485a9 touches ac4.mak + script + ref only; dd69611/0747972/08b4f00
likewise mak/api-only). Decoder-first delta (combo-6 EXECUTED on
staged bytes): C5' (1985788; HISTORICAL r4 C5' was b3f3a1f) still
carries the create-hunk in-branch — it rebases to DROP the hunk at C
post (same mechanical drop E2/D4 already carry; combo-6 dropped the
C5 Makefile hunk, single include confirmed). The r2/r3-era conflict
record (identical line after `…/ac3.mak`, before `adpcm.mak`; add/add,
`git apply` failed for the 2nd and 3rd) is why only the creator keeps
it. Re-verified on final bytes by combo-7 (now runnable).

## Rule 3 — tests/fate/ac4.mak: B creates, C/E2/D4/D5/D6 append before aggregate

One creator (B2 42d53d9 FINAL mak: decoder stanzas + wiring + seed
`fate-ac4:` aggregate with the B2 vars; HISTORICAL staged B2 af19c5e
was a 105-line mak) plus five appends at their time (C5' 1985788
41-line encoder stanza REBASED create→append; E2 a2485a9 42-line TS
stanza; D4 dd69611 12-line parser stanza; D5 0747972 mp4 stanza via
the Rule-3b 2-edit surgery; D6 08b4f00 ts-demux stanza).
HISTORICAL 20-stack-core form (combo-5 validated): C created (43 lines →
C-final 53) + four in-branch appends, mak progression 53→96→107→117→126,
E-split precedent scratch merge 4c65a7e.
Decoder-first delta (combo-6 EXECUTED on staged bytes): the C5'
create→append rebase + E2/D re-anchor onto the B(+C) tail + full
B-first-stack fate green all ran — every stanza byte-faithful
(E2 43/43, D4 11/11, D5 11/11, D6 10/10 non-aggregate +lines),
single aggregate 9 vars at mak 230. Re-verified on B-final + C-r5
bytes by combo-7 (same mechanism, new creator bytes — now runnable).

- B2 creates the file verbatim (decoder stanzas + wiring + seed
  aggregate); C5' appends the encoder stanza (41 lines + blank) before
  the aggregate and extends it with `$(FATE_AC4_ENC-yes)`; C6 flips the
  FATE list line (SAMPLES-unset) and W3 inserts the 10-line fold8 block
  (C7/C8 touch lavc + fate refs only, zero mak impact).
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
- Final aggregate (B-first stack, combo-6 line 230: B vars seed order,
  then ENC, MPEGTS pair, PARSE, MP4, TSDEMUX — single `fate-ac4:` line,
  all 9 vars; HISTORICAL 20-stack-core form was line 126:
  `fate-ac4: $(FATE_AC4_ENC-yes) $(FATE_AC4_MPEGTS_PMT-yes)
  $(FATE_AC4_MPEGTS_PROBE-yes) $(FATE_AC4_PARSE-yes) $(FATE_AC4_MP4-yes)
  $(FATE_AC4_TSDEMUX-yes)`). Combo-7 re-verifies the line on final bytes (now runnable).
- No `--- Series X ---` merge markers in posted patches (4c65a7e's marker
  was scratch-only); each stanza's own comment header is the section header.

## Rule 4 — verified disjoint core + characterized A-side adjacencies

Core file:line evidence at r4 tip 08b4f00 (combo-5 validated, stands;
E/D still-clean: zero drift since):

- `configure`: C probe block @7962–7972 + `CFLAGS-ac4enc_float` @8597
  vs D1 `ac4_demuxer_select` @3891.
- `libavcodec/Makefile`: C encoder OBJS block @222–234 (DSI line rides
  inside the C block; `CFLAGS +=` guard @250) vs D1 parser OBJS @1288.
- Single-owner files: `parsers.c` D1; `version.h` C;
  `mov.c` D2; `movenc.c` D3; `mpegtsenc.c` E1; `ac4dec.c` A0;
  `demux.mak`+`seek.mak` A0 (M3 fate, never in ac4.mak);
  `doc/encoders.texi`+`doc/general_contents.texi` C;
  `tests/api/Makefile` D4(create-line)/D5(append-line)/D6(append 2-line,
  intra-D only).
- All new files have unique names (34 lavc `ac4_*` + 5 DSI `ac4dsi*`,
  `ac4_parser.c`, 3 apitest tools incl `api-ac4-ts-demux-test.c`,
  `ac4-pmt-section.sh`, 7 fate refs, `tests/ac4-neg-check.sh`;
  A-side adds lavc-dec core files + `ac4dec.c`, B-side adds the mak +
  fate refs; combo-6 operationally assembled all 24 with zero
  filename collisions).
  No APIchanges, no lavf Makefile/allformats touches anywhere in the stack.
- Disjointness proven operationally: `git am` 20/20 clean onto pristine
  3e6cd5e (combo-5), applied tree == tip 08b4f00.

Decoder-first adjacencies (combo-6 CHARACTERIZED on staged bytes —
expected-disjoint was wrong; resolutions proven byte-faithful):

- `allcodecs.c`: A 2/2 (decoder registration) + C3 (encoder
  registration) — SAME-SPOT add, resolved by hand (dec line + enc
  line, A bytes stable; combo-6 F3).
  (HISTORICAL "single-owner C" was the pre-A 20-stack core.)
- `libavcodec/Makefile`: A 1/2 (decoder OBJS line) + C1/C2 (encoder OBJS
  block) — context shift, resolved `-3`-auto (both blocks intact;
  combo-6 F2). C3 Makefile hunk likewise `-3`-auto.
- B-side doc adjacency: B1 (decoder table row + @section ac4) + C4'
  (encoder table row at the same spot) — resolved by hand to a merged
  `@item AC-4 IX IX` row; C4 "separate series" note dropped as stale
  (decoder landed; combo-6 F4).
- Link adjacency (combo-6 L1, now CLOSED): A1 `huffman_tab.o` + C1
  `ac4_huffman_tables.o` both defined the same 34 AJOC/DE/DRC globals
  (identical bytes) → 34 duplicate symbols at link. Fixed in C r5
  01/11 (AC4_ completion, decoder untouched); A+C links ZERO dup syms,
  rename byte-neutral per 5/5 fate + goldens.
Re-verified on B-final + C-r5 master bytes by combo-7 (now runnable).

## Validation (combo-6: `git am` 24/24 with resolutions onto pristine
3e6cd5e, read-only sources; record `doc/post/COMBO-RERUN-6.md` —
HISTORICAL r4 record was `ff-combo-rerun-5/COMBO-RERUN-5.md`
(20/20 clean), r3 `ff-combo-rerun-4/COMBO-RERUN-4.md`, r2
`results/apply.log` + `logs/final-ac4.mak` in ff-combo-rerun)

Apply order with result (A+A0+B+C+E+D runbook order, `logs/am-*.log`):

- A1 A2, A0 (cover `0001` skipped, patch `0002` applied), B1 B2:
  plain-clean (5/5).
- C1, C2: plain FAIL (Makefile context shift) → `-3`-auto, both blocks
  intact (F2). C3: plain FAIL (Makefile+allcodecs) → `-3` + hand
  (allcodecs same-spot add → dec line + enc line, A bytes stable; F3).
  C4: plain FAIL (general_contents) → `-3` + hand (merged `@item AC-4
  IX IX` row, stale note dropped; F4). C5: plain FAIL (Makefile:154 +
  mak exists) → `-3` + hand (Makefile hunk dropped, B-head + C-stanzas
  appended + SINGLE merged aggregate, refs as-is; F5). C6: plain FAIL
  (mak:38) → hand (one-line flip; F6). C7-C11: plain-clean.
- E1: plain-clean. E2: plain FAIL (mak:50) → hand (stanza inserted
  before aggregate, vars appended; 43/43 non-agg +lines byte-identical;
  F6).
- D1 D2 D3: plain-clean (configure/parser/mov/movenc disjoint). D4:
  plain FAIL (mak:93) → hand (11/11 identical). D5: plain FAIL
  (mak:100) → hand (11/11 identical, alias verified present). D6:
  plain FAIL (mak:109) → hand (10/10 identical).
- Assembly HEAD `a031022` + 2 combo6-resolution commits (`e34ba44`
  L1-fix ref, `89dcb7c` P1 probe `-strict -2`); every failure logged
  verbatim, every hand merge byte-faithful.

Final-state checks, all pass: `grep -c ac4.mak tests/Makefile` = 1;
zero conflict markers; Changelog = encoder (top) + decoder (bottom);
14/14 executable + 10/10 nosamples fate + `fate-ac4` aggregate green
(both SAMPLES modes) + D alias aggregates + neg-gate 2/2; single
`fate-ac4:` aggregate with all 9 vars (line 230); combo build exit 0,
34 baseline warnings, 0 ac4; N5 pins 14/14; encoder goldens unchanged
(stereo 86828e84…, 5.1 53a44aca… — L1 fix ref byte-neutral); PMT
section = REF hex; 10 B failures = pre-existing stage-state (F8),
not regressions; parents untouched (master f23777b before/after).
HISTORICAL 20-stack checks (combo-5): `git am` 20/20 clean, tree ==
tip 08b4f00, mak 126, 13/13 fate, neg-controls N1/N2/N3 verbatim.

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

Final cross-check (ff/doc-runbook-final): the E-tail append form above
is now HISTORICAL under the approved decoder-first order — B lands
before C/E/D and CREATES the file (Rule 3), so no E-track tail exists
at B time. Append+merge stands as the mechanism (all later stanzas
still land byte-verbatim before the single aggregate); only the creator
(B, not C5') + anchors change. Residual (a) owns the re-validation.

Recut cross-check (ff/doc-runbook-refresh-2): combo-6 EXECUTED the
Final mechanism on staged bytes (C5'/E2/D4/D5/D6 appends +
aggregate extension, no wholesale mak copy anywhere) — residual (a)
closed for staged bytes. B-final changes the creator bytes (F8
fixes), so combo-7 re-executes on B-final + C-r5 master bytes
(now runnable); mechanism unchanged.

## Verdict: ASSEMBLES-WITH-RESOLUTIONS (combo-6) — combo-7 runnable: 1 OPEN

Stack verdict: A→A0→B→C→E→D 24/24 `git am` with 2 `-3`-auto + 8 hand
resolutions (all byte-faithful) + 2 resolution commits, build rc=0
with 0 ac4 warnings, 14/14 + 10/10 fate + aliases + neg-gate 2/2,
N5 14/14, single aggregate 9 vars at mak 230 (combo-6 CONDITIONAL
PASS — the 10 B failures were pre-existing stage-state F8, since
fixed in the B-final re-cut). HISTORICAL core verdict (combo-5):
A0→C→E→D 20/20 clean, zero rebases.
Residual list (post-recut follow-up, NOT validated here, owned by a
combo-7 lane — now runnable on master ef93aa9 bytes):

(c) Final-bytes re-validation: `am` full A→A0→B-final→C-r5→E→D 24/24
    + build + fate green both SAMPLES modes + single-aggregate check;
    re-characterize the C1/C2/C3/C4/C5'/C6/E2/D4/D5/D6 anchors on the
    B-final creator bytes (combo-6 F2-F6 resolutions are the template);
    confirm L1 stays closed in-stack (C r5 01/11) and P1 probe note
    stands while A experimental. CLOSED by combo-6 for staged bytes:
    (a) B-first-stack re-validation, (b) A+C/B+C adjacency re-verify.
Not validated here (needs built tree, owned by other lanes): per-patch
build gates, patcheck pass. (Fate-test execution on the full stack:
covered by COMBO-RERUN-6, 14/14 + 10/10 green modulo staged-B F8.)
