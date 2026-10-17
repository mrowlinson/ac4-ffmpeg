# A-OWNER-SEND-STEPS — Series A decoder post (owner posts)

Final pack re-cut 5, gated 2026-10-16 on master `ef93aa9` +
the guard fix on this branch (fate-source conformance; supersedes
the 620505-B re-cut-4 pack, whose 13 `AC4_*` header guards fail
`fate-source` — upstream CI would fail on it — and is STALE).
Pre-flight record: `doc/post/A-PREFLIGHT-CHECKLIST.md`.
Behavioral gates re-proven on these bytes (smoke/F2) or carried via
byte-identity from the revendor-N5 validation: see the checklist.

Pack: `doc/post/series-A-decoder.mbox` (2 patches vs `3e6cd5e`,
620888 B) + `doc/covers/COVER-A.txt` (`[PATCH 0/2]`, unchanged from
re-cut 4 — subjects and prose still exact).
From/Signoff: `Michael Rowlinson <mrowlinson@gmail.com>`.

## 0. Preconditions (all true at gate time)

- P3 confirmed: Series-A letter + decoder-first flip approved. (This
  file existing on `ff/doc-A-guard-fix` with READY-TO-POST = the flip
  is approved; runbook step 10 still says A0 first — stale, fix later.)
- NOTHING sent yet: no SENT marker in repo; Series A and mail-1 both
  unsent. No suite/sample wait for this series (zero sample deps).
- Posts AFTER Series A0 merges (stack order A0 -> A: every later
  series assumes the carriage fill; zero file overlap, but the
  stack applies head-first).

## 1. Send Series A to ffmpeg-devel (this pack)

To: `ffmpeg-devel@ffmpeg.org`
Patches only — NO attachments (no samples, no .md docs, no mbox file).

In a scratch dir (NOT the project repo):

```sh
mkdir -p send && git mailsplit -osend doc/post/series-A-decoder.mbox   # -> 0001 0002
cp doc/covers/COVER-A.txt send/0000                       # whole file: Subject line + blank + body
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  send/0000 send/0001 send/0002
```

(`mailsplit` takes the attached `-o` form and needs the dir to exist;
bare `-o send/` fails on this git — see `A-SENDSTEPS-NITFIX.md`.)

Expected send order / threading:

1. `[PATCH 0/2] native AC-4 decoder for raw .ac4 streams` (cover)
2. `[PATCH 1/2] avcodec: add native AC-4 decoder core` (25 files:
   Makefile OBJS + 24 core incl. `ac4_src.h` s5.11 count header,
   guards in `AVCODEC_<file>_H` form, builds inert under undefined
   CONFIG_AC4_DECODER)
3. `[PATCH 2/2] avcodec: wire ff_ac4_decoder` (ac4dec.c + allcodecs.c)

Expect ZERO 8bit warnings from `git send-email`: mbox + cover are
pure ASCII in this re-cut (carried from re-cut 4; the guard fix adds
ASCII-only lines). No MIME headers in the mbox.
Verify the archive/outbox shows 0/2 + 1/2 + 2/2 threaded
in order, then record the Message-IDs + date in the checklist copy.

## 2. Separately: mail-1 samples request (blocks A0/B2, NOT this series)

To: `samples-request@ffmpeg.org`
Body: paste `doc/mail-combined/REQUEST.txt` verbatim (subject:
`[samples-request] new AC-4 decoder FATE samples (ac4/)`).
Attachments (15, ~279 KB, from `ff-samples-pack-combined/`, NOT the
.md docs): 11 synth/self-made + 3 acquired clips +
`chromium-LICENSE.txt` — exact list in REQUEST.txt header.
Then: +24 h clock runs only after suite-live confirmation before
A0/B2 post.

## 3. Post order (full sequence)

Series A now (this send) → mail-1 (any time, owner) → A0 (after
mail-1 +24 h) → B1/B2 → C(11) → E1 → E2-R → D(5) → (D6 TBD).

Constraints: mail-1+24 h before A0 (M3 fate refs) and B2 (decoder
FATE); A0 before B2 (demux copy/pts); B2-before-C NOT required.
