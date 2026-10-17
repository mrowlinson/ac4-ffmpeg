# C-OWNER-SEND-STEPS — Series C encoder post (owner posts)

Final pack, gated 2026-10-16 on master `240dad8` (r5 re-cut for the
combo-6 L1 link fix; cover carries the r5 01/11 note + branch-state).
Pre-flight record: `doc/post/C-PREFLIGHT-CHECKLIST.md`. Stack gates:
`ff-submit-fix` `FIX-REPORT.md` + `ff-mbp-r4build` `MBP-R4BUILD.md`
(carried, cited in the checklist) + `ff-combo-6-verify`
`COMBO-RERUN-6.md` (L1 source + goldens).

Pack: `doc/post/series-C-encoder.mbox` (11 patches vs `3e6cd5e`,
653799 B) + `doc/covers/COVER-C.txt` (`[PATCH 0/11]`).
From/Signoff: `Michael Rowlinson <mrowlinson@gmail.com>`.

## 0. Preconditions (all true at gate time)

- Source line: `ac4/C-encoder-split-r5@fd578ca` (11 commits, linear,
  no merges). Mbox is a fresh `format-patch` of that line; 02/11..
  11/11 byte-identical to the gated r4 `submit-fix/C/0002..0011`,
  01/11 carries only the L1 AC4_ completion (table bytes identical).
- NOTHING sent yet: no SENT marker in repo; all series unsent. No
  suite/sample wait for this series (asynth-only, zero new samples).
- Sequencing only: this series applies standalone onto pristine
  `3e6cd5e` (proven `git am` rc=0; in-history it stacks on A0 but
  shares zero files with it). Post order below is convention, not a
  dependency.

## 1. Send Series C to ffmpeg-devel (this pack)

To: `ffmpeg-devel@ffmpeg.org`
Patches only — NO attachments (no samples, no .md docs, no mbox file).

In a scratch dir (NOT the project repo):

```sh
mkdir -p send && git mailsplit -osend doc/post/series-C-encoder.mbox   # -> 0001..0011
cp doc/covers/COVER-C.txt send/0000                       # whole file: Subject line + blank + body
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  send/0000 send/0001 send/0002 send/0003 send/0004 send/0005 \
  send/0006 send/0007 send/0008 send/0009 send/0010 send/0011
```

Expected send order / threading:

1. `[PATCH 0/11] native AC-4 encoder` (cover)
2. `[PATCH 01/11] avcodec/ac4: add encoder static tables` (7 files:
   Makefile OBJS + 6 tables; build no-op, guard undefined; r5
   AC4_-prefixes the 34 AJOC/DE/DRC table symbols, bytes identical)
3. `[PATCH 02/11] avcodec/ac4: add encoder core + lib adapter`
   (28 files: engine + DSP + algo + adapter; no lavc surface)
4. `[PATCH 03/11] avcodec: add native AC-4 encoder` (7 files:
   ac4enc.c/h + allcodecs + configure probe + version + Changelog;
   flips the codec live)
5. `[PATCH 04/11] doc: AC-4 encoder docs` (encoders.texi +
   general_contents.texi)
6. `[PATCH 05/11] fate: add ac4 encoder tests` (creates
   tests/fate/ac4.mak + include; 4 files; goldens stereo
   `86828e84..` 98193 B, 5.1 `53a44aca..`)
7. `[PATCH 06/11] fate: run ac4 encoder tests without SAMPLES`
   (1-line re-home; `make fate-ac4` passes SAMPLES-unset)
8. `[PATCH 07/11] doc/ac4enc: note CLI silent auto-fold,
   explicit-layout exit 234, immersive deferred` (1 file)
9. `[PATCH 08/11] tests/ac4enc: add negatives gate for explicit
   unsupported layouts` (tests/ac4-neg-check.sh; 2/2 PASS, exit 234)
10. `[PATCH 09/11] fate/ac4enc: pin 8ch silent fold to 5.1 (fold8)`
    (mak + ac4-fold8 ref)
11. `[PATCH 10/11] avcodec/ac4: emit DSI extradata via putbits
    serializer` (10 files: DSI-11/DSI-12 vendor + emitter + 3 ref
    `#extradata` lines)
12. `[PATCH 11/11] avcodec/ac4enc: drive DSI layout from the
    lavc-mask table` (ac4enc.c only; stereo/5.1 wire-identical)

Expect ZERO `git send-email` warnings (mbox fully ASCII — no 8bit
warnings possible). Verify the archive/outbox shows 0/11 + 01/11 ..
11/11 threaded in order, then record the Message-IDs + date in the
checklist copy.

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
→ B1/B2 → C(11, this send) → E1 → E2-R → D(5) → (D6 later).

Constraints: mail-1+24 h before A0 (M3 fate refs) and B2 (decoder
FATE); A0 before B2 (demux copy/pts); B2-before-C NOT required.
E2/D4/D5 append to the `ac4.mak` this series' 05/11 creates.
