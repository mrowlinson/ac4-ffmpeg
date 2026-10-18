# ASSEMBLY-PROTOCOL-R2 — A0→A→B→C→E→D post-order stacking, MUST-final union packs (design doc, no code)

Date: 2026-10-16 (R2 refresh of `ff-assembly-protocol/ASSEMBLY-PROTOCOL.md`; original untouched).
R-MUST refresh: 2026-10-17, branch `ff/doc-runbook-r3` (post-MUST-final:
STACK-DAG order A0→A→B→C→E→D, 23 patches, six union send packs,
MUST-verify verdicts, combo-8 HOLD record; pre-MUST branch tips,
24-stack counts and combo-5/6/7 validations HISTORICAL).
R-R4 refresh: 2026-10-18, branch `ff/doc-runbook-r4` (post-reverify
reality: C/D/E HOLDs lifted to POST-OK, MUST-final FIXED mboxes,
23/23 plain `git am` zero-hand, 29/29 fate; combo-8 HOLD record
HISTORICAL — pre-fix bytes; live proof is
`doc/post/ASSEMBLY-DEBTS-R2.md` + the three reverify docs; Rules
0-5 mechanism text otherwise intact, debts marked LANDED).
Copy: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-r4/wt`
(TMPDIR in scratch). Parent repos untouched, no pushes.
Supersedes the dry-run finding (RUNBOOK-DRYRUN top: 11/28 post-order conflicts)
with a conflict-free assembly procedure, validated by stacking
(HISTORICAL: r2 ff-combo-rerun; r3 `ff-combo-rerun-4/COMBO-RERUN-4.md`;
r4 `ff-combo-rerun-5/COMBO-RERUN-5.md`; recut `doc/post/COMBO-RERUN-6.md`;
pre-MUST `doc/post/COMBO-RERUN-7.md`; pre-fix `doc/post/COMBO-RERUN-8.md`;
live: `doc/post/ASSEMBLY-DEBTS-R2.md` + `doc/post/{C-R3,D-R2,E-R2}-VERIFY.md`).

## Tip verification (six FIXED send packs on master 8c9ba74, file pins;
post cuts are the mboxes, not branches)

| Series | Shape used | Pin | Verified |
|---|---|---|---|
| A0 | union walker head (M21, POST-OK) | mbox 43263 B, 1 patch (`36b9eec`) | creates `ac4_toc.h` 678 l (mbox:57-74), demux walks it (mbox:411); MUST-VERIFY.md:50-63; bytes unchanged (ASSEMBLY-DEBTS-R2.md:33) |
| A | Series-A decoder re-cut 6 (M4-M9, POST-OK) | mbox 507373 B, 2 patches (`554b92b`) | staging `b2df32f`/`1c1b2b0`, `git am` ZERO warnings, trees exact (A-PREFLIGHT-CHECKLIST.md:47-72); MUST-VERIFY.md:22-48; bytes unchanged (ASSEMBLY-DEBTS-R2.md:33) |
| B | Series-B doc + decoder-FATE union (POST-OK) | mbox 40729 B FIXED, 2 patches (`36b9eec`) | creates mak (mbox:114-139) + include; encneg/T2/T5/M8 in 2/2; MUST-VERIFY.md:50-63; debt-5 strict LANDED (ASSEMBLY-DEBTS-R2.md:18) |
| C | Series-C encoder union 10/10 (M10-M18, POST-OK) | mbox 650794 B FIXED, 10 mails (`91c214b`) | C08 dropped, appendix shape (C-PREFLIGHT-CHECKLIST.md:1-21); HOLD lifted (C-R3-VERIFY.md:34-38) |
| E | Series-E TS stack re-cut (POST-OK) | mbox 12774 B FIXED, 2 patches (`3b65f39`) | M22 true + M19 line (E-R2-VERIFY.md:70-90); HOLD lifted (E-R2-VERIFY.md:3-7) |
| D | Series-D union 6/6 (M19-M21/T3, POST-OK) | mbox 163076 B FIXED, 6 patches (`50e7de5`) | static green + 29/29 + 3/3 (D-R2-VERIFY.md:19-26); HOLD lifted (D-R2-VERIFY.md:11-13) |

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
pair, strict CMDs in fixed bytes). C: 10 mails (01 tables AC4_ /
02 core+lib / 03 wire+reg / 04 docs single-row / 05 fate / 06
nosamples / 07 doc-note / 08 fold8 / 09 DSI-wire / 10 DSI-mask).
E: 2 patches (1/2 TS descriptors + Changelog line, 2/2 PMT +
probe fate, strict tail + GEN in fixed bytes). D: 6 patches
(1/6 parser + toc-use / 2/6 dac4 demux / 3/6 dac4 mux + M20 /
4/6 parser fate / 5/6 mp4 roundtrip + T3 strict GEN / 6/6
ts-demux fate; 7 re-pins in 1/6).
Validated stack (debts-R2, fixed bytes): A0→A→B→C→E→D assembles
23/23 plain `git am`, zero hand; build rc=0, 0 ac4 warnings, 0
dupsyms; N5 14/14 (combo-8, carried); live suite 12/12
(preflight, carried); `fate-ac4` 29/29 + A0D 3/3 (see
Validation). HISTORICAL cores: A→A0→B→C→E→D 24/24 with
resolutions (combo-6); 17/23 plain + 6 hand + G1 reds (combo-8,
pre-fix bytes). R-R4 notes: C carries no neg script (C08
dropped; layout negatives ride B 2/2 as FATE behind the encoder
ifeq — STACK-DAG.md:74-78); C-final mak region + E-tip + D-tip
shapes are appendix-form in-mbox (STACK-DAG.md:48-49); D T3
rides 5/6 as `FATE_AC4_RT` (UNION-D.md:36-41); the A-r6 decoder
comment reword is REFRESHED into C01-C03 (debt 1 LANDED,
ASSEMBLY-DEBTS-R2.md:14; proven context-only,
C-R3-VERIFY.md:24).

## Rule 1 — Changelog: B appends, C prepends, E + D chain, no conflict

Claimants (STACK-DAG.md:46 + UNION-D.md:12-16 + E-VERIFY.md:59-61):
B 1/2 appends the decoder line at the `<next>` list end; C 03/10
prepends the encoder line at the list top; E 1/2 inserts the
MPEG-TS line directly after C's (mbox:33); D 1/6 adds the parser
line after E's (mbox:58), D 3/6 adds the mov line after the
parser line (mbox:1349-1350). A/A0 carry no Changelog hunk
(STACK-DAG.md:53-54). Proven stacking at E-tip: L5 encoder (C),
L6 TS (E), L24 decoder (B) — re-proven on fixed bytes
(E-R2-VERIFY.md:87-90). Rule stands unchanged into the post
round (no Changelog debt ever filed).

## Rule 2 — tests/Makefile include: B creates, all others carry none

B 2/2 CREATES the include line
`include $(SRC_PATH)/tests/fate/ac4.mak` (STACK-DAG.md:48);
C05's duplicate hunk is dropped in the union
(STACK-DAG.md:48); E2/D4/D5/D6 carry no tests/Makefile hunk
(appendix shape, STACK-DAG.md:49). The r2-era add/add conflict
record (identical line after `…/ac3.mak`, before `adpcm.mak`)
is why only the creator keeps it. Rule stands unchanged into
the post round (no Makefile-include debt ever filed).

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
11 vars (COMBO-RERUN-8.md:38-39, shape carried to fixed bytes).

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
- R-R4 mak debts: ALL LANDED (mechanism unchanged) — E2
  tail-CMD context + GEN `-strict -2` (ASSEMBLY-DEBTS-R2.md:15;
  E-R2-VERIFY.md:124-137); B encneg CMDs `-strict -2` x2
  (ASSEMBLY-DEBTS-R2.md:18; green at C/E/D tips —
  C-R3-VERIFY.md:27, E-R2-VERIFY.md:35, D-R2-VERIFY.md:24); D RT
  GEN `-strict -2` (ASSEMBLY-DEBTS-R2.md:17; D-R2-VERIFY.md:21).

## Rule 4 — verified disjoint core + resolved adjacencies

Disjoint regions (STACK-DAG.md:40-54, debts-R2 re-proven):
`libavcodec/Makefile` parser OBJS (~1285, D only) vs codec block
(~220, A-then-C); `configure` demuxer selects (~3888) vs float
probe (~7957); `tests/api/Makefile` (D only); single-owner files
(`parsers.c` D1, `version.h` C, `mov.c` D2, `movenc.c` D3,
`mpegtsenc.c` E1, lavf `ac4dec.c` + `demux.mak`/`seek.mak` A0,
lavc decoder core A). No APIchanges, no lavf
Makefile/allformats touches anywhere in the stack.

Resolved adjacencies (fixed bytes apply plain; combo-8
characterization HISTORICAL — COMBO-RERUN-8.md:26-36):

- `libavcodec/Makefile` (~220): A keeps pristine position (after
  AC3_MF); C01/C02/C03 now quote A-r6's reworded 2-line decoder
  comment (debt 1 LANDED — ASSEMBLY-DEBTS-R2.md:14; normalized
  diff proves context-only, C-R3-VERIFY.md:24) → plain-clean,
  A0+A+B+C 15/15 rc=0 (C-R3-VERIFY.md:23).
- `libavcodec/allcodecs.c`: A adds the decoder extern; C03 adds
  the encoder extern after it — decoder-first order, applies
  plain (STACK-DAG.md:42-43).
- `doc/general_contents.texi`: B lands the decode-only row; C04
  single-row fold chained below it (M18; STACK-DAG.md:47).
- `libavcodec/ac4_toc.h` (new): A0 creates the shared TOC walk,
  D 1/6 uses it (STACK-DAG.md:51) — fixed D drops its own
  684-line create block (debt 3 LANDED —
  ASSEMBLY-DEBTS-R2.md:16) and keeps the parser include + calls;
  tree header `cmp`-EQ A0's (D-R2-VERIFY.md:20).
- `tests/fate/ac4.mak` tail context: fixed E2 carries the union-C
  strict tail + strict GEN in-shipped-bytes (debt 2 LANDED —
  ASSEMBLY-DEBTS-R2.md:15; E-R2-VERIFY.md:124-137) → plain-clean,
  A0→E 17/17 rc=0 (E-R2-VERIFY.md:31).
- Link adjacency CLOSED: A1 + C1 huffman objects share ZERO
  symbols (`nm` intersection EMPTY, MUST-VERIFY.md:79-81).

## Validation (debts-R2: plain `git am` 23/23 onto pristine 3e6cd5e;
record `doc/post/ASSEMBLY-DEBTS-R2.md` — HISTORICAL combo-8 was
`doc/post/COMBO-RERUN-8.md`, combo-7 `doc/post/COMBO-RERUN-7.md`,
combo-6 `doc/post/COMBO-RERUN-6.md`)

Apply order with result (A0→A→B→C→E→D, `logs/am-rebase.log`):

- A0, A1 A2 (r6), B1 B2: plain-clean (5/5).
- C1–C10: plain-clean (context refreshed in-shipped-bytes; no
  `csplit` hand-fix — E-R2-VERIFY.md:42-43).
- E1, E2: plain-clean (strict tail + strict GEN in-shipped-bytes;
  E-tip `107d2ad` — E-R2-VERIFY.md:44-47).
- D1–D6: plain-clean (toc-create dropped in-shipped-bytes).
- Full stack: 6/6 mboxes rc=0, 23 commits, zero hand
  (ASSEMBLY-DEBTS-R2.md:41-44); re-proven 6/6 rc=0, 23 commits,
  0 am warnings/errors (D-R2-VERIFY.md:19).

Final-state checks, all pass: `grep -c ac4.mak tests/Makefile`
= 1; zero conflict markers; Changelog encoder-top +
decoder-bottom + TS/parser/mov chained; build exit 0, 34
baseline warnings, 0 ac4, 0 dupsyms
(ASSEMBLY-DEBTS-R2.md:46-48); `fate-ac4` 29/29 rc=0, zero
fail/error (ASSEMBLY-DEBTS-R2.md:49-50); A0D trio 3/3
(ASSEMBLY-DEBTS-R2.md:51); single `fate-ac4:` aggregate, 11
vars (shape carried); T3 GEN deterministic rerun byte-identical
(D-R2-VERIFY.md:25).

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

R-R4 cross-check (this refresh): the MUST-final re-cuts LANDED
with no mechanism change (strict token adds + toc-hunk drop +
ref-only re-pins — ASSEMBLY-DEBTS-R2.md:12-19), and the fixed
stack assembles + builds + fates green (see Validation).
Append+merge stands.

## Verdict: POST-OK (assembly: 23/23 plain, zero hand, 29/29 fate)

Stack verdict: A0→A→B→C→E→D 23/23 plain `git am`, zero hand,
build rc=0 with 0 ac4 warnings and 0 dupsyms, `fate-ac4` 29/29
rc=0 with zero fail/error, A0D trio 3/3 (debts-R2). HISTORICAL
verdicts (combo-5/6/7/8): pre-fix bytes, superseded.
Landed re-cuts (were combo-8/MUST-verify owned items —
COMBO-RERUN-8.md:99-111; MUST-VERIFY.md:121-131):

1. D re-pin re-cut (content): 7 d-repin files in the D goldens —
   LANDED (ASSEMBLY-DEBTS-R2.md:19), 7/7 `cmp`-MATCH
   (D-R2-VERIFY.md:22).
2. E2 strict recut (mechanical): tail context + GEN `-strict -2` —
   LANDED (ASSEMBLY-DEBTS-R2.md:15), spec §1-5 closed
   (E-R2-VERIFY.md:122-137).
3. B encneg strict (mechanical): both CMDs `-strict -2` —
   LANDED (ASSEMBLY-DEBTS-R2.md:18), green at C/E/D tips.
4. D RT GEN strict (mechanical): `-strict -2` —
   LANDED (ASSEMBLY-DEBTS-R2.md:17; D-R2-VERIFY.md:21).
5. C01-C03 Makefile context (mechanical): refit to A-r6 comment
   bytes — LANDED (ASSEMBLY-DEBTS-R2.md:14), proven context-only
   (C-R3-VERIFY.md:24).
No code changes needed anywhere (COMBO-RERUN-8.md:113-116,
carried). Next: combo-9 closer (re-run of the step-8 recipe on
fixed bytes) + sweep-r2 (fresh size/count sweep on fixed bytes),
then the step-9 send; both PENDING — DO-NOT-SEND stands.
