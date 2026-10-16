# lavc-dec: AC-4 lavc decoder wrapper (staged for the ffmpeg tree)

`ff/dec-wrap` stages the Series-A decoder against merged dec-lib
(d505b36). Lands in-tree as `libavcodec/ac4dec.c` + the two wiring
hunks once the Q1 port question is settled (below).

## Files

- `ac4dec.c` — the wrapper (thin; no DSP). Landing:
  `libavcodec/ac4dec.c`.
- `wiring.patch` — `allcodecs.c` extern + `Makefile` OBJS hunks, vs
  ffmpeg master `3e6cd5e`. Apply with `git apply` inside the tree,
  then copy `ac4dec.c` in. `configure` auto-detects
  `CONFIG_AC4_DECODER` from the new extern; no `configure` edit needed
  (v1 has no parser dep).
- `README.md` — this file.

## Bases

- ffmpeg tree: master `3e6cd5e` (raw `ac4` demuxer/muxer already
  upstream; no parser, no `dac4`, no lavc `ac4*`).
- dec-lib: `d505b36` (`ff/dec-lib-merged`, REVIEW-DEC-LIB
  APPROVE-WRAP). Core links out-of-tree until Q1 (the in-tree port)
  is decided; the wrapper is written so the port only swaps the
  `ac4d_*` provider, not the wrapper logic.

## Local validation build (exact recipe)

```
# 1. core archive (AC4D_LIB_ONLY; its own flags, zero warnings)
cp -r <parent>/dec-lib /tmp/corelib && cd /tmp/corelib && make
# 2. ffmpeg tree with ac4dec.c + wiring.patch applied
mkdir build && cd build
$FFWT/configure --extra-cflags="-I/tmp/corelib/src" \
  --extra-libs="/tmp/corelib/build/libac4d.a" --disable-doc
make -j$(nproc) ffmpeg
# decode needs -strict experimental (EXPERIMENTAL cap until mature)
./ffmpeg -strict experimental -i in.ac4 -map 0:a -f s16le out.pcm
```

Upstream will never take the out-of-tree link; that is Q1's port
(DEC-WRAPDESIGN Q1, still open, owner decision). The staged
`Makefile` hunk carries a comment saying so.

## Lib-contract honors (brief checklist, with code sites)

- Drain-per-feed loop-until-NEED_MORE (`ac4_drain`): after every
  accepted feed; ready frames wait in a wrapper FIFO. Flush loops
  `ac4d_flush` until NEED_MORE (`ac4_drain_flush`) — never
  flush-once; no `fi` from flush (driftcheck 10), shape comes from
  the latch.
- Process-wide serialization: `static AVMutex ac4d_mutex` around
  EVERY `ac4d_*` call (REVIEW MAJOR-3; header overclaims MT).
- Seek = close + reopen (`ac4_decode_flush`): feed-after-flush is
  `AC4D_ERR`, no reset API. Flush drops FIFO/pts/sticky error and
  re-opens; stream params re-latch from the first post-seek frame
  (T2/T3 prove equivalence + relatch).
- UNSUPPORTED at init (`ac4_lib_open`): `drc=1` or `de_level!=0`
  fails open with `EINVAL` + a log naming the option. (Also note the
  lib reads `AC4DEC_DRC=1` from the environment — ambient-env wart,
  lib-owned, documented not fixed.)
- Error table (`ac4_lib_fatal` + feed map): BAD_PACKET skips loud
  (`WARNING` per packet); BITSTREAM(-5) -> `INVALIDDATA` fatal
  (never skip); CONFIG_CHANGE -> `INVALIDDATA` fatal (D6, not
  `INPUT_CHANGED`: zero audio consumers per P13); ERR -> `EINVAL`
  fatal; NO_MEM -> `ENOMEM`; unknown -> `EINVAL` + `strerror`.
- Consecutive-BAD cap (16): a stuck `pres_sel` fails every feed
  identically and there is no parser to resync, so >16 in a row
  aborts loud (`INVALIDDATA`).
- channels==0 fails loud at the latch (`ac4_latch`): no-bed/exotic
  class → `EINVAL` naming channels/mask/note. Mask→layout must also
  agree with the channel count.
- `AC4D_LIB_ONLY` defines the core build (its `Makefile` already
  does for `lib_*.o`); without it lib diagnostics hit real stderr.
- 24-bit packing pinned s16LE+`0x00` (3-byte LE): known, NOT wired
  (carries no true 24-bit path); S16-only v1 per D3. FLT is
  requantized int (`s16/32768`) and stays ungated behind the same
  arrival rule (native float-stage plumbing); requesting it fails
  init honestly (T4).

## Design deltas from the read-list docs (all deliberate)

- Latch source: first EMITTED frame (real or gap-with-shape), not
  first real frame. D1's "don't latch from a gap frame" is honored
  for re-latching (latch-once; gap never overwrites good params),
  but all-stub streams (T2 file, 40+/40+ gap frames) must still emit
  shaped silence per D4's `framemd5` gate — so the first gap frame's
  presented shape latches, flagged `[params from stub-gap class]`.
  Proven by the all-gap gate vectors (byte-exact silence).
- Gap frames: silence + `WARNING` once-per-change + trailing summary
  at close + `FF_DECODE_ERROR_CONCEALMENT_ACTIVE` on the frame (D1).
  ffprobe does NOT print the flag (frame writer has no such field;
  D4's verify-during-port item resolves to API-only observability).
- `delay` stays 0 (Q2 open). D2's delay=spf needs the D5 `priming`
  lib field, which does not exist at d505b36; the wrapper applies no
  trim, so output is the lib's raw frame stream. When `priming`
  lands: set `avctx->delay` + seed `skip_samples` at the latch site.
- Fatal-once-then-EOF (scheduler rule, found live): a sticky fatal
  with no EOF makes fftools retry forever (1.2M repeats observed).
  So: queued audio emits first, mid-stream fatal eats remaining
  input (one bounded report per discarded packet), the drain reports
  the fatal once, then EOF. Poison queued frames (latch/shape
  failure is deterministic per frame) are dropped, never retried.
- Virgin drain (zero feeds: empty input or every packet skipped
  under the cap) ends clean EOF; the muxer reports the empty output.
- Re-framing: the raw demuxer strips sync/size/CRC (payload-only
  packets), so the wrapper re-frames every packet as AC40 (no CRC).
  Consequence: a CRC-corrupted AC41 frame is ACCEPTED (its payload
  decodes or BAD-packets on content), where the shim reading the raw
  file would reject it on CRC. CRC validation belongs at
  demuxer/parser (a future parser should check or pass CRC through);
  the wrapper layer cannot recover bytes the demuxer dropped. No
  corpus vector trips this (all gate vectors byte-match the shim).
- Pts: pushed per accepted feed, popped per emitted frame
  (emitted==fed, verified across the corpus); flush tails pop the
  remainder. Underflow (lib invariant break) falls back to last+spf
  with a one-time warning.

## Gates (this wave, all in scratch, out-of-tree builds only)

- Build: `make ffmpeg` exit 0, zero `ac4` warnings (normal and
  ASan/UBSan builds). dec-lib core: zero warnings under its own
  flags (unchanged behavior).
- Byte-exact vs dec-lib: 11/11 vectors `cmp`-identical to BOTH the
  shim (`build/shim decode --bits 16`) and an independent streaming
  driver (feed-all + drain-per-feed + flush loop): stereo (2),
  5.1 cm14 incl 44.1k (3), 7.1 cm253 (3), IMS120 (2), all-gap
  `5x-acpl-unimpl` (2, silence bytes). Plus the ims splice prefix
  (12 frames) identical to the streaming driver.
- Neg controls: splice CONFIG_CHANGE → loud `INVALIDDATA`,
  byte-exact 12-frame prefix, terminates (exit 69); ajoc ch=0 →
  loud `EINVAL` + `Conversion failed!`; mid-stream garbage →
  1 loud skip, exit 0, 87/88 frames, 38-frame head prefix exact
  (frame 39+ inside the ~2-frame DN latch horizon, expected);
  garbage file → 17 skips + cap abort, terminates; under-cap
  all-skip → clean EOF; `drc=1` / `de_level=6` → init
  `UNSUPPORTED` fail-open; `request_sample_fmt=FLT` → init `EINVAL`
  (API-level, T4); empty file → loud mux-level failure.
- C-API suite (`tseek`, scratch): T1 straight, T2 flush+re-decode
  IDENTICAL, T3 flush+other-layout IDENTICAL with 2ch→6ch relatch,
  T4 req-flt fails, T5 stuck `pres_sel=15` → 17 skips + fatal,
  T6 virgin-drain clean EOF. ALL PASS.
- ASan/UBSan (`-fsanitize=address,undefined`, instrumented core):
  6/6 clean, zero reports — clean, all-gap, splice-fatal,
  cap-fatal, no-bed (64 poison-drops), mid-skip. Sanitizer PCM
  bytes identical to the normal build.

## N5 interplay (N5fix unlanded — wrap tracks it)

The wrapper passes lib rate labels straight through, INCLUDING the
known N5 mislabel: on fri 0-12 the lib reports/stamps the
internal-grid rate (`h_sr` from `e4_sr[]`, e.g. 46034 Hz / spf 1536
for the gpac vector) and emits pre-resampler PCM, where ETSI TS 103
190 Table E.1/s5.11 wants `base_fs x sf_mult` + resampled output
counts. The gpac gate vector pins CURRENT (mislabeled) bytes +
labels deliberately — it is the regression tripwire, not an
endorsement.
When N5fix lands (lib-owned: `h_sr`/`out_sr` = base×sf on all rows,
s5.11 resampler locked to `sequence_counter`, `ac4d_info` docs
pinned): expected wrapper impact is ZERO code change (passthrough),
but ALL fri 0-12 gate goldens/bytes must be re-recorded (resampled
PCM + new labels), and the A0-demux-vs-lib timing contradiction
(48 kHz container vs 46034 Hz lib) resolves on the lib side. If
N5fix extends `ac4d_info`, recompile; if it changes the GAP/pace
contract, re-run the full gate table above.

## Tracked follow-ups (not this wave)

Q1 in-tree port (owner decision; until then the out-of-tree link);
D5 `priming` + D2 delay/trim; drain-and-reopen upgrade for Q3 (the
ims splice is live evidence real streams switch); MAJOR-1/2 silent
corners (ap5 tripwire, cross-splice DN compare); MAJOR-3/4 MT/cache
redesign (wrapper serializes; env + DUMP-file warts stay lib-owned);
CRC-at-demux/parser; ffprobe flag export (declined: writer lacks the
field).
