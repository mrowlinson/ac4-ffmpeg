# A0-PREFLIGHT-CHECKLIST — Series A0 head FINAL (owner posts)

> SCOPE NOTE (size sweep `COVER-SIZE-SWEEP.md`): this file records
> the r2 bytes (staging `submit-fix/A0/0001` 35400 B, 6 files).
> The merged mbox is the M21 union re-cut (r3 content):
> `series-A0-head.mbox` 43263 B, 1 patch, 7 files (adds
> `libavcodec/ac4_toc.h` +678). Subject/signoff/ASCII/mailsplit
> claims below still hold on current bytes; size/file claims
> describe r2, not current bytes.

Branch: `ff/doc-A0-post-final` from master `f23777b`.
Gated 2026-10-16 in scratch `ff-A0-post-pack` (TMPDIR there);
parent repos untouched, nothing pushed, no MBP/remote, nothing sent.
Send procedure: `doc/post/A0-OWNER-SEND-STEPS.md`.

## Source line

- [x] `ac4/A0-submit-r2@fc4866b` — 1 commit on `3e6cd5e`, linear
  (ASCII-only comments re-cut of `c22b0fe`: 3 comment lines
  `Sec. ` x3; object lives in the submit-fix scratch clone,
  FIX-REPORT "in scratch clone only"; supersedes V2-plan
  `ac4/A0-submit@ee82042` pre-peek-rereview name).
- [x] Mbox is a fresh `format-patch -1 fc4866b`, byte-identical to
  the gated `submit-fix/A0/0001` (`cmp` clean, 35400 B,
  1 From line).

## Final gates (this wave, all green)

- [x] `git am` clean — A0 alone on pristine `3e6cd5e` rc=0
  (`689392d`, 6 files); A(2, master mbox)+A0(1) sequential on
  pristine rc=0, 3/3 (`38dd3b8` 1/2, `461bec4` 2/2, `e3ce706` A0).
  Applied A0-only tree == `fc4866b` tip tree (diff EMPTY);
  all 6 A0 files in the combo tree blob-identical to the tip.
- [x] Standalone-after-A proven — A touches 26 files, A0 touches
  6, intersection EMPTY (zero overlap); A0 applies alone AND
  after A, so the A-before-A0 post order is sequencing, not a
  technical dependency.
- [x] `git show --check` zero on the applied A0 commit
  (no whitespace errors).
- [x] Subjects 1:1 cover<->mbox — the distinctive subject string
  appears exactly once in `COVER-A0.txt` and once in the mbox
  (mechanical counts 1/1); both subject lines byte-identical
  (`[PATCH] lavf/ac4: fill codecpar + timestamps from
  first frame` — single-patch form, as gated).
- [x] Zero placeholders — zero `[OWNER` / `[TODO` / FIXME / XXX /
  YOUR NAME in the cover and in the mbox (both `grep` rc=1).
- [x] Signoffs — `Michael Rowlinson <mrowlinson@gmail.com>` From
  (line 2) + `Signed-off-by` (1/1) on the patch.
- [x] ASCII-only — zero non-ASCII bytes in mbox and cover (perl
  byte count 0/0; 0 TAB lines); expect zero `send-email` 8bit
  warnings.
- [x] `git mailsplit` 1/1 on the mbox.
- [x] No-drift — `COVER-A0.txt` byte-identical to master
  (`cmp` clean vs `f23777b`: r2 cover + filled signoff line);
  mbox is new, cut from the pinned r2 tip.
- [x] Nothing sent yet — no SENT marker in repo or mail-combined
  (no `Message-ID.*@`, no `^SENT:`); only prose mentions
  (unsent/nothing-sent) plus blank fill-in lines; all series
  unsent at gate time.
- [x] Checklists complete — this file + `A0-OWNER-SEND-STEPS.md`
  (devel address, order, no-attachments rule) written and committed.

## Carried gates (identical bytes, still stand)

- submit-fix: full 20-stack `git am` 20/20, applied tree diff vs
  tip EMPTY; configure + `make` exit 0, zero ac4 warnings;
  `fate-ac4` 10/10 incl pmt/probe + aliases; `.text` IDENT 25/26
  (sole diff = movenc.o cold-path immediates, outside this
  series). A0' diff is the 3 comment lines only.
- MBP-R4BUILD: 20/20 `git am` on MBP, TREE-MATCH, build clean,
  `fate-ac4` 10/10 incl. pmt/probe passing.

## Disclosed carries (reviewer-visible, non-blocking)

- Cover subject == patch subject (no 0/1 counting): the gated
  single-patch form (`submit-fix/A0/0000-cover.patch` + `0001`;
  FIX-REPORT verified `send-email` `Result: OK` with identity
  filled — identity is filled on master).
- Cover "head patch" / "Why first" prose describes stack position
  (every later series assumes copy-exit-0 + seekable pts), not
  post order: post order is A first, A0 second per the A-pack P3
  decoder-first flip. No contradiction; no re-cut.
- In-history C stacks on A0 (`282ac3b` parent `fc4866b`, per
  C-PREFLIGHT): post is standalone vs C regardless (A0 is the
  stack base; its 6 files apply on pristine).
- M3 FATE refs need suite samples: post waits for mail-1 +24 h
  after `ac4/clip-stereo.ac4` + `ac4/clip-51-real.ac4` go live
  (cover states it; FIX-REPORT gate 3 pre-existing note).
- Branch names never appear in the patch body (cover
  `Branch state (draft, unposted)` line only, accurate at gate
  time).

## Owner-at-post (fill when sending)

- [ ] A landed (Series A0 posts only after A merges): _______________
- [ ] Mail-1 +24 h after suite-live confirmed: _______________
- [ ] Sent date + Message-IDs (cover, 1/1): _______________
- [ ] Archive shows cover + 1/1 threaded in order.

## Verdict

**READY-TO-POST** — Series A0 head send pack final (after A lands
and mail-1 +24 h).
