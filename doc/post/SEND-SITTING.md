# SEND-SITTING — all six series in ONE sitting (owner posts)

Owner sends A0, A, B, C, E, D back-to-back in one sitting, in
stack order (see STACK-DAG.md). No drip-sends. Source: the six
`*-OWNER-SEND-STEPS.md` in this dir (MUST-final bytes: A0/A/B =
MUST-fix union re-cuts incl. M21 one-walker + T2/T5, A = r6 with
M4-M9 + guard fix, C = union 10 mails with M10-M18 and C08
dropped, E = stack re-cut with M22/M19, D = union with
M19/M21/T3). Mail-1 already done (suite live
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
git log --oneline -1                     # expect 2def950 ff/merged: must-fix-A-r6 (or later parent merge)
ls -l doc/post/*.mbox                    # sizes MUST read:
```
Expected bytes: `series-A-decoder` 507373, `series-A0-head` 43263,
`series-B-fate` 40729, `series-C-encoder` 650794, `series-E-ts`
12774, `series-D-mp4` 163076. All six are the MUST-final fixed
mboxes at master `fc5573a` (B/C/D/E fixed per
`ASSEMBLY-DEBTS-R2.md`; A0/A blob-identical to the `2def950`
union bytes; C is the corrected union, `ff/doc-union-C`, 10
mails — old 08/11 neg gate dropped, its two layout negatives now
ride B 2/2 as FATE); stale STAGED `series-B-decfate.mbox`
removed from tree; B pack is `series-B-fate.mbox` only.

```sh
perl -ne '$c += tr/\x80-\xFF//; END { print "non-ascii=$c (want 0)\n" }' \
  doc/post/series-*.mbox doc/covers/COVER-*.txt
grep -c "^From " doc/post/series-A-decoder.mbox doc/post/series-A0-head.mbox \
  doc/post/series-B-fate.mbox doc/post/series-C-encoder.mbox \
  doc/post/series-E-ts.mbox doc/post/series-D-mp4.mbox
# want 2 1 2 10 2 6 in that file order (23 patch mails)
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
apply with plain `git am` in that order only -- 23/23 per the
union re-proof (24/24 pre-drop, C08 dropped after).
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
MUST-final: M21 one-walker (`libavcodec/ac4_toc.h`) +
`need_parsing=HEADERS`.

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
approved by posting. MUST-final: r6 (M4 strip, M5 scrub, M6
pres_sel-only, M7 unref, M8 skip-bad, M9 portability) + guard fix.

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
packs: `series-B-fate.mbox` ONLY. MUST-final: union re-cut (M17
layout negatives folded as FATE, T2 frac pair, T5 seek-eof
-copyts, M8 skip-bad rename).

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

## 6. Series C — encoder (10 patches + cover)

Preflight: M10-M18 union in (01/10 `AC4_`-prefixed `*_tab` tables,
M11 EXPERIMENTAL, M18 single-row docs chained on B); old C08 neg
gate dropped (negatives ride B 2/2); asynth-only, no sample wait.

```sh
mkdir -p sendC && git mailsplit -osendC $R/doc/post/series-C-encoder.mbox  # -> 0001..0010
cp $R/doc/covers/COVER-C.txt sendC/0000
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  sendC/0000 sendC/0001 sendC/0002 sendC/0003 sendC/0004 sendC/0005 \
  sendC/0006 sendC/0007 sendC/0008 sendC/0009 sendC/0010
```

Thread: `[PATCH 0/10] native AC-4 encoder`,
`[PATCH 01/10] avcodec/ac4: add encoder static tables`,
`[PATCH 02/10] avcodec/ac4: add encoder core + lib adapter`,
`[PATCH 03/10] avcodec: add native AC-4 encoder`,
`[PATCH 04/10] doc: AC-4 encoder docs`,
`[PATCH 05/10] fate: add ac4 encoder tests`,
`[PATCH 06/10] fate: run ac4 encoder tests without SAMPLES`,
`[PATCH 07/10] doc/ac4enc: note CLI silent auto-fold, explicit-layout`,
`[PATCH 08/10] fate/ac4enc: pin 8ch silent fold to 5.1 (fold8)`,
`[PATCH 09/10] avcodec/ac4: emit DSI extradata via putbits serializer`,
`[PATCH 10/10] avcodec/ac4enc: drive DSI layout from the lavc-mask`.
Expect zero warnings.

## 7. Series E — TS mux (2 patches + cover)

Preflight: none beyond global (asynth-only); posts-after-C is
functional (2/2 appends the B-created, C-extended `ac4.mak`) —
landing order covers it. MUST-final: M22 message rewrite, M19
Changelog line, probe `-strict -2` (post-D parser).

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
MUST-final: union (M19 lines after E, M21 `ac4_toc.h` use +
post-D re-pins, T3 roundtrip area, M20 split).

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
Message-IDs + date in Owner-at-post. 29 messages total on the wire
(23 patch mails + 6 covers: 2+3+3+11+3+7). If ANY series shows
warnings or a broken thread: STOP the sitting, do not continue to
the next series (rollback notes).

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

## Dry-run record (this seat, 2026-10-17T09:12Z, git 2.56.0)

> SCOPE NOTE (R2 sweep `COVER-SIZE-SWEEP-R2.md`): ran against
> `2def950` bytes. Counts/subjects/signoffs/hygiene re-verified
> identical on `fc5573a` (1/2/2/10/2/6 = 23, 29 wire, non-ASCII
> 0); B/C/D/E sizes are now 40729/650794/12774/163076 (see §1).

Every command in steps 0-8 EXCEPT the six live `git send-email`
invocations was run verbatim against master `2def950` bytes in
scratch `ff-send-sitting-2/dry`: all six `mailsplit -osendX`
rc=0 with exact file counts (1/2/2/10/2/6 = 23 patch files); all
six cover copies 0000 shape (Subject + blank + body);
`git send-email --dry-run` `Result: OK` on all 29 wire messages
(2+3+3+11+3+7) with ZERO warning lines; subjects match the threads
above; non-ASCII byte count 0 across all mboxes + covers; signoff
counts 2/1/2/10/2/6; byte sizes match the MUST-union table exactly.
Attached `-osendX` form used throughout (`-o send/` fails on this
git — see `A-SENDSTEPS-NITFIX.md`). TB-IP check dry-ran absent
here (owner link only).

## Owner-at-post (fill when sending)

- [ ] Clock >= 2026-10-17T22:23:09Z confirmed: _______________
- [ ] TB link 169.254.103.231 confirmed: _______________
- [ ] B cover Testing refresh confirmed (incl. fate-source): _______________
- [ ] A0 sent date + Message-IDs (cover,1/1): _______________
- [ ] A sent date + Message-IDs (0/2,1/2,2/2): _______________
- [ ] B sent date + Message-IDs (0/2,1/2,2/2): _______________
- [ ] C sent date + Message-IDs (0/10..10/10): _______________
- [ ] E sent date + Message-IDs (0/2,1/2,2/2): _______________
- [ ] D sent date + Message-IDs (0/6..6/6): _______________
- [ ] Archive shows all six threads complete, in order.
