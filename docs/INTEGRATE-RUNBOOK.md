# INTEGRATE-RUNBOOK — AC-4 FFmpeg endgame sequence

Doc only. No code. All repos read-only until the step that says commit.
Date: 2026-10-16. Scratch/TMPDIR: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-integrate-runbook`.
Refresh: 2026-10-16 (r2 pins, mechanical only — no new procedures, no redesign).
Refresh scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-refresh`.
Branch copy: `ff/doc-b2f1` (F1 fix — appends `## B2 landing addendum` only;
base text verbatim from the ff-integrate-runbook scratch copy).

Inputs read: `ff-submit-v2/SUBMIT-PLAN-V2.md` (192 l),
`ff-C-splitplan/C-SPLITPLAN.md` (131 l), `ff-D-plan/D-PLAN.md` (142 l),
`ff-E-plan/E-PLAN.md` (139 l), `ff-open-items/OPEN-ITEMS.md` (113 l, 93 rows),
`ff-runbook-dryrun/RUNBOOK-DRYRUN.md` (per-step validation table),
`ff-assembly-protocol/ASSEMBLY-PROTOCOL.md` (stacking Rules 0-4),
`ff-split-polish/REPORT.md` (r2 reword/C6/fold8 provenance),
`ff-combo-rerun/HEARTBEAT.md` (r2 assembly validation),
branch tips re-verified read-only this run (below, §0 = pin table).

Assembly reference: stack per `ff-assembly-protocol/ASSEMBLY-PROTOCOL.md`
Rules 0-4 (C-only Changelog; C creates tests/Makefile include, E2/D4 drop;
C creates ac4.mak, E2/D4/D5 aggregate-anchored appends; disjoint-file table).
r2 re-validation: `ff-combo-rerun` (17/17: 14 as-is + 3 rebased, 3/3
negative controls, final mak 119 lines exact).

## 0. Pin table — r2 post branches (read-only `git log/show/rev-parse`)

ffmpeg repo (`Projects/ac4-ffmpeg/ffmpeg`, master = 3e6cd5e clean):

| post branch (= pin) | tip | shape over master |
|---|---|---|
| ac4/A0-submit | c22b0fe | 1 commit squash, parent 3e6cd5e; tree == ac4/A0-demux-params@bbad279 (code + M3 fate in demux.mak/seek.mak, never ac4.mak) |
| ac4/C-encoder-split-r2 | d37a7b8 | 9 commits, linear, no merges: 9115609 (C1 tables), b327617 (C2 core+lib), ce7a9fc (C3 wrapper+reg, keeps sole Changelog line), 2f8f1c1 (C4' docs), efb75b7 (C5' fate, creates ac4.mak + include), b4a924f (C6 nosamples), ef9eaf2 + 21ff22e + d37a7b8 (fold8 x3) |
| ac4/E1-tsdesc-r2 | 98486f6 | 1 commit on master; tree == 5772a4e (message reflow only, words identical) |
| ac4/E2-tsfate-r2 | 09a1bd2 | 98486f6 + 09a1bd2; tree == 39d4eab (message reflow only); 09a1bd2 still create-form (M tests/Makefile + A ac4.mak) → E2-R per protocol at assembly |
| ac4/D-split-r2 | fcc9dbe | 5 commits, linear, no merges: 4ba9790 (D1 parser), 14c6b50 (D2 mov demux), 5cd1740 (D3 movenc mux), b0e64ff (D4 parser fate), fcc9dbe (D5 mp4 fate); D1-D3 content == r1 D-split; D4/D5 carry small content deltas, still protocol-form (D4: M tests/Makefile + A ac4.mak; D5: M ac4.mak) → D4-R/D5-R at assembly; no Changelog hunk anywhere in D |

Superseded for post purposes — never stack (conflict hunks documented only
as the reason for each protocol rule): ac4/A0-demux-params@bbad279 (dev
line; post from the squash), ac4/C-encoder e065111-era (merge era dead;
`log --merges` empty on all r2 lines), ac4/C-encoder-split@790c073 (r1;
`diff 790c073 d37a7b8` = C6 1-line change only), ac4/E-tsmux@3fdb20c,
ac4/E1-tsdesc@5772a4e, ac4/E2-tsfate@39d4eab, ac4/D-split@356a375,
ac4/D-parser, ac4/D-demux, ac4/D-demux-r3, ac4/D-mux, ac4/fuzz-lavf.

Parent repo (`Projects/ac4-ffmpeg`, master = 6ae9514):

| branch | state |
|---|---|
| ff/dec-lib | f751b2f UNMERGED + DIRTY (M dec-lib/src/acpl.c, acpl.h, dec_lib.c; ?? dec-lib/Makefile, parity/, src/ac4d.h — same set as first run) |
| ff/enc-libr2 (+R3, +minors) | DONE, all MERGED (c2 a8b4b1c … c5 45548e8, merge faefdc5, R3 2754a42/c6a9059, nits 3c89b94, minors a59a74c) |
| ff/dsi-ser (+r2, +minors, +lavcshape) | DONE, all MERGED (ser 4e1d1e6/6b4e3be … lavcshape 6ae9514); contract `dsi/README.md` on master (payload-only, no box header) |
| ff/enc-lib, ff/enc-libfix | merged (0d53cc9, cf94072) |

V2 deltas since it was written: A0 squash posted-shape committed (M3 fate
folded); C split + C6 + fold8 committed on the -r2 line; E1'/E2' reworded
(trees identical); D 5/5 committed on the -r2 line; enc-libr2 c2-c5 + R3
+ minors and dsi-ser + r2 + minors all merged. V2 "not started" claims
for D4-draft/E1/R2-c2 are stale; the c223fe1 merge is dead history.
Dry-run deltas since IT ran: parent master 3c89b94 -> 6ae9514; A0-submit
ee82042 -> c22b0fe (squash of full bbad279 line incl MINOR-1); C-split
d1b9e0e/790c073 -> d37a7b8 (C4'/C5' reword + C6 + fold8 replay);
E1/E2 5772a4e/39d4eab -> 98486f6/09a1bd2 (message-only); D-split 356a375
-> fcc9dbe (D1-D3 identical, D4/D5 small deltas).

Patch-count rollup: A0(1) + A1 + A2 + (A3?) + B1 + B2 + C(9)
+ E1 + E2-R + D(5, D4-R/D5-R) = **21-22 patches**.

## 1. Endgame sequence

### Step 1 — Freeze lib prep in parent repo (blocks A1, C-resync, D-P3)

1a. Land `ff/dec-lib`: resolve dirt (keep or drop parity/, ac4d.h, Makefile;
commit acpl/dec_lib edits), merge to master. Gates A1. STILL OPEN (same
dirty set re-verified this run).
1b. DONE — `ff/enc-libr2` c2-c5 + R3 + minors all merged to master
(faefdc5 … 6ae9514). Struck; C-resync gate open.
1c. DONE — `ff/dsi-ser` + r2 + minors + lavcshape all merged; contract
settled and on master (`dsi/README.md`: raw DSI payload bytes as
extradata, E.5 packing, no box header). Struck; D-P3 gate open.

### Step 2 — Stack A0 for post (head of submission order)

2a. `ac4/A0-submit@c22b0fe` = single squash (parent 3e6cd5e, tree ==
A0-demux-params@bbad279): code + M3 fate (demux.mak/seek.mak refs, never
ac4.mak). Posts FIRST as one patch (raw-first premise, V2 D2). The old
split instruction (withhold M3 0003 to ride the B2 block, V2 D5) applies
ONLY if posting from `ac4/A0-demux-params` instead of the squash.
2b. Verify A0 gates closed: A0-B1/B2 BLOCKERs, A0-M1..M4, A0-m1..m7,
D12 peek bound (VERIFIED in c22b0fe: `AC4_TOC_PEEK_MAX` 64 KiB @
ac4dec.c:98 + fast-path/seekback guards); standalone demux build;
patcheck in files form (step 5/9).

### Step 3 — C line (post from ac4/C-encoder-split-r2@d37a7b8, pins §0)

~~3a. unmerge C~~ — REMOVED, obsolete: all C lines linear, `log --merges`
empty; c223fe1 survives only as dead history. Post from the -r2 branch.
3b. R2 re-sync — DONE (S1 semantics on r2 structure in C2 b327617;
parent c1-c5 merged; C1-C3 SHAs unchanged from the validated r1 line).
3c. M3 mechanical rename — DONE (r2 C1-C3 SHAs unchanged; lavc files
`ac4_*`-prefixed, globals `ff_ac4_*`/`ac4_*`, unique names per
assembly Rule 4).
3d. MSVC rework — DONE (configure probe + `$(CFLAGS-ac4enc_float)`
in C3 ce7a9fc; zero raw fast-math flags).
3e. REVIEW-C fold — DONE (presence: c4cb474 in line; code unposted,
folded, no fixups).
3f. Split — DONE: -r2 line is C1 tables / C2 core+lib / C3 wrapper+reg
(flips codec live) / C4' docs / C5' fate / C6 nosamples / +3 fold8.
Shape delta vs splitplan 5: +C6 +fold8 follow-ups; COVER-C must
describe all 9 (re-emit at post per step 9).

### Step 4 — Stack D (post from ac4/D-split-r2@fcc9dbe, pins §0)

Order D1->D2->D3->D4->D5 (D-plan order, overrides V2).
4a. D1 = 4ba9790 (parser; content == r1 d93900d): review it (TOR-1/2/3
asserts pinned for D4; design-over-oracle t22/t29 emit-1; t25 bounded
re-lock), then rebase point.
4b. D2 = 14c6b50 (mov demux, `mov.c` only; content == r1 d9091f3):
need_parsing dead without it.
4c. D3 = 5cd1740 (movenc mux; content == r1 622c9db): Step 1c contract
settled (DONE above); E.5 byte validation vs `mp4_extract.py walk`
hex of real files (34 B payloads).
4d. P4 wiring + `ac4_header.h` shared TOC/sync helpers (>=2 call sites
or drop) — STILL OPEN: no wiring commit on the -r2 line (D4/D5 are
fate, not wiring); resolve feed-shape FLAG with decoder owner;
unlocks TS-demux params (D6).
4e. D4 = b0e64ff (parser fate) + D5 = fcc9dbe (mp4 fate), folded
per-branch as-branched (B2-vs-D4 split: demux INVALIDDATA vs parser
drop-bad); D4-R/D5-R per assembly Rules 2-3 at assembly time.

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

### Step 6 — Combined-build recipe (integration worktree, trees untouched)

```sh
export TMPDIR=/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-refresh
# detached worktree stacking post-order per ASSEMBLY-PROTOCOL Rules 0-4
# (pins §0): A0(1) -> C(9) -> E1 -> E2-R -> D1..D3 -> D4-R -> D5-R (17 patches)
git -C ~/Projects/ac4-ffmpeg/ffmpeg worktree add --detach /Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-refresh/wt-combo <tip>
B=/Volumes/NVME/ac4-ffmpeg-ws/build/ff-combo; mkdir -p $B
cd /Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-refresh/wt-combo && ./configure --disable-doc --disable-debug --prefix=$B  # baseline flags
make -j18  # baseline: exit 0, <5 min, Apple clang 21
./ffmpeg -hide_banner -codecs 2>/dev/null | grep -c ac4   # expect codec rows
```

Stacking detail (which 3 need rebase, stanza anchors, negative controls):
`ff-assembly-protocol/ASSEMBLY-PROTOCOL.md`; r2 validation record (17/17,
mak 119): `ff-combo-rerun/HEARTBEAT.md`.

Per-component standalone builds (same worktree, separate build dirs):
`--disable-everything --enable-decoder=ac4`, `--enable-encoder=ac4`,
`--enable-parser=ac4`, mp4-demux, mp4-mux, mpegts-mux configs. Full build
must stay zero-new-warnings throughout.

### Step 7 — Stack E on C-merge HEAD, then full FATE execution

7a. `ac4/E2-tsfate-r2@09a1bd2` = E1' (98486f6, lavf-only mpegtsenc.c)
as-is + E2' rebased per protocol Rules 2-3 (drop duplicate
tests/Makefile hunk, convert ac4.mak create to aggregate-anchored
append) onto the C post line (C-split-r2 tip d37a7b8); E1/C still
disjoint (E1 lavf-only, C lavc-only).
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
8b. No silence round-trip tests (V2 D9): md5/framecrc only.
8c. OWNER-RUN (suite access): suite files uploaded FIRST, never
overwritten: `make fate-rsync`, verify `ac4/clip-*` present, then the
FATE patch may post. Lanes never run this.
8d. OWNER-RUN (mail + clocks): B2 clock: owner samples mail (7 files,
REQUEST.txt) +24h confirm reply starts B2; D-P5 tiny-MP4 sample may
need a 2nd mail +24h — file it early so D never waits. Lanes never send.

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
COVER-C must describe the 9-commit -r2 shape (C1..C6+fold8), not the
splitplan 5 (dry-run 3f mismatch resolved by posting from -r2).

### Step 10 — Per-series ship checklist (post in this order)

Order: A0(1) -> A1 -> A2 -> (A3?) -> B1 -> B2 -> C(9) -> E1 -> E2-R -> D(5, D4-R/D5-R).

- A0: Step 2 done from `ac4/A0-submit@c22b0fe` (no split; fate folded);
  D12 pins; standalone demux build; posts first
  (rest of A/B assumes copy exit 0 + seekable pts).
- A1: Step 1a landed; lavc port of dec-lib; gates dec-lib review closure.
- A2: A1 merged; S4 drop policy; S5 wrapper gain policy; T2 gap-note API;
  T3 skip_samples/latency; standalone decoder build.
- A3 (conditional): Q2 priming measure; T3 1-frame delay suggests nonzero ->
  likely NEEDED, else fold into A2.
- B1: DOC-1 decoder-opts freeze from A2; DOC-3 changelog rebase;
  DOC-7 anchors re-verified; NO APIchanges entry.
- B2: Step 8d mail +24h (OWNER); F1-F7 FAILs closed; D5 A0-unblocked tests;
  T1/T4 pins; D9 md5/framecrc only; A1 advisory (wiring); landing per
  `## B2 landing addendum` (append+merge, never copy).
- C: Step 3 all-DONE on `ac4/C-encoder-split-r2@d37a7b8`; EL-B1/B2 + EL-M/m,
  C-B1/M1-M5/m1-m9 closed; C-M3 syms, C-M4 MSVC; H1 MANUAL-only confirm;
  D1 ENCODE_CB cover question; byte-parity gates re-run at C3.
- E: Step 7 done (E1' as-is + E2-R); C merged (E never posts before C);
  D6 VERIFIED on green.
- D: Step 4 done except 4d wiring (STILL OPEN); Step 1c contract DONE;
  D1 review; TOR-1/2/3 asserts in D4; 2nd samples mail (OWNER) if needed;
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
   1-150 (draft header, staging-only), 371-417 (parser staging
   block), or 422 (the staged file's own `fate-ac4:` line).
4. Land the B2 wiring with the other wiring lines, just above the
   aggregate: `# --- B2 wiring (decode/demux only; ENC terms
   re-added at C4) ---` + staged line 420 (`FATE_SAMPLES_AVCONV +=
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
