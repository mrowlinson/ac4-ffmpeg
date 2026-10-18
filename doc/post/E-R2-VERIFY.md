# E-R2-VERIFY — HOLD lift re-verify on MUST-final fixed mboxes (master `fc5573a`)

Verdict: **POST-OK** — Series E as fixed posts. Every E-VERIFY re-cut
spec item (§1–5) landed in the MUST-final bytes (debt 2, `20bb917`),
and every gate below is GREEN first-hand on the fixed stack. No new
HOLD, no fresh debts. The E HOLD from E-VERIFY (master `2def950`) is
lifted.

Seat: `ff-E-reverify`. Scratch:
`/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-E-reverify` (own clones only:
`proj/` = project repo @ `fc5573a`, branch `ff/doc-E-reverify`;
`ffsrc/` = fresh ffmpeg clone reset to pristine `3e6cd5e`; `suite/` =
16-file 464K ac4-sample copy; `mbox/` = fixed A0/A/B/C/E exports;
build out-of-tree `/Volumes/NVME/ac4-ffmpeg-ws/build/ff-E-reverify`;
TMPDIR in scratch). Fully local: MBP-TB
`169.254.103.231` unreachable this round (ssh rc=255 timeout, logged
in `HEARTBEAT.md`), so venue is LOCAL-ONLY pure-FFmpeg, zero Dolby
binaries run; Dolby-dependent checks UNRUN (none in E scope).
`10.0.2.163` never touched. Nothing pushed, nothing sent, merges
parent-only.

Source mboxes (fixed, from master `fc5573a`):
A0 43263 / A 507373 / B 40729 / C 650794 / E 12774 B
(1/2/2/10/2 patches; D not in this verify).
E-tip after plain stack: `107d2ad`.

## Gate table

| Gate | Spec | Result |
|---|---|---|
| (a) E 2/2 plain-am on fixed C-tip | tail-CMD context + GEN `-strict -2` apply clean | GREEN — full stack 17/17 rc=0, zero hand |
| (b) E fate 2/2 + deterministic rerun | pmt + probe pass, re-pass after GEN delete | GREEN — 2/2 rc=0, rerun 2/2 rc=0 |
| (c) M22 paragraph still TRUE | code cites + empirical on fixed bytes | GREEN — cites 1:1, verbose GEN exactly-1 warn |
| (d) Changelog stacking exact | C line, E directly after, B at end | GREEN — L5/L6/L24 1:1 |
| (e) full fate-ac4, encneg pair GREEN | B debt-5 fix flips the 2 reds | GREEN — 25 TEST rc=0, zero red |
| (f) statics 1:1 + send-steps pin | subjects/signoffs/ASCII/MIME; pin check | GREEN statics; pin STALE → refreshed in-commit |

## (a) Stack assembly on pristine `3e6cd5e` (plain `am`, zero hand)

- A0 1/1 + A 2/2 + B 2/2 plain `--whitespace=error`: rc=0 each
  (`am-A0.log`, `am-A.log`, `am-B.log`).
- C 10/10 plain rc=0 (`am-C.log`) — the C01–C03 Makefile 2-line
  context refresh (debt 1) landed; no `csplit` hand-fix needed.
- E 1/2 + 2/2 plain rc=0 (`am-E.log`) — the debt-2 forward-fix is in
  the shipped bytes: tail-CMD context carries `-strict -2` (M11
  union tail) and the GEN rule encodes `-c:a ac4 -strict -2`.
  E-tip `107d2ad` (`a620bf2` E1, `107d2ad` E2).
- Total 17/17 rc=0. Full-stack plain-am on master bytes (incl D,
  23/23) already proven by `ASSEMBLY-DEBTS-R2.md`; not re-run here.

## (b) Build + E fate (E-tip `107d2ad`, out-of-tree)

- `configure --samples=$SCRATCH/suite --disable-doc --disable-debug
  --disable-autodetect` rc=0 (`configure-E.log`); `make -j16` rc=0
  (`make-E.log`). 8 warning lines, all upstream shape (nspdec
  fallthrough + note + summary, 4× adpcm unused + summary),
  **0 ac4**, 0 errors (3 "error" greps are object filenames only).
  Codec registers `DEA.L. ac4`. No in-tree configure residue.
- E fate **2/2 PASS** (`fate-E2.log`): `ac4-mpegts-pmt` (PMT hex ==
  REF `02b01d...`) + `ac4-mpegts-probe` (codec_name=ac4,
  codec_tag_string=AC-4). Deterministic rerun after deleting
  `tests/data/fate/ac4-mpegts.ts`: 2/2 rc=0 (`fate-E2-rerun.log`).
- Probe `-strict -2` load-bearing confirmed both directions:
  with → rc=0 + ref bytes; without → rc=1 with the experimental
  decoder refusal naming `-strict -2` (`probe-strict.log`,
  `probe-nostrict.log`, refusal text in run log).

## (c) M22 paragraph: TRUE on fixed bytes (code + empirical)

Code cites re-proved on the E-tip tree, all 1:1 with E-VERIFY:
- `libavformat/mpegtsenc.c:1083`
  (`av_get_audio_frame_duration2(pcr_st->codecpar, 0)`).
- `libavcodec/utils.c:804` (`if (frame_size > 1 && frame_bytes)`).
- Zero `AV_CODEC_ID_AC4` hits in all of `libavcodec/utils.c` — no
  switch case anywhere; returns 0 → muxer warns once + uses 512.
- Encoder sets 2048: `ac4enc.c:262`
  (`avctx->frame_size = AC4ENC_FRAME_SAMPLES`),
  `ac4enc_lib.h:100` (`#define AC4ENC_FRAME_SAMPLES 2048`),
  `codec_par.c:179` (`par->frame_size = codec->frame_size`).
- Empirical (`m22-verbose.log`): GEN command at default loglevel →
  rc=0 with exactly 1× `[mpegts] frame size not set`; fate GEN at
  loglevel error silent (0 in `fate-E2.log`). Old blame still
  false, new cause still true.

## (d) Changelog stacking exact

At E-tip: L5 `- native AC-4 audio encoder` (C),
L6 `- MPEG-TS muxer: AC-4 descriptors` (E, directly after C),
L24 `- native AC-4 audio decoder` (B, list end). 1:1 with E-VERIFY
M19 claim.

## (e) Full `fate-ac4` at E-tip (`-k`): 25/25 GREEN

`make -k fate-ac4` rc=0, 25 TEST lines, zero fail/error
(`fate-Etip-k.log`). The E-VERIFY 2 reds (`ac4-enc-neg71`,
`ac4-enc-neg514`, rc=88 experimental refusal on B-as-merged) are
GREEN via the B debt-5 fix (both CMDs now carry `-strict -2`,
verified in the fixed B mbox L287/L292). E's 2 tests green inside
the run. E-VERIFY shape (25 TESTs) reproduced exactly, reds gone.

## (f) Statics 1:1 + send-steps pin

Fixed E mbox vs E-VERIFY record, all 1:1:
- Subjects: `[PATCH 1/2] lavf/mpegtsenc: write AC-4 descriptors`
  mbox L4 ↔ cover L12; `[PATCH 2/2] fate: ac4 ts mux tests` mbox
  L94 ↔ cover L20. `COVER-E.txt` untouched since `3b65f39`
  (pre-dates the `f827e56` gate; no cover change through
  `fc5573a` — the send-steps "untouched since" claim still TRUE).
- Signoffs 2/2 + `From:` 2/2 owner, `From ` seps L1/L91, dates
  ordered (03:13 → 03:14). Files: 1/2 = 2 files +27; 2/2 =
  3 files +152/-1. `git log --check` on the E range clean.
- Zero placeholders, zero non-ASCII bytes (mbox + cover), zero
  MIME headers → zero `send-email` 8bit warnings expected.
- Pin check: `E-OWNER-SEND-STEPS.md` STALE — Pack line still
  12693 B (fixed: 12774 B) and the source pin `ac4/E-ts-r3@a2485a9`
  is dead (object absent from the ffmpeg main repo, verified
  first-hand; no `E-ts-r3` branch). Refreshed in this commit:
  Pack 12774 B, MUST-final debt-2 source note, re-gate master
  `fc5573a` + this verify doc. (B/C/D send-steps Pack lines are
  likewise pre-fix — their seats' scope, noted for the parent.)

## Re-cut spec §1–5 closure (E-VERIFY HOLD lift)

Debt-2 delta pre→fixed E mbox (`E-debt2.diff`, 26 lines, first-hand;
pre-fix pulled from `20bb917^`):
1. §1 tail-CMD context `-strict -2` — IN (mak context line).
2. §2 GEN rule `-strict -2` — IN (GEN encode line).
3. §3 message clause `passes -strict -2 (the native encoder is
   EXPERIMENTAL)` — IN (L110); M22 paragraph otherwise verbatim;
   probe rationale kept; subjects unchanged; E 1/2 untouched
   (only `From` hash churn + mak index/hunk-header drift `@@
   -218→-222`, all mechanical).
4. §4 send-steps refresh + E 2/2 + full `fate-ac4` re-verify —
   DONE in this commit/run.
5. §5 not-E's — C01–C03 ctx + B encneg ×2 both landed (proven by
   plain 17/17 am + 25/25 fate here, 23/23 full-stack in
   `ASSEMBLY-DEBTS-R2.md`).

RED PROTOCOL: nothing red outside predicted shapes — nothing red
at all. No parked gates, no fresh debts.

## Evidence map (all in `scratch/ff-E-reverify/`)

`am-{A0,A,B,C,E}.log` (17/17 plain), `E-prefix.mbox` +
`E-debt2.diff` (debt-2 delta), `configure-E.log`, `make-E.log`,
`fate-E2.log`, `fate-E2-rerun.log`, `fate-Etip-k.log`,
`m22-verbose.log`, `m22-out.ts`, `probe-strict.log`,
`probe-nostrict.log`, `HEARTBEAT.md`, `HO-E-REVERIFY.md`.
Build tree: `/Volumes/NVME/ac4-ffmpeg-ws/build/ff-E-reverify`
(out-of-tree, kept for parent spot-checks).
