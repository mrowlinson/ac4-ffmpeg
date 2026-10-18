# D-R2-VERIFY — D HOLD lift re-verify on fixed master mboxes

Seat `ff-D-reverify`. Branch `ff/doc-D-reverify` @ `fc5573a`
(debts 3+4+6 landed in `20bb917`). Own clone
`scratch/ff-D-reverify/wt`, out-of-tree build `scratch/ff-D-reverify/build`,
TMPDIR scratch, suite = 16-file mirror copy (read-only). Nothing pushed,
nothing sent. Venue: MBP `169.254.103.231:22` DOWN (first-hand) →
LOCAL-ONLY pure-FFmpeg; zero Dolby-binary invocation; Dolby-dependent
checks UNRUN (none in scope).

Verdict: **POST-OK** — MUST-VERIFY union-D HOLD (header-hunk drop + T3
GEN strict + 7 re-pins) is lifted. One fresh cosmetic nit (N1, cover
1 word, no mbox impact).

## Gate table

| Gate | Check | Result |
|---|---|---|
| (a) assembly | fresh `3e6cd5e` tree, plain `git am` A0→A→B→C→E→D | 6/6 rc=0, 23 commits (`rev-list --count 3e6cd5e..HEAD`), 0 am warnings/errors |
| (b) D1 toc-drop | D 1/6 drops `ac4_toc.h` create, keeps A0 header | 2 refs (msg + `#include`), 0 `new file` creates; tree header `cmp`-EQ A0's; A0-vs-D delta comment-only (2 hunks) |
| (b) T3 GEN strict | D 5/6 roundtrip GEN carries `-strict -2` | 1 line (`-c:a ac4 -strict -2 -ab 128k`); 0 bare `-c:a ac4` across all 6 mboxes |
| (b) 7 re-pins | byte-match MUST-verify shapes | 5/5 F-ONLY-EXACT vs B-tip (strip `, F=0x0` → identical); seek-eof 1-line→2-line (+2048/+4096, F-suffixed); seek/extra-ac4 695→348 clamp (9 lines); **7/7 `cmp`-MATCH vs `ff-union-B-A0/d-repin/`** oracle |
| (c) build | configure + `make -j18` | rc=0/rc=0, 34 warnings, **0 ac4** (matches R2 exactly) |
| (d) fate | `make fate-ac4 SAMPLES=suite` + A0D trio | **29/29 rc=0, 0 fail**; trio `ac4-demux/ac4-demux-51/seek-extra-ac4` **3/3 rc=0** (prior 4+3 reds GREEN via debt 6) |
| (e) T3 determinism | `fate-ac4-roundtrip` x2, GEN `cmp` | rc=0/rc=0, GEN byte-identical (md5 `86828e84ccecb4171d775806616e0e93`) |
| (f) statics | subjects/signoffs/ASCII/mailsplit | mbox 6/6 `[PATCH n/6]` sequential, signoffs 6/6, high-byte lines 0, mailsplit 6/6, conflict markers 0; cover↔mbox 5/6 exact + **N1** |

## Fresh nit

- N1 (cosmetic, pre-existing since union-D `50e7de5`, untouched by
  debts re-cut): `COVER-D.txt:42` lists 5/6 as `...mp4 mux/demux
  roundtrip`, mbox subject is `...roundtrip test` (drops ` test`).
  C-seat precedent is exact (C 07/10 cover↔mbox identical). Fix = 1
  word at send; no mbox/am/build/fate impact. Not a HOLD.

## Owner-at-post

- Suite +24h live clock EXPIRED (ended 2026-10-17T22:23:09Z; now
  2026-10-18T00:53Z) — live re-confirm owed before A0/B2 post.
- N1 cover word fold at send.

## Logs (`scratch/ff-D-reverify/logs/`)

`am-D.log`, `configure-D.log`, `make-D.log`, `fate-ac4-D.log` (29/29),
`fate-A0D-D.log` (3/3), `fate-T3-run1/2.log` + `rt1.ac4`, `frac/copy/
demux-B-tip.txt`, `HEARTBEAT.md`. Trees: `ffD` (D-tip assembly).
