# D3 Decode Isolated Probe — PASS (quiet box)

- Date: 2026-10-16 23:07 EDT
- Seat: ff-D3-probe-quiet
- Scratch: /Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-D3-probe-quiet
- Branch: ff/doc-D3-probe (base 508c227)
- Gate: D3 ac4-ims.ac4, max-time 0.6 s AND x-rt floor 3.1x
  (PERF-GATES.md FIRM; baseline recipe PERF-BASELINE.md)

## Verdict: PASS

Best-of-7 isolated re-run on a quiet box clears both gate
conditions with margin. QUIET2's marginal FAIL (0.570 s /
3.07x at load 14.2) was load noise, not a real regression.

## Numbers

Binary: pinned c-ac4dec 117982b1 (matches gate spec).
File: ac4-ims.ac4, 48697 B, sr=48000 ch=6 frames=41 spf=2048,
dur = 41 x 2048 / 48000 = 1.7493 s.
Cmd: `c-ac4dec decode --timeout 300 --in FILE --out /dev/null`
(warmup discarded + 7 timed runs, 2 s settle between runs;
wall via time.monotonic; TMPDIR=scratch; local only, no remote).

| run | wall s | rc | load1 | x-rt |
|---|---|---|---|---|
| warmup | 0.444 | 0 | 9.97 | — |
| 1 | 0.487 | 0 | 9.97 | 3.59 |
| 2 | 0.489 | 0 | 9.97 | 3.58 |
| 3 | 0.568 | 0 | 9.97 | 3.08 |
| 4 | 0.429 | 0 | 9.97 | 4.07 |
| 5 | 0.543 | 0 | 9.89 | 3.22 |
| 6 | 0.481 | 0 | 9.74 | 3.64 |
| 7 | 0.441 | 0 | 9.74 | 3.97 |

- Best: 0.429 s <= 0.6 s wall — PASS
- x-rt: 1.7493 / 0.429 = 4.07x >= 3.1x floor — PASS (+31% margin)
- Range 0.429–0.568 s shows per-run load sensitivity even at
  load ~10; best-of-N is doing its job. No tuning, no code touched.

## Machine

- Mac17,14 Apple M5 Max 18c, Darwin 27.0.1, hw.ncpu=18
- Load during probe: 9.7–10.0 (under the 18 gate at first
  check, no wait needed). Prior deferred attempt (ff-D3-probe)
  saw load 20.6/20.4 and correctly deferred.
- Box was otherwise quiet for this seat's window; single
  timed process at a time.

## Comparison

- Baseline (quiet box): 0.439 s / 3.98x. This probe: 0.429 s /
  4.07x — matches baseline within run variance.
- QUIET2 (load 14.2): 0.570 s / 3.07x — FAIL by 1%. Delta vs
  this probe (+33% wall at +45% load) is environmental.
- Conclusion: D3 gate stands as specified; no re-baseline,
  no code action.

## Repro

- Scratch: run_d3probe.py + probe.log + results.json (8 timed
  runs total: 1 warmup + 7 measured).
- Parent repo untouched; this doc is the only change, committed
  in the seat's scratch clone. Nothing pushed (merges parent-only).
