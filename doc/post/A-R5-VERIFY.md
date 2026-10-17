# A-R5-VERIFY — independent verify of FINAL Series-A re-cut 5 (e72a4b9)

Verdict: **POST-OK** — no gap. Owner may post per
`A-OWNER-SEND-STEPS.md`. Re-cut 5 supersedes re-cut 4 (r4 is STALE:
its 13 headers fail `fate-source`; upstream CI would fail on it).

Scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-A-r5-verify-retry`
(TMPDIR there). Source: fresh clone of the project repo @
`e72a4b9` (`ff/merged: runbook-r2`, contains `d1214d7` A-guard-r5),
branch `ff/doc-A-r5-verify` holds only this file. Parent repo
untouched, nothing pushed, no MBP/remote, nothing sent. All builds
out-of-tree (`ffbuild/`); the in-tree `make clean` survival probe
ran in a separate disposable clone (`cleansrc/`). Predecessor seat
`ff-A-r5-verify` died mid-run; zero of its artifacts are cited as
evidence here — every gate below re-ran fresh in this scratch.

Pack: `doc/post/series-A-decoder.mbox` 620888 B / 15226 lines
(2 patches vs `3e6cd5e`) + `doc/covers/COVER-A.txt` (`[PATCH 0/2]`,
N5 note LANDED, untouched this re-cut).

## Re-run gates (all green)

- `git am` mbox on pristine `3e6cd5e`: rc=0, ZERO warnings
  (`am.log` = 2 `Applying:` lines only). Commits `4e5118b` (1/2) +
  `bcf0995` (2/2); hashes differ from the re-cut seat's `038cb8f` /
  `43a0562` (committer metadata only) — post-am TREE hashes
  `fa7baa5` / `a739797` EXACT-match the re-cut seat's am trees; the
  shipped `From ` lines name the staging commits `1ef339d` /
  `8875e17` as the checklist claims.
- `git show --check`: 0/0 (both patches, empty-pretty form).
- File counts: 1/2 = 25 files (Makefile OBJS + 24 core incl.
  `ac4_src.h`, zero `*_tables` filenames); 2/2 = `ac4dec.c` +
  `allcodecs.c`. 27 `diff --git` headers total.
- Subjects 1:1: each of `[PATCH 1/2] avcodec: add native AC-4
  decoder core` / `[PATCH 2/2] avcodec: wire ff_ac4_decoder`
  appears exactly once in `COVER-A.txt` (lines 11, 19) and once
  (Subject:) in the mbox (lines 4, 14282).
- Zero placeholders: zero `[OWNER` / `[TODO` / FIXME / XXX /
  YOUR NAME in cover + mbox. Single `TBD` is mbox line 5467, the
  in-code 3ch honest-gap note, as disclosed.
- Signoffs: `Michael Rowlinson <mrowlinson@gmail.com>` on both
  patches (mbox lines 28, 14308); `From ` separators lines 1,
  14279; `From:` headers lines 2, 14280, match.
- ASCII audit: mbox ZERO + cover ZERO non-ASCII lines. Zero MIME
  headers → zero 8bit warnings expected.
- `_tables` string hits (6) are content-only: 2 in the 1/2 commit
  message (lines 15, 17, rename rationale) + 4 in-code `src/*`
  provenance comments (lines 279, 4400, 13696, 13746); zero
  `*_tables` filenames shipped.
- Blob identity vs merged `e72a4b9`: 24/24 core files
  `cmp`-identical am-tree vs `lavc-dec/core/`, plus `ac4dec.c`
  SAME vs `lavc-dec/ac4dec.c` (25/25).
- `format-patch` roundtrip from my am tree: byte-identical to the
  shipped mbox modulo the 2 `From ` hash lines (same 620888 B;
  15224/15226 content lines `cmp`-identical).
- r4→r5 delta = guard triples only: `diff r4mbox r5mbox` = 112
  `<>` lines = 2 `Date:` + blob `index` lines + `#ifndef` /
  `#define` / `#endif` guard lines; zero other delta lines (+383 B
  from the longer `AVCODEC_` names + `#endif` comments).
- `fate-source` GREEN (the re-cut's whole point):
  `tests/fate/source-check.sh` output on my am tree is
  `cmp`-identical to BOTH the pristine-`3e6cd5e` run and the pinned
  `tests/ref/fate/source` (zero ours listed). Prefix repro on a
  fresh r4 am tree lists all 13 (`ac4_src`, `ac4_stages`, `ac4d`,
  `acpl`, `aspx_noise`, `aspx_tab`, `entropy`, `huffman_tab`,
  `imdct`, `qmf`, `qwin_tab`, `sfb_offsets`, `snf_tab`) — fix
  proven necessary and sufficient. Upstream CI blocker closed.
- Full build (out-of-tree `configure --disable-doc`,
  `make ffmpeg`): rc=0; 34 warnings case-insensitive (29
  `warning:` + 5 summary lines), 0 in our files — shape matches
  the re-cut seat's log exactly; decoder row
  `A..X.D ac4 ATSC A/344 Dolby AC-4` registered; buildconf shows
  bare `--disable-doc`; no ac4 encoder row.
- Smoke 11/11: exact `gate11.sh` recipe (`-strict experimental
  -v info -i vec -map 0:a -f s16le`), every `vec/*.ac4` rc=0
  `cmp`-identical to BOTH the N5-current shim (dec-lib @
  `e72a4b9`, built fresh here) and the independent streaming
  driver (N5-current `sprobe.c`, all `ff-vs-shim=SAME`,
  `ff-vs-sprobe=SAME`), AND to the re-cut-4 bytes
  (`ff-vs-r4=SAME` all 11: unchanged-bytes proof).
- N5 pins live-confirmed: ff gpac log shows 48000 Hz / 1602 spf
  max / 6ch; shim `FMT sr=48000 ch=6 frames=298 note=pcm`;
  sprobe `sr=48000 ch=6 spf=1602`; gpac PCM 5727312 B, sha256
  `8b41f7cf…b39f6`, input sha `b82fabba…70ce1`: all three
  EXACT-match `fate-n5/pins.txt`.
- F2 gate (`lavc-dec/tests/f2-gap-trunc.sh` vs fresh binary):
  `== F2 GATE PASS ==` (rc=69 no-sync loud 0 B; stereo control
  rc=0).
- `make clean` survival (separate in-tree clone): 0 tracked dirty,
  0 deletions; 25/25 new files survive incl. 8/8 ours `*_tab`
  sources (13 `*_tab.*` on disk = 8 ours + 5 pre-existing).
- patcheck: 738 findings, normalized-identical (path + ANSI +
  input-name normalized) to the re-cut seat's log — same lines,
  same classes, no new class. Clean-under-triage stands.
- Send-shape: `mkdir -p send && git mailsplit -osend` → 0001+0002
  with correct subjects; `COVER-A.txt` has Subject + blank + body
  0000 shape.

## Drift check: dec-lib guards (NO PORT — deliberate, documented)

The guard fix touched `lavc-dec/core/*.h` only (13 headers, per
`cf0ca9d` stat). `dec-lib/src/*.h` keeps the old-style `AC4_*`
guards with bare `#endif` (12 headers inventoried). This seat
checked all three port triggers and all are negative:

- No in-tree consumer: `grep -c dec-lib/src` = 0 in ALL SEVEN
  shipped mboxes (A, A0, B-decfate, B-fate, C, D, E). `dec-lib/`
  never enters the ffmpeg tree by construction (vendor source +
  out-of-tree oracle only).
- `fate-source` scope is in-tree only: it runs
  `tests/fate/source-check.sh` inside the ffmpeg tree (derives the
  required macro from the in-tree path). Proven live: my r5 am
  tree run is ref-identical with dec-lib untouched.
- Divergence is the merged design, not drift: the re-cut-5
  checklist ("`dec-lib/` keeps its standalone `AC4_*` guards
  (correct out-of-tree; never enters the ffmpeg tree)") and
  `lavc-dec/README.md` port-delta + re-vendor rule + port
  equivalence (guard remap is a documented mechanical re-vendor
  step) all pin the split. Porting `AVCODEC_*` macros into the
  standalone lib would put ffmpeg-namespace guards out-of-tree and
  invalidate those merged statements, for zero gate movement.

So: no port, no rebuild owed by the port (build health is covered
by the fresh full-build + oracle-build gates above, all rc=0).

## Findings

None blocking. Checklist pins spot-checked (mbox size/lines,
subject/signoff/`From`/TBD line numbers, `_tables` split, N5
numbers, warning counts, tree hashes, `fatesource` prefix/postfix)
— all accurate. No new nits.

## Heartbeats

`HEARTBEAT.md` T+0..T+9 (10 entries). Logs: `am.log`,
`check1.log`, `check2.log`, `configure.log`, `make.log`,
`clean-configure.log`, `clean.log`, `fatesource-ref.log`,
`fatesource-r5.log`, `fatesource-r4.log`, `am-r4.log`,
`oracle-build.log`, `sprobe-build.log`, `gate11.log`,
`smoke/*.pcm|err`, `smokeref/*`, `f2work/`, `send/0001|0002`,
`patcheck.log`, `patcheck.norm`, `roundtrip.mbox`, `rt.diff`
(empty), `r4r5.diff`; vector guard (11 hashes, `8d0af04e`
aggregate, 3-way match with the r4 seat and predecessor inputs).
