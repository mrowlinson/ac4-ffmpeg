# A0-OWNER-SEND-STEPS — Series A0 head post (owner posts)

Final pack, gated 2026-10-16 on master `f23777b` (r2 cover in-tree;
`COVER-A0.txt` untouched since). M21 union re-cut (r3 content:
shared TOC walk + need_parsing), re-gated 2026-10-17 (A0 3/3
standalone + parser-stack proof). Pre-flight record:
`doc/post/A0-PREFLIGHT-CHECKLIST.md`. Stack gates: `ff-submit-fix`
`FIX-REPORT.md` + `ff-mbp-r4build` `MBP-R4BUILD.md` (carried, cited in
the checklist).

Pack: `doc/post/series-A0-head.mbox` (1 patch, 43263 B) +
`doc/covers/COVER-A0.txt` (cover; single-patch series, same subject
as the patch, as gated).
From/Signoff: `Michael Rowlinson <mrowlinson@gmail.com>`.

## 0. Preconditions (all true at gate time)

- Source line: `ac4/A0-submit-r3@f8ddf06` (1 commit on `3e6cd5e`,
  M21 one-walker re-cut: shared TOC walk `libavcodec/ac4_toc.h` +
  `need_parsing=HEADERS` for the Series D parser). Mbox is a
  fresh `format-patch` of the union staging commit (same tree
  `1ecac2a2`; am-tree `cmp`-identical).
- NOTHING sent yet: no SENT marker in repo; all series unsent.
- Head of the stack (stack order A0 -> A -> B -> C -> E -> D):
  applies on pristine `3e6cd5e`; every later series assumes this
  behavior. Posts after mail-1 +24 h only (M3 FATE refs need
  `ac4/clip-stereo.ac4` + `ac4/clip-51-real.ac4` live in
  fate-suite). Do not post before the clock holds.

## 1. Send Series A0 to ffmpeg-devel (this pack)

To: `ffmpeg-devel@ffmpeg.org`
Patches only — NO attachments (no samples, no .md docs, no mbox file).

In a scratch dir (NOT the project repo):

```sh
mkdir -p send && git mailsplit -osend doc/post/series-A0-head.mbox   # -> 0001
cp doc/covers/COVER-A0.txt send/0000                  # whole file: Subject line + blank + body
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  send/0000 send/0001
```

Expected send order / threading:

1. `[PATCH] lavf/ac4: fill codecpar + timestamps from first frame`
   (cover; same subject as the patch — single-patch series form,
   exactly as gated in `submit-fix/A0/0000-cover.patch`)
2. `[PATCH] lavf/ac4: fill codecpar + timestamps from first frame`
   (1 patch, 7 files: libavcodec/ac4_toc.h +678 shared TOC walk
   (also used by the Series D parser), libavformat/ac4dec.c +290
   first-frame peek over it publishing sample rate / channel
   layout / bit_rate to codecpar + exact per-frame pts synthesis
   with NOPTS-on-miss seek rebase + need_parsing=HEADERS;
   tests/fate/demux.mak +4, tests/fate/seek.mak +2, 3 new refs:
   fate-ac4-demux/-51 + fate-seek-extra-ac4)

Expect ZERO `git send-email` warnings (mbox + cover fully ASCII —
no 8bit warnings possible). Verify the archive/outbox shows cover
+ 1/1 threaded in order, then record the Message-IDs + date in the
checklist copy.

## 2. Separately: mail-1 samples request (BLOCKS this series)

To: `samples-request@ffmpeg.org`
Body: paste `doc/mail-combined/REQUEST.txt` verbatim (subject:
`[samples-request] new AC-4 decoder FATE samples (ac4/)`).
Attachments (15, ~279 KB, from `ff-samples-pack-combined/`, NOT the
.md docs): 11 synth/self-made + 3 acquired clips +
`chromium-LICENSE.txt` — exact list in REQUEST.txt header.
Then: +24 h clock runs only after suite-live confirmation before
A0/B2 post. This series MUST wait for it (M3 fate refs).

## 3. Post order (full sequence)

Series A now -> mail-1 (any time, owner) -> A0 (after A merges AND
mail-1 +24 h; this send) -> B1/B2 -> C(11) -> E(2) -> D(6) ->
(parser follow-ups).

Constraints: A-before-A0 (runbook sequencing; standalone proven,
zero overlap); mail-1+24 h before A0 (M3 fate refs) and B2
(decoder FATE); A0 before B2 (demux copy/pts); B2-before-C NOT
required. C-before-E REQUIRED (E 2/2 appends to C's ac4.mak;
tests need the encoder). D appends to the `ac4.mak` C creates
and E extends.
