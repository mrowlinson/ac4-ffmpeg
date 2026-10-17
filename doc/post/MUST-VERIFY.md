# MUST-VERIFY — independent re-verify of the 4 MUST branches on master bytes

Seat `ff-must-verify-wave`. Base master `2def950` (first-hand; own clone
`scratch/ff-must-verify-wave/wt`, branch `ff/doc-must-verify` holds only
this file). Parent repo untouched, nothing pushed, nothing sent. All builds
out-of-tree, TMPDIR in scratch. Suite = local mirror copy (16 files under
`suite/ac4/`); live rsync not re-run (B-PREFLIGHT proved live 12/12;
+24h clock ends 2026-10-17T22:23:09Z, still pending at gate time).

Mbox bytes at master: A 507373, A0 43263, B 40708, C 650974, E 12693,
D 162643 — all match SEND-SITTING expectations exactly.

## Verdicts

| Branch | Commit | Verdict |
|---|---|---|
| A-r6 | `554b92b` | **POST-OK** |
| union-B-A0 | `36b9eec` | **POST-OK** |
| union-C | `91c214b` | **HOLD** (6-line context re-cut; content green) |
| union-D | `50e7de5` | **HOLD** (own re-cut: header-hunk drop + T3 GEN strict + 7 re-pins) |

## A-r6: POST-OK (am/build/smoke/N5/fate-source, all green)

- `git am` on pristine `3e6cd5e`: rc=0, ZERO warnings (2 `Applying:`
  lines). Trees `b7f3d49`/`10ac64b` EXACT-match preflight staging trees
  (commits differ, committer metadata only). `git show --check` 0/0.
- Static: 507373 B/12844 lines, subjects 1:1 cover↔mbox, 0 placeholders,
  single TBD at line 4820 (disclosed 3ch note), ASCII 0/0, `_tables` x2
  message-only, From/Signoff lines exact (1,2,32,11929,11930,11964),
  26 diffs (24+2, `ac4_stages.h` gone).
- MUST-byte audit: 0 `getenv`/`fprintf`/`fopen`/`AC4DEC_`; 0
  `duet`/`tools/`/`spec/`/`ANSWER-`/`WEB-e`/`tmp/wave`/`census.c`/
  `lavc-dec/README`; wrapper `drc` gone (lib-internal fields only);
  0 `signal.h`/`unistd.h`/`_POSIX_C_SOURCE`; CRC disclosure present.
- `fate-source` GREEN: 35-line output identical to BOTH
  `tests/ref/fate/source` and pristine run (zero ours).
- Build (out-of-tree `--disable-doc`, `make ffmpeg`): rc=0, 34 warnings,
  0 in our files; decoder row `A..X.D ac4 ATSC A/344 Dolby AC-4`; no
  encoder row; buildconf bare `--disable-doc`.
- Smoke 11/11: exact `gate11.sh` recipe, every `vec/*.ac4` rc=0
  `cmp`-identical to shim (dec-lib @ master, fresh build), sprobe
  (`-DAC4D_LIB_ONLY` rebuild), AND r5 bytes (unchanged-bytes proof).
- N5 live: ff log 48000 Hz/6ch/1602 spf max; gpac PCM 5727312 B sha256
  `8b41f7cf1239a1a4a256a5e6a5ae7a935763d1d0b251e998151400126c6b39f6`
  EXACT vs `fate-n5/pins.txt`; input sha matches too.
- Roundtrip: am-tree `format-patch` byte-identical (507373 B,
  From-stripped `cmp`-identical). M6: `-pres_sel` only, `-drc 1`
  rejected. F2: `== F2 GATE PASS ==`.

## union-B-A0: POST-OK (fate 16/16 + A0 3/3 + M21 sanity)

- Stack A0+A+B: 5/5 rc=0 `--whitespace=error` on pristine. B-tip defines
  16 `fate-ac4` (2+9+5; encneg correctly absent pre-C).
- Build B-tip (`--samples=suite`): rc=0, 34/0. `fate-ac4` 16/16 rc=0 +
  deterministic rerun 16/16. A0 3/3 rc=0 (`ac4-demux`, `ac4-demux-51`,
  `seek-extra-ac4`).
- M21 FIX sane first-hand: A0 creates `libavcodec/ac4_toc.h` (678 lines)
  and demuxer includes + walks it (no private fps table — rows live only
  in header); `need_parsing=HEADERS` set + documented (parser clears KEY
  on non-iframes). Pre-D PARSE_NONE degrade proven by green 16/16+3/3.
- A0-vs-D header delta is comment-only (8 lines: A0 says shared reader,
  D says parser-now/demuxer-later). D-side create-hunk collision rides
  the D verdict, not this one.

## union-C: HOLD (content green; 6-line re-cut owed)

- Static green: 650974 B/10 mails, full subjects 1:1 x10, 10/10 signoffs,
  ASCII 0/0, 0 placeholders. Goldens full-hash in 05/10
  (`86828e84ccecb4171d775806616e0e93`, `53a44acaa21853f91cfef7dbdcccb67e`,
  match r5) + cover extradata (`0xb7a30a0b`, `0xbafa0ae9`).
- M10–M18 residue: zero `*_tables.*`, zero raw allocator calls (sole hit
  is comment text), zero TLS/mutex/`REORDERED_OPAQUE`, `sincos` inside
  `#if HAVE_SINCOS` with sin/cos fallback, EXPERIMENTAL cap + 13
  `-strict -2` CMDs, single texi `@tab IX @tab IX` row, no version bump.
- SEAM (first-hand): C 01/10 fails at `Makefile:219` on A-r6 tree — 3
  hunks quote r5's 2-line decoder comment, A-r6 reworded it (predicted in
  A-PREFLIGHT, C-seat-owned). FIXED mbox (6 context lines refreshed, no
  content change) → A0+A+B+C 15/15 rc=0 `--whitespace=error`.
- Content on fixed assembly: build rc=0 34/0/ZERO dupsyms; `nm`
  `ac4_huffman_tab.o` (110) ∩ `huffman_tab.o` (110) = EMPTY; `DEA.L. ac4`;
  encoder fate 5/5 rc=0 (pinned md5s, no re-pins).
- Full `fate-ac4` at C-tip: 17 pass, then `ac4-enc-neg71` FAILS first-hand
  on experimental refusal — B 2/2 CMDs lack `-strict -2` (predicted B
  forward-fix; assembly debt, recorded below, not a B-A0 HOLD: B-tip
  gates are green and encneg activates only post-C).

## union-D: HOLD (D fate + T3 green on fixed assembly; own re-cut owed)

- Static green: 162643 B/6 patches, 6/6 signoffs, ASCII 0/0,
  0 placeholders, subjects 1:1 x6. M19 lines after E's; M20
  `mov_ac4_is_multiframe` + EINVAL + negative test; M21 parser
  `#include "ac4_toc.h"` + `ff_ac4_toc_*` calls (no second walk); T3
  `FATE_AC4_RT` + `fate-ac4-roundtrip` + 150-line ref, first frame
  dts/pts 4096 pinned.
- SEAMS (all first-hand):
  1. E-as-master 2/2 fails at `ac4.mak:218` (1 context line: union-C tail
     CMD carries `-strict -2`; predicted). E GEN also lacks `-strict -2`
     (M11). FIXED E (2 lines) → 2/2 rc=0. (E-seat debt.)
  2. D-as-master 1/6 fails: `libavcodec/ac4_toc.h: already exists`
     (predicted UNION-D parent seam, unresolved). FIXED D drops the
     684-line create block (keeps A0's header; delta comment-only).
     (D-seat debt.)
  3. T3 GEN lacks `-strict -2` (M11; UNION-D gated pre-M11). 1-line fix
     in D 5/6. (D-seat debt.)
  4. B enc-neg CMDs lack `-strict -2` (proven at C-tip). 2-line
     assembly commit in scratch (parent folds into B re-cut).
- D-tip (fixed assembly, 23 commits): build rc=0 34/0/0dupsym.
  `fate-ac4`: 25/29 green incl D 4/4 (parser, mp4-roundtrip, ts-demux,
  roundtrip) + parse/mp4/ts/rt aliases + E pair + enc-neg pair; T3
  deterministic rerun green.
- 4 reds = EXACTLY the predicted post-D demux flips, shapes confirmed
  parser-correct: copy-stereo/51 last-line `F=0x0` suffix; frac-copy
  F-only suffixes (pts/dur/size/crc identical); seek-eof 1-line → 2-line
  (+2048/+4096, F-suffixed). A0 3/3 red at D-tip likewise: demux pair
  F-suffix, seek-extra 695→348 clamp.
- Missing owner: STACK-DAG assigns these 7 re-pins to the D seat;
  union-D shipped zero-churn (valid only without B-A0's `need_parsing`
  A0). At master they are unpinned → D re-cut must carry them (exact
  bytes observed in this seat's `fate-D-k.log` + `fate-A0-D.log`).

## Assembly delta vs master bytes (all mechanical, first-hand proven)

| # | Owner | Fix | Size |
|---|---|---|---|
| 1 | C | refresh r5→r6 Makefile comment context (C01/C02/C03) | 6 lines, context-only |
| 2 | E | tail-CMD context + GEN `-strict -2` | 2 lines |
| 3 | D | drop D 1/6 `ac4_toc.h` create block (keep A0's) | -684 lines |
| 4 | D | T3 GEN `-strict -2` | 1 line |
| 5 | B | enc-neg CMDs `-strict -2` | 2 lines |
| 6 | D | post-D re-pins (B x4 + A0 x3 refs) | 7 ref files |

## Owner-at-post

- Suite +24h clock ends 2026-10-17T22:23:09Z — confirm before A0/B2 post.
- Nothing sent, nothing pushed; no SENT markers in repo.
- C/D re-cuts: content verified here; re-cut seats preserve these bytes
  except the listed assembly deltas, then re-run the red gates.

## Heartbeats / logs

`HEARTBEAT.md` T+0..T+8. Logs: `am-A.log`, `am-A0.log`, `am-A-onA0.log`,
`am-B.log`, `am-C-*.log`, `am-C-fixed.log`, `am-E*.log`, `am-D-*.log`,
`fatesource-A.log` (+`-ref`), `configure-{A,B,C,D}.log`,
`make-{A,B,C,D}.log`, `gate11.log`, `smoke/*`, `smokeref/*`, `f2.log`,
`fate-B.log` (+`-rerun`), `fate-A0.log`, `fate-Cenc.log`, `fate-C.log`,
`fate-D.log`, `fate-D-k.log`, `fate-A0-D.log`, `roundtrip-A.mbox`,
`rt-a.txt`, `rt-b.txt`, `nm-a.txt`, `nm-c.txt`, `series-{C,E,D}.FIXED.mbox`.
Trees: `ffbase` (pristine), `ffsrcA` (A), `ffA0` (A0+A+B),
`ffsrcC` (full assembly + enc-neg commit).
