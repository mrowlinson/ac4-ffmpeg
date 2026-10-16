# PARITY.md: encoder-lib vs `c-ac4enc` gate

Date: 2026-10-16. Oracle: cleanroom prebuilt `c-ac4enc`
(read-only; repo untouched, `git status` clean). Method:
`tests/parity_run.py` runs oracle + `encharness` per case,
strips oracle sync+size+trailer, byte-compares bare frames,
and cross-checks lib iframe flags against an independent
Python recompute of `iframe_set`. Vectors: 73 synthetic files
(24M, 48 kHz) in scratch `vectors/` — repo has zero PCM
fixtures, so the battery generates its own (cap 100 files).

## Result: 100/102 PASS, 0 non-parity

108 runs total: **100 PASS** (byte-identical bare frames +
iframe flags), **2 BOTH-FAIL** (bilateral consistent refusal,
same input class fails identically on both sides), **6
DIVERGE-neutral** (neutral-mode divergence docs, by design).

r2 (ff/enc-libr2, 2026-10-16): same 100/102 + 2 + 6 through
the incremental lib (frames pulled per feed, not buffered).
The 2 BOTH-FAIL cases now refuse fail-fast at open (sparse
stamp shape, `-EIO`) instead of late at flush; the parity
class is unchanged (both sides refuse). Neutral stats are
byte-identical to v1 (the neutral spec-lines default matches
on all 6 cases).

Per-file: 71/73 vector files pass outright; the 2
`st_noise_2049` files refuse on both sides at default -24
(bake peak overflow: short noise measures loud, rebake
exceeds s16 — oracle exits 1 silently, lib returns -EIO).
Same files pass at `--target-db 0` (incl. dedicated case).

## Matrix (all PASS unless noted)

- Stereo s16 wav: 10 contents x {2049, 0.5s, 5s} + 8
  length edges (1..96000 samples) + 4 extras = 42 runs.
- 5.1 s16 wav: 6 contents x {2049, 0.5s, 2s} = 18 runs.
- Raw stereo s16/s24/f32 (ingest scaling incl. s24
  floor-div and f32 round-half-even clamp) = 8 runs.
- Raw 5.1 f32 = 1 run.
- 8ch->stereo fold (`_f` path) = 4 runs; 6ch->stereo
  downmix fold = 3 runs; 8ch->5.1 = 4 runs (H10: meter
  fold == audio fold, genuine parity).
- Option variants: target-db 0 (4), shorts 0 (2),
  joint-shorts 1 (2), bake-db +3/-6 (2), data-rate
  256/320 (2), forced-iframes csv (2; CLI k+1 quirk
  mapped), iframe interval step 48/11 (2), presets
  ebu_r128/atsc_a85 (2), force-float `_f`-on-ints (4).
- Negatives + `_m`-vs-original equivalence: `tests/unit.c`
  all pass (20 misuse checks; 12 randomized equiv runs over
  int/float x stereo/mc x target -24/0 with fractional PCM).

## Neutral-stamp divergences (measured, documented)

Neutral = no stamp, no bake, flat_file=0. Lengths ALWAYS
equal (CBR sizes/iframes are measure-independent). Byte
divergence has 3 components: (a) stamp bytes (sub0/sub2/
prim0 templates; whenever target != 0), (b) rebaked PCM
(whenever bake gain != 0 — most content), (c) flat_file
floor flag (when f > 0.3):

| Case | ndiff/total | frames differing |
|---|---|---|
| st_silence_24000 | 30/4779 | 1 (stamp only; gain 0, flat 0 both) |
| st_sparse_24000 | 279/4779 | 2 |
| st_clicks_24000 | 2165/4779 | 13 |
| st_sweep_24000 | 3329/4779 | 13 |
| mc_noise_24000 | 7356/9557 | 13 |
| mc_sweep_96000 | 23713/33451 | 48 |

Silence is the clean minimal case: only the 30 stamp bytes
in the priming frame differ. All else follows the bake gain.

## Reproduce

```sh
make && make harness OUTDIR=$SCRATCH && make unit OUTDIR=$SCRATCH
python3 tests/gen_vectors.py $VECDIR   # or reuse an existing set
ENC_PARITY_S=$SCRATCH ENC_PARITY_VEC=$VECDIR \
  python3 tests/parity_run.py   # expect 100 PASS + 2 BOTH-FAIL + 6 neutral
```

Full per-run rows: `parity_results.csv` (copied beside this
file) and scratch `parity-out/` (oracle `.ac4` + lib frames).
