# RUNBOOK-R3-NOTES — post-MUST-final refresh of the runbook + assembly protocol

Seat: `ff-runbook-r3` run R3-final (docs only, no Dolby invocation).
Base: master `224ec0b`. Clone: own `wt-final` in
`/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-r3/`, branch
`ff/doc-runbook-r3` (worktree identity only). Parent checkout
untouched, nothing pushed, nothing sent. Venue check first action:
`ssh 169.254.103.231` → `MacBook-Pro.lan`, Darwin 27.0.0 arm64,
2026-10-17T17:54:44Z (TB-only; 10.0.2.163 never used).

## What changed

`docs/INTEGRATE-RUNBOOK.md` (676 → 441 lines): full refresh to
post-MUST-final state. New §0 pin table (six union mboxes with
exact bytes 43263/507373/40708/650974/12693/162643, From counts
1/2/2/10/2/6, union commits `36b9eec`/`554b92b`/`91c214b`/`3b65f39`/
`50e7de5`), MUST-verify verdict table (A + B-A0 POST-OK; C/D/E
HOLD with debt refs), combo-8 HOLD status, send-sitting-2 sizes
(23 patches / 29 wire) and the B2 clock gate
`>= 2026-10-17T22:23:09Z`. Steps renumbered 0–11 in DAG order
A0→A→B→C→E→D with DO-NOT-SEND gates on the HOLD series.
DELETED stale pre-MUST material: re-cut-3/4/5 chains, staged-B,
24-patch counts, combo-5/6/7 as live, mail-1 prep, suite-watch
pending, N5-pending, D-P3 wiring-open (closed by M21), the old
step-6 24-stack worktree recipe. B2-landing mechanism kept
verbatim; historical cross-checks condensed to one paragraph;
MUST-final cross-check appended (combo-8 executed the mechanism
on union bytes, 11-var aggregate). Every claim cites a merged
master file + line.

`docs/ASSEMBLY-PROTOCOL-R2.md` (355 → 256 lines): R-MUST
refresh. Union send-pack tip table replaces branch tips; Rule 0
is the 23-patch DAG stack; Rules 1–3 carry the MUST-final
creator/chain facts (Changelog B-append/C-prepend/E+D-chain,
B-creates include, B-creates mak + appendix groups, 11-var
aggregate); Rule 4 adjacencies updated to the combo-8
characterization with the three HOLD seams attributed
(C01–C03 context, E2 strict tail, D toc-create collision);
Validation is combo-8 (pre-MUST combos HISTORICAL); Verdict is
HOLD with the 1-content + 4-mechanical cut list. Rule 5
mechanism kept + R-MUST cross-check.

## Verification log (this seat, 2026-10-17T18:02–18:04Z)

Every command the refreshed runbook tells the owner to run was
re-run here (dry-run where send). Full log:
`scratch/ff-runbook-r3/tmp/verify-r3final/verify.log`.

- `date -u +%Y-%m-%dT%H:%M:%SZ` → runs (18:02:22Z; gate correctly
  red for sending until 2026-10-17T22:23:09Z — gate working).
- `ifconfig | grep 169.254.103.231` → absent (exit 1), expected on
  the Studio docs seat; owner-link-only (same as the
  SEND-SITTING dry-run record).
- `git log --oneline -1` → `224ec0b ff/merged: ff/doc-must-verify`.
- `ls -l doc/post/*.mbox` → §0-exact sizes.
- `perl` non-ASCII sweep over mboxes + covers → 0.
- `grep -c "^From "` → 1 2 2 10 2 6 in DAG file order.
- `grep -n "incl. fate-source" COVER-B.txt` → line 33.
- Six `git mailsplit -osendX` → 1/2/2/10/2/6 files + six 0000
  cover copies shape-ok (run in `tmp/verify-r3final/ac4-send`,
  mirroring the owner `~/ac4-send` path).
- Six `git send-email --dry-run` (explicit file lists per
  SEND-SITTING) → 29 `Result: OK` (2+3+3+11+3+7), ZERO warning
  lines. NOTE: the glob `sendC/000*` matches only 10 of the 11
  C files (misses `0010`); the explicit 11-file form from
  SEND-SITTING is required — docs already use it.
- Zero mbox/cover drift `2def950..224ec0b` (only COMBO-RERUN-8,
  E-VERIFY, MUST-VERIFY, SEND-SITTING changed) — so MUST-VERIFY's
  base-`2def950` verdicts apply verbatim to 224ec0b bytes.

Not re-run (cited, not owner-gated): full `git am` assembly +
build + FATE — executed on identical master bytes by
`doc/post/COMBO-RERUN-8.md` and `doc/post/MUST-VERIFY.md`; the
combo-9 closer re-runs the recipe after the re-cuts land.
