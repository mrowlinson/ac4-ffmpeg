# ED-STILL-CLEAN — Series E (TS-mux) + D (MP4 demux) postable after N5 re-vendor

Branch: `ff/doc-ED-still-clean` from master `240dad8` (post-N5fix+repin+revendor).
Gated 2026-10-16 in scratch `ff-ED-still-clean` (TMPDIR there);
parent repo untouched, nothing pushed, local-only, nothing sent.

## Question

E (`series-E-ts.mbox`, cut pre-revendor from `ac4/E-ts-r3@a2485a9`) and D
(`series-D-mp4.mbox`, cut pre-revendor from `ac4/D-split-r3@08b4f00`)
embed no decoder/encoder core files, so the N5 re-vendor should not disturb
them. Prove both packs still post as-is, against the newest A/C bytes when
final, else current.

## Base selection

- [x] A-r4 (`ff-A-post-r4-recut`) NOT final — no DONE/HO/branch (staging T+4).
- [x] C-r2 (`ff-C-post-r2-recut`) NOT final — no DONE/HO/HEARTBEAT/branch
  (`C-new.mbox` uncommitted work-in-progress).
- [x] Base = current master bytes: `series-C-encoder.mbox` (11) on pristine
  `3e6cd5e`, then E (2), then D (6). Committed E/D mboxes md5-match the
  E-post/D-post seat originals (`515b90df…` / `59edccf8…`).

## No-drift (no re-cut needed)

- [x] Fresh `format-patch 1ccd10a..a2485a9` concatenated == committed
  `series-E-ts.mbox` byte-identical (`cmp` clean, 11605 B).
- [x] Fresh `format-patch a2485a9..08b4f00` concatenated == committed
  `series-D-mp4.mbox` byte-identical (`cmp` clean, 128977 B).
- [x] Source tips unmoved: `ac4/E-ts-r3=a2485a9`,
  `ac4/D-split-r3=08b4f00` in `ff-submit-fix/ffmpeg`.
- [x] E/D embed zero core files — E: `libavformat/mpegtsenc.c` + 3 fate
  files; D: `configure`, `libavcodec/Makefile`, `parsers.c`, new
  `ac4_parser.c`, `mov.c`, `movenc.c`, `tests/api/*` x3 + `ac4.mak`.
  No `libavcodec/ac4*` core files (sole lavc addition is D's own parser).

## `git am` matrix (this wave, all as designed)

- [x] E 1/2 alone on pristine rc=0 (`04a1b73`); E 2/2 alone fails rc=1
  (`tests/fate/ac4.mak: No such file` — posts-after-C confined, as gated).
- [x] D 1-3/6 alone on pristine rc=0 (`e1c5e45`); D 4/6 alone rc=1
  (missing ac4.mak), 5-6/6 rc=1 (`tests/api/Makefile` context) — as gated.
- [x] C(11)+E(2)+D(6) sequential on pristine rc=0, 19/19
  (`97b0a32` / `4485bf4` / `5c2d735`).
- [x] D 4/6 after C-alone fails at `ac4.mak:93` (E-before-D REQUIRED, as gated).
- [x] Applied E diff == source `1ccd10a..a2485a9` diff byte-identical;
  applied D diff == source `a2485a9..08b4f00` diff byte-identical.
- [x] Applied tip tree == `08b4f00` minus exactly A0's 6 files
  (735 insertions, 0 deletions — matches D-post record to the file).
- [x] `git diff --check` zero over the E+D range.
- [x] A-on-pristine rc=0 (`ecd44d0`), A0-on-pristine rc=0 (`4ea7123`)
  recorded. A+C naive stack fails at C 1/11 `libavcodec/Makefile:219`
  (pre-existing post-order property — C rebases onto landed A at post
  time per decoder-first runbook; E/D stack onto C only, unaffected).

## Build + fate (this wave, out-of-tree `ffbuild/`, plain `./configure`)

- [x] `configure` rc=0, `make -j18` rc=0, zero ac4 warnings.
- [x] E/D fate 5/5 rc=0: `ac4-mpegts-pmt`, `ac4-mpegts-probe`,
  `ac4-parser`, `ac4-mp4-roundtrip`, `ac4-ts-demux` (zero fail lines).
- [x] Aggregate `fate-ac4` 10/10 rc=0 (matches submit-fix carried 10/10).

## Carried (bytes identical, still stand)

- E-post + D-post preflight gates: subjects 1:1, signoffs 2/2 + 6/6,
  ASCII-only, zero placeholders, cover no-drift, nothing sent — all carry
  verbatim since both mboxes are byte-identical to the gated packs.
- Post order unchanged: E posts only after C merges; D posts last, after
  C AND E merge. A-r4/C-r2 landing does not invalidate this record for
  E/D application (zero core/file overlap), but the base-selection note
  above should be re-checked if C-r2 changes `ac4.mak` or `api/Makefile`.

## Verdict

**STILL-CLEAN** — Series E + D post as-is on current bytes; no re-cut.
Re-verify against A-r4/C-r2 only if their final bytes touch `ac4.mak`,
`api/Makefile`, `mpegtsenc.c`, `mov.c`, or `movenc.c`.
