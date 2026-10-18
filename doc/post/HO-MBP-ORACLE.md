# HO-MBP-ORACLE (ff/doc-mbp-oracle-r1, 2026-10-18)

## DONE
- (a) venue: MBP MacBook-Pro.lan, DRP 4.3.0.19350 headless OK, DME 4.0.0.2165 CLI present (help non-exiting, SIGTERM'd).
- (b) b81c10 ON MBP: run1 14/2/0 (stale ac4dec --wav root-caused first-hand) → run2 with c-ac4dec **24/0/0**. Live pins reconfirmed.
- (c) drift-audit: G8 + KDAF DRP wavs pulled; local shim compare → DIVERGE-expected table w/ byte evidence. NEW: DRP +2 frames on BOTH vectors.
- Doc: doc/post/MBP-ORACLE-R1.md. Remote /tmp/mbp-oracle-r1 DELETED (verified absent).

## Artifacts (NVME scratch ff-mbp-oracle-r1/, NOT in repo)
- pulled/: b81c10.log, b81c10-r2.log, G8/KDAF-decode.log (0B, quiet-success), G8-drp-20.wav, KDAF-drp-20.wav
- stage/, shimbuild/, drift-table.txt, compare_drift.py, HEARTBEAT.md (3 rounds)

## Flags for parent
1. `~/mnt-nvme-tb` EXISTS on MBP — brief said not mounted. Used scp-only anyway. Parent may want to re-gate the venue brief.
2. DME CLI `--help` doesn't exit — any future DME leg needs watchdog + owner ack.
3. DRP decode logs empty (harness quiet-on-success) — evidence = exit codes + wav shas + b81 pins.
4. DRP +2-frame shape on both vectors (G8 120/118 known; KDAF 902/900 new) — unexamined, pinned in doc.

## Next (not this lane)
- DME encode legs (needs flag 2 resolved). G8-mp4 variant if queued.
- Merge: parent-only. Branch ff/doc-mbp-oracle-r1, 2 files, no push.

DONE ff-mbp-oracle-r1
