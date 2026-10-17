# A-OWNER-SEND-STEPS — Series A decoder post (owner posts)

Final pack re-cut 6, gated 2026-10-17 on master `6905822` +
the MUST fixes M4-M9 on this branch (env/dump/probe strip,
comment scrub, DRC/DE option delete, flush unref, CRC
disclosure, portability; supersedes the 620888-B re-cut-5 pack,
which is STALE).
Pre-flight record: `doc/post/A-PREFLIGHT-CHECKLIST.md`.
Behavioral gates re-proven on these bytes (smoke/F2/tseek) or
carried via byte-identity from the re-cut-5 validation: see the
checklist.

Pack: `doc/post/series-A-decoder.mbox` (2 patches vs `3e6cd5e`,
507373 B) + `doc/covers/COVER-A.txt` (`[PATCH 0/2]`, scrub line
past-tense this re-cut — subjects and prose exact).
From/Signoff: `Michael Rowlinson <mrowlinson@gmail.com>`.

## 0. Preconditions (all true at gate time)

- A0 merged: this series posts AFTER Series A0 merges (stack
  order A0 -> A; every later series assumes the carriage fill).
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
2. `[PATCH 1/2] avcodec: add native AC-4 decoder core` (24 files:
   Makefile OBJS + 23 core incl. `ac4_src.h` s5.11 count header,
   guards in `AVCODEC_<file>_H` form, builds inert under undefined
   CONFIG_AC4_DECODER)
3. `[PATCH 2/2] avcodec: wire ff_ac4_decoder` (ac4dec.c + allcodecs.c)

Expect ZERO 8bit warnings from `git send-email`: mbox + cover are
pure ASCII in this re-cut (mechanical grep: 0 non-ASCII lines).
No MIME headers in the mbox.
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

Mail-1 (any time, owner) → +24 h after suite-live → A0 (head of
stack) → Series A (this send, after A0 merges) → B → C → E → D
(merged stack DAG: A0 → A → B → C → E → D; see `STACK-DAG.md`).

Constraints: mail-1+24 h before A0 (M3 fate refs) and B2 (decoder
FATE); A0 before A (stack order); B before C (M18 single docs
row); D declares E (D 6/6 asserts E's bytes).
