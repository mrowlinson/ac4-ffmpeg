# B-OWNER-SEND-STEPS -- Series B doc + decoder-FATE post (owner posts)

Final pack re-cut on the live suite, gated 2026-10-16 on master
`df3f2de` (supersedes the `d731060` STAGED pack with PENDING golden
stubs -- never post that one). Pre-flight record:
`doc/post/B-PREFLIGHT-CHECKLIST.md`. Stage background (deltas,
stanza parity, suite gate history):
`doc/post/B-POST-CHECKLIST.md`.

Pack: `doc/post/series-B-fate.mbox` (2 patches vs `3e6cd5e`,
19113 B) + `doc/covers/COVER-B.txt` (`[PATCH 0/2]`).
From/Signoff: `Michael Rowlinson <mrowlinson@gmail.com>`.

## 0. Preconditions (all true before step 1)

- Series A LANDED upstream (B rides the A tail; all 10 decode
  legs fail without the Series-A decoder, proven by negative).
- Series A0 LANDED upstream (the 3 copy/seek-eof legs need the
  A0 demuxer fill -- upstream demuxer leaves params unset -- and
  garbage needs the A0 failure shape; proven by negative).
- Suite-landed gate P1 green: 12/12 `ac4/` files sha-ok per
  `ff-suite-watch/EXPECTED.sha256`, live +24h. Landed
  2026-10-16T22:23:09Z -> clock ends 2026-10-17T22:23:09Z.
- P2-P5 done by the re-cut lane (preflight): 9 goldens recorded
  from the real wrapper + suite bytes, B2EXEC-REAL F1/F1b/F2/F3
  calls folded, `make fate-ac4` 14/14 + gate-split 14/14 green,
  negatives green, mbox re-cut with zero PENDING stubs.
- Cover `COVER-B.txt` Testing block refreshed for the fate-source
  finding (cover track; see preflight disclosed carries).

## 1. Send Series B to ffmpeg-devel (this pack only)

To: `ffmpeg-devel@ffmpeg.org`
Patches only -- NO attachments (no samples, no .md docs, no mbox file).

In a scratch dir (NOT the project repo):

```sh
mkdir -p send && git mailsplit -osend doc/post/series-B-fate.mbox   # -> 0001 0002
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

Mbox is ASCII-only: expect no 8bit warnings. Verify the
archive/outbox shows 0/2 + 1/2 + 2/2 threaded in order, then
record the Message-IDs + date in the checklist copy.

## 2. Post order (full sequence)

Series A (landed, precondition) -> A0 (landed, precondition) ->
B1/B2 (this send) -> C(11) -> E(2) -> D(6).

Constraints: mail-1+24h before B2 (this series' FATE, P1); A0
before B2 (demux copy/pts + garbage shape); B2-before-C NOT
required. C05/E2-2 rebase to append to the ac4.mak this series
creates (their packs carry the rebase).
