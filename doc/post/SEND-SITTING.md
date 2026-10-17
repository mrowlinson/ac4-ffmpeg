# SEND-SITTING — all six series in ONE sitting (owner posts)

Owner sends A0, A, B, C, E, D back-to-back in one sitting, in
stack order (see STACK-DAG.md). No drip-sends. Source: the six
`*-OWNER-SEND-STEPS.md` in this dir (A = re-cut 5 guard fix, B/E =
stack re-cut with T2/M17/M22/T5 fixes, C/D = stack-context re-cut;
see Per-series notes). Mail-1 already done (suite live
2026-10-16T22:23:09Z) — no mail step here.

To: `ffmpeg-devel@ffmpeg.org` every series. Patches only — NO
attachments (no samples, no .md, no mbox files).
From/Signoff everywhere: `Michael Rowlinson <mrowlinson@gmail.com>`.

## 0. Clock + link gates (BOTH must pass before step 1)

```sh
date -u +%Y-%m-%dT%H:%M:%SZ   # must print >= 2026-10-17T22:23:09Z (B2 +24h); else STOP
ifconfig | grep 169.254.103.231  # TB-only send link; must print a line; else STOP
```

## 1. Global preflight (repo root, merged master)

```sh
git log --oneline -1                     # expect e72a4b9 ff/merged: runbook-r2 (or later parent merge)
ls -l doc/post/*.mbox                    # sizes MUST read:
```
Expected bytes: `series-A-decoder` 620888, `series-A0-head` 35400,
`series-B-fate` 40394, `series-C-encoder` 653750, `series-E-ts`
12693, `series-D-mp4` 129514. B/C/E/D sizes are the stack re-cut
(STACK-DAG.md); stale STAGED `series-B-decfate.mbox` removed from
tree; B pack is `series-B-fate.mbox` only.

```sh
perl -ne '$c += tr/\x80-\xFF//; END { print "non-ascii=$c (want 0)\n" }' \
  doc/post/series-*.mbox doc/covers/COVER-*.txt
grep -c "^From " doc/post/series-A-decoder.mbox doc/post/series-A0-head.mbox \
  doc/post/series-B-fate.mbox doc/post/series-C-encoder.mbox \
  doc/post/series-E-ts.mbox doc/post/series-D-mp4.mbox
# want 2 1 2 11 2 6 in that file order
```

B cover note (closed post-guard-fix): `COVER-B.txt` Testing block now
reads `full make fate green incl. fate-source (Series-A guard fix
in)` -- true since the A re-cut-5 guard fix closed the `fate-source`
CI blocker (`A-PREFLIGHT-CHECKLIST.md` gate GREEN; B-cut-time failure
history in `B-PREFLIGHT-CHECKLIST.md` disclosed carries). Check:

```sh
grep -n "incl. fate-source" doc/covers/COVER-B.txt  # must print one line
```

No owner call needed; confirm the incl-fate-source line pre-split.

Stack order (accepted by running this): the six packs form ONE
ordered stack (A0 -> A -> B -> C -> E -> D, see STACK-DAG.md) and
apply with plain `git am` in that order only -- 24/24 verified.
One sitting sends before anything lands, so tell the maintainer
the application order in each cover thread if asked: A0 first,
then A, B, C, E, D. B's A/A0 need stays functional (FATE legs),
everything else in the chain is textual (Makefile/allcodecs/docs/
mak context).

## 2. Work dir (NOT the repo)

All six series split side by side; one dir for the whole sitting:

```sh
mkdir -p ~/ac4-send && cd ~/ac4-send
R=<repo-root>   # the merged-master checkout from step 1
```

## 3. Series A0 — demux head (1 patch + cover, FIRST)

Preflight: suite live +24h (step 0 clock); M3 FATE refs need
`ac4/clip-stereo.ac4` + `ac4/clip-51-real.ac4` in the suite.

```sh
mkdir -p sendA0 && git mailsplit -osendA0 $R/doc/post/series-A0-head.mbox  # -> 0001
cp $R/doc/covers/COVER-A0.txt sendA0/0000
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  sendA0/0000 sendA0/0001
```

Thread: cover + `[PATCH] lavf/ac4: fill codecpar + timestamps from
first frame` (single-patch form: cover subject == patch subject).
Expect zero warnings.

## 4. Series A — decoder (2 patches + cover)

Preflight: stack order (applies after A0); P3 decoder-first flip
approved by posting.

```sh
mkdir -p sendA && git mailsplit -osendA $R/doc/post/series-A-decoder.mbox  # -> 0001 0002
cp $R/doc/covers/COVER-A.txt sendA/0000
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  sendA/0000 sendA/0001 sendA/0002
```

Thread: `[PATCH 0/2] native AC-4 decoder for raw .ac4 streams`,
`[PATCH 1/2] avcodec: add native AC-4 decoder core`,
`[PATCH 2/2] avcodec: wire ff_ac4_decoder`. Expect zero 8bit warnings.

## 5. Series B — doc + decoder FATE (2 patches + cover)

Preflight: step-0 clock (P1); B cover call from step 1 recorded;
packs: `series-B-fate.mbox` ONLY.

```sh
mkdir -p sendB && git mailsplit -osendB $R/doc/post/series-B-fate.mbox  # -> 0001 0002
cp $R/doc/covers/COVER-B.txt sendB/0000
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  sendB/0000 sendB/0001 sendB/0002
```

Thread: `[PATCH 0/2] doc + FATE for the native AC-4 decoder`,
`[PATCH 1/2] doc: AC-4 decoder docs`,
`[PATCH 2/2] fate: add ac4 decoder tests`. Expect zero 8bit warnings.

## 6. Series C — encoder (11 patches + cover)

Preflight: r5 L1 fix in (01/11 `AC4_`-prefixed tables); asynth-only,
no sample wait.

```sh
mkdir -p sendC && git mailsplit -osendC $R/doc/post/series-C-encoder.mbox  # -> 0001..0011
cp $R/doc/covers/COVER-C.txt sendC/0000
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  sendC/0000 sendC/0001 sendC/0002 sendC/0003 sendC/0004 sendC/0005 \
  sendC/0006 sendC/0007 sendC/0008 sendC/0009 sendC/0010 sendC/0011
```

Thread: `[PATCH 0/11] native AC-4 encoder` + 01/11 tables, 02/11
core+adapter, 03/11 encoder, 04/11 docs, 05/11 fate, 06/11 no-SAMPLES,
07/11 CLI note, 08/11 neg gate, 09/11 fold8, 10/11 DSI extradata,
11/11 DSI layout. Expect zero warnings.

## 7. Series E — TS mux (2 patches + cover)

Preflight: none beyond global (asynth-only); posts-after-C is
functional (2/2 appends the B-created, C-extended `ac4.mak`) —
landing order covers it.

```sh
mkdir -p sendE && git mailsplit -osendE $R/doc/post/series-E-ts.mbox  # -> 0001 0002
cp $R/doc/covers/COVER-E.txt sendE/0000
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  sendE/0000 sendE/0001 sendE/0002
```

Thread: `[PATCH 0/2] MPEG-TS mux descriptors for AC-4`,
`[PATCH 1/2] lavf/mpegtsenc: write AC-4 descriptors`,
`[PATCH 2/2] fate: ac4 ts mux tests`. Expect zero warnings.

## 8. Series D — MP4 + parser (6 patches + cover, LAST)

Preflight: none beyond global (self-contained); posts-after-C+E is
functional (4/6-6/6 append the B/C/E-extended `ac4.mak` + the
`tests/api/Makefile` entries) — landing order covers it.

```sh
mkdir -p sendD && git mailsplit -osendD $R/doc/post/series-D-mp4.mbox  # -> 0001..0006
cp $R/doc/covers/COVER-D.txt sendD/0000
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  sendD/0000 sendD/0001 sendD/0002 sendD/0003 sendD/0004 sendD/0005 sendD/0006
```

Thread: `[PATCH 0/6] MP4 ac-4+dac4 support + AC-4 parser` + 1/6
parser, 2/6 dac4 demux, 3/6 dac4 mux, 4/6 parser fate, 5/6 mp4
roundtrip fate, 6/6 ts demux fate. Expect zero warnings.

## 9. After each series

Verify the archive/outbox shows the full thread in order, then record
Message-IDs + date in Owner-at-post. 30 messages total
(3+2+3+12+3+7). If ANY series shows warnings or a broken thread:
STOP the sitting, do not continue to the next series (rollback notes).

## Rollback notes

Mail cannot be unsent. Per-series recovery, worst first:

- Wrong bytes sent: reply `v2` superseding thread for that series
  only; other sent series stand, but a v2 must re-carry the stack
  context (every series past A0 is cut against its parent — a C v2
  re-carries B context, E re-carries C, D re-carries E).
- Broken thread / missing patch: resend the missing message as a
  reply in the same thread, note it in Owner-at-post.
- Mid-sitting halt (later series fails preflight): sent series stay
  sent; record which landed where; remaining series go in a second
  sitting in the same A0,A,B,C,E,D order.
- Out-of-order upstream landing (maintainer lands E before C):
  follow-up rebase of the landed-later dependency onto the landed
  tree, re-verify `git am`, repost as v2. The chain A0->A->B->C->E->D
  is the application order; B's A/A0 need is the only purely
  functional link.
- B landed before A/A0: B FATE legs fail upstream until A+A0 land;
  no repost needed once they do (same bytes).

## Dry-run record (this seat, 2026-10-17T01:05Z, git 2.56.0)

Every command in steps 0-8 EXCEPT the six live `git send-email`
invocations was run verbatim against master `e72a4b9` bytes in
scratch `ff-send-sitting-prep/dry`: all six `mailsplit -osendX`
rc=0 with exact file counts (2/1/2/11/2/6); all six cover copies
0000 shape (Subject + blank + body); `git send-email --dry-run`
`Result: OK` on all 30 messages with ZERO warning lines; subjects
match the threads above; non-ASCII byte count 0 across all mboxes +
covers; signoff counts 2/1/2/11/2/6. Attached `-osendX` form used
throughout (`-o send/` fails on this git — see
`A-SENDSTEPS-NITFIX.md`; C/D/E/A0 per-series docs showed the broken
form at dry-run time, fixed since to the attached form). TB-IP
check dry-ran absent here (owner link only).

## Owner-at-post (fill when sending)

- [ ] Clock >= 2026-10-17T22:23:09Z confirmed: _______________
- [ ] TB link 169.254.103.231 confirmed: _______________
- [ ] B cover Testing refresh confirmed (incl. fate-source): _______________
- [ ] A sent date + Message-IDs (0/2,1/2,2/2): _______________
- [ ] A0 sent date + Message-IDs (cover,1/1): _______________
- [ ] B sent date + Message-IDs (0/2,1/2,2/2): _______________
- [ ] C sent date + Message-IDs (0/11..11/11): _______________
- [ ] E sent date + Message-IDs (0/2,1/2,2/2): _______________
- [ ] D sent date + Message-IDs (0/6..6/6): _______________
- [ ] Archive shows all six threads complete, in order.
