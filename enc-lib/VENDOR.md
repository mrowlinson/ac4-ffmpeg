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
block). Verify with `diff` (expect 0 `^<` lines) plus
`tests/unit.c` `_m`-vs-original equivalence.
