# B-OWNER-SEND-STEPS -- Series B doc + decoder-FATE post (at clock end)

Staged pack: `doc/post/series-B-decfate.mbox` (STAGED with PENDING
golden stubs -- NEVER post as-is) + `doc/covers/COVER-B.txt`.
Gate record: `doc/post/B-POST-CHECKLIST.md` (P1-P6 must ALL be
checked before sending).
From/Signoff: `Michael Rowlinson <mrowlinson@gmail.com>`.

## 0. Preconditions (all true before step 1)

- Series A LANDED upstream (B rides the A tail).
- Suite-landed gate P1 green: 12/12 `ac4/` files sha-ok per
  `ff-suite-watch/EXPECTED.sha256`, live +24h.
- P2-P4 done by a lane: 9 goldens recorded from the real wrapper
  + suite bytes, B2EXEC-REAL F1/F1b/F2/F3 calls folded, `make
  fate-ac4` + full fate green, pre-patch negative confirmed.
- P5 done: mbox RE-CUT (zero PENDING stubs), am-clean re-verified,
  subjects still 1:1 with the cover.

## 1. Send Series B to ffmpeg-devel (re-cut pack only)

To: `ffmpeg-devel@ffmpeg.org`
Patches only -- NO attachments (no samples, no .md docs, no mbox file).

In a scratch dir (NOT the project repo):

```sh
git mailsplit -o send/ doc/post/series-B-decfate.mbox   # -> 0001 0002
cp doc/covers/COVER-B.txt send/0000                       # whole file: Subject line + blank + body
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  send/0000 send/0001 send/0002
```

Expected send order / threading:

1. `[PATCH 0/2] doc + FATE for the native AC-4 decoder` (cover)
2. `[PATCH 1/2] doc: AC-4 decoder docs` (table row + @section ac4
   + Changelog)
3. `[PATCH 2/2] fate: add ac4 decoder tests` (tests/fate/ac4.mak
   + Makefile include + 9 goldens)

Verify the archive/outbox shows 0/2 + 1/2 + 2/2 threaded in order,
then record the Message-IDs + date in the checklist copy.

## 2. Post order (full sequence)

Series A (landed, precondition) -> mail-1 +24h (P1) -> A0 (landed
before B2 runs green; demux copy/pts) -> B1/B2 (this send) ->
C(11) -> E1 -> E2-R -> D(5) -> (D6 TBD).

Constraints: mail-1+24h before B2 (this series' FATE); A0 before
B2; B2-before-C NOT required. C05/E2-2 rebase to append to the
ac4.mak this series creates.
