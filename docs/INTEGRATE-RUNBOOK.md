# INTEGRATE-RUNBOOK — AC-4 FFmpeg endgame sequence

Doc only. No code. All repos read-only until the step that says commit.
Date: 2026-10-16. Scratch/TMPDIR: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-integrate-runbook`.
Refresh: 2026-10-16 (r2 pins, mechanical only — no new procedures, no redesign).
Refresh scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-refresh`.
Branch copy: `ff/doc-b2f1` (F1 fix — appends `## B2 landing addendum` only;
base text verbatim from the ff-integrate-runbook scratch copy).
Refresh2: 2026-10-16 (r3 pins, mechanical only — no new procedures, no redesign).
Refresh2 scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-r3`.
Branch copy: `ff/doc-runbook-r3` (r3 pin sweep; r2 SHAs survive only in
explicitly-marked historical notes).
Refresh3: 2026-10-16 (r4 pins, mechanical only — no new procedures, no redesign).
Refresh3 scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-r4-sweep`.
Branch copy: `ff/doc-runbook-r4` (r4 pin sweep; r2/r3 SHAs survive only in
explicitly-marked historical notes).

Inputs read: `ff-submit-v2/SUBMIT-PLAN-V2.md` (192 l),
`ff-C-splitplan/C-SPLITPLAN.md` (131 l), `ff-D-plan/D-PLAN.md` (142 l),
`ff-E-plan/E-PLAN.md` (139 l), `ff-open-items/OPEN-ITEMS.md` (113 l, 93 rows),
`ff-runbook-dryrun/RUNBOOK-DRYRUN.md` (per-step validation table),
`ff-assembly-protocol/ASSEMBLY-PROTOCOL.md` (stacking Rules 0-4),
`ff-split-polish/REPORT.md` (r2 reword/C6/fold8 provenance),
`ff-combo-rerun/HEARTBEAT.md` (r2 assembly validation),
`ff-combo-rerun-4/COMBO-RERUN-4.md` (r3 assembly validation, 19 patches),
`ff-submit-fix/FIX-REPORT.md` (r4 recut: F1-F6 fixes, 20-commit stack),
`ff-combo-rerun-5/COMBO-RERUN-5.md` (r4 assembly validation, 20 patches),
`ff-Q1-verify/VERIFY-Q1.md` (Q1 independent verify, PASS),
`ff-D6-verify/VERIFY-D6.md` (D6 independent verify, PASS),
`ff-D-cover-align/D-COVER-ALIGN.md` (D 0/6 decision + stale-§0 finding),
`ff-N5-reconcile/N5-RECONCILE.md` (N5 CONTRADICTION),
`ff-dec-lib-review/REVIEW-DEC-LIB.md` §8 (APPROVE-WRAP conditional),
`doc/covers/COVER-*.txt` (r4 cover set, parent 0177e87),
`ff-mail1-prep/HEARTBEAT.md` + `ff-mail2-prep/HEARTBEAT.md` (mails prepped),
branch tips re-verified read-only this run (below, §0 = pin table).

Assembly reference: stack per `ff-assembly-protocol/ASSEMBLY-PROTOCOL.md`
Rules 0-4 + Rule 5 (C-only Changelog; C creates tests/Makefile include,
E2/D4 drop; C creates ac4.mak, E2/D4/D5/D6 aggregate-anchored appends;
disjoint-file table; B2 append+merge, never copy).
r4 re-validation: `ff-combo-rerun-5` (`git am` 20/20 onto pristine
3e6cd5e, applied tree == tip 08b4f00, final mak 126 lines, 13/13 fate +
aggregate both SAMPLES modes + aliases + neg-gate 2/2, goldens
86828e84…/53a44aca… byte-identical vs combo-4).

## 0. Pin table — r4 post branches (read-only `git log/show/rev-parse`)

r4 stack lives in the submit-fix scratch clone
(`ff-submit-fix/ffmpeg`: linear 20-commit stack A0'→C'→E'→D' over
pristine 3e6cd5e, 0 merges, 20/20 sign-offs — verified read-only this
run). Parent `Projects/ac4-ffmpeg/ffmpeg` repo + its branches untouched
(still r2/r3-era tips); post cuts are the mbox series exported from
the clone (`ff-submit-fix/mbox/series-{A0,C,E,D}.mbox`).

| post branch (= pin) | tip | shape over master |
|---|---|---|
| ac4/A0-submit-r2 | fc4866b | 1 commit, parent 3e6cd5e; HISTORICAL c22b0fe + 3 comment lines (§→`Sec.`, ASCII-only); M3 fate in demux.mak/seek.mak (byte-identical to r3), never ac4.mak |
| ac4/C-encoder-split-r4 | 1ccd10a | 11 commits, linear, no merges: 282ac3b (C1 tables), acc1bb8 (C2 core+lib, R3 guards + S5FU x2.0 + fuzz EINVAL/saturate), 0b3f890 (C3 wrapper+reg, keeps sole Changelog line), 3631fcf (C4' docs), b3f3a1f (C5' fate, creates ac4.mak + include), 38a19be (C6 nosamples), f36f407 (W1 doc-note) + f07df7f (W2 neg-gate) + e6f99ec (W3 fold8), e017d9b (C7 DSI-wire putbits) + 1ccd10a (C8 DSI-mask lavc table); recut of HISTORICAL r3 065976d (same structure + submit hygiene: sign-offs, 80-col hand code, ASCII-only, ordered dates, de-jargoned messages); C-final mak 53 lines byte-identical to r3; stereo golden 86828e84ccecb4171d775806616e0e93 (98193 B), 5.1 53a44acaa21853f91cfef7dbdcccb67e unchanged |
| ac4/E-ts-r3 | a2485a9 | 2 commits on the C' tip: 5e2d5d5 (E1, lavf-only mpegtsenc.c; blob-identical to HISTORICAL 98486f6) + a2485a9 (E2, already protocol-form: no tests/Makefile hunk, 42-line stanza append + aggregate extension); E-tip mak 96 lines |
| ac4/D-split-r3 | 08b4f00 | 6 commits, linear, no merges, on the E' tip: 083c676 (D1 parser; blob-identical to HISTORICAL 4ba9790), c01ad3f (D2 mov demux; blob-identical to HISTORICAL 14c6b50), 499892d (D3 movenc mux; HISTORICAL r2 content + 2-line 80-col rewrap only), dd69611 (D4 parser fate, append-form, no include hunk), 0747972 (D5 mp4 fate, append-form), 08b4f00 (D6 ts-demux fate; test .c md5 5296d6ad3006ab7f120e48a03474c9ee, identical to the verified fold blobs); final mak 126 lines, single aggregate with 6 vars; no Changelog hunk anywhere in D |

D6 note (LANDED — placement closed, rides D in-series): D6 = 08b4f00
(6th commit of D-split-r3; E1 in-history, no cherry-pick, no
out-of-series dep). HISTORICAL fold placements 73daef5 (D-tip stack)
and 70016fb (standalone stack) superseded — same test .c bytes, now
posted from the series. D6-fold gates + VERIFY-D6 (3x PASS, vectors
pinned) + combo-5 ts-demux PASS all green.

Superseded for post purposes — never stack (conflict hunks documented only
as the reason for each protocol rule): ac4/A0-demux-params@bbad279 (dev
line; post from the squash), HISTORICAL A0-submit@c22b0fe (superseded by
the -r2 ASCII recut fc4866b), ac4/C-encoder e065111-era (merge era dead;
`log --merges` empty on all r2/r3/r4 lines), ac4/C-encoder-split@790c073 (r1),
HISTORICAL r2 C line ac4/C-encoder-split-r2@d37a7b8 (9/9: 9115609,
b327617, ce7a9fc, 2f8f1c1, efb75b7, b4a924f, ef9eaf2, 21ff22e, d37a7b8;
r2 stereo golden was 0f0e9f6092ea4d489f91a41acef75a59; the -r2 branch tip
has since moved to bdffc8f — never post a mid-branch pin),
HISTORICAL r3 C line ac4/C-encoder-split-r3@065976d (11/11: a6f5544,
3c8cafc, 607e9fa, ed26bdd, ed73469, be72f52, c747e4f, 41e00f5, f03e143,
89adb71, 065976d; content carried into the r4 recut 1ccd10a + submit
hygiene; C-final mak byte-identical),
ac4/E-tsmux@3fdb20c, ac4/E1-tsdesc@5772a4e, ac4/E2-tsfate@39d4eab,
HISTORICAL E1'/E2'@98486f6/09a1bd2 (create-form; content carried into
E-ts-r3, E2 now protocol-form), ac4/D-split@356a375,
HISTORICAL D-split-r2@fcc9dbe (5/5: 4ba9790, 14c6b50, 5cd1740, b0e64ff,
fcc9dbe; D4/D5 create-form; content carried into D-split-r3, fate
commits now append-form + D6),
ac4/D-parser, ac4/D-demux, ac4/D-demux-r3, ac4/D-demux-r4 (fold branch),
ac4/D-mux, ac4/fuzz-lavf.

Parent repo (`Projects/ac4-ffmpeg`, master = bd8c505):

| branch | state |
|---|---|
| ff/dec-lib | DONE, MERGED (d505b36 = 32b0f1a H1 streaming); wrap STAGED+MERGED (54d3922/13ccfc6, 11/11 byte-exact, tseek T1-T6); Q1 CLOSED (96ce7bf/d41f784 in-tree core port, VERIFY-Q1 PASS); ngis fix MERGED (d20e772/68fcd33 FINDING-1 SBO + gate); N5fix PENDING (zero commits on ff/dec-n5fix beyond d505b36 — step 1a) |
| ff/enc-libr2 (+R3, +minors, +overflow, +S5FU, +covers) | DONE, all MERGED (c2 a8b4b1c … c5 45548e8 faefdc5, R3 2754a42/c6a9059, nits 3c89b94, minors a59a74c, overflow 789b8c8 + nits 3543732, S5FU 4f41c92 + cosmetic 9c7fb2e, cover-r3 0f6c6e2, cover-r4 0177e87/277c5ef) |
| ff/dsi-ser (+r2, +minors, +lavcshape) | DONE, all MERGED (ser 4e1d1e6/6b4e3be … lavcshape 6ae9514); contract `dsi/README.md` on master (payload-only, no box header); drift verdict DOC-ONLY ba001e9, merged via 7c35c41 — step 1c |
| ff/dec-b2 | DONE, MERGED (afb3beb: B2 decoder-FATE mak + lib-validated pins) |
| ff/doc-mail2 | MERGED (945d23e/bd8c505: owner-name fills + mail-2 T-pin pack finalized in scratch — step 8d) |
| ff/enc-lib, ff/enc-libfix | merged (0d53cc9, cf94072) |

V2 deltas since it was written (HISTORICAL chains — live pins in §0):
A0 squash posted-shape committed (M3 fate folded), then recut to -r2
(fc4866b, ASCII-only comments); C split + C6 + fold8 committed on the
-r2 line, RECUT to r3 (11/11 @065976d), then recut to r4 (11/11 @1ccd10a:
same content + submit hygiene, goldens unchanged); E1'/E2' reworded
(trees identical), then folded to E-ts-r3 (@a2485a9: E1 blob-identical +
E2 protocol-form); D 5/5 committed on the -r2 line, then recut to
D-split-r3 (@08b4f00: D1/D2 blob-identical, D3 +2-line rewrap, D4/D5
append-form, D6 LANDED in-series); full stack now
one linear 20-commit post line (0 merges, 20/20 sign-offs), `git am`
20/20 + 13/13 fate green (combo-5); enc-libr2 c2-c5 + R3 + minors +
overflow + S5FU and dsi-ser + r2 + minors all merged; dec-lib H1 merged,
wrap+Q1+ngis merged (Q1 independently verified PASS; N5fix pending);
submit covers @doc/covers rewritten for r4 (D 0/6); mail-1 + mail-2 both
prepped (mail-2 fills merged).
V2 "not started" claims for D4-draft/E1/R2-c2 are stale; the c223fe1
merge is dead history.
Dry-run deltas since IT ran (HISTORICAL chains — live pins in §0):
parent master 3c89b94 -> bd8c505 (dec-lib,
dec-b2, overflow, S5FU, covers-r3, B2F1, wrap, Q1, ngis, samples-manifest,
covers-r4, mail2 all merged since 6ae9514); A0-submit ee82042 -> c22b0fe
(squash of full bbad279 line incl MINOR-1) -> fc4866b (ASCII recut);
C-split d1b9e0e/790c073 -> r2 d37a7b8 (C4'/C5' reword + C6 + fold8 replay)
-> r3 065976d (recut: guards + S5FU + fuzz + DSI pair + new stereo golden)
-> r4 1ccd10a (recut: submit hygiene, content carried, goldens unchanged);
E1/E2 5772a4e/39d4eab -> 98486f6/09a1bd2 (message-only) -> 5e2d5d5/a2485a9
(E-ts-r3: E1 blob-identical, E2 protocol-form); D-split 356a375
-> fcc9dbe (D1-D3 identical, D4/D5 small deltas) -> 08b4f00 (D1/D2
blob-identical, D3 +2-line rewrap, D4/D5 append-form, +D6).

Patch-count rollup: A0(1) + A1 + A2 + (A3?) + B1 + B2 + C(11)
+ E(2) + D(6) = **24-26 patches**.

## 1. Endgame sequence

### Step 1 — Freeze lib prep in parent repo (blocks A1, C-resync, D-P3)

1a. DONE — `ff/dec-lib` merged to master (d505b36 = 32b0f1a H1 streaming:
opaque ctx, bounded rings, parity 100/100 DIFF 0). Wrap DONE — staged +
merged (54d3922/13ccfc6: thin RECEIVE_FRAME wrapper, 11/11 byte-exact vs
dec-lib CLI, tseek T1-T6, ASan/UBSan clean). Q1 CLOSED — in-tree core
port merged (96ce7bf/d41f784: snapshot-vendoring, WHERE = in-tree
libavcodec/ under CONFIG_AC4_DECODER) and independently verified PASS
(ff-Q1-verify: wiring.apply-check clean, 11/11 byte-exact x3 legs, neg
splice + garbage-cap, tseek T1-T6, port-delta audit 23/23 headers+strip
only). ngis fix DONE — merged (d20e772/68fcd33: FINDING-1 cfg==6
uninit-ngis SBO + E4_MAXGRP bound + regression gate). One follow-up
left, not blocking A1 start: N5 rate-semantics verdict CONTRADICTION
(N5-RECONCILE: A0 matches Table E.1/s5.11, dec-lib labels pre-resampler
grid values — ~4.3% drift + bogus 46034 Hz on fri 0-12; fix owned by
dec-lib: output-rate label + s5.11 resampler); N5fix PENDING (zero
commits on `ff/dec-n5fix` beyond d505b36; wrap runs passthrough, fri
0-12 goldens re-record on land). Struck; A1 gate open.
1b. DONE — `ff/enc-libr2` c2-c5 + R3 + minors + overflow + S5FU all merged
to master (faefdc5 … 9c7fb2e). Struck; C-resync gate open.
1c. DONE — `ff/dsi-ser` + r2 + minors + lavcshape all merged; contract
settled and on master (`dsi/README.md`: raw DSI payload bytes as
extradata, E.5 packing, no box header). Drift verdict DOC-ONLY at
`ff/doc-dsi-l3@ba001e9`, merged via 7c35c41: parent dsi/ moved (minors2)
since C7/C8 vendored from lavcshape-merged, proven wire-neutral
(goldens identical, suites ALL PASS both ends, parent-master oracle+putbits
reproduce r3 extradata bytes exactly, 32 B both layouts) — no follow-up,
no re-vendor. Struck; D-P3 gate open.

### Step 2 — Stack A0 for post (head of submission order)

2a. `ac4/A0-submit-r2@fc4866b` = single squash (parent 3e6cd5e; HISTORICAL
r3 c22b0fe + 3 ASCII comment lines): code + M3 fate (demux.mak/seek.mak
refs, never ac4.mak). Posts FIRST as one patch (raw-first premise, V2
D2). The old split instruction (withhold M3 0003 to ride the B2 block,
V2 D5) applies ONLY if posting from `ac4/A0-demux-params` instead of
the squash.
2b. Verify A0 gates closed: A0-B1/B2 BLOCKERs, A0-M1..M4, A0-m1..m7,
D12 peek bound (VERIFIED in HISTORICAL c22b0fe, carried into fc4866b:
`AC4_TOC_PEEK_MAX` 64 KiB @ ac4dec.c:98 + fast-path/seekback guards);
standalone demux build; patcheck in files form (step 5/9).

### Step 3 — C line (post from ac4/C-encoder-split-r4@1ccd10a, pins §0)

~~3a. unmerge C~~ — REMOVED, obsolete: all C lines linear, `log --merges`
empty; c223fe1 survives only as dead history. Post from the -r4 branch.
3b. R3 re-sync — DONE (R3 guard strictness policy-a + S5FU x2.0 stereo +
fuzz crash-1 saturate / crash-2+3 EINVAL, re-cut C1-C3 282ac3b/acc1bb8/
0b3f890; parent c1-c5 + R3 + minors + overflow + S5FU merged; HISTORICAL
r3 C1-C3 were a6f5544/3c8cafc/607e9fa, r2 C2 was b327617 with S1
semantics only).
3c. M3 mechanical rename — DONE (r4 C1-C3 carry it; lavc files
`ac4_*`-prefixed, globals `ff_ac4_*`/`ac4_*`, unique names per
assembly Rule 4; +5 DSI files `ac4dsi*` likewise unique).
3d. MSVC rework — DONE (configure probe + `$(CFLAGS-ac4enc_float)`
in C3 0b3f890; zero raw fast-math flags).
3e. REVIEW-C fold — DONE (folded content carried through the recuts;
code unposted, folded, no fixups; HISTORICAL review commit c4cb474).
3f. Split — DONE: -r4 line is C1 tables / C2 core+lib / C3 wrapper+reg
(flips codec live) / C4' docs / C5' fate / C6 nosamples / +3 (W1 doc-note,
W2 neg-gate, W3 fold8) / +2 DSI (C7 putbits wire, C8 lavc-mask table).
Shape delta vs splitplan 5: +C6 +W1-W3 +C7/C8; COVER-C =
`doc/covers/COVER-C.txt` (rewritten for r4 0/11, parent 0177e87 —
re-emit at post per step 9).
3g. Submit hygiene — DONE (r4 recut: 20/20 `Signed-off-by: Michael
Rowlinson <mrowlinson@gmail.com>` across the stack, 80-col hand code,
ASCII-only code+messages+covers, ordered dates, de-jargoned messages;
C-final mak 53 lines byte-identical to r3; goldens unchanged).

### Step 4 — Stack D (post from ac4/D-split-r3@08b4f00, pins §0)

Order D1->D2->D3->D4->D5->D6 (D-plan order, overrides V2).
4a. D1 = 083c676 (parser; blob == HISTORICAL r2 4ba9790, content == r1 d93900d):
review it (TOR-1/2/3 asserts pinned for D4; design-over-oracle t22/t29
emit-1; t25 bounded re-lock), then rebase point.
4b. D2 = c01ad3f (mov demux, `mov.c` only; blob == HISTORICAL r2 14c6b50,
content == r1 d9091f3): need_parsing dead without it.
4c. D3 = 499892d (movenc mux; HISTORICAL r2 5cd1740 content + 2-line
80-col rewrap only, == r1 622c9db modulo the rewrap): Step 1c contract
settled (DONE above); E.5 byte validation vs `mp4_extract.py walk`
hex of real files
(34 B payloads).
4d. P4 wiring + `ac4_header.h` shared TOC/sync helpers (>=2 call sites
or drop) — STILL OPEN: no wiring commit on the -r3 line (D4/D5/D6 are
fate, not wiring); resolve feed-shape FLAG with decoder owner. D6
PLACEMENT now closed (lands in-series as 08b4f00, E1 in-history) — what
is open is wiring only, not the test.
4e. D4 = dd69611 (parser fate) + D5 = 0747972 (mp4 fate) + D6 = 08b4f00
(ts-demux fate), all already in protocol append-form in-branch (no
tests/Makefile hunks; mak stanzas aggregate-anchored): post as-is, no
assembly rebase (B2-vs-D4 split: demux INVALIDDATA vs parser drop-bad).

### Step 5 — Per-patch gates (every patch, every series)

patcheck clean-under-triage (files form — the tool takes patch FILES,
verified in `tools/patcheck`: greps `$*`, stdin only with zero args;
the old `<base>..HEAD` range form is WRONG); `git commit -s` signoff;
one logical change; lavc K&R style; no spec prose (clause numbers
only); LGPL SPDX on new files; standalone
`--disable-everything --enable-{decoder,encoder,parser,muxer}=ac4` build per
patch step; no new warnings; ASan + trasher on new demux/decoder/parser paths.
macOS notes (dry-run): `xargs -d` + BSD-grep ERE break 2 check sections
(use GNU grep or wrapper); literal "clean" unachievable (false-pos:
`for(;;)`, tabs-in-.mak, awk-script hits, comment-text "missing }") —
clean means triaged, every hit dispositioned.
r4 compliance (submit-fix F3/F4/F5, re-verified this run): 20/20
sign-offs; 80-col hand code (generated tables + .mak left per baseline
precedent, justified in FIX-REPORT); ASCII-only code+messages+covers;
`send-email` 8bit warnings 0 on all 4 series.

### Step 6 — Combined-build recipe (integration worktree, trees untouched)

```sh
export TMPDIR=/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-r4-sweep
# detached worktree stacking post-order per ASSEMBLY-PROTOCOL Rules 0-5
# (pins §0): A0(1) -> C(11) -> E(2) -> D(6) (20 patches, zero rebases)
git -C ~/Projects/ac4-ffmpeg/ffmpeg worktree add --detach /Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-r4-sweep/wt-combo <tip>
B=/Volumes/NVME/ac4-ffmpeg-ws/build/ff-combo; mkdir -p $B
cd /Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-r4-sweep/wt-combo && ./configure --disable-doc --disable-debug --prefix=$B  # baseline flags
make -j18  # baseline: exit 0, <5 min, Apple clang 21
./ffmpeg -hide_banner -codecs 2>/dev/null | grep -c ac4   # expect codec rows
```

Stacking detail (protocol form now pre-applied in-branch, stanza anchors,
negative controls): `ff-assembly-protocol/ASSEMBLY-PROTOCOL.md`; r4
validation record (`git am` 20/20, tree == tip, mak 126, 13/13 fate +
aggregate + neg-gate 2/2):
`ff-combo-rerun-5/COMBO-RERUN-5.md` (HISTORICAL r3 record, 19/19:
`ff-combo-rerun-4/COMBO-RERUN-4.md`; r2 record, 17/17:
`ff-combo-rerun/HEARTBEAT.md`).

Per-component standalone builds (same worktree, separate build dirs):
`--disable-everything --enable-decoder=ac4`, `--enable-encoder=ac4`,
`--enable-parser=ac4`, mp4-demux, mp4-mux, mpegts-mux configs. Full build
must stay zero-new-warnings throughout.

### Step 7 — Stack E on C-merge HEAD, then full FATE execution

7a. `ac4/E-ts-r3@a2485a9` = E1' (5e2d5d5, lavf-only mpegtsenc.c,
blob-identical to HISTORICAL 98486f6) + E2' (a2485a9, already protocol-form:
no tests/Makefile hunk, 42-line stanza append + aggregate extension),
posted as-is onto the C post line (C-split-r4 tip 1ccd10a); E1/C still
disjoint (E1 lavf-only, C lavc-only). C-final mak 53 lines
(byte-identical to r3, same `fate-ac4:` seed anchor), E-tip mak 96.
7b. E1 gates: remux `ffmpeg -i tone.wav -c:a ac4 -f mpegts out.ts` ->
`codec_name=ac4`, `codec_tag_string=AC-4`; PMT bytes stream_type 0x06 +
regdesc `AC-4` + SYSTEM_B 0x7F/0x15 flags 0x80; TSDuck/MP4Box parse clean;
NEGATIVE: pre-patch binary on same cmd MUST print "may not be recognized".
7c. Full `make fate` green on the combined worktree (step 6). Encoder-sourced
FATE only: NEVER `.ts` stream-copy source (mux.c:239 block until parser lands).
E2 cmd must dodge loudness refusal: `-loudness_target 0` (or 5.1/short span).

### Step 8 — Golden blessing (record, verify, upload-before-post)

8a. Goldens come ONLY from real wrapper builds (this combined tree):
md5 + framecrc prints; every golden re-run byte-identical (`cmp` clean);
deterministic under `-flags +bitexact` / `-bitexact` (C-FATE-LOCAL: 4/4 with
T1 `-loudness_target 0` correction; known goldens recorded there).
r4 encoder goldens UNCHANGED from r3 (COMBO-RERUN-5 byte-spot vs combo-4
binary, `cmp` clean both layouts): stereo
86828e84ccecb4171d775806616e0e93 (98193 B), 5.1
53a44acaa21853f91cfef7dbdcccb67e.
8b. No silence round-trip tests (V2 D9): md5/framecrc only.
8c. OWNER-RUN (suite access): suite files uploaded FIRST, never
overwritten: `make fate-rsync`, verify `ac4/clip-*` present, then the
FATE patch may post. Lanes never run this.
8d. OWNER-SEND (mails prepped, owner sends): mail-1 READY-TO-SEND
(scratch branch ff/doc-mail1 @203872f: owner-name fills; ff-samples-pack:
11 files, 271986 B, signed REQUEST.txt) + mail-2 T-pin pack finalized
(ff-samples-pack2: 4 built files, 13366 B, signed REQUEST2.txt; name
fills merged to master 945d23e/bd8c505). B2 clock: mail-1 +24h confirm
reply starts B2. D needs no samples mail (D4/D5/D6 self-contained,
null-cmp). Lanes never send.

### Step 9 — patcheck + format-patch per series (post mechanics)

```sh
# in the series worktree, linear branch over its base, N patches:
git format-patch -s --cover-letter -o /tmp/patches/<series> <base>..HEAD
./tools/patcheck /tmp/patches/<series>/*.patch   # files form; must be clean-under-triage, every patch
```

Cover letter re-emitted from V2 template per series (fixes V2 D11):
`[PATCH 0/N] native AC-4 {decoder for raw .ac4 streams / encoder /
MPEG-TS mux descriptors / MP4 ac-4+dac4 support}`; body = what series adds,
raw-first ordering note, test recipe (`ffmpeg -i clip.ac4 out.wav`,
`make fate-ac4-*`), suite files (FATE series only), open questions
(H1/D1 for C; need_parsing for D); Signed-off-by each patch.
Covers = `doc/covers/COVER-*.txt` (r4 set, parent 0177e87: A0/C/E/D
rewritten for the rebased stack, D 0/5→0/6 with D6; A/B carried verbatim).
Send artifacts = `ff-submit-fix/mbox/series-{A0,C,E,D}.mbox` (cut from r4
tips, subjects 20/20 match, zero placeholders after mail-1 prep).

### Step 10 — Per-series ship checklist (post in this order)

Order: A0(1) -> A1 -> A2 -> (A3?) -> B1 -> B2 -> C(11) -> E(2) -> D(6).

- A0: Step 2 done from `ac4/A0-submit-r2@fc4866b` (no split; fate folded);
  D12 pins; standalone demux build; posts first
  (rest of A/B assumes copy exit 0 + seekable pts).
- A1: Step 1a landed (d505b36); wrap staged+merged (54d3922/13ccfc6);
  Q1 closed + independently verified (96ce7bf/d41f784, VERIFY-Q1 PASS);
  ngis fix merged (d20e772/68fcd33); N5 CONTRADICTION open (N5fix pending).
- A2: A1 merged; S4 drop policy; S5 wrapper gain policy; T2 gap-note API;
  T3 skip_samples/latency; standalone decoder build.
- A3 (conditional): Q2 priming measure; T3 1-frame delay suggests nonzero ->
  likely NEEDED, else fold into A2.
- B1: DOC-1 decoder-opts freeze from A2; DOC-3 changelog rebase;
  DOC-7 anchors re-verified; NO APIchanges entry.
- B2: Step 8d mails prepped (OWNER sends); F1-F7 FAILs closed; D5
  A0-unblocked tests; T1/T4 pins; D9 md5/framecrc only; A1 advisory
  (wiring); landing per `## B2 landing addendum` (append+merge, never
  copy — Rule 5 cross-check: COMBO-RERUN-5 stacked no B2 mak, every mak
  merge a Rule-3 append, so the rule stands unviolated and unchanged).
- C: Step 3 all-DONE on `ac4/C-encoder-split-r4@1ccd10a`; EL-B1/B2 + EL-M/m,
  C-B1/M1-M5/m1-m9 closed; C-M3 syms, C-M4 MSVC; R3 guards, S5FU x2.0,
  fuzz crash-1/2/3 guards, DSI C7/C8 (drift DOC-ONLY ba001e9, merged);
  submit hygiene (20/20 sign-offs, 80-col, ASCII-only); H1 MANUAL-only
  confirm; D1 ENCODE_CB cover question; byte-parity gates re-run at C3
  (stereo golden 86828e84…, unchanged).
- E: Step 7 done (E-ts-r3 as-is, E2 already protocol-form); C merged (E
  never posts before C); disjoint E1/C re-verified.
- D: Step 4 done except 4d wiring (STILL OPEN); D6 LANDED in-series
  (08b4f00, placement closed); Step 1c contract DONE; D1 review;
  TOR-1/2/3 asserts in D4; no samples mail needed (self-contained);
  full `make fate` green.
- Parallelizable prep (never post early): B1 w/ A2-tail; C4 w/ C3-tail;
  mak/docs prep anytime. Each series independently reviewable + revertable.

### Step 11 — Rollback notes

- Revert order is reverse post order; each series reverts cleanly alone.
- C3 revert deactivates the codec (tables/core stay inert under the guard).
- A0 revert breaks A/B FATE (copy/pts premise) — revert B2 with it.
- E revert is lavf-only, safe anytime after C (no lavc symbols).
- D-P1 revert orphans P2 need_parsing — revert P2-P5 with P1.
- Posted patches are never rebased: review feedback -> v2/v3 re-posts
  (`git format-patch -v2`), same per-patch gates each round.
- Merge commits never post: any accidental merge -> reset to
  pre-merge tip in a fresh worktree, re-stack linearly, re-run Steps 5-7.
- Golden recalls: suite files are append-only; a bad golden is superseded by
  a new file + mak update patch, never overwritten in place.

## B2 landing addendum (F1 fix, ff/doc-b2f1)

Source: `ff-B2-review/REVIEW-B2.md` F1 (medium). The staged B2 note
(`fate-b2/README.md`: "copy to tests/fate/ac4.mak; add include")
is stale and DELETED by this branch — it would clobber the E-track
TS-mux tests already living in `tests/fate/ac4.mak`, and both files
define a single-colon `fate-ac4:` target (make keeps the last, the
other set silently drops from the aggregate). No var-name collision
(`FATE_AC4_MPEGTS_*` vs `FATE_AC4/MD5/REMUX` — clean).

Landing procedure (mechanical, byte-verbatim stanza move, zero
content change; mirrors ASSEMBLY-PROTOCOL-R2 Rule 3; validated by
the ff/doc-b2f1 dry-run — merged mak parses under the real
tests/Makefile, single aggregate, 18/18 tests expand):

1. No include step: `include $(SRC_PATH)/tests/fate/ac4.mak` is
   already at tests/Makefile:157.
2. Anchor: the single `fate-ac4:` aggregate line of in-tree
   `tests/fate/ac4.mak` (E-track tail).
3. Insert fate-b2/ac4.mak lines 151-369 (16 test stanzas + F6
   gates + F6 comment, byte-verbatim, single-blank separators)
   immediately BEFORE the aggregate line. Do NOT land lines
   1-150 (draft header, staging-only), 371-420 (parser staging
   block), or 425 (the staged file's own `fate-ac4:` line).
4. Land the B2 wiring with the other wiring lines, just above the
   aggregate: `# --- B2 wiring (decode/demux only; ENC terms
   re-added at C4) ---` + staged line 423 (`FATE_SAMPLES_AVCONV +=
   $(FATE_AC4-yes) $(FATE_AC4_MD5-yes) $(FATE_AC4_REMUX-yes)`).
5. Extend the ONE aggregate with the B2 vars (E vars first):
   `fate-ac4: $(FATE_AC4_MPEGTS_PMT-yes)
   $(FATE_AC4_MPEGTS_PROBE-yes) $(FATE_AC4-yes)
   $(FATE_AC4_MD5-yes) $(FATE_AC4_REMUX-yes)`.
6. Verify: `grep -c '^fate-ac4:'` = 1; E stanza + all 16 B2 tests
   present; `make -p` shows zero `overriding recipe` warnings.

Create-case note: if a from-scratch post-order stack reaches B2
with NO `tests/fate/ac4.mak` in tree, the B2 block lands as the
seed instead — but that re-opens the B/C creator boundary (C5'
also creates); do not improvise, re-stack per the assembly owner.

r3 cross-check (ff/doc-runbook-r3): Rule 5 text above is unchanged from
the F1 fix — COMBO-RERUN-4 re-audited the premise (B2-F1 caution: the
r3 A0→C→E→D stack contains no B2 mak at any step, every mak merge a
Rule-3 stanza append + `fate-ac4` prereq extension, no wholesale mak
copy anywhere), so append+merge stands as the B2 landing procedure.

r4 cross-check (ff/doc-runbook-r4): Rule 5 text still unchanged —
COMBO-RERUN-5 re-audited the premise on the 20-stack (no B2 mak at any
step, `git am` 20/20, every mak merge a Rule-3 append + aggregate
extension incl D6), so append+merge stands as the B2 landing procedure.
