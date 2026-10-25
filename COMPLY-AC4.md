# COMPLY-AC4: ffmpeg-rules compliance lane (ff/comply1)

Base: `ff/dec-lib` sealed set `upstream/submit1/` (23 patches on
`b593998340c76a8269515448b565c93d060ae8b4`).
Re-cut set: `upstream/comply1/comply-0001..0023.patch` +
`SHA256SUMS-comply.txt` (this lane). Verify tree:
`/Volumes/NVME/ac4-ffmpeg-ws/scratch/comply1/verify`
(applied `git am --signoff`, tip tree `77db959af42946edf895f43566ad689680bc7efb`).

## §5 new-codec checklist (12)

| # | item | verdict |
|---|------|---------|
| 1 | av_cold init/close | PASS: decoder init/close + encoder init/close `av_cold`; +6 one-per-open helpers annotated in re-cut (state/imm/5x/dsi/pbw) |
| 2 | long_name | PASS: `CODEC_LONG_NAME` decoder + encoder; parser N/A (no such field) |
| 3 | version bump | FIXED in re-cut: `libavcodec/version.h` MINOR 25→26 in 03/23 (was missing) |
| 4 | allcodecs/allformats | PASS: externs + generated codec_list; lavf entries pre-exist in base |
| 5 | AVCodecID + codec_desc | PASS, pre-exists in base (`AV_CODEC_ID_AC4` + desc) |
| 6 | FourCC riff.c | N/A: no AVI/WAV FourCC assigned to AC-4 (same shape as E-AC-3 comment in riff.c); mov `ac-4` tag added in 20/23 |
| 7 | Makefile rules | PASS: decoder/encoder/parser OBJS, incl. per-object float flags |
| 8 | general_contents.texi | PASS: `AC-4 IX IX`, alpha order kept |
| 9 | Changelog | PASS: 5 entries (04/08/16/18/20) |
| 10 | configure deps | PASS: `ac4_demuxer_select=ac4_parser`, sincos probe, per-object flags |
| 11 | git add files | PASS: `git status` clean, am 23/23 |
| 12 | standalone build | PASS 3/3: `--disable-everything` decoder-only / encoder-only / parser+demuxer(+muxer): cfg rc0, make rc0, 0 AC-4 warnings; functional decode + encode + standalone roundtrip |

## §6 patch-submission checklist

- `make fate`: PASS, 3121/3121, rc0, 0 FAIL (final lib-final tree; includes fate-source CLEAN and all 13 fate-ac4 legs; suite-clip legs do not register without samples, see B1).
- format-patch: 23/23 `[PATCH 01/23..23/23]`, re-apply 23/23 clean, tip trees identical.
- signoffs: 23/23 commits carry one `Signed-off-by` each.
- subjects: prefixed, numbered, match sealed subjects.
- minimal: mov/movenc/mpegtsenc/ac4dec hunks all functional; no cosmetics (reviewed).
- tabs/trailing ws: `git diff --check` clean; tabbed lines only REQUIRED make-recipe tabs in `tests/fate/ac4.mak`.
- license headers: all new files carry FFmpeg LGPL headers (fate-source license section clean).
- alpha order: allcodecs/parsers/Makefile/mov-table/doc entries verified in order.
- regression tests: 13/13 self-contained fate-ac4 PASS (main + ASAN); 16 suite-clip legs BLOCKED (no fate-suite samples).
- damaged data: trasher(raw+mp4+ts) + noise bsf(10/100) 5/5 graceful, peak RSS ≤120MB; ASAN decode of all three: 0 reports.
- ASAN: full fate-ac4 13/13 rc0, 0 sanitizer errors.
- patcheck: every patch run; residual hits all triaged as false positives with base-tree precedent (details below); tool itself partly broken on macOS (BSD `xargs -d`), noted.

## Fixes applied in re-cut (6, all in-series)

- F1 (03/23): `libavcodec/version.h` MINOR 25→26 (checklist §5 item 3).
- F2 (02/23, 03/23): `_ac4d_self` → `ac4d_self` (reserved identifier, file scope).
- F3 (21/23): parser-test feeds via zero-padded copies (API contract: input + 64 readable bytes); found by ASAN stack-buffer-overflow in `ff_combine_frame`.
- F4 (22/23): mp4-roundtrip test extradata `av_memdup(dsi, len+PAD)` → malloc+memcpy (global-buffer-overflow; found by ASAN).
- F5 (06/07/14): 19 header guards → `AVCODEC_*` form (fate-source gate was red).
- F6 (02/14): `av_cold` on 6 once-per-open init helpers (+3 `attributes.h` includes).

Re-gated after re-cut: rebuild rc0 (0 warnings outside upstream macOS-SDK `tls_securetransport`),
fate-ac4 13/13, ASAN fate-ac4 13/13, fate-source CLEAN, standalone decoder rebuild + functional decode,
re-apply 23/23 identical trees, patcheck + `--check` triaged.

## patcheck residuals (all reviewed, no change)

- `for (;;)` (~10 hits): upstream idiom, 135 uses in base tree.
- `x == 0` style (~40 hits): suggestion only; base uses it widely.
- `__GNUC__`/`__cplusplus`/`__VA_ARGS__`: standard guards/macros.
- `double ;` = the `for(;;)` hits above.
- `divide by 2^x`: comments (`1/2/4`), float `/2048.0`, `/8` rounding idiom, python test scripts.
- `missing } prior to else`: unbraced single-statement else (upstream style) + the word "else" in comments.
- tabs: only required make-recipe tabs in `tests/fate/ac4.mak`.
- Changelog-per-patch: series groups Changelog hunks in doc patches (04/08/16/18/20); upstream-accepted series shape.
- `assert()`: 47 in 4 new lavc files; 10 pre-existing upstream lavc files use bare `assert()` — kept (debug-only; `av_assert0` would turn them into release aborts on untrusted input).
- `av_cold` remaining hints (append_frame/append_packet/ac4_append/qmf/ebr): hot paths, correctly NOT cold.
- `__x86`/`Binary file matches` on 0007: macOS BSD-grep/patcheck artifact; patch has 0 NUL bytes (verified by byte scan).

## BLOCKED (not faked)

- B1: 16 suite-clip decoder FATE legs need `ac4/` fate-suite clips (not in suite). Only the standard missing-samples shape; 0 other failures. Needs samples mail + suite upload (owner-gated, as in Oct-18 send).
- B2 (dropped, was provisional): full `make fate` goes green without samples — sample-conditional tests (incl. all 16 suite-clip legs) do not register, so the runnable set is 3121/3121 PASS. No environmental failures observed. B1 remains the only BLOCKED item.

## Files changed

- `upstream/comply1/`: 23 re-cut patches + `SHA256SUMS-comply.txt` (new dir; sealed `upstream/submit1/` untouched).
- This report: `COMPLY-AC4.md` (lane root).

No merges performed. No mail sent. No remote push (see DONE line for remote state).
