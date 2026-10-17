# E-VERIFY — independent verify of Series-E stack re-cut (master `2def950`)

Verdict: **HOLD** — E-as-merged cannot post: E 2/2 neither applies
to nor passes on current master C (union M11 made the encoder
EXPERIMENTAL). Needs a mechanical E re-cut carrying the 2-line
forward-fix spec'd below (independently derived here; shape matches
the union-C HO). Every E-content gate is GREEN: M22 message true,
Changelog stacking exact, E fate 2/2 green + deterministic,
subjects/signoffs/ASCII clean. Nothing in this HOLD faults E's own
bytes beyond the 2-line M11 catch-up.

Seat: `ff-E-verify`. Scratch:
`/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-E-verify` (own clones only:
`proj/` = project repo @ `2def950`, branch `ff/doc-E-verify` holds
only this file; `ffsrc/` = fresh ffmpeg clone reset to pristine
`3e6cd5e`; `suite/` = 464K ac4-sample copy; `build/` out-of-tree;
TMPDIR in scratch). Fully local (no TB peer, nothing pushed, nothing
sent, merges parent-only). Source mboxes exported from master:
A0 43263 / A 507373 / B 40708 / C 650974 / E 12693 B
(1/2/2/10/2 patches; D not in this verify).

## Stack assembly on pristine `3e6cd5e` (DAG order, `am-E.log`)

- A0 + A(r6) + B plain `--whitespace=error`: 5/5 rc=0 (`am-A0AB.log`).
- C 01/10–03/10 Makefile hunks quote the r5 decoder comment; A r6
  reworded those 2 lines → plain am FAILS at C01 (`am-E.log` first
  run; `-3` also fails, no common blob). Known + disclosed (A-r6
  preflight stack-span; C-seat-owned). Resolved mechanically in
  scratch copies only (`csplit/0001..0003`, 2 context lines each,
  `.orig` diffs kept): C 10/10 rc=0 (`am-C.log`). Applied region
  verified: encoder block before AC3_MF, r6 decoder block intact.
- E 1/2 plain on C-tip: rc=0 (Changelog anchor after C's line held).
- E 2/2 shipped: FAILS on union-C `tests/fate/ac4.mak` — the tail
  CMD context now carries `-strict -2` (M11). Reproduces the union-C
  checklist disclosure ("E-as-merged cannot apply to any M11 C").
  Forward-fix, independently derived (2 lines in `esplit/0002`,
  `.orig` diff kept): tail-CMD context touch + GEN rule
  `-c:a ac4 -strict -2 -ab 128k ...` (the GEN encodes through the
  EXPERIMENTAL encoder). E 2/2 rc=0. E-tip `9191f1a`.
- Confinement on pristine (`ffprist/` worktree): E 1/2 alone fails
  ONLY `Changelog:3` (mpegtsenc-only `apply --check` rc=0); E 2/2
  alone fails ONLY `tests/fate/ac4.mak: No such file` (rest rc=0).
  Both C-dependencies real and single-file-confined.

## Mechanical (shipped E bytes, all green)

- Subjects 1:1: `[PATCH 1/2] lavf/mpegtsenc: write AC-4 descriptors`
  cover L12 ↔ mbox Subject L4; `[PATCH 2/2] fate: ac4 ts mux tests`
  cover L20 ↔ mbox Subject L94. COVER-E states the DAG chain + stack
  parent C (L4–L5).
- Signoffs 2/2 + `From:` 2/2 owner, `From ` separators L1/L91,
  dates ordered (03:13 → 03:14).
- Files: 1/2 = Changelog + mpegtsenc.c (2 files, +27); 2/2 = .sh +
  mak + ref (3 files, +152/-1). `git show --check` clean; zero added
  trailing-whitespace lines.
- Zero placeholders (`[OWNER`/`[TODO`/FIXME/XXX/YOUR NAME) in mbox +
  cover; zero non-ASCII bytes in both (perl count 0/0); zero MIME
  headers → zero `send-email` 8bit warnings expected.
- Changelog at E-tip: L5 `- native AC-4 audio encoder` (C),
  L6 `- MPEG-TS muxer: AC-4 descriptors` (E, directly after C),
  L24 decoder line (B, list end). M19 E-part stacking exact.

## M22 message: TRUE (code + empirical)

Every clause of the rewritten 2/2 paragraph verified:
- `libavformat/mpegtsenc.c:1083` (`enable_pcr_generation_for_stream`)
  calls `av_get_audio_frame_duration2(pcr_st->codecpar, 0)`.
- `libavcodec/utils.c:804`: `if (frame_size > 1 && frame_bytes)` —
  with frame_bytes=0 the frame_size fallback cannot fire.
- `get_audio_frame_duration` (utils.c:580–815) has NO `AV_CODEC_ID_AC4`
  case in any switch (fixed-duration 593–616, sr, frame_bytes
  sections all read). Returns 0 → muxer warns once + uses 512 for
  `pcr_period` only.
- Encoder DOES set 2048: `ac4enc.c:262`
  (`avctx->frame_size = AC4ENC_FRAME_SAMPLES`),
  `ac4enc_lib.h:100` (`#define AC4ENC_FRAME_SAMPLES 2048`),
  `codec_par.c:179` copies it to `par->frame_size`.
- Empirical (`m22-verbose.log`): GEN command at default loglevel →
  exactly 1× `[mpegts] frame size not set` (muxer-side, rc=0);
  fate GEN at loglevel error silent. Old blame ("encoder never sets
  frame_size") confirmed false; new cause confirmed true.

## Build + FATE (E-tip `9191f1a`, out-of-tree)

- `configure --samples=../suite --disable-doc --disable-debug
  --disable-autodetect` rc=0; `make -j8` rc=0 (`make-E.log`).
  8 warning lines, all upstream (nspdec fallthrough + note,
  4× adpcm unused + summary), **0 ac4**, 0 errors.
  Codec registers `DEA.L. ac4`. (One in-tree configure ran by
  mistake mid-run; residue fully removed, `git status` clean apart
  from the `ffprist/` worktree dir; the gated build is out-of-tree.)
- E fate **2/2 PASS** (`fate-E2.log`): `ac4-mpegts-pmt` (PMT section
  hex == REF `02b01d...`) + `ac4-mpegts-probe` (output == ref:
  codec_name=ac4, codec_tag_string=AC-4). Deterministic re-run after
  deleting the GEN artifact: 2/2 rc=0 (`fate-E2-rerun.log`).
- Probe `-strict -2` load-bearing pre-D too: with → rc=0; without
  → rc=1 (experimental decoder). Message's post-D rationale true;
  the flag is required on both sides of D.
- Full `fate-ac4` (`-k`): 25 TESTs, 23 green, 2 red — both
  `ac4-enc-neg*` (B-as-merged lacks `-strict -2`: rc=88 experimental
  refusal, `.err` kept). Manual proof: same CMD + `-strict -2` →
  rc=234 + "not supported by the ac4 encoder". Disclosed B-staleness
  (union-C HO fix #2), NOT E's. E's 2 tests green inside the run.

## Re-cut spec (E seat — the HOLD lift)

1. E 2/2 mak hunk context: union tail CMD
   (`-c:a ac4 -strict -2 -ab 128k ...`) — 1 line.
2. E 2/2 GEN rule: insert `-strict -2` after `-c:a ac4` — 1 line.
3. Message: add one GEN `-strict -2` clause (M11: encoder
   EXPERIMENTAL); keep the M22 paragraph verbatim (proven true);
   keep the probe rationale. Subjects unchanged.
4. Refresh `E-OWNER-SEND-STEPS.md` source line (still pins r3
   `a2485a9`) + size at re-cut; re-verify E 2/2 + full `fate-ac4`.
5. NOT E's (repeated for parent): C01–C03 Makefile 2-line context
   refresh (C-seat, disclosed); B encneg `-strict -2` ×2 (B-seat,
   disclosed union-C HO #2). Full-stack plain am on master bytes
   stays red until those + this land.

## Evidence map (all in `scratch/ff-E-verify/`)

`am-E.log` (C01 fail + E-OK runs), `am-A0AB.log` 5/5,
`am-C.log` 10/10, `csplit/000{1,2,3}[.orig]` (C ctx refresh),
`esplit/000{1,2}[.orig]` (E fwd-fix), `probe-E{1,2}-pristine.log`,
`configure-E.log`, `make-E.log`, `fate-E2.log`, `fate-E2-rerun.log`,
`fate-Etip-k.log`, `m22-verbose.log`, `encneg-manual.log`,
`encneg-nostrict.log`, `HEARTBEAT.md`, `HANDOFF.md`.
