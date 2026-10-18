# RUNBOOK-R4-NOTES — post-reverify refresh of the runbook + assembly protocol

Seat: `ff-runbook-r4` (docs only, zero Dolby invocation).
Base: master `8c9ba74`. Clone: own `wt` in
`/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-r4/`, branch
`ff/doc-runbook-r4` (worktree identity only). Parent checkout
untouched, nothing pushed, nothing sent. Venue check first action:
`ssh 169.254.103.231` → Operation timed out (local host
`Mac-Studio.lan`); per venue rule this round ran LOCAL-ONLY.
TB-only respected all run; `10.0.2.163` never touched.

## What changed

`docs/INTEGRATE-RUNBOOK.md` (441 → 488 lines): full refresh to
post-reverify state. New §0 pin table (six FIXED mboxes with
exact bytes 43263/507373/40729/650794/12774/163076, From counts
1/2/2/10/2/6, union commits unchanged), reverify verdict table
(A + B-A0 + C + D + E ALL POST-OK with lift cites;
MUST-VERIFY HOLD rows marked HISTORICAL), debts-R2 assembly
verdict (23/23 plain-am zero-hand, build 34/0-ac4, fate-ac4
29/29 + A0D 3/3), drift-audit note (zero FF-drift + plain-N5
owner call), and the B2 clock gate KEPT with the D-seat live
re-confirm flag. Steps 0–11 kept in DAG order A0→A→B→C→E→D:
steps 5/6/7 HOLD language DELETED (re-cuts LANDED, proven by
the three reverify docs); per-series carries recorded instead
(B/C/D send-steps Pack lines stale pre-fix sizes, D N1 cover
word, E send-steps already refreshed). DO-NOT-SEND stays on
every series until combo-9/sweep-r2 land — pending, explicitly
NOT lifted. DELETED stale pre-reverify material: C/D/E HOLD
gated steps, combo-8-as-live, pre-fix mbox sizes, MUST-verify
as the live verdict. B2-landing mechanism kept verbatim;
MUST-final cross-check updated to the LANDED debt rows.
Every claim cites a merged master file + line.

`docs/ASSEMBLY-PROTOCOL-R2.md` (256 → 260 lines): R-R4
refresh. Fixed-bytes tip table with POST-OK + lift cites;
Rule 0 validated stack is 23/23 plain zero-hand + 29/29 +
3/3 (combo-8 numbers HISTORICAL); Rules 1–3 mechanism stands
with the mak debts marked LANDED; Rule 4 adjacencies rewritten
as RESOLVED with landed cites (C context refresh, E2 strict,
D toc-drop, link closed); Validation is the debts-R2 record
(plain 6/6 mboxes, 23 commits, zero hand); Rule 5 mechanism
kept + R-R4 cross-check. Verdict is POST-OK (assembly) with
the 5-item cut list marked LANDED and combo-9 + sweep-r2 as
the pending next.

`doc/post/RUNBOOK-R4-NOTES.md` (this file): change record.

Out of scope (noted, not done): B/C/D-OWNER-SEND-STEPS Pack
line refreshes (their seats' scope — E-seat precedent
E-R2-VERIFY.md:119-120); combo-9 closer run; sweep-r2.

## Verification log (this seat, 2026-10-18T00:58Z)

Every command the refreshed runbook tells the owner to run was
re-run here (dry-run where send). Full log:
`scratch/ff-runbook-r4/tmp/verify-r4/verify.log`.

- `date -u +%Y-%m-%dT%H:%M:%SZ` → 2026-10-18T00:58:06Z (gate
  time passed; live re-confirm still owed per D-R2-VERIFY).
- `ifconfig | grep 169.254.103.231` → absent (exit 1), expected
  on the Studio docs seat; owner-link-only.
- `git log --oneline -1` → `8c9ba74 ff/merged: ff/doc-D-reverify`.
- `ls -l doc/post/*.mbox` → §0-exact sizes
  (43263/507373/40729/650794/12774/163076).
- `perl` non-ASCII sweep over mboxes + covers → 0.
- `grep -c "^From "` → 1 2 2 10 2 6 in DAG file order.
- `grep -n "incl. fate-source" COVER-B.txt` → line 33.
- `git diff --stat 20bb917..HEAD -- doc/post/series-*.mbox` →
  EMPTY (zero mbox drift; reverify verdicts apply verbatim).
- Six `git mailsplit -osendX` → 1/2/2/10/2/6 files + six 0000
  cover copies shape-ok (run in `tmp/verify-r4/ac4-send`,
  mirroring the owner `~/ac4-send` path).
- Six `git send-email --dry-run` (explicit file lists per
  SEND-SITTING) → 29 `Result: OK` (2+3+3+11+3+7), ZERO warning
  lines.

Not re-run (cited, not owner-gated): full `git am` assembly +
build + FATE — executed on identical master bytes by
`doc/post/ASSEMBLY-DEBTS-R2.md` and the three reverify docs;
the combo-9 closer re-runs the recipe on the fixed bytes.
