# enc-lib: AC-4 encoder library (opaque-ctx API, r2 incremental)

Merged status (r2 + S1): measured-loudness parity vs
`c-ac4enc` is **100/102 runs PASS + 2 LIB-AHEAD** (the frozen
oracle still refuses the S1 sparse class while the lib
encodes, byte-identical to `enc.py`; 0 non-parity); see
`parity/PARITY.md`. The core is refactored into an incremental
engine (see `VENDOR.md`); the whole-stream drivers survive as
thin wrappers proving zero-divergence.

## What this is

`src/ac4enc_lib.h` implements the opaque-ctx API from
`ENC-LIBDESIGN.md` sec 1 over vendored encoder-core sources.
Packets are bare AC-4 raw frames (TOC..pad, NO sync/size/CRC);
the muxer adds framing. Byte-identical to the CLI's raw
payloads when opts+measure (+spec lines) match (parity-gated).

## r2 (incremental ctx)

1. **TRUE incremental encode (B2).** `encode_frame[_f]` feeds
   2048 samples and returns 1 frame ready / 0 need-more-input;
   lookahead is 1 frame (2 for priming=0 joint mode). Memory is
   bounded (sliding window + scratch, ~1MB worst case);
   open-ended streams work. No whole-stream run exists in the
   lib path. The engine (`src/enc_step.h`, private) is shared
   with the whole-stream core drivers (now wrappers), so
   wrapper-vs-original equivalence is structural.
2. **H1 is real:** stamp+bake from the CALLER's pass-1 measure
   (`has_measure` via `ac4enc_measure2[_f]`); per-domain stamp
   select at first feed (int default). Without a measure:
   NEUTRAL (no stamp, no bake, flat_file=0, default spec
   lines). **H6 is real:** measured stereo takes caller spec
   lines (`ac4enc_spectral_lines` over the full, possibly
   baked, L row); without them the neutral default
   (nlines=0, fullband-only LONG path) applies and diverges on
   tonal content. Neutral divergence stays documented in
   `parity/PARITY.md` (lengths always equal).
3. **Thread-safe separate ctx (B1).** All mutable scratch is
   heap-owned by the ctx; zero shared mutable state
   tree-wide. Concurrent encodes on separate ctx are safe
   (threaded unit test, TSan-clean). One ctx stays
   single-threaded.
4. **Diagnostics (M1).** `ac4enc_strerror`, sticky
   `ac4enc_err_sub/_stage/_frame` (subcode + stage +
   frame). Refusals fail fast: stamp-shape at open (-EIO),
   bake overflow at feed, baked peak at tail/flush. NOTE: the
   lib may emit prefix frames before discovering a baked-peak
   refusal (the check needs the whole stream); callers must
   treat a mid-stream error as failed output. R3: bake
   refusals also carry gate + offending values via
   `ac4enc_err_detail`, and the 8 core peak gates record
   through `ac4enc_last_error()` (policy (a): the gates are
   pyref-mandated, so refusals stay — but loud).
5. **Open validates everything (M2).** Interval rule, DRC
   contents (dry-run through the real writer), all flags 0/1,
   measure/peak/line ranges, rate, preset, sample rate — all
   `-EINVAL` at open. `iframe_base`/`iframe_total` accept any
   int (`total<0` = open-ended); `dmx` is range-free
   (bit-packed, any 5 ints safe + deterministic). Refusals
   (valid config, core says no) stay `-EIO`.
6. **48 kHz only (M3).** `sample_rate` 0 (=48000) or 48000;
   else `-EINVAL`. Loudness ops assume 48 kHz throughout.
7. **Slim public header (M4).** `ac4enc_lib.h` exposes no core
   internals (no `enc_core.h`); public DRC mirror converted at
   open (layout-static-asserted). In-tree core consumers
   include `enc_core.h` directly.
8. **Flags contract (M5).** Build requires `-ffp-contract=off
   -fno-builtin-sin -fno-builtin-cos -fno-builtin-pow` (pinned
   in `Makefile`); the header hard-errors on
   `-ffast-math`/`-ffinite-math-only`, and `ac4enc_fp_ok()`
   probes contraction at runtime (open refuses when unsafe).
9. **Length reporting (M8).** `ac4enc_max_packet(rate)` is the
   exact CBR period max; sized buffers never go short. The
   `2`-variants (`encode_frame2[_f]`, `encode_tail2[_f]`,
   `flush2`) set `*out_len` on success AND to the needed size
   on short-buffer `-EINVAL` (retryable, consumes nothing).
   Pinning test asserts no emit exceeds the max.
10. **Lifecycle (M7).** One stream per ctx; `ac4enc_reset`
    reuses it (same opts) for a new stream. After an
    encode/memory error the ctx is sticky-failed until reset.
11. **priming=0 covered (M6).** Joint-mode unit test (bytes +
    count + flags vs core, default + explicit total). Joint
    open-ended edge: `iframe_base>0` + default total keeps
    late-cadence marks past the unknown total (documented;
    base==0 and explicit totals are exact).
12. **Forced indices are CORE indices.** The CLI shifts csvfile
    user indices +1 (`enc_cli.c`: `idx[nidx++] = k + 1`).
    The lib takes post-shift indices (parity spawns both sides
    accordingly).
13. **Meter taps (H10): no gap exists.** `fold_8ch_to_51_meter`
    is literally `fold_8ch_to_51`, so measuring on audio rows
    == measuring on meter rows. 8ch->5.1 parity PASSES (4/4).

## API breaks vs v1 (all review-mandated)

- `encode_frame[_f]`/`encode_tail[_f]` now emit incrementally
  (1/0 contract live; `out`/`outcap` required when a frame is
  ready) instead of buffering to flush.
- `opts.drc` is now `const ac4enc_drc_t *` (public mirror, no
  core header). Field names identical.
- New required-ish fields: `sample_rate` (0 ok),
  `has_spec_lines`/`n_spec_lines`/`spec_lines` (0 ok, neutral
  default).
- `has_bake` without `has_measure`, `mc+joint_shorts`,
  `mc+priming=0` now fail open (were silently ignored).
- Refusals surface earlier (open/feed/tail instead of flush)
  with subcodes; mid-stream failure may follow emitted
  frames (see 4 above).

## H-status vs the design (H1-H11)

| # | State | Note |
|---|---|---|
| H1 stamp+bake | RESOLVED | caller measure + per-domain stamp + neutral fallback |
| H2 nframes planning | RESOLVED | positional CBR/iframe rules; plan at end |
| H3 MDCT rings | RESOLVED | sliding window, bounded |
| H4 orient lookahead | RESOLVED | 1-frame latency (2 joint); flush drains |
| H5 bake rewrites PCM | RESOLVED | same op order on ingest; peak check at end |
| H6 freq estimate | RESOLVED | caller pass-1 lines + neutral default |
| H7 thread-safety | RESOLVED | zero shared mutable state; TSan-clean |
| H8 error model | RESOLVED | -EINVAL/-ENOMEM/-EIO + subcode/stage/frame |
| H9 float discipline | RESOLVED | flags pinned + compile guard + runtime probe |
| H10 meter taps | RESOLVED | meter fold == audio fold; 851 passes |
| H11 malloc-per-frame | RESOLVED | ctx-owned reusable scratch + emit bufs |

R-notes: R1 holds as stated (blind single-pass = neutral,
divergent by design); R2 closed (incremental reorder proven
byte-identical by parity + equivalence matrix); R3 closed
(priming=0 unit-tested vs core incl. flags); R4 closed (`_f`
on integer-valued input == int path, 4 force-float variants
PASS); R5 closed (flags pinned + guard + probe + parity gate).

## Heisenbugs / quirks found

- CLI `--forced-iframes csvfile:` paths may not contain `:`.
- CLI fails SILENTLY (rc 1, zero stderr) on the S1 sparse
  class (e.g. short noise at -24); the frozen oracle keeps
  the pre-fix `nfr0 == 12` demand while the lib encodes
  (2 LIB-AHEAD cases, byte-identical to `enc.py`).
- Sparse I-frame stamp shape (`compose_sub0` returns 11, not
  12) is legitimate (`enc.py` sparse branch), not a refusal
  path: the core drivers and the lib accept
  `sparse ? 11 : 12` (S1 fix, `t_sparse`-gated).
- CLI requires `--output-manifest` (hard error without it).
- CLI `--target-db` accepts only -24/0 (other -31..-10 values
  are a CLI validation error, not a core refusal); the lib
  accepts the full -31..-10 range.
- R3 Step 0 closed SPAN-FU: the Series C 58/59 refusal was the
  S1 sparse class (the wrapper's failing pass-1 matches the
  60-frame span measure exactly); post-fix the lib encodes
  every SPAN span, core-direct 42697 B == `enc.py` bytes.

## Build / test

```sh
make            # libac4enc.a (gitignored)
make harness    # $OUTDIR/encharness (OUTDIR=... to override)
make unit       # $OUTDIR/unit
python3 tests/gen_vectors.py $SCRATCH/vectors
ENC_PARITY_S=$SCRATCH ENC_PARITY_VEC=$VECDIR \
  python3 tests/parity_run.py
```

Parity vectors (~50MB) stay out of the repo; point
`ENC_PARITY_VEC` at them (v1 scratch has a set).
