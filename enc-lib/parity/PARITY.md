# PARITY.md: encoder-lib vs `c-ac4enc` gate

Date: 2026-10-16. Oracle: cleanroom prebuilt `c-ac4enc`
(read-only; repo untouched, `git status` clean). Method:
`tests/parity_run.py` runs oracle + `encharness` per case,
strips oracle sync+size+trailer, byte-compares bare frames,
and cross-checks lib iframe flags against an independent
Python recompute of `iframe_set`. Vectors: 73 synthetic files
(24M, 48 kHz) in scratch `vectors/` — repo has zero PCM
fixtures, so the battery generates its own (cap 100 files).

## Result: 100/105 PASS + 2 LIB-AHEAD + 3 BOTH-FAIL, 0 non-parity

111 runs total: **100 PASS** (byte-identical bare frames +
iframe flags), **2 LIB-AHEAD** (frozen oracle still refuses
the S1 sparse class, lib encodes), **3 BOTH-FAIL** (2
R3-pinned genuine peak-gate refusals + 1 m8 cause pin, all
cause-checked), **6 DIVERGE-neutral** (neutral-mode
divergence docs, by design).

r2 (ff/enc-libr2, 2026-10-16): same 100/102 + 2 + 6 through
the incremental lib (frames pulled per feed, not buffered).
Neutral stats are byte-identical to v1 (the neutral
spec-lines default matches on all 6 cases).

Per-file: 71/73 vector files pass outright; the 2
`st_noise_2049` files are LIB-AHEAD at default -24 (sparse
class: the oracle exits 1 silently on the pre-fix stamp
check while the lib encodes — see the S1 rerun note below).
Same files pass at `--target-db 0` (incl. dedicated case).

## Rerun 2026-10-16 (ff/enc-libfix, S1): still 100/102, 0 non-parity

The S1 fix (sparse-iframe fr0 stamp is legitimately 11
bytes, not 12 — `enc_core_enc.c`) flips the 2
`st_noise_2049` runs from BOTH-FAIL to **LIB-AHEAD**: the
frozen oracle still refuses them (silent exit 1, same S1
bug), while the lib now encodes. The v1 "bake peak
overflow" diagnosis was wrong — those vectors are
sparse-flagged (m=-9.18, f=0.537, tf=0.5, peak=9830; bake
gain -11.93 dB never overflows) and always refused at the
stamp-length check. Byte-proof that lib-ahead is correct:
lib bare frames are byte-identical to `encoder/enc.py`
`encode_pcm` output (1366/1366 B on `st_noise_2049.wav`;
17020/17020 B on the S1 vector `imp-V300.wav`), and the
S1 output oracle-decodes cleanly (48 frames, rej 1 —
same shape as the tone control). The suite encodes the
new expectation (`expect="LIB-AHEAD"`: oracle rc!=0,
lib rc==0, lib frames>0); all other 106 verdicts are
unchanged, incl. all 6 neutral-divergence ndiff counts.

## Rerun 2026-10-16 (ff/enc-libr3, R3 guard strictness): 0 non-parity

Policy (a): the bake peak gates are pyref-mandated (`enc.py`
asserts `peak < 32767` on the stereo AND mc paths), so the
refusals stay — but loud (R3 Step 0 verdicts in scratch
`ff-R3-impl/STEP0-VERDICTS.md`). Two new pinned cases
(`expect="BOTH-FAIL"`: both sides must refuse, the lib must
name `baked peak overflow`, and the vector must be dense):

- `r3:hotimp-20x`: self-generated `r3_hotimp_8192.wav` (bed
  440/880 Hz amp 1000 + impulses amp 20000 @0/4096): quiet
  bed + positive correction gain (+5.41 dB) bakes past s16.
  Dense (f~0.23), so the frozen oracle's silent exit 1 can
  only be its peak gate — same cause class as the lib's
  `EBAKE_PEAK` (baked_peak=37281, matching pyref's assert
  tuple exactly).
- `var:bake+12` (`st_sine1k_24000.wav`, explicit +12 dB bake):
  same shape (baked 65226 == pyref assert).

Cause-equality note (EL-m8, pinned cases only): the gate
asserts the lib subcode from harness stderr plus vector
density from the lib's own pass-1 measure, which rules out
the oracle's stamp gate by construction (the peak gate also
precedes stamp compose in prologue order). All other 108
verdicts unchanged, incl. all 6 neutral ndiff counts.

## Rerun (ff/enc-lib-minors, m8): 100/103 + 1 BOTH-FAIL, 0 non-parity

109 runs (108 + new `var:refuse-bake60`): **100 PASS**,
**1 BOTH-FAIL** (`cause=encode-refusal` both sides: oracle
silent exit 1 after `Step 2/2`, lib `baked peak overflow`
at tail), **2 LIB-AHEAD**, **6 DIVERGE-neutral** (all ndiff
counts unchanged). BOTH-FAIL now requires cause equality
(`lib_fail_class == oracle_fail_class`; unknown on either
side fails loud as CAUSE-MISMATCH, count 0).

## Rerun (ff/enc-lib-minors-res merge, 2026-10-16): 111 runs, 0 non-parity

Union battery (R3's 2 pins + m8's cause pin): **100 PASS**,
**3 BOTH-FAIL**, **2 LIB-AHEAD**, **6 DIVERGE-neutral** (all
ndiff counts unchanged), **0 CAUSE-MISMATCH**. Only CSV delta
vs the per-side pins: `var:refuse-bake60`'s detail now also
carries the R3 `detail=[baked peak gate: ...]` string.

## Rerun (ff/enc-s5fu, S5FU stereo staging, 2026-10-16): 114 runs, 0 non-parity

Root cause (S5-FU): the uniform spectral gain 3.0 is wrong
for stereo. DEE stages stereo coefficients at x2.0 of the
forward correlation (offset-+2 aligned pre-IMDCT CODUMP
peak pairing: DEE/forward = 1.9995 on G7 997Hz, residual
-0.002 dB) and 5.1 at x3.0 (G5 spectral ratio ours/DEE =
0.99): every stereo file decoded exactly 3/2 (+3.52 dB)
hot on all correct decoders at the same dialnorm (G7:
DRP -30.17, frozen CLI -30.12 vs our monly -26.56).
Fix: `ENC_CORE_DIG_GAIN_STEREO 2.0` at the 6 stereo
spectral sites (`try_fit`, `try_fit_mixed`,
legs `quantize_frame`); all 6 multichannel sites keep 3.0.
Truth check (splice fixed bare frames into sync framing,
decode with `c-ac4dec`): 997 -30.08 vs DEE -30.12,
multitone-ext -33.05 vs -33.07, pseudomusic -33.67 vs
-33.71 (pre-fix: -26.56/-29.53/-30.15).

114 runs (111 + 3 new `st_sine997` pin vectors):
**34 PASS** (every 5.1/fold-851 case byte-identical — the
MC staging is untouched — plus 4 gain-invariant stereo
silences), **69 S5FU-FIXED** (both encode, identical CBR
lengths, bytes differ, iframes clean, lib bytes match
`parity/s5fu_goldens.txt`), **3 BOTH-FAIL**,
**2 LIB-AHEAD**, **6 DIVERGE-neutral**, **0
CAUSE-MISMATCH**. Stereo neutral ndiffs move with the
refs (sweep 3329->3059, sparse 279->306, clicks
2165->2247; silence stays 30, MC unchanged). Unit
`t_s5fu` pins the constants, the x2.0 legs ratio, and a
lib-bytes golden. Full unit + battery also pass under
ASan. The frozen CLI keeps x3.0, so S5FU-FIXED stays the
expectation until upstream syncs (see VENDOR.md).

## Matrix (all PASS unless noted)

- Stereo s16 wav: 11 contents x {2049, 0.5s, 5s} + 8
  length edges (1..96000 samples) + 4 extras = 45 runs
  (s5fu adds `sine997` dual-mono).
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
  ebu_r128/atsc_a85 (2), force-float `_f`-on-ints (4),
  refusal-cause pin `var:refuse-bake60` (1 BOTH-FAIL).
- R3 pins: self-generated `r3_hotimp_8192.wav` @ -24 (1) +
  `var:bake+12` on `st_sine1k_24000.wav` (1), both
  `expect="BOTH-FAIL"` (genuine peak-gate refusals).
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
| st_sparse_24000 | 306/4779 | 2 (s5fu: was 279) |
| st_clicks_24000 | 2247/4779 | 13 (s5fu: was 2165) |
| st_sweep_24000 | 3059/4779 | 13 (s5fu: was 3329) |
| mc_noise_24000 | 7356/9557 | 13 |
| mc_sweep_96000 | 23713/33451 | 48 |

Silence is the clean minimal case: only the 30 stamp bytes
in the priming frame differ. All else follows the bake gain.

## Reproduce

```sh
make && make harness OUTDIR=$SCRATCH && make unit OUTDIR=$SCRATCH
python3 tests/gen_vectors.py $VECDIR   # or reuse an existing set
ENC_PARITY_S=$SCRATCH ENC_PARITY_VEC=$VECDIR \
  python3 tests/parity_run.py   # expect 34 PASS + 69 S5FU-FIXED +
                                # 3 BOTH-FAIL + 2 LIB-AHEAD +
                                # 6 neutral, 0 non-parity
```

Full per-run rows: `parity_results.csv` (copied beside this
file) and scratch `parity-out/` (oracle `.ac4` + lib frames).
