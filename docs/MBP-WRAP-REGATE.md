# MBP-WRAP-REGATE — post-revendor MBP battery re-gate

Date: 2026-10-16. Tree: ac4-ffmpeg master `df3f2de` (re-vendor-N5 merged).
Verdict: **BUILD-CLEAN + N5-GREEN + SMOKE-48000** — re-gate PASSES.
The MBP-N5-BUILD SMOKE-SPLIT stale-core finding (`45f2826`) is CLOSED:
the ffmpeg path now reports 48000 Hz with POST bytes equal to the
shim pin (46034 Hz fully absent).

## 1. Reachability

- Transport: easy-ssh `--remote mbp --thunderbolt`, peer **169.254.103.231**
  (pinned `thunderbolt_host`, `transport: thunderbolt`). Shell check
  attempt 1 of 3, submit attempt 2, status attempt 3. Job `finished
  (exit 0)`.
- NVME Thunderbolt mount live (`~/mnt-nvme-tb`): tree tarball + samples
  shared, zero Thunderbolt sync bytes for payload.
- Parent repos untouched: no pushes, merges parent-only.

## 2. Tree shipped

- `tree-df3f2de.tar`, sha256
  `9d745c529877fbeb2ad15d932300e14da6cb0b36e7d0c2eb3c39a43c94ec7053`,
  186 entries, `git get-tar-commit-id` = `df3f2de...`.
- MBP-side: sha verified OK, extracted to `~/ff-mbp-wrap-regate/tree`
  (166 files), sentinels `fate-n5/pins.txt` gpac line +
  `lavc-dec/core/ac4_src.h` present + `sf_mult` hunk in
  `lavc-dec/core/dec_lib.c` (re-vendored core proven in-tree).

## 3. dec-lib (MBP, MBP-built shim)

- `make -j10`: **MAKE_DECLIB_EXIT=0**, **0 warnings** (`-Wall -Wextra
  -Wpedantic`). `build/shim` 316888 B, `build/libac4d.a` 302536 B.
- `make test`: **SRC_TEST_EXIT=0**, `ac4_src_test: ALL PASS`.

## 4. N5 pins (MBP, `fate-n5/validate.py`, MBP-built shim)

- **N5_EXIT=0, 14/14 PASS**: gpac FMT sr=48000 ch=6 frames=298 note=pcm,
  5727312 B, pcm pin `8b41f7cf`, wav-sr 48000; sf4 sr=192000 exit-3
  lead-trunc; fs1 sr=48000 exit-3 lead-trunc.

## 5. ffmpeg + lavc-dec (MBP, out-of-tree)

- Base: clone NVME `ff-combo-rerun-5/wt`, checkout `3e6cd5e`, BASE-CLEAN.
- Wrap: `wiring.patch` check+apply OK, `ac4dec.c` + 24 `core/*` staged
  (27 files, +1 `ac4_src.h` vs pre-revendor 26), STAGE-OK.
- Toolchain: `MacBook-Pro.lan`, macOS 27.0.1, clang 21.0.0
  (clang-2100.3.34.2), arm64, git 2.53.0, 10 CPUs.
- Configure out-of-tree (`--disable-doc --disable-debug`):
  **CONFIGURE_EXIT=0**. `make ffmpeg`: **MAKE_EXIT=0**, binary 22969960 B.
- Warnings: total 34 (matches tree baseline); **ac4-touching: 0**.
  The 9 `error`-grep lines are byte-identical to the pre-revendor
  baseline (CHECK_ERROR context + error.o names), zero real errors.

## 6. Decoder smoke (MBP, gpac vector)

- `./ffmpeg -strict experimental -i gpac...ac4 -map 0:a -f s16le`:
  **SMOKE_EXIT=0**, **SMOKE-SR-48000**, 46034 Hz absent (0 hits),
  **SMOKE-BYTES-POST**: 5727312 B == shim pin 5727312.
- Closes the `45f2826` finding: `lavc-dec/core` now carries the N5fix
  (base_fs x sf_mult, s5.11 converter, 4-arg flush) and the ffmpeg
  path labels OUTPUT values (gpac 48000 Hz).

## 7. Logs

- `logs/mbp-run.log` (full run), `logs/dec-lib-make.log`,
  `logs/dec-lib-test.log`, `logs/n5-pins.log`, `logs/configure.log`,
  `logs/make.log`, `logs/smoke.log`; script `mbp-wrapregate.sh`; tarball
  `tree-df3f2de.tar`. `TMPDIR` = scratch `tmp/`.

## 8. Verdict

- BUILD-CLEAN (configure+build rc=0, zero ac4 warnings, both venues of
  the tree). N5-GREEN (gpac sr=48000 pinned on MBP silicon).
  SMOKE-48000 (ffmpeg path 48000 Hz + POST bytes, stale 46034 gone).
- FINDING: none. MBP venue re-gated; ready for parent merge.
