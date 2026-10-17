# ASSEMBLY-PROTOCOL-R2 — A0→A→B→C→E→D post-order stacking, MUST-final union packs (design doc, no code)

Date: 2026-10-16 (R2 refresh of `ff-assembly-protocol/ASSEMBLY-PROTOCOL.md`; original untouched).
R-MUST refresh: 2026-10-17, branch `ff/doc-runbook-r3` (post-MUST-final:
STACK-DAG order A0→A→B→C→E→D, 23 patches, six union send packs,
MUST-verify verdicts, combo-8 HOLD record; pre-MUST branch tips,
24-stack counts and combo-5/6/7 validations are HISTORICAL below —
live proof is `doc/post/COMBO-RERUN-8.md`; Rules 0-5 mechanism text
otherwise intact, debts appended per rule).
Copy: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-r3/wt-final`
(TMPDIR in scratch). Parent repos untouched, no pushes.
Supersedes the dry-run finding (RUNBOOK-DRYRUN top: 11/28 post-order conflicts)
with a conflict-free assembly procedure, validated by stacking
(HISTORICAL: r2 ff-combo-rerun; r3 `ff-combo-rerun-4/COMBO-RERUN-4.md`;
r4 `ff-combo-rerun-5/COMBO-RERUN-5.md`; recut `doc/post/COMBO-RERUN-6.md`;
pre-MUST `doc/post/COMBO-RERUN-7.md`; live: `doc/post/COMBO-RERUN-8.md`).

## Tip verification (six union send packs on master 224ec0b, file pins;
post cuts are the mboxes, not branches)

| Series | Shape used | Pin | Verified |
|---|---|---|---|
| A0 | union walker head (M21, POST-OK) | mbox 43263 B, 1 patch (`36b9eec`) | creates `ac4_toc.h` 678 l (mbox:57-74), demux walks it (mbox:411); MUST-VERIFY.md:50-63 |
| A | Series-A decoder re-cut 6 (M4-M9, POST-OK) | mbox 507373 B, 2 patches (`554b92b`) | staging `b2df32f`/`1c1b2b0`, `git am` ZERO warnings, trees exact (A-PREFLIGHT-CHECKLIST.md:47-72); MUST-VERIFY.md:22-48 |
| B | Series-B doc + decoder-FATE union (POST-OK) | mbox 40708 B, 2 patches (`36b9eec`) | creates mak (mbox:114-139) + include; encneg/T2/T5/M8 in 2/2; MUST-VERIFY.md:50-63 |
| C | Series-C encoder union 10/10 (M10-M18, HOLD) | mbox 650974 B, 10 mails (`91c214b`) | C08 dropped, appendix shape (C-PREFLIGHT-CHECKLIST.md:1-21); content green, 6-line ctx re-cut owed (MUST-VERIFY.md:65-81) |
| E | Series-E TS stack re-cut (HOLD) | mbox 12693 B, 2 patches (`3b65f39`) | M22 true + M19 line (E-VERIFY.md:59-81); 2-line M11 re-cut owed (E-VERIFY.md:105-114) |
| D | Series-D union 6/6 (M19-M21/T3, HOLD) | mbox 162643 B, 6 patches (`50e7de5`) | static green (MUST-VERIFY.md:89-94); toc-drop + T3 strict + 7 re-pins owed (MUST-VERIFY.md:95-119) |

Order A0→A→B→C→E→D (STACK-DAG.md:10-21; the single ordered stack
resolving MUST M1/M2 — MUST-TRIAGE.md:19-47). Old branch tips
(A0-submit-r2 fc4866b, C-split-r5 fd578ca, E-ts-r3 a2485a9,
D-split-r3 08b4f00, all earlier r-lines, staged-B, re-cut 1-5) are
SUPERSEDED for post purposes — never stack them; their conflict
hunks are documented only as the reason for each rule.

## Rule 0 — what each series is (post set, 23 patches: A0→A→B→C→E→D)

A0: 1 patch (walker head + demux fill). A: 2 patches (1/2 decoder
core 24 files post-M4/M5/M9, 2/2 `ac4dec.c` + `allcodecs.c`
post-M6/M7/M8). B: 2 patches (1/2 docs pres-only, 2/2 fate:
decoder legs + skip-bad pair + frac pair + seek-eof + encneg
pair). C: 10 mails (01 tables AC4_ / 02 core+lib / 03 wire+reg /
04 docs single-row / 05 fate / 06 nosamples / 07 doc-note /
08 fold8 / 09 DSI-wire / 10 DSI-mask). E: 2 patches (1/2 TS
descriptors + Changelog line, 2/2 PMT + probe fate). D: 6 patches
(1/6 parser + toc-use / 2/6 dac4 demux / 3/6 dac4 mux + M20 /
4/6 parser fate / 5/6 mp4 roundtrip + T3 / 6/6 ts-demux fate).
Validated stack (combo-8, union bytes): A0→A→B→C→E→D assembles
23/23 — 17/23 plain-clean, 6 hand, all byte-faithful + 2
test-only resolution commits; build rc=0, 0 ac4 warnings, 0
dupsyms; N5 14/14; live suite 12/12; 25/29 fate + G1 7 =
predicted-exact, reswap 32/32 (see Validation). HISTORICAL core:
A→A0→B→C→E→D 24/24 with resolutions (combo-6). R-MUST notes: C
carries no neg script (C08 dropped; layout negatives ride B 2/2
as FATE behind the encoder ifeq — STACK-DAG.md:74-78); C-final
mak region + E-tip + D-tip shapes are appendix-form in-mbox
(STACK-DAG.md:48-49); D T3 rides 5/6 as `FATE_AC4_RT`
(UNION-D.md:36-41); the A-r6 decoder comment reword is the
C01-C03 context debt (MUST-VERIFY.md:75-78).

## Rule 1 — Changelog: B appends, C prepends, E + D chain, no conflict

Claimants (STACK-DAG.md:46 + UNION-D.md:12-16 + E-VERIFY.md:59-61):
B 1/2 appends the decoder line at the `<next>` list end; C 03/10
prepends the encoder line at the list top; E 1/2 inserts the
MPEG-TS line directly after C's (mbox:33); D 1/6 adds the parser
line after E's (mbox:58), D 3/6 adds the mov line after the
parser line (mbox:1349-1350). A/A0 carry no Changelog hunk
(STACK-DAG.md:53-54). Proven stacking at E-tip: L5 encoder (C),
L6 TS (E), L24 decoder (B) (E-VERIFY.md:59-61). Rule stands
unchanged into the re-cut round (no Changelog debt in
COMBO-RERUN-8.md:99-111).

## Rule 2 — tests/Makefile include: B creates, all others carry none

B 2/2 CREATES the include line
`include $(SRC_PATH)/tests/fate/ac4.mak` (STACK-DAG.md:48);
C05's duplicate hunk is dropped in the union
(STACK-DAG.md:48); E2/D4/D5/D6 carry no tests/Makefile hunk
(appendix shape, STACK-DAG.md:49). The r2-era add/add conflict
record (identical line after `…/ac3.mak`, before `adpcm.mak`)
is why only the creator keeps it. Rule stands unchanged into
the re-cut round (no Makefile-include debt in
COMBO-RERUN-8.md:99-111).

## Rule 3 — tests/fate/ac4.mak: B creates, C/E/D append before aggregate

One creator (B 2/2: decoder stanzas + wiring incl `FATE_AC4_ENCNEG`
+ seed `fate-ac4:` aggregate — series-B-fate.mbox:139-317) plus
appends at their time: C05 encoder stanza (appendix shape) + C06
tail flip + C08 fold8 block (C07/C09/C10 touch lavc + docs +
fate refs only, zero mak impact); E2 TS stanza; D4 parser
stanza; D5 mp4 stanza + T3 `FATE_AC4_RT` section (which must
precede the aggregate — UNION-D.md:54-55); D6 ts-demux stanza.
Aggregate grows monotonically B… → …ENC → …PMT PROBE → …PARSE
MP4 TSDEMUX RT (STACK-DAG.md:49); final single `fate-ac4:` with
11 vars (COMBO-RERUN-8.md:38-39; HISTORICAL combo-6 form was 9
vars at line 230).

- Each fate patch inserts its stanza (own comment header + rules +
  own `FATE_* +=` lines, byte-verbatim, single-blank separators)
  immediately BEFORE the `fate-ac4:` aggregate line (unique
  anchor), and EXTENDS that line. Nobody keeps their own second
  `fate-ac4:` target (make would take the last and silently drop
  earlier tests from the aggregate).
- D5's `tests/api/Makefile` hunk posts AS-IS (intra-D chain,
  unaffected by the mak resolutions).
- No `--- Series X ---` merge markers in posted patches; each
  stanza's own comment header is the section header.
- R-MUST mak debts (same-stanza token adds, mechanism unchanged):
  E2 tail-CMD context + GEN `-strict -2` (COMBO-RERUN-8.md:102-104);
  B encneg CMDs `-strict -2` x2 (COMBO-RERUN-8.md:105-106); D RT
  GEN `-strict -2` (COMBO-RERUN-8.md:107-108).

## Rule 4 — verified disjoint core + characterized adjacencies

Disjoint regions (STACK-DAG.md:40-54, combo-8 re-proven):
`libavcodec/Makefile` parser OBJS (~1285, D only) vs codec block
(~220, A-then-C); `configure` demuxer selects (~3888) vs float
probe (~7957); `tests/api/Makefile` (D only); single-owner files
(`parsers.c` D1, `version.h` C, `mov.c` D2, `movenc.c` D3,
`mpegtsenc.c` E1, lavf `ac4dec.c` + `demux.mak`/`seek.mak` A0,
lavc decoder core A). No APIchanges, no lavf
Makefile/allformats touches anywhere in the stack.

Characterized adjacencies (combo-8 on union bytes,
COMBO-RERUN-8.md:26-36):

- `libavcodec/Makefile` (~220): A keeps pristine position (after
  AC3_MF); C01/C02/C03 hunks quote r5's 2-line decoder comment
  but A-r6 reworded it → plain FAIL, hand-resolved verbatim
  (anchor pre-AC3_MF identical; COMBO-RERUN-8.md:31). THIS IS the
  C HOLD debt: 6-line context refresh, no content change
  (MUST-VERIFY.md:75-78).
- `libavcodec/allcodecs.c`: A adds the decoder extern; C03 adds
  the encoder extern after it — decoder-first order, applied
  plain in combo-8 (STACK-DAG.md:42-43; COMBO-RERUN-8.md:31).
- `doc/general_contents.texi`: B lands the decode-only row; C04
  single-row fold chained below it (M18; STACK-DAG.md:47).
- `libavcodec/ac4_toc.h` (new): A0 creates the shared TOC walk,
  D 1/6 uses it (STACK-DAG.md:51) — but union-D still carries its
  own 678-line create block (UNION-D.md:20-28), so D 1/6 FAILS
  `already exists` on the A0 tree; fixed D drops the create hunk
  (headers differ ONLY in comment lines) and keeps the parser
  include + calls (MUST-VERIFY.md:99-102; COMBO-RERUN-8.md:35).
  THIS IS the D HOLD seam debt (content half is the 7 re-pins).
- `tests/fate/ac4.mak` tail context: union-C tail CMD carries
  `-strict -2` (M11), so E2-as-merged fails at `mak:218`;
  resolved by carrying the strict tail + strict GEN
  (COMBO-RERUN-8.md:34). THIS IS the E HOLD debt
  (E-VERIFY.md:105-114).
- Link adjacency CLOSED: A1 + C1 huffman objects share ZERO
  symbols (`nm` intersection EMPTY, MUST-VERIFY.md:79-81).

## Validation (combo-8: `git am` 23/23 onto pristine 3e6cd5e;
record `doc/post/COMBO-RERUN-8.md` — HISTORICAL combo-7 was
`doc/post/COMBO-RERUN-7.md`, combo-6 `doc/post/COMBO-RERUN-6.md`)

Apply order with result (A0→A→B→C→E→D, `logs/am-*.log`):

- A0, A1 A2 (r6), B1 B2: plain-clean (5/5).
- C1 C2 C3: plain FAIL (Makefile decoder-comment context drift)
  → hand (C1 2 lines, C2 9 lines, C3 17 lines verbatim; C3
  allcodecs/configure/Changelog parts plain).
- C4-C10: plain-clean (docs row, mak appendix, fold8, DSI x2).
- E1: plain-clean. E2: plain FAIL (mak:218) → hand + fwd-fix
  (strict tail carried, GEN gains `-strict -2`; 43/43 block
  lines = mbox + 1 strict token).
- D1: FAIL (toc exists) → hand seam (678-line create dropped,
  parser include kept, code identical). D2-D6: plain-clean
  (mov, movenc, parser/mp4/ts tests incl T3 roundtrip).
- Assembly HEAD `2f067f6` + 2 combo-tree test-only resolution
  commits (`aadbdc6` B-encneg-strict, RT-GEN-strict);
  every failure logged verbatim, every hand merge byte-faithful.

Final-state checks, all pass: `grep -c ac4.mak tests/Makefile`
= 1; zero conflict markers; Changelog encoder-top +
decoder-bottom + TS/parser/mov chained; build exit 0, 34
baseline warnings, 0 ac4, 0 dupsyms; N5 14/14; live suite
12/12; `fate-ac4` 25/29 + G1 7 = predicted-exact, reswap 32/32;
no-samples shape 13/13 + 16 gated; deterministic rerun;
single `fate-ac4:` aggregate, 11 vars
(COMBO-RERUN-8.md:38-59 + :61-92).

## Rule 5 — B2 landing: append+merge, never copy (F1 fix, ff/doc-b2f1)

Source: `ff-B2-review/REVIEW-B2.md` F1 (medium). The staged B2 note
says "copy to tests/fate/ac4.mak; add include" — stale on both
counts: the file EXISTS and the include is already at
tests/Makefile:157. A plain copy CLOBBERS the resident tests;
worse, both files define a single-colon `fate-ac4:` target, so
even a concatenated file with two aggregates silently drops one
set (make keeps the last). No var-name collision
(`FATE_AC4_MPEGTS_*` vs `FATE_AC4/MD5/REMUX` — clean).

B2 lands exactly like a Rule-3 append (byte-verbatim stanza move,
zero content change), with the resident tail as the anchor:

- Insert fate-b2/ac4.mak lines 151-369 (16 test stanzas + F6
  gates + F6 comment, byte-verbatim, single-blank separators)
  immediately BEFORE the `fate-ac4:` aggregate line. Do NOT land
  lines 1-150 (draft header, staging-only), 371-420 (parser
  staging block), or 425 (the staged file's own `fate-ac4:`).
- Land the B2 wiring with the other wiring lines, just above the
  aggregate: `# --- B2 wiring (decode/demux only; ENC terms
  re-added at C4) ---` + staged line 423 (`FATE_SAMPLES_AVCONV +=
  $(FATE_AC4-yes) $(FATE_AC4_MD5-yes) $(FATE_AC4_REMUX-yes)`).
- The ONE aggregate gains the B2 vars (resident vars first).

Validation (ff/doc-b2f1 dry-run): merged mak 267 lines; resident
region + B2 block each byte-exact; single `fate-ac4:`; parses
under the real tests/Makefile with zero `overriding recipe`
warnings and all 18 tests expanding in the aggregate.
Negative control: the plain-copy form drops the resident tests —
the F1 premise, confirmed.

Create-case note: if a from-scratch post-order stack reaches B2
with NO `tests/fate/ac4.mak` in tree, the B2 block lands as the
seed instead — but that re-opens the B/C creator boundary (C05
also creates); do not improvise, re-stack per the assembly owner.

HISTORICAL cross-checks (r3/r4/Final/recut — pre-MUST bytes):
every re-audit re-confirmed append+merge; the Final flip made B
the creator.

R-MUST cross-check (this refresh): combo-8 EXECUTED the
mechanism on union bytes (creator B 2/2 + five appendix groups,
no wholesale mak copy anywhere, single aggregate 11 vars —
COMBO-RERUN-8.md:26-42). Pending re-cuts change no mechanism
(COMBO-RERUN-8.md:99-111: strict token adds + toc-hunk drop +
ref-only re-pins). Append+merge stands.

## Verdict: HOLD (combo-8: 1 content re-cut + 4 mechanical re-cuts, all bytes proven)

Stack verdict: A0→A→B→C→E→D 23/23 `git am` (17 plain + 6 hand,
all byte-faithful) + 2 test-only resolution commits, build rc=0
with 0 ac4 warnings and 0 dupsyms, N5 14/14, live suite 12/12,
`fate-ac4` 25/29 with the 7 G1 reds predicted-exact and the
32/32 reswap proof (combo-8). HISTORICAL verdicts (combo-5/6/7):
pre-MUST bytes, superseded.
Owned re-cuts before posting (COMBO-RERUN-8.md:99-111;
MUST-VERIFY.md:121-131):

1. D re-pin re-cut (content): 7 d-repin files into the D goldens
   (proven flips 32/32).
2. E2 strict recut (mechanical): tail context + GEN `-strict -2`.
3. B encneg strict (mechanical): both CMDs `-strict -2`.
4. D RT GEN strict (mechanical): `-strict -2` (T3 GEN dies rc88
   without it).
5. C01-C03 Makefile context (mechanical): refit trailing context
   to A-r6 comment bytes (anchor unchanged).
No code changes needed anywhere (COMBO-RERUN-8.md:113-116).
Next: one re-cut seat per series (or a single stack re-cut),
then combo-9 closer (or parent rules the reswap proof sufficient
for the D bytes + mechanicals).
