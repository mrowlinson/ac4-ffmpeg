# D-OWNER-SEND-STEPS — Series D MP4+dac4 post (owner posts)

Final pack, gated 2026-10-16 on master `2436f47` (r4 covers in-tree;
`COVER-D.txt` untouched since the signoff fill). Pre-flight record:
`doc/post/D-PREFLIGHT-CHECKLIST.md`. Stack gates: `ff-submit-fix`
`FIX-REPORT.md` + `ff-mbp-r4build` `MBP-R4BUILD.md` (carried, cited in
the checklist).

Pack: `doc/post/series-D-mp4.mbox` (6 patches on Series E, 163076 B, union: D-seat content + stack mechanics + T3) +
`doc/covers/COVER-D.txt` (`[PATCH 0/6]`).
From/Signoff: `Michael Rowlinson <mrowlinson@gmail.com>`.

## 0. Preconditions (all true at gate time)

- Source line: `ac4/D-split-r3@08b4f00` (6 commits, linear, no merges).
  Mbox is the union re-cut (163076 B, 6 patches; see Pack line):
  it supersedes the r3 cut that was byte-identical to the gated
  `submit-fix/D/0001..0006` (128977 B) — union adds the M19
  Changelog lines, M21 `ac4_toc.h` use, and T3, plus the
  MUST-final fixes (D 1/6 toc.h-create drop for A0's shared
  header, 7 re-pins, T3 GEN `-strict -2`;
  `ASSEMBLY-DEBTS-R2.md` debts 3/4/6), so the staging bytes no
  longer match.
- NOTHING sent yet: no SENT marker in repo; all series unsent. No
  suite/sample wait for this series (fully self-contained, zero new
  samples).
- Posts AFTER Series C AND Series E merge (functional, not
  convention): patches 4/6-6/6 append to the `tests/fate/ac4.mak`
  that B creates and C/E extend, and 5/6-6/6 need the
  `tests/api/Makefile` entries. Do not post before C and E land
  (stack order ... -> C -> E -> D).

## 1. Send Series D to ffmpeg-devel (this pack)

To: `ffmpeg-devel@ffmpeg.org`
Patches only — NO attachments (no samples, no .md docs, no mbox file).

In a scratch dir (NOT the project repo):

```sh
mkdir -p send && git mailsplit -osend doc/post/series-D-mp4.mbox   # -> 0001..0006
cp doc/covers/COVER-D.txt send/0000                  # whole file: Subject line + blank + body
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  send/0000 send/0001 send/0002 send/0003 send/0004 send/0005 send/0006
```

Expected send order / threading:

1. `[PATCH 0/6] MP4 ac-4+dac4 support + AC-4 parser` (cover)
2. `[PATCH 1/6] avcodec: add ff_ac4_parser` (12 files:
   libavcodec/ac4_parser.c new +420, parsers.c +1, Makefile +1,
   configure +1, Changelog +1 (toc.h create dropped — parser
   uses A0's shared `libavcodec/ac4_toc.h`), + 7 re-pins
   (copy-stereo 1/1, copy-51 1/1, frac-copy 116/116, seek-eof
   2/1, demux 1/1, demux-51 1/1, extra-ac4 9/9):
   ParseContext + combine_frame split loop, AC40/41 sync scan,
   chain-anchor + TOC-head gate, COMPLETE_FRAMES passthrough;
   configure selects for the ac4 demuxer)
3. `[PATCH 2/6] avformat/mov: add AC-4 dac4 box support` (1 file:
   mov.c +47: mov_read_dac4, opaque DSI extradata, parse-table
   entry, HEADERS need_parsing; truncated dac4 -> INVALIDDATA)
4. `[PATCH 3/6] avformat/movenc: add AC-4 ac-4/dac4 muxing support`
   (2 files: movenc.c +141, Changelog +1: mp4 tag entry,
   mov_write_dac4_tag verbatim DSI with EINVAL on empty, stsd
   arms; 1 packet = 1 sample)
5. `[PATCH 4/6] fate: add self-contained ac4 parser tests` (3 files:
   tests/api/api-ac4-parser-test.c new +1007, tests/api/Makefile +1,
   tests/fate/ac4.mak +12/-1 append; fate-ac4-parser, vectors built
   in code, no samples)
6. `[PATCH 5/6] fate: add self-contained ac4 mp4 mux/demux roundtrip
   test` (4 files: tests/api/api-ac4-mp4-roundtrip-test.c new +708,
   tests/api/Makefile +1, tests/fate/ac4.mak +35/-2 append,
   tests/ref/fate/ac4-roundtrip new +150 (T3 encoder->decoder
   roundtrip); fate-ac4-mp4-roundtrip + fate-ac4-roundtrip,
   dac4 byte-exact, 4 loud negatives)
7. `[PATCH 6/6] fate: add self-contained ac4 ts demux test` (3 files:
   tests/api/api-ac4-ts-demux-test.c new +691, tests/api/Makefile
   +2, tests/fate/ac4.mak +12/-2 append; fate-ac4-ts-demux,
   Annex G vectors in code, 2 loud negatives)

Expect ZERO `git send-email` warnings (mbox fully ASCII — no 8bit
warnings possible; the 6 TAB lines are make-recipe indentation,
ASCII). Verify the archive/outbox shows
0/6 + 1/6 .. 6/6 threaded in order, then record the Message-IDs +
date in the checklist copy.

## 2. Separately: mail-1 samples request (blocks A0/B2, NOT this series)

To: `samples-request@ffmpeg.org`
Body: paste `doc/mail-combined/REQUEST.txt` verbatim (subject:
`[samples-request] new AC-4 decoder FATE samples (ac4/)`).
Attachments (15, ~279 KB, from `ff-samples-pack-combined/`, NOT the
.md docs): 11 synth/self-made + 3 acquired clips +
`chromium-LICENSE.txt` — exact list in REQUEST.txt header.
Then: +24 h clock runs only after suite-live confirmation before
A0/B2 post. Irrelevant to this series (no sample refs).

## 3. Post order (full sequence)

Series A now → mail-1 (any time, owner) → A0 (after mail-1 +24 h)
→ B → C(10) → E(2) → D(6, this send, last).

Constraints: mail-1+24 h before A0 (M3 fate refs) and B2 (decoder
FATE); A0 before B2 (demux copy/pts); B2-before-C NOT required.
C-before-E REQUIRED (2/2 appends to C's ac4.mak; tests need the
encoder). C+E-before-D REQUIRED (4/6-6/6 append to the `ac4.mak`
C creates and E extends; 5/6-6/6 need C's `tests/api/Makefile`
entries). D 1/6-3/6 are standalone vs everything (apply clean on
pristine baseline alone).
