# A-OWNER-SEND-STEPS — Series A decoder post (owner posts)

Final pack re-cut 3, gated 2026-10-16 on master `cc42e2c` (contains
`b86587f` re-vendored core + `5c8740a` hygiene; re-cut from the merged
staging, supersedes the `a66f84e` pack). Pre-flight record:
`doc/post/A-PREFLIGHT-CHECKLIST.md`. Background gates (re-cut 2,
behavioral, carried via strip-identity proof):
`doc/post/DEC-POST-CHECKLIST.md`.

Pack: `doc/post/series-A-decoder.mbox` (2 patches vs `3e6cd5e`,
606385 B) + `doc/covers/COVER-A.txt` (`[PATCH 0/2]`, unchanged).
From/Signoff: `Michael Rowlinson <mrowlinson@gmail.com>`.

## 0. Preconditions (all true at gate time)

- P3 confirmed: Series-A letter + decoder-first flip approved. (This
  file existing on `ff/doc-A-post-r2` with READY-TO-POST = the flip
  is approved; runbook step 10 still says A0 first — stale, fix later.)
- NOTHING sent yet: no SENT marker in repo; Series A and mail-1 both
  unsent. No suite/sample wait for this series (zero sample deps).

## 1. Send Series A to ffmpeg-devel (this pack)

To: `ffmpeg-devel@ffmpeg.org`
Patches only — NO attachments (no samples, no .md docs, no mbox file).

In a scratch dir (NOT the project repo):

```sh
git mailsplit -o send/ doc/post/series-A-decoder.mbox   # -> 0001 0002
cp doc/covers/COVER-A.txt send/0000                       # whole file: Subject line + blank + body
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  send/0000 send/0001 send/0002
```

Expected send order / threading:

1. `[PATCH 0/2] native AC-4 decoder for raw .ac4 streams` (cover)
2. `[PATCH 1/2] avcodec: add native AC-4 decoder core` (24 files:
   Makefile OBJS + 23 core, builds inert under undefined
   CONFIG_AC4_DECODER)
3. `[PATCH 2/2] avcodec: wire ff_ac4_decoder` (ac4dec.c + allcodecs.c)

Expect benign informational 8bit warnings from `git send-email`
(3 non-ASCII lines total, no MIME headers in mbox: 1 em-dash in a
1/2 code comment + the 2 long-carried cover em-dashes) — sends
fine. The `entropy.c` whitespace wart is GONE (hygiene re-cut).
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
