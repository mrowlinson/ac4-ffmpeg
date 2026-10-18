# C-OWNER-SEND-STEPS — Series C encoder post (owner posts)

Final pack, gated 2026-10-17 on master `6905822` (union of the r6
C content re-cut (MUST-TRIAGE M10-M18) with the stack DAG mechanics;
cover carries the DAG header + r6 per-patch notes, 10 mails).
Pre-flight record: `doc/post/C-PREFLIGHT-CHECKLIST.md`. Stack order:
A0 -> A -> B -> C -> E -> D (see STACK-DAG.md). Stack gates:
`ff-submit-fix` `FIX-REPORT.md` + `ff-mbp-r4build` `MBP-R4BUILD.md`
(carried, cited in the checklist) + `ff-combo-6-verify`
`COMBO-RERUN-6.md` (L1 source + goldens).

Pack: `doc/post/series-C-encoder.mbox` (10 patches on Series B,
650794 B) + `doc/covers/COVER-C.txt` (`[PATCH 0/10]`).
From/Signoff: `Michael Rowlinson <mrowlinson@gmail.com>`.

## 0. Preconditions (all true at gate time)

- Source line: union C range (10 commits, linear, no merges) over
  A0+A+B(stack). Mbox is a fresh `format-patch` of the C range;
  stacked `git am` 10/10 onto B tip, tree-identical.
- Encoder bytes identical to r5/r6 (stereo `86828e84..` 98193 B,
  5.1 `53a44aca..`, `#extradata` lines unchanged).
- NOTHING sent yet: no SENT marker in repo. No suite/sample wait
  for this series (asynth-only, zero new samples).
- HOLD ALL still stands (MUST-TRIAGE M1): post only as the one
  ordered stack A0→A→B→C→E→D, not standalone.
- Posts AFTER A0 + A + B land (functional, not convention):
  04/10 extends B's docs row, 05/10 appends B's `ac4.mak`
  appendix, and the Makefile/allcodecs hunks are cut against the
  A-applied tree.

## 1. Send Series C to ffmpeg-devel (this pack, in stack order)

To: `ffmpeg-devel@ffmpeg.org`
Patches only — NO attachments (no samples, no .md docs, no mbox file).

In a scratch dir (NOT the project repo):

```sh
mkdir -p send && git mailsplit -osend doc/post/series-C-encoder.mbox   # -> 0001..0010
cp doc/covers/COVER-C.txt send/0000                       # whole file: Subject line + blank + body
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  send/0000 send/0001 send/0002 send/0003 send/0004 send/0005 \
  send/0006 send/0007 send/0008 send/0009 send/0010
```

Expected send order / threading:

1. `[PATCH 0/10] native AC-4 encoder` (cover)
2. `[PATCH 01/10] avcodec/ac4: add encoder static tables` (7 files:
   Makefile OBJS + 6 tables; build no-op, guard undefined; r6
   `*_tab` names, bytes identical)
3. `[PATCH 02/10] avcodec/ac4: add encoder core + lib adapter`
   (28 files: engine + DSP + algo + adapter; no lavc surface;
   r6 sincos probe support, av_ allocators, no TLS)
4. `[PATCH 03/10] avcodec: add native AC-4 encoder` (6 files:
   ac4enc.c/h + allcodecs + configure probes + Changelog; flips
   the codec live as EXPERIMENTAL; no version bump)
5. `[PATCH 04/10] doc: AC-4 encoder docs` (encoders.texi +
   single general-contents row + experimental/limits notes)
6. `[PATCH 05/10] fate: add ac4 encoder tests` (appends the
   encoder section to the B ac4.mak appendix; 3 files; goldens
   stereo `86828e84..` 98193 B, 5.1 `53a44aca..`;
   `-strict -2` CMDs)
7. `[PATCH 06/10] fate: run ac4 encoder tests without SAMPLES`
   (1-line re-home; `make fate-ac4` passes SAMPLES-unset)
8. `[PATCH 07/10] doc/ac4enc: note CLI silent auto-fold,
   explicit-layout exit 234, immersive deferred` (1 file)
9. `[PATCH 08/10] fate/ac4enc: pin 8ch silent fold to 5.1 (fold8)`
   (mak + ac4-fold8 ref; comment points at the B negatives)
10. `[PATCH 09/10] avcodec/ac4: emit DSI extradata via putbits
    serializer` (10 files: DSI-11/DSI-12 vendor + emitter + 3 ref
    `#extradata` lines)
11. `[PATCH 10/10] avcodec/ac4enc: drive DSI layout from the
    lavc-mask table` (ac4enc.c only; stereo/5.1 wire-identical)

(No 08/11-style negatives gate: the two explicit-layout negatives
ride Series B 2/2 as FATE behind the encoder ifeq; the committed
check script is gone.)

Expect ZERO `git send-email` warnings (mbox fully ASCII — no 8bit
warnings possible). Verify the archive/outbox shows 0/10 + 01/10 ..
10/10 threaded in order, then record the Message-IDs + date in the
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

Series A0 now → A → mail-1 (any time, owner) → B (after mail-1
+24 h) → C(10, this send) → E(2) → D(6).

Constraints: mail-1+24 h before A0 (M3 fate refs) and B (decoder
FATE); A0 before B (demux copy/pts); C stacks on B (docs row +
mak appendix; Makefile/allcodecs cut on the A tree). E2/D4/D5/D6
append to the `ac4.mak` appendix this series extends.
