# HO-SEND-SITTING-3 — MUST-final POST-OK send pack handoff

Seat `ff-send-sitting-3`, branch `ff/doc-send-sitting-3` @
`717941a` + this commit. Scratch
`/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-send-sitting-3/`
(TMPDIR `tmp/`), build `/Volumes/NVME/ac4-ffmpeg-ws/build/
ff-send-sitting-3` (kept). Venue: MBP-TB `169.254.103.231`
DOWN first-hand (ssh timeout, `Host is down`) → LOCAL-ONLY;
zero Dolby-binary invocation (docs + dry-run only);
`10.0.2.163` never touched. NOTHING sent, nothing pushed —
actual send is the owner action (no send link on this seat).

## What changed (4 files + this HO)

- `doc/post/series-D-mp4.mbox`: N1 fix, 1 word. `Subject:`
  line of 5/6 drops ` test` (`...roundtrip test` →
  `...roundtrip`, L2313), matching `COVER-D.txt:42` first line.
  Only the header touched — the 4 other `roundtrip test`
  strings are patch-body code/comments, verified intact.
  Size 163076 → **163071** (-5 B).
- `doc/post/SEND-SITTING.md`: send-ready refresh. §1 pin
  `717941a`, sizes A0 43263 / A 507373 / B 40729 / C 650794 /
  E 12774 / D **163071** (all re-measured first-hand);
  23 patches / 29 wire; plane order A0→A→B→C→E→D with
  POST-OK cites (combo-9 POST-OK, B2 ALL READY, sweep-r2);
  clock-gate satisfied note (expired first-hand, this seat
  2026-10-18T01:06Z + B2 + combo-9); §8 N1 fold step;
  new send-sitting-3 dry-run record (old record kept, scoped).
  Owner-at-post boxes stay UNCHECKED (DO-NOT-SEND).
- `doc/post/D-OWNER-SEND-STEPS.md`: Pack + §0 sizes →
  163071 + N1 note; §1 5/6 subject → `...roundtrip`
  (was wrapped `roundtrip test`); §1 N1 fold `sed` step.
  A/A0/B/C/E send-steps untouched: their mboxes + all six
  covers are blob-SAME `717941a` vs `fc5573a`, so the R2
  MATCH verdicts carry — verified, not assumed.
- `doc/post/D-PREFLIGHT-CHECKLIST.md`: scope-note head →
  163071 + N1 (R2 FIXED-head pattern); body scoped as-is.

Era records keep era bytes by design (NOT edited):
COMBO-RERUN-9, B2-POST-READY, COVER-SIZE-SWEEP-R2,
D-R2-VERIFY, ASSEMBLY-DEBTS-R2, COVER-SIZE-SWEEP (R1).
PREPOST-REVIEW's `roundtrip test` (L204) is unrelated T3
prose, left verbatim.

## N1 zero-impact proof (re-proven first-hand)

- Assembly: pristine-`3e6cd5e` base (own `ff/` copy,
  reset+clean verified), plain `git am --whitespace=error`
  A0→A→B→C→E→D with the N1-fixed D mbox: **23/23 rc=0**,
  HEAD `e5d41ba`, tree `1601eddc487752942d663a9bd3098d16991397a7`
  == combo-9 proof tree **byte-exact** (subject-only change
  alters the commit message, never the tree).
- Statics: subjects 23/23, zero `roundtrip test`;
  `git diff --check` 0 lines; mailsplit D 6/6.
- Build (out-of-tree, `configure --disable-doc
  --disable-debug`, `make -j18`): rc=0/rc=0, 34 warnings,
  **0 ac4** (matches combo-9 / D-R2 exactly).
- fate-D subset (`SAMPLES`=13-file mirror copy):
  `fate-ac4-parser`, `fate-ac4-mp4-roundtrip`,
  `fate-ac4-ts-demux`, `fate-ac4-roundtrip` — **rc=0, 4/4
  TEST, zero fail/error**.
- T3 determinism: GEN md5
  `86828e84ccecb4171d775806616e0e93` == D-R2 pin;
  delete+rebuild byte-identical (`cmp`).
- Aggregate `fate-ac4`: **rc=0, 29/29 TEST, 0 fail**.

## Dry-run log (git 2.56.0, 2026-10-18T01:06Z)

- Repo bytes: `mailsplit -osendX` 6/6 rc=0 (1/2/2/10/2/6 =
  23 files); `git send-email --dry-run` all 29 wire
  messages (2+3+3+11+3+7): **29 `Result: OK`, 0 warning
  lines**. Non-ASCII 0 (mboxes + covers); signoffs
  2/1/2/10/2/6; `^From ` 2 1 2 10 2 6; COVER-B L33
  `incl. fate-source` present. Log: `logs/
  sendemail-dryrun.log`.
- N1 fold variant (sendD/0000 copy minus wrapped L43
  `  test`, 1-line diff): D thread **7/7 OK, 0 warnings**;
  folded-cover↔mbox subjects **6/6 identical**. The §8/§1
  `sed -i '' '43{/^  test$/d;}'` recipe ran verbatim here
  (BSD sed) and reproduces the folded copy byte-exact
  (`cmp`). Log: `logs/sendemail-dryrun-Dfold.log`.

## First-hand notes for parent

- N1 anatomy: the cover NEVER lacked the word — L42-43 wrap
  the 5/6 subject (`...roundtrip` + `  test`), so cover
  (unwrapped) already equalled the mbox. The brief-ordered
  mbox-side fix makes the mbox subject == cover L42, and the
  send-time fold drops the wrap residue: wire identity both
  line-based and logical. Repo cover stays frozen (R2 send
  artifact); the 1-line fold applies to the sendD/0000 COPY
  only, per D-R2-VERIFY's own "1 word at send" prescription.
- No RED: every gate green first-hand. Two own probe errors
  caught and corrected mid-seat (range-`show` vs `diff
  --check`; BSD-sed fold pattern) — final numbers above are
  from the corrected probes; logs hold the true runs.
- Owner-at-post remainder: step-0 TB-link check (owner link
  only), six live sends in A0,A,B,C,E,D order with the §8
  fold, Message-ID record. No owner taps beyond the send.

## Evidence map (`scratch/ff-send-sitting-3/`)

`wt/` (branch `ff/doc-send-sitting-3`), `ff/` (assembly tip
`e5d41ba`), `samples/ac4/` (13), `dry/send{A0,A,B,C,E,D}[,-fold]`
(29 + 7 wire files), `logs/am-{A0-head,A-decoder,B-fate,
C-encoder,E-ts,D-mp4}.log` (6/6 rc0), `logs/configure.log`,
`logs/make.log`, `logs/fate-D.log` (4/4), `logs/fate-ac4.log`
(29/29), `logs/fate-T3-run2.log` + `tmp/rt1.ac4` (T3
determinism), `logs/sendemail-dryrun{,-Dfold}.log` (29 + 7
OK), `tmp/recipe-test-0000` (verbatim recipe proof),
`HEARTBEAT.md`. Build:
`/Volumes/NVME/ac4-ffmpeg-ws/build/ff-send-sitting-3`.
