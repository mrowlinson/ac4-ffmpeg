# lavc-dec: AC-4 lavc decoder (staged for the ffmpeg tree)

`ff/dec-wrap` stages the Series-A decoder; `ff/dec-wrap-q1` closes
Q1 by staging the in-tree core port with it. Lands in-tree as
`libavcodec/ac4dec.c` + the 24 `core/` files + the two wiring
hunks. No out-of-tree link, no extra libs/flags.

## Files

- `ac4dec.c` — the wrapper (thin; no DSP). Landing:
  `libavcodec/ac4dec.c`. UNCHANGED by the Q1 port (it already calls
  only the public `ac4d_*` API; the port swaps the provider, not the
  wrapper logic).
- `core/` — the 24 vendored dec-lib sources (11 `.c` + 13 `.h`,
  `ac4_src.h` included: N5 s5.11 count rules, header-only, no OBJS),  landing flat in `libavcodec/`. Filenames kept except the 8 static
  table files, renamed `*_tables.c/h` → `*_tab.c/h` (zero collisions
  at `3e6cd5e` either way); port delta is headers + CLI strip +
  that rename + the scripted ASCII map (see Q1).
- `wiring.patch` — `allcodecs.c` extern + `Makefile` OBJS hunks, vs
  ffmpeg master `3e6cd5e`. Apply with `git apply` inside the tree,
  then copy `ac4dec.c` + `core/*` in. `configure` auto-detects
  `CONFIG_AC4_DECODER` from the new extern; no `configure` edit needed
  (v1 has no parser dep).
- `README.md` — this file.

## Bases

- ffmpeg tree: master `3e6cd5e` (raw `ac4` demuxer/muxer already
  upstream; no parser, no `dac4`, no lavc `ac4*`).
- dec-lib: `2436f47` tree `8e27f25`
  (`ff/merged` content: FINDING-1 fixes + N5 rate semantics
  (`ff/dec-n5fix-r2`: base_fs x sf_mult, s5.11 converter, 4-arg
  flush) in; dec-lib/ identical `63dd7eb..2436f47`).  `core/` is the in-tree port of exactly this rev (see Q1).

## Q1 resolution (DEC-WRAPDESIGN Q1, CLOSED by `ff/dec-wrap-q1`)

Q1's exact text (DEC-WRAPDESIGN.md §8):

> - **Q1 (HARDEST): where does the ported core live and who owns its
>   upstreaming?** Native decoder means ~10 KLOC of ported DSP moves
>   INTO `libavcodec/` — but the cleanroom repo keeps evolving (gap
>   fixes flip stubs to real). Either (a) port once and develop future
>   fixes directly in ffmpeg (cleanroom repo becomes stale, dual-home
>   drift), or (b) keep cleanroom authoritative and re-port per wave
>   (ffmpeg reviewers see bulk drops, review pain, `git blame` lies).
>   No good answer; needs owner decision before patch 1. Sub-issue:
>   lib "24-bit" packing must be pinned before advertising S32.

Answer:

- WHERE: in-tree, `libavcodec/` flat, same filenames except the 8
  table files below (zero collisions at `3e6cd5e`), built under
  `CONFIG_AC4_DECODER` with no defines and no extra flags — the
  `AC4D_LIB_ONLY` behavior (no `main`, callback logging) is
  unconditional in the ported copy.
- WHO OWNS: snapshot-vendoring, i.e. (b) before upstream acceptance
  and (a) after. Until the patch lands upstream, dec-lib stays
  authoritative for DSP and `core/` is a pinned snapshot, re-ported
  per release by the mechanical procedure below (re-ports diff
  cleanly against the staged set, so reviewers never see a second
  bulk drop). Once upstream accepts, the ffmpeg tree is authoritative:
  fixes land upstream-first and backport to dec-lib/cleanroom.
- PORT DELTA (whole list): FFmpeg LGPL header prepend per file
  (title = original title verbatim, DSI-L3 precedent) + `dec_lib.c`
  `AC4D_LIB_ONLY` strip (39 CLI/shim spans deleted, 2 lib spans
  spliced inline; guards verified else-free) + one provenance line
  atop `dec_lib.c` + rename of the 8 static table files
  `aspx/huffman/qwin/snf _tables.c/h` → `*_tab.c/h` (with their
  `#include`s, `wiring.patch` OBJS, and sibling comments; include
  guards and dec-lib-provenance `src/*` comments keep the old
  spelling) + the scripted ASCII map (the `5c8740a` 8-char comment
  map + entropy-tail rule: dec-lib keeps comment UTF-8, `core/`
  ships pure ASCII). Nothing else: no behavior edits.  Provenance pin: dec-lib tree `8e27f25`.
  The rename dodges `libavcodec/Makefile CLEANFILES =
  *_tables.c *_tables.h` (plus the matching `libavcodec/.gitignore`
  lines), which otherwise deletes the committed sources on `make
  clean`; `*_tab` follows the in-tree `ac3tab`/`aactab` precedent.
- RE-VENDOR RULE (mechanical): take `dec-lib/src/*` at the new pin
  → apply the delta above (headers, strip, then the rename map
  `s/_tables\./_tab./` on filenames + `#include`s + OBJS + sibling
  comments only — never on `src/*` provenance or guard macros,
  then the ASCII map) → diff against staged `core/` (expect DSP
  changes only) → update the pin → re-run the full gate table below.
  Never hand-edit `core/` (generate, then diff); never re-prepend
  headers over headed files (always regenerate from dec-lib source).
- SUB-ISSUE (already settled): 24-bit packing pinned s16LE+`0x00`
  (WRAP-DRIFTCHECK 27, no true 24-bit path); S32 stays unwired.

## Local validation build (exact recipe)

```
# ffmpeg tree with ac4dec.c + core/* + wiring.patch applied
mkdir build && cd build
$FFSRC/configure --disable-doc   # NO extra-cflags/extra-libs: Q1 closed
make -j$(nproc) ffmpeg
# decode needs -strict experimental (EXPERIMENTAL cap until mature)
./ffmpeg -strict experimental -i in.ac4 -map 0:a -f s16le out.pcm
```

The built binary's `configuration:` line shows `--disable-doc` with no
extras — the Q1 close is visible in the artifact itself.

## Lib-contract honors (brief checklist, with code sites)

- Drain-per-feed loop-until-NEED_MORE (`ac4_drain`): after every
  accepted feed; ready frames wait in a wrapper FIFO. Flush loops
  `ac4d_flush` until NEED_MORE (`ac4_drain_flush`) — never
  flush-once; flush fills `fi` with the drained frame's actual
  count (N5 4-arg form), and emit checks read per-frame `fi->spf`
  against the `get_info` latched max (fractional rows roam the
  Table-70 pattern at/below it).- Process-wide serialization: `static AVMutex ac4d_mutex` around
  EVERY `ac4d_*` call (REVIEW MAJOR-3; header overclaims MT).
- Seek = close + reopen (`ac4_decode_flush`): feed-after-flush is
  `AC4D_ERR`, no reset API. Flush drops FIFO/pts/sticky error and
  re-opens; stream params re-latch from the first post-seek frame
  (T2/T3 prove equivalence + relatch).
- UNSUPPORTED at init (`ac4_lib_open`): `drc=1` or `de_level!=0`
  fails open with `EINVAL` + a log naming the option. (Also note the
  lib reads `AC4DEC_DRC=1` from the environment — ambient-env wart,
  lib-owned, documented not fixed. `AC4DEC_CMPLEAD`/`AC4DEC_HEIGHTS`
  are silently dropped by the lib (REVIEW MINOR-5/6); `AC4DEC_*DUMP`
  file writes stay lib-owned too.)
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
- In-tree, the `AC4D_LIB_ONLY` behavior is unconditional (no
  `main`, lib diagnostics via the log callback); the define no longer
  exists in the ported copy. (In dec-lib proper, its `Makefile` still
  passes `-DAC4D_LIB_ONLY` for `lib_*.o`.)
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
  lib field, which does not exist at this pin; the wrapper applies no
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
- Truncation mirrors the shim's sync-chain rule (F2/B2EXEC-REAL):
  a lone clean framing (one unflagged packet) followed by skipped
  packets is unanchored sync: the lone prefix is discarded and EOF
  fails loud (`INVALIDDATA`, `no-sync` diagnostic, zero output, CLI
  exit 69 via the error-rate abort). Two chained framings (t11 with
  a runt tail: exit 0 + valid prefix) or a lone framing at clean EOF
  (exit 0) decode normally. First-frame emit waits one framing for
  the shape decision (second clean framing or EOF); a latched fatal
  releases the hold. Committed gate: `lavc-dec/tests/f2-gap-trunc.sh`
  + pinned `gap-trunc.ac4` (500 B, `e54faf83…`).

## Gates (Q1 wave, all in scratch, out-of-tree builds only)

- Build: pure in-tree (`configure --disable-doc`, no extras):
  `make ffmpeg` exit 0, zero `ac4` warnings (normal and ASan/UBSan
  builds; 34 pre-existing tree warnings, none `ac4`). Ported core:
  zero warnings under dec-lib's own flags too, with no `-D`.
- Port equivalence (mechanical): ported `dec_lib.c` preprocessed
  output is byte-identical to `-DAC4D_LIB_ONLY` dec-lib source
  modulo blank lines; all other core files are comment-prepend
  only. Neg outputs (`bad40`, `ims`) are `cmp`-identical to the
  out-of-tree wrap's own outputs.
- Byte-exact vs dec-lib: 11/11 vectors `cmp`-identical to BOTH the
  shim (`build/shim decode --bits 16`) and an independent streaming
  driver (feed-all + drain-per-feed + flush loop): stereo (2),
  5.1 cm14 incl 44.1k (2), 5.1 all-gap (2, silence bytes),
  7.1 (3), IMS (2). Plus the ims splice prefix (12 frames)
  identical to the streaming driver.
- Neg controls: splice CONFIG_CHANGE → loud `INVALIDDATA`,
  byte-exact 12-frame prefix, terminates (exit 69); ajoc ch=0 →
  loud `EINVAL` + `Conversion failed!`; mid-stream garbage →
  1 loud skip, exit 0, 87/88 frames, 39-frame head prefix exact
  (divergence at frame 40, inside the ~2-frame DN latch horizon,
  expected); garbage file → 17 skips + cap abort, terminates;
  under-cap all-skip → decoder-clean EOF (mux reports empty);
  `drc=1` / `de_level=6` → init `UNSUPPORTED` fail-open;
  `request_sample_fmt=FLT` → init `EINVAL` (API-level, T4);
  empty file → loud mux-level failure.
- C-API suite (`tseek`, scratch): T1 straight, T2 flush+re-decode
  IDENTICAL, T3 flush+other-layout IDENTICAL with 2ch→6ch relatch,
  T4 req-flt fails, T5 stuck `pres_sel=15` → 17 skips + fatal,
  T6 virgin-drain clean EOF. ALL PASS.
- ASan/UBSan (`-fsanitize=address,undefined`, whole tree
  instrumented): 6/6 clean, zero reports — clean, all-gap,
  splice-fatal, cap-fatal, no-bed (poison-drop + fatal-once + EOF),
  mid-skip. Sanitizer PCM bytes identical to the normal build.

## N5 interplay (N5fix LANDED — wrap re-vendored + re-gated)

N5fix (`ff/dec-n5fix-r2`, dec-lib tree `8e27f25`) is lib-owned: output
rate = `base_fs x sf_mult` on all fri rows, the s5.11 converter maps
each internal frame to Table-70 output counts locked to
`sequence_counter`, and `ac4d_flush` grew a 4th `fi` arg (the drained
frame's actual count, NULL-ok, keyed by `AC4D_FLUSH_FI`). `get_info`
reports the latched max; each `get_frame`/`flush` `fi` reports that
frame's actual count.

The fix changed the lib contract, so the wrapper adapted: `ac4dec.c`
takes the flush `fi` (the old get-info-after-flush fallback is gone),
latches `spf` from the `get_info` max instead of the first frame's
actual count, lets per-frame `spf` roam at/below the max (fractional
rows), sizes byte checks per frame (`frame_size` = max, `nb_samples` =
actual), and bumps the drain-scratch max to 8ch x `AC4_SRC_OMAX` 8192.
Channels still latch once. `wiring.patch` is unchanged (no new `.c`).
Rate labels stay straight passthrough, now OUTPUT values (gpac: 48000
Hz, 1601/1602-sample frames, 5727312 B); the A0-demux-vs-lib timing
contradiction is gone (container and lib agree at 48 kHz). Gate
goldens/refs are re-recorded POST bytes (`fate-n5/pins.txt` pins gpac +
the g-fri2 pair); the gpac tripwire now guards the fixed labels. The
GAP/pace contract is unchanged.
## Tracked follow-ups (not this wave)

D5 `priming` + D2 delay/trim (re-vendor + re-gate per the Q1 rule when
it lands); drain-and-reopen upgrade for Q3 (the ims splice is live
evidence real streams switch); MAJOR-1/2 silent corners (ap5 tripwire,
cross-splice DN compare); MAJOR-3/4 MT/cache redesign (wrapper
serializes; env + DUMP-file warts stay lib-owned); CRC-at-demux/parser;
ffprobe flag export (declined: writer lacks the field).
