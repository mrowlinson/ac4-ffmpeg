# WRAP-FUZZ-ROUND4 — CLEAN on master e72a4b9

Date: 2026-10-16. Tree: ac4-ffmpeg master `e72a4b9` (brief pin, static
throughout the run — staged core == current master core).
Verdict: **CLEAN** — full round-4 quotas pass (3000 iters, 120/120
batches, zero crash/hang/ubsan/oom) and every FINDING-1 repro stays fixed.
Seat: `ff-wrap-fuzz-round4-retry` (predecessor died server_error at
launch with zero work; this run redid the full quota set).
Scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-wrap-fuzz-round4-retry`
(full `WRAP-FUZZ-ROUND4.md` + `smoke1.log` + `pins.log` + `pin-*.log` there).
Parent repo: READ-ONLY (`git archive` export only). Out-of-tree build.
TMPDIR=scratch. No MBP/remote. No push. Fix nothing (nothing to fix).
ffmpeg base: myff @`3e6cd5e`, ASan/UBSan, same flags as
exec/rerun/round3/round4. lavc-dec delta round4-ref→staged
(`df3f2de..e72a4b9`): inclusion-guard remap on all 13 headers to
`AVCODEC_<file>_H` + README reword only (zero DSP/behavior change;
FINDING-1 lines unchanged). Corpora (explicit `--corpus` x2, same as
rerun/round3/round4 baseline): `ff-fuzz-wrap-harness/corpus` +
`ff-fuzz-dec/corpus`.

## Counts

| target | seed | iters | batches ok | crash | hang | ubsan | oom |
| wrap   | 1 | 1000 | 40/40 | 0 | 0 | 0 | 0 |
| corner | 2 | 1000 | 40/40 | 0 | 0 | 0 | 0 |
| seek   | 3 | 1000 | 40/40 | 0 | 0 | 0 | 0 |

3000 iters, 120/120 batches ok. `logs/` absent (zero non-ok batches).
No WRAPHARNESS-FAIL anywhere (quotas + pins + smoke).
Max RSS ~525-625MB vs 2048MB cap (oom bucket 0/120).

## FINDING-1 repros (must stay fixed: all PASS)

- Doc pins 3/3 rc=0, zero sanitizer hits:
  `wrap 13272178844 25 --repro 6`, `wrap 34507664996 25 --repro 2`,
  `corner 2654435771 25`.
- All 11 rerun batch-context crash batches re-run full N=25, rc=0,
  zero sanitizer:
  wrap 13272178844 (B4), 34507664996 (B12), 45125408072 (B16),
  66360894224 (B24); corner 2654435771 (CB0), 18581050381 (CB6),
  100868559220 (CB37); seek 29198793456 (SB10), 31853229231 (SB11),
  42470972307 (SB15), 58397586917 (SB21).
- N=50 isolated variants (`34507664996/50r2`, `13272178844/50r6`):
  rc=0, zero sanitizer. Total 16/16 prior-crash pins clean.

## New-vs-old diff

1. FINDING-1 STAYS FIXED on master `e72a4b9`: `ngis[i]` bounds
   check (`dec_lib.c:1387`) + `ngis[sel]<=0` reject (`:1398`) +
   `*ngi=0` init (`:1058`) all present (verified on the e72a4b9 blob
   AND the staged build tree). All 11 rerun crash batches + 2
   isolated pins + 3 doc pins pass.
2. NO NEW BUG: 3000/3000 quota iters clean across wrap/corner/seek,
   same seeds/batches/corpora/mode as round3/round4 (directly comparable).
3. Methodology unchanged from round4 (batch seeds
   `seed ^ (bno+1)*0x9E3779B9`, batch=25, ASan/UBSan env).

## Not run

flushprobe supplement: out of brief scope (quotas + FINDING-1
repros only), same as round3/round4.
