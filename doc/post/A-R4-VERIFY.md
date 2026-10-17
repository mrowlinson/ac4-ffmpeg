# A-R4-VERIFY — independent verify of FINAL Series-A re-cut 4 (2018c89)

Verdict: **POST-OK** — no gap. Owner may post per
`A-OWNER-SEND-STEPS.md`. The re-cut-3 mailsplit nit (VERIFY-A-R3 T+7)
is already fixed in the r4 send-steps line (`-osend` attached form).

Scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-A-r4-verify`
(TMPDIR there). Source: read-only clone of the project repo @
`2018c89` (`ff/merged: A-r4`), branch `ff/doc-A-r4-verify` holds only
this file. Parent repo untouched (still on `ff/dec-lib`, clean),
nothing pushed, no MBP/remote, nothing sent. All builds out-of-tree
(`ffbuild/`); the in-tree `make clean` survival probe ran in a
separate disposable clone (`cleansrc/`).

Pack: `doc/post/series-A-decoder.mbox` 620505 B / 15226 lines
(2 patches vs `3e6cd5e`) + `doc/covers/COVER-A.txt` (`[PATCH 0/2]`,
N5 note LANDED).

## Re-run gates (all green)

- `git am` mbox on pristine `3e6cd5e`: rc=0, ZERO warnings
  (`am.log` = 2 `Applying:` lines only). Commits `f733e76` (1/2) +
  `b9095a0` (2/2); hashes differ from the re-cut seat's `6f026cb` /
  `f6a620e` (committer metadata only) — post-am TREE hashes
  `ac4539c` / `19b7b82`; the shipped `From ` lines name the staging
  commits `7955a1e` / `6144274` as the re-cut record claims.
- `git show --check`: 0/0 (both patches, empty-pretty form).
- File counts: 1/2 = 25 files (Makefile OBJS + 24 core incl. new
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
  14279; `From:` headers match.
- ASCII audit: mbox ZERO + cover ZERO non-ASCII lines (the r3
  carried em-dash is gone; the 2 cover em-dashes transliterated).
  Zero MIME headers → zero 8bit warnings expected.
- `_tables` string hits (6) are content-only: 2 in the 1/2 commit
  message (lines 15, 17, rename rationale) + 4 in-code provenance
  comments; zero `*_tables` filenames shipped.
- Blob identity vs merged `2018c89`: 24/24 core files
  `cmp`-identical am-tree vs `lavc-dec/core/`, plus `ac4dec.c`
  SAME vs `lavc-dec/ac4dec.c`. Makefile hunk byte-identical
  r3→r4 (patch-1 inert carried by mechanism-identity).
- `format-patch` roundtrip from my am tree: byte-identical to the
  shipped mbox modulo the 2 `From ` hash lines (15224/15226).
- Full build (out-of-tree `configure --disable-doc`,
  `make ffmpeg`): rc=0; 34 warnings case-insensitive (29
  `warning:` + 5 summary lines), 0 in our files — shape matches
  the re-cut seat's log exactly; decoder row
  `A..X.D ac4 ATSC A/344 Dolby AC-4` registered; buildconf shows
  bare `--disable-doc`; no ac4 encoder row.
- Smoke 11/11: exact `gate11.sh` recipe (`-strict experimental
  -v info -i vec -map 0:a -f s16le`), every `vec/*.ac4` rc=0
  `cmp`-identical to BOTH the N5-current shim (dec-lib @
  `2018c89`, built fresh here) and the independent streaming
  driver (all `ff-vs-shim=SAME`, `ff-vs-sprobe=SAME`).
- N5 pins live-confirmed: ff gpac log shows 48000 Hz / 1602 spf
  max / 6ch; gpac PCM 5727312 B, sha256
  `8b41f7cf…b39f6`, input sha `b82fabba…70ce1`: all three
  EXACT-match `fate-n5/pins.txt`. Shim FMT + sprobe info agree
  (sr=48000 ch=6 frames/fed=298).
- F2 gate (`lavc-dec/tests/f2-gap-trunc.sh` vs fresh binary):
  `== F2 GATE PASS ==` (rc=69 no-sync loud 0 B; stereo control
  rc=0).
- `make clean` survival (separate in-tree clone): 0 tracked dirty,
  0 deletions; 25/25 new files survive incl. 8/8 ours `*_tab`
  sources (13 `*_tab.*` on disk = 8 ours + 5 pre-existing).
- patcheck: 738 `FILE:` hits, findings byte-identical
  (path-normalized) to the re-cut seat's log — same classes, no
  new class. Clean-under-triage stands.
- Send-shape: `git mailsplit -o send/` → 0001+0002 with correct
  subjects; `COVER-A.txt` has Subject + blank + body 0000 shape.

## Findings

None blocking. Checklist pins spot-checked (mbox size/lines,
subject/signoff/TBD line numbers, N5 numbers, counts) — all
accurate. No new nits.

## Heartbeats

`HEARTBEAT.md` T+0..T+9 (10 entries). Logs: `am.log`,
`configure.log`, `make.log`, `clean-configure.log`, `clean.log`,
`oracle-build.log`, `sprobe-build.log`, `smoke/*.pcm|err`,
`smokeref/*`, `f2work/`, `send/0001|0002`, `patcheck.log`,
`roundtrip.mbox`, `mk-r3.txt`, `mk-r4.txt`.
