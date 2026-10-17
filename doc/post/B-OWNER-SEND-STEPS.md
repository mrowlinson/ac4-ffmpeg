# B-OWNER-SEND-STEPS -- Series B doc + decoder-FATE post (owner posts)

Final pack re-cut 2 on the live suite, gated 2026-10-16 on master
`e72a4b9` (supersedes the `2330a26` FINAL pack with the stale 51200
714 golden -- never post that one -- and the `d731060` STAGED pack
with PENDING golden stubs -- never post that one either).
Union re-cut (MUST-fix B/A0 union, branch `ff/doc-union-B-A0`):
same 16 tests + encoder negatives, comment/message adds only,
re-gated 2026-10-17 (fate-ac4 16/16, gate-split 19/19, E-tip 25).
Pre-flight record: `doc/post/B-PREFLIGHT-CHECKLIST.md`. Stage
background (deltas, stanza parity, suite gate history):
`doc/post/B-POST-CHECKLIST.md`. Blocker record:
`doc/post/COMBO-RERUN-7.md` (finding F1, closed by this re-cut).

Pack: `doc/post/series-B-fate.mbox` (2 patches on Series A,
40708 B) + `doc/covers/COVER-B.txt` (`[PATCH 0/2]`).
Stack order: A0 -> A -> B -> C -> E -> D (see STACK-DAG.md).
From/Signoff: `Michael Rowlinson <mrowlinson@gmail.com>`.

## 0. Preconditions (all true before step 1)

- Series A LANDED upstream (B rides the A tail; all 11 decode
  legs fail without the Series-A decoder, proven by negative).
- Series A0 LANDED upstream (the 4 copy/seek-eof legs need the
  A0 demuxer fill -- upstream demuxer leaves params unset -- and
  garbage needs the A0 failure shape; proven by negative).
- Suite-landed gate P1 green: 12/12 `ac4/` files sha-ok per
  `ff-suite-watch/EXPECTED.sha256`, live +24h. Landed
  2026-10-16T22:23:09Z -> clock ends 2026-10-17T22:23:09Z.
- P2-P5 done by the re-cut lane (preflight + stack re-cut): 11
  goldens recorded from the real wrapper + suite bytes (frac pair
  over the pre-existing 29.97 fps clip; skip-bad rename;
  seek-eof -copyts re-pin), encoder-layout negatives folded from
  C08 (AC4_ENCODER-guarded, empty until C lands), B2EXEC-REAL
  F1/F1b/F2/F3 calls folded, combo-7 F1 (stale 714 golden) fixed
  + re-gated, `make fate-ac4` 16/16 pre-C (18/18 post-C) green,
  full `make fate` green (fate-source fixed via A-r5), negatives green
  (carried -- comment-only mak delta), mbox re-cut with zero
  PENDING stubs.
- Cover `COVER-B.txt` Testing block re-verified for literal
  full-fate-green (cover track; see preflight disclosed carries).

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
   + Makefile include + 11 goldens)

Mbox is ASCII-only: expect no 8bit warnings. Verify the
archive/outbox shows 0/2 + 1/2 + 2/2 threaded in order, then
record the Message-IDs + date in the checklist copy.

## 2. Post order (full sequence)

Series A (landed, precondition) -> A0 (landed, precondition) ->
B (this send) -> C(10) -> E(2) -> D(6).

Constraints: mail-1+24h before B2 (this series' FATE, P1); A0
before B2 (demux copy/pts + garbage shape); B2-before-C NOT
required. C05/E2-2 rebase to append to the ac4.mak this series
creates (their packs carry the rebase).
