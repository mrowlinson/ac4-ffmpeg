# MBP-N5-BUILD — post-N5fix master battery on MBP

Date: 2026-10-16. Tree: ac4-ffmpeg master `2436f47` (N5fix merged).
Verdict: **BUILD-CLEAN + N5-GREEN, SMOKE-SPLIT** — all builds exit 0 with
zero ac4 warnings, N5 pins 14/14 green, ffmpeg-path smoke exits 0 but
still reports 46034 Hz (lavc-dec/core predates N5fix — re-vendor owed).

## 1. Reachability

- Transport: easy-ssh `--remote mbp --thunderbolt`, peer **169.254.103.231**
  (pinned `thunderbolt_host`). Attempt 1 of 3. Job `finished (exit 0)`.
- NVME Thunderbolt mount live (`~/mnt-nvme-tb`): tree tarball + samples
  shared, zero Thunderbolt sync bytes for payload.
- Parent repos untouched: no pushes, merges parent-only.

## 2. Tree shipped

- `tree-2436f47.tar`, sha256
  `0245ef618602f96d45aa4ec81dd0b8ac64e8fa2c8dce0a0c72273ffcc5872cf3`,
  173 entries, `git get-tar-commit-id` = `2436f47...`.
- MBP-side: sha verified OK, extracted to `~/ff-mbp-n5-build/tree`
  (153 files), sentinel `fate-n5/pins.txt` gpac line present.

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
- Wrap: `wiring.patch` check+apply OK, `ac4dec.c` + 23 `core/*` staged
  (26 files), STAGE-OK.
- Toolchain: `MacBook-Pro.lan`, macOS 27.0.1, clang 21.0.0
  (clang-2100.3.34.2), arm64, git 2.53.0, 10 CPUs.
- Configure out-of-tree (`--disable-doc --disable-debug`):
  **CONFIGURE_EXIT=0**. `make ffmpeg`: **MAKE_EXIT=0**, binary 22953448 B.
- Warnings: total 34 (matches tree baseline); **ac4-touching: 0**.

## 6. Decoder smoke (MBP, gpac vector)

- `./ffmpeg -strict experimental -i gpac...ac4 -map 0:a -f s16le`:
  **SMOKE_EXIT=0**, full 9.9 s decode, 5492736 B out.
- Rate label: `Audio: ac4, 46034 Hz` → **SMOKE-SR-OTHER**, bytes != shim
  pin 5727312. Root cause (tree fact, not battery flake): N5fix
  (`a7d1036`) touched `dec-lib/src/*` only; `lavc-dec/core` last changed
  at `85bdde8` (1344 ASCII) and lacks the s5.11/base×sf hunk
  (`sf_mult|out_sr`: 24 hits dec-lib vs 6 core). Q1 RE-VENDOR RULE owes
  a re-vendor + re-gate of `core/` before the ffmpeg path reports 48000.

## 7. Logs

- `logs/mbp-run.log` (full run), `logs/dec-lib-make.log`,
  `logs/dec-lib-test.log`, `logs/n5-pins.log`, `logs/configure.log`,
  `logs/make.log`, `logs/smoke.log`; script `mbp-n5build.sh`; tarball
  `tree-2436f47.tar`. `TMPDIR` = scratch `tmp/`.

## 8. Verdict

- BUILD-CLEAN (configure+build rc=0, zero ac4 warnings, both venues of
  the tree). N5-GREEN (gpac sr=48000 pinned on MBP silicon).
- FINDING (for parent): lavc-dec/core is pre-N5fix; ffmpeg-path smoke
  still 46034 Hz. Needs a re-vendor lane, not a battery re-run.
