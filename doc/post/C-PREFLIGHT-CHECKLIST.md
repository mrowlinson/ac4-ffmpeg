# C-PREFLIGHT-CHECKLIST — Series C encoder union (r6 content + DAG mechanics)

Branch: `ff/doc-union-C` from master `6905822`.
Gated 2026-10-17 in scratch `ff-union-C` (TMPDIR there);
parent repo untouched, nothing pushed, no MBP/remote, nothing sent.
Send procedure: `doc/post/C-OWNER-SEND-STEPS.md`.

Union driver: the C content seat (`da61f32`, M10-M18 fixed, 11
mails on B-r5) diverged from the stack seat (`3b65f39`, DAG
mechanics on stack-B). This pack is the one gated union: r6
content re-chained onto stack-B, mak in appendix shape
(per STACK-DAG.md), old C08 dropped (M17 negatives ride
Series B 2/2 as FATE behind the encoder ifeq). 10 mails.

M10-M18 all present, folded into their patches: M10 `*_tab`
rename (01/10), M11 EXPERIMENTAL + documented limits (03/10 +
04/10 + fate `-strict -2`), M12 opaque-cap drop (03/10), M13
sincos configure probe (02/10 + 03/10), M14 av_ allocators
(02/10, 298 calls), M15 TLS deletion + ctx-carried detail
(02/10 + 03/10 + cover), M16 version-bump drop (03/10), M17
negatives in B FATE + script gone (C08 dropped), M18 single
texi row onto B (04/10).

## Source line

- [x] Union C range — 10 commits, linear, no merges
  (`1dccbbf/cf1a396/b3dc278/4eed00c/27632d5/8c3b486/
  7a6df90/ad5e6a1/dcca457/80933ad`, in scratch
  `ff-union-C/ff`).
- [x] Base is stack A0+A+B (`69d2b70` + `d532e32/3ace213` +
  `c6984bf/b1dab00`; pristine root `3e6cd5e`). Mbox is a fresh
  `format-patch` of the C range (10/10, 650974 B).
- [x] Full DAG line in scratch `ff-union-C/ff-am` (A0+A+B+
  C-union+E+D): 23 commits on pristine `3e6cd5e`.

## Final gates (this wave, all green)

- [x] Stacked `git am` clean on B tip — rc=0, A0+A+B+C-union
  15/15 applied with plain settings + `--whitespace=error` on
  a fresh pristine clone. Applied C-tip tree == source C-tip
  tree (`c91a7064` both — mbox integrity proven).
- [x] DAG stack-am 23/23 on the fresh clone (A0/A/B/C-union/
  E/D), with the proven E 2/2 forward-fix (tail-CMD `-strict`
  context + GEN `-strict -2`; E-as-merged cannot apply under
  M11 — see Disclosed carries).
- [x] Full-stack build (A0+A+B+C): out-of-tree
  `configure --samples=suite --disable-doc --disable-debug` +
  `make -j18` exit 0, 34 warnings (baseline) / 0 ac4 / ZERO
  duplicate symbols; `nm` dupsym sweep empty (A+C link: L1
  stays closed under the `_tab` rename).
- [x] Encoder fate 5/5 at C-tip — rc=0: `fate-ac4-encode`
  (md5 `86828e84..`, 98193 B), `-encode-51` (md5
  `53a44aca..`), `-framecrc`, `-tail`, `-fold8` (goldens
  matched — union is byte-neutral vs r5/r6). B 16/16 passes
  on union-C. Codec registers `DEA.L. ac4`.
- [x] DAG-tip fate 28/28 (`fate-ac4`, suite): union-C + E2fix
  + B enc-neg `-strict` proof fixes, build rc=0 / 0 ac4
  warns. The two B enc-neg tests pass with the forward-fix
  (exit 234 + pinned message).
- [x] Experimental refusal: encode without `-strict -2` fails
  loud (rc=88 + `experimental codecs are not enabled`).
- [x] Per-step builds in stack order: C01/C02/C03/C09/C10 +
  tip all rc=0 with 0 ac4 warnings (reconfigure per step;
  C03+ needs the encoder-on config). Mak parse-checked
  (`make -n`) at C05/C06/C08.
- [x] `git show --check` zero on all 10 (no whitespace errors).
- [x] Subjects 1:1 cover<->mbox — all 10 distinctive subject strings
  appear exactly once in `COVER-C.txt` and once in the mbox
  (mechanical counts 1/1 x10).
- [x] Zero placeholders — zero `[OWNER` / `[TODO` / FIXME / XXX /
  YOUR NAME in cover or mbox.
- [x] Signoffs — `Michael Rowlinson <mrowlinson@gmail.com>` on all
  10 patches (10/10 `Signed-off-by` lines), ordered dates.
- [x] Goldens pinned — stereo `86828e84..` (98193 B) x2 and 5.1
  `53a44aca..` x2 in mbox patch 05/10 refs; cover carries the
  golden refs + `#extradata` stereo `0xb7a30a0b`, fold8
  `0xbafa0ae9`. Byte-identical to r5/r6.
- [x] MUST residue sweeps — zero encoder `*_tables.*`, zero raw
  allocators, zero `_Thread_local`/mutex/last_error, zero
  `REORDERED_OPAQUE`, zero `__sincos`/bare-sincos (bare call
  only inside the `HAVE_SINCOS` guard), zero `ac4-neg-check`
  refs, single `@tab IX @tab IX` row, `version.h` untouched.
- [x] ASCII-only — zero non-ASCII bytes in the mbox; expect zero
  `send-email` 8bit warnings.
- [x] Cover current — `COVER-C.txt` carries the DAG header, the r6
  per-patch notes (M10-M18), the 10-mail list (C08-drop noted),
  appendix + FATE + `-strict -2` testing lines. Provenance kept
  at the stack wording.
- [x] Nothing sent yet — no SENT marker in repo or mail-combined;
  only blank fill-in lines.
- [x] Checklists complete — this file + `C-OWNER-SEND-STEPS.md`
  (union revisions) written and committed.

## Carried gates (identical bytes, still stand)

- submit-fix: full 20-stack `git am` 20/20; configure + `make`
  exit 0, zero ac4 warnings; `.text` IDENT 25/26.
- MBP-R4BUILD: 20/20 `git am` on MBP, TREE-MATCH, build clean,
  `fate-ac4` 10/10 incl. stereo `86828e84..` + 5.1 `53a44aca..`.
- combo-6: 24-patch A+A0+B+C+E+D assembly record
  (`COMBO-RERUN-6.md`); L1 duplicate-symbol fix intact in the
  union (re-proven: this wave's A+C link + C goldens).
- Encoder output bytes r5→r6→union identical (fate goldens match
  with zero re-pins, including under the sin/cos fallback on
  macOS and the av_ conversion).

## Disclosed carries (reviewer-visible, non-blocking)

- E 2/2 forward-fix required at assembly (proven shape, NOT in
  this branch): mak-context touch (union tail CMD carries
  `-strict -2`) + GEN rule `-strict -2` (M11: the GEN encodes).
  E-as-merged (parent `6905822`) cannot apply to any M11 C.
  Exact 3-line diff in the union HO.
- B 2/2 forward-fix required (belongs in B-union or assembly):
  the two `fate-ac4-enc-neg*` CMDs need `-strict -2` (the
  experimental check precedes the layout gate; without it the
  tests fail with the experimental refusal instead of exit
  234). Proven: DAG-tip 28/28 with the fix.
- B 2/2 M17 mak comment is stale (`which Series C 08/11
  deletes`): there is no C08 in the union. Suggest rewording to
  the committed-script deletion at B-union.
- Generator wide rows (172: huffman 129 + aspx 43) kept verbatim
  per FIX-REPORT justification (upstream accepts
  generator-emitted table rows; rewrapping desyncs generators).
- Stacked slice, not standalone: applies onto stack B; the
  gated tree is the DAG order A0→A→B→C. Direct post waits on
  the one-stack (HOLD ALL still stands).
- `HAVE_SINCOS` is 0 on macOS (no public sincos there) and 1 on
  glibc: the fallback path is the macOS-tested one; Linux takes
  the sincos path with identical output bytes (same code as r6).

## Owner-at-post (fill when sending)

- [ ] Sent date + Message-IDs (0/10 .. 10/10): _______________
- [ ] Archive shows 0/10 + 01/10 .. 10/10 threaded in order.
- [ ] Mail-1 (separate, `samples-request@ffmpeg.org` + 15
  attachments) sent date: _______________ (starts no clock for C).

## Verdict

**READY-FOR-STACK** — Series C encoder union (r6 content M10-M18
+ DAG mechanics, byte-neutral, 10 mails). Posts only as the one
ordered stack (HOLD ALL still stands); do not post standalone.
