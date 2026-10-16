# VENDOR.md: vendored encoder sources

Source: `dolby-ac4-cleanroom` @ `b60852043`
(`watch-2867: stand-down holds (nothing owed)`), read-only copy.
Date: 2026-10-16. The cleanroom repo was never written to
(`git status` clean before and after) and never built in place.

## File set (30 files, `src/`)

Core (whole-stream drivers + 5.1 rate loop):

- `enc_core.h`, `enc_core_enc.c`, `enc_core_long51.c`,
  `enc_core_fit51.c`

Link deps (stateless per-frame units, untouched):

- `bits.c/.h`, `asf.c/.h`, `mdct.c/.h`, `loudness.c/.h`,
  `enc_mc3.c/.h`, `enc_jshort.c/.h`, `enc_tail59.c/.h`,
  `enc_aspx.c/.h`, `enc_acpl.c/.h`, `huffman_tables.c/.h`,
  `aspx_tables.c/.h`, `sfb_offsets.c/.h`, `pycompat.c/.h`

NOT vendored (CLI layer, stays out of the lib): `enc_cli_run.c`,
`enc_cli.c`, `enc_eopts.c`, `enc_lm.c`, `enc_manifest.c`,
`enc_help_text.c`, `report.c`, `trim.c`, `main_cli.c`.

## Patch delta (H1 caller-measured entries)

Only 2 files differ from upstream, purely ADDITIVE
(0 removed lines in each; originals untouched and still used
as the equivalence oracle in `tests/unit.c`):

- `enc_core.h`: +66 lines — `enc_core_measure_t` +
  `enc_core_encode_pcm[_mc][_f]_m` declarations.
- `enc_core_enc.c`: +538 lines — the four `_m` definitions
  appended at end of file. Each duplicates its original's
  bake/stamp prologue verbatim with the measure (`m/f/tf/
  peak`) supplied by the caller; `ms==NULL` selects the
  NEUTRAL path (no bake, no stamp, `flat_file=0`).

Regeneration: re-copy the 30 files, re-apply the appended
`/* ac4enc-lib H1 */` blocks (header block + trailing .c
block) plus the S1 fix hunks below. Verify with `diff`
(expect 0 `^<` lines outside the S1 hunks) plus
`tests/unit.c` `_m`-vs-original equivalence.

## Fix delta (ff/enc-libfix, S1, 2026-10-16)

No longer purely additive: `enc_core_enc.c` carries a
behavioral fix + diagnostic (upstream `dolby-ac4-cleanroom`
left untouched, read-only oracle).

- Root cause: the sparse-iframe fr0 stamp is legitimately
  11 bytes (`enc.py compose_sub0` sparse branch), but all 4
  stereo bake prologues demanded `nfr0 == 12` → silent -1
  on sparse-impulse input (S1).
- Fix (4 stereo drivers): accept `sparse ? 11 : 12` and
  store the actual length (`n_fr0 = nfr0`; downstream
  splice is length-agnostic).
- Loud diagnostic: `enc_core_last_error()` (new, declared
  in `enc_core.h`) + `ac4enc_last_error()` passthrough;
  all 8 drivers clear it on entry and the 8
  stamp-composition checks record refusals; the store is
  `_Thread_local` after the r2 merge (B1), and `t_sparse`
  pins the empty-on-success contract (the r2 harness
  prints M1 subcode/stage on failures).
- `diff` vs upstream now shows the fix hunks as `^<`/`^>`
  pairs in `enc_core_enc.c` (8 check sites + `#include
  <stdio.h>` + error block); everything else still 0 `^<`.
## r2 delta (ff/enc-libr2: engine split, upstream-readiness)

The v1 "0 removed lines" claim is superseded: r2 refactors the
core drivers into an incremental engine (review BLOCKERs B1/B2
+ MAJORs). Upstream source unchanged; all edits are ours:

- `enc_core_enc.c`: `encode_pcm_frames` /
  `encode_pcm_mc_frames` loop bodies moved into the appended
  `/* ac4enc-lib r2 */` engine (ctx-owned scratch, sliding
  window, orient ring, positional CBR/iframe rules); the
  drivers are now thin wrappers (feed all, drain with sync
  wrap), byte-identical by construction (parity + equivalence
  gates). All 10 mutable function-static scratch tiles
  removed; `is_iframe` subsumed by rule predicates (deleted).
  `+errno.h/+limits.h/+enc_step.h` includes.
- `enc_core.h`: untouched by r2 (no core public-API change;
  the engine API is the private, lib-owned `enc_step.h`).
- `ac4enc_lib.[hc]`: rewritten over the engine (incremental
  API); `tests/` updated to the incremental protocol.

Regeneration for r2: re-copy upstream, re-apply H1 blocks,
then re-apply the r2 branch (or cherry-pick `ff/enc-libr2`
c1). Gates: unit (incl. `_m`-vs-original + lib-vs-core
equivalence) + parity 100/102.

## r3 delta (ff/enc-libr3: loud peak-gate refusals, ours)

Behavior-preserving: refusal SETS are unchanged (policy (a));
only diagnostics are added. Upstream `dolby-ac4-cleanroom`
left untouched.

- `enc_core_enc.c`: `peak_fail()` + calls at all 8 baked-peak
  gates (4 stereo `peak`, 4 mc `pk`); refusals now record
  `(entry, baked, gain, in_peak)` via `enc_core_last_error()`.
- Engine (`enc_core_enc.c` + `enc_step.h`, lib-owned):
  EBAKE failing-sample locus (`err_ch/idx/val`) +
  `enc_step_err_peak` / `enc_step_err_sample` accessors.
- `ac4enc_lib.[hc]`: sticky `err_detail` + public
  `ac4enc_err_detail()` (gate + values for the bake
  subcodes; cleared by `record`/`reset`, NULL-safe).
- `tests/`: harness prints `detail=[...]` on failures;
  `unit.c` gains `t_r3` (SPAN-encode pin, stereo/mc peak
  pins incl. core messages, bake_round locus pin, target-0
  controls); `parity_run.py` gains 2 pinned `BOTH-FAIL`
  cases (self-generated hot-impulse vector + bake+12).

## minors delta (ff/enc-lib-minors, EL-m1/m7/m14, 2026-10-16)

- m1: all PCM/baked int peak scans (8 core prologues + the
  r2 engine ingest scan) use `enc_core_abs_sat` (new
  `enc_core.h` static inline): `abs(INT_MIN)` UB becomes a
  saturating INT_MAX. Byte-identical on every other input
  (equivalence + parity gates re-pass; `t_absmin` pins the
  saturation under UBSan). `asf.c` keeps `abs()` (quantized
  domain, assert-bounded, INT_MIN unreachable).
- m7: deleted the two vendored-but-unused symbols
  (`enc_core_encode_stream`,
  `enc_core_fold_8ch_to_51_meter`) + their `enc_core.h`
  declarations (zero in-repo callers; `nm` confirms them
  gone from `libac4enc.a`).
- m14: file-header comments no longer carry "C port of
  encoder/*.py" provenance (comment-only, no code change;
  deeper behavioral Python notes stay).
