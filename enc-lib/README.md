# enc-lib: AC-4 encoder library (opaque-ctx API, v1)

V1 status: measured-loudness parity vs `c-ac4enc` is **100/102
runs PASS** (2 bilateral consistent refusals, 0 non-parity);
see `parity/PARITY.md`. Vendored core patch is purely additive
(0 removed lines); see `VENDOR.md`.

## What this is

`src/ac4enc_lib.h` implements the opaque-ctx API from
`ENC-LIBDESIGN.md` sec 1 over vendored encoder-core sources.
Packets are bare AC-4 raw frames (TOC..pad, NO sync/size/CRC);
the muxer adds framing. Byte-identical to the CLI's raw
payloads when opts+measure match (parity-gated).

## v1 adaptations (deliberate, documented)

1. **Buffered whole-stream ctx.** `encode_frame[_f]` buffers and
   returns 0; the first `flush` runs the core once over the full
   input, splits the sync-framed blob, and serves bare frames
   one per call. H2/H3/H4/H6 are sidestepped honestly: nframes
   is known at flush and the core sees the exact whole input it
   was written for. Cost: whole-stream latency/RAM. The header
   is forward-compatible with a future incremental ctx.
2. **H1 is real, not buffered away.** The stamp+bake path is
   driven by the CALLER's pass-1 measure (`has_measure` via
   `ac4enc_measure2[_f]`) through new `_m` core entries that
   take the measure as input. Without a measure the lib uses
   the NEUTRAL path (no stamp, no bake, flat_file=0). No silent
   default pretends CLI parity. Neutral divergence is measured
   and documented in `parity/PARITY.md` (lengths always equal;
   stamp bytes + rebaked PCM + flat flag differ).
3. **`ac4enc_flush2` (design gap).** The design's `flush` never
   reports the frame length, so callers cannot size packets.
   `flush2` adds the required `out_len` out-param. `flush`
   remains as a draining shim. Prefer `flush2`.
4. **`ac4enc_encode_tail[_f]` (design gap).** Streams are rarely
   a multiple of 2048; the tail call feeds the final
   1..2048 samples so the lib encodes the exact sample count.
5. **`ac4enc_measure2[_f]` (design gap).** The design's measure
   twins return only an int peak, which loses the float-path
   peak (`content16_f` needs the double). The `2` twins fill
   the full struct incl. `d_peak`. The narrow twins remain.
6. **Forced indices are CORE indices.** The CLI shifts csvfile
   user indices +1 (`enc_cli.c`: `idx[nidx++] = k + 1`).
   The lib takes post-shift indices (parity spawns both sides
   accordingly).
7. **Meter taps (H10): no gap exists.** `fold_8ch_to_51_meter`
   is literally `fold_8ch_to_51`, so measuring on audio rows
   == measuring on meter rows. 8ch->5.1 parity PASSES (4/4).
8. **Thread-safety (H7): OPEN.** Ctx owns lib state, but the
   core keeps function-static scratch. Single-threaded only.

## H-status vs the design (H1-H11)

| # | State | Note |
|---|---|---|
| H1 stamp+bake | RESOLVED | caller measure + `_m` entries + neutral fallback |
| H2 nframes planning | v1-buffered | known at flush; open-ended bounded by RAM |
| H3 MDCT rings | v1-buffered | whole input to core |
| H4 orient lookahead | v1-buffered | latency = stream |
| H5 bake rewrites PCM | RESOLVED | same op order; bake-db variants pass |
| H6 freq estimate | v1-buffered | runs inside core on buffered input |
| H7 thread-safety | OPEN | core statics remain; single-threaded only |
| H8 error model | RESOLVED | -EINVAL/-ENOMEM/-EIO; unit-tested |
| H9 float discipline | RESOLVED | flags pinned in Makefile; parity enforces |
| H10 meter taps | RESOLVED | meter fold == audio fold; 851 passes |
| H11 malloc-per-frame | PARTIAL | ctx owns PCM; core internals unchanged |

R-notes: R1 holds as stated (blind single-pass = neutral,
divergent by design); R2 moot in v1 (no reorder); R3:
priming=0 encodes (unit-smoked) but has no CLI oracle (CLI
always passes priming=1); R4 closed (`_f` on integer-valued
input == int path, 4 force-float variants PASS); R5 closed
(flags pinned + parity gate).

## Heisenbugs / quirks found

- CLI `--forced-iframes csvfile:` paths may not contain `:`.
- CLI fails SILENTLY (rc 1, zero stderr) on bake peak
  overflow (e.g. short noise at -24); the lib returns -EIO
  for the same input. Both refuse consistently (2 cases).
- CLI requires `--output-manifest` (hard error without it).

## Build / test

```sh
make            # libac4enc.a (gitignored)
make harness    # $OUTDIR/encharness (default: scratch)
make unit       # $OUTDIR/unit
python3 tests/gen_vectors.py $SCRATCH/vectors
python3 tests/parity_run.py
```

Parity paths are absolute to the `ff-enc-librefactor` scratch
by design (vectors ~50MB stay out of the repo).
