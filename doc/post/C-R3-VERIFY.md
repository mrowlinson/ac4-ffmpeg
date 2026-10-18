# C-R3-VERIFY — union-C HOLD lift on fixed master mboxes

Seat `ff-C-reverify`. Base master `fc5573a` (first-hand; own clone
`scratch/ff-C-reverify/wt`, branch `ff/doc-C-reverify` holds only
this file). Parent repo untouched, nothing pushed, nothing sent. All
builds out-of-tree (`build/ff-C-reverify`), TMPDIR in scratch. Suite
= copy of the must-verify mirror (16 files under `suite/ac4/`).

Venue: MBP TB probe `ssh 169.254.103.231` TIMED OUT (local host
`Mac-Studio.lan`). Per venue rule this round ran LOCAL-ONLY
pure-FFmpeg: zero Dolby-binary invocation. Dolby-dependent checks:
none in scope (a)–(f).

Lifts the MUST-VERIFY union-C HOLD (`91c214b`: 6-line context
re-cut; content green). Fixed mboxes landed in `20bb917`
(debt 1 = C01/C02/C03 r5→r6 refresh). This round re-gates the
fixed master bytes end to end.

## Gate table (all green)

| Gate | Check | Result |
|---|---|---|
| (a) | A0+A+B+C plain `git am --whitespace=error` on pristine `3e6cd5e` | **15/15 rc=0**, 15 commits, zero hand (`am-A0ABC.log`) |
| (b) | C01–C03 refresh context-only + mechanical drift zero-content-change (fixed-vs-shipped mbox) | **proven**: normalized diff = exactly 6 context lines (3× 2-line Makefile decoder comment r5→r6); From/index/`@@`/blank churn mechanical; zero `+`/`-` content lines (`C-shipped-vs-fixed.diff`, `C-*.norm`) |
| (c) | build rc=0, 0 ac4 warnings | **rc=0**, 34 warn lines baseline / **0 ac4**; `DEA.L. ac4` + decoder rows (`configure-C.log`, `make-C.log`) |
| (d) | fate-Cenc 5/5 pinned md5s | **5/5 rc=0**: encode (`86828e84ccecb4171d775806616e0e93`), encode-51 (`53a44acaa21853f91cfef7dbdcccb67e`), framecrc, tail, fold8 — byte-identical to r5/r6 (`fate-Cenc.log`) |
| (e) | full `fate-ac4`: enc-neg pair GREEN via B debt-5 fix (was the 1 C-tip red) | **23/23 rc=0, 0 FAIL**; `ac4-enc-neg71` + `ac4-enc-neg514` pass; tree CMDs carry `-strict -2` (`fate-C.log`) |
| (f) | statics: subjects 1:1, signoffs 10/10, ASCII 0, placeholders 0 | **10 mails, 10/10 signoffs, ASCII 0, placeholders 0**; subjects 1:1 ×10 cover↔mbox (wrap-insensitive; 07/10 + 10/10 fold in both files); `git show --check` 0 on C range |

Fixed-vs-shipped bytes: shipped `650974` B (matches MUST-VERIFY) →
fixed `650794` B (matches R2); delta = context refresh + `@@`
`-167→-170`, `-206→-209`, `-201→-204` drift + From/index churn.

## Verdict

**POST-OK** — Series C encoder (fixed master mbox, 10 mails) posts
only as the one ordered stack (HOLD ALL still stands); do not post
standalone.

## Carry (non-blocking, owner-at-post)

- N1: `C-OWNER-SEND-STEPS.md` still quotes the shipped size
  `650974 B`; master fixed mbox is `650794 B`. Refresh the size
  line (and gate master `6905822` → fixed line) at send time.
  Content gates unaffected.

## Logs

`HEARTBEAT.md` R0–R4. `am-A0ABC.log`, `C-shipped-vs-fixed.diff`,
`C-shipped.norm`, `C-fixed.norm`, `configure-C.log`, `make-C.log`,
`fate-Cenc.log`, `fate-C.log`, `series-C-encoder.SHIPPED.mbox`.
Trees: `ffC` (pristine + 15, C-tip `2f89ac0`).
