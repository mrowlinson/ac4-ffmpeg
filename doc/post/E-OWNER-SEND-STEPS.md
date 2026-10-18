# E-OWNER-SEND-STEPS — Series E TS-mux post (owner posts)

Final pack, gated 2026-10-16 on master `f827e56` (r3 covers in-tree;
`COVER-E.txt` untouched since). Pre-flight record:
`doc/post/E-PREFLIGHT-CHECKLIST.md`. Stack gates: `ff-submit-fix`
`FIX-REPORT.md` + `ff-mbp-r4build` `MBP-R4BUILD.md` (carried, cited in
the checklist).

Pack: `doc/post/series-E-ts.mbox` (2 patches on Series C, 12774 B) +
`doc/covers/COVER-E.txt` (`[PATCH 0/2]`).
From/Signoff: `Michael Rowlinson <mrowlinson@gmail.com>`.

## 0. Preconditions (all true at gate time)

- Source line: `ac4/E-ts-r3@a2485a9` (2 commits, linear, no merges).
  Mbox is the stack re-cut (12774 B, 2 patches; see Pack line):
  it supersedes the r3 cut that was byte-identical to the gated
  `submit-fix/E/0001..0002` (11605 B) — the re-cut adds the M22
  message rewrite, M19 Changelog line, and probe `-strict -2`,
  plus the MUST-final GEN `-strict -2` + message clause
  (`ASSEMBLY-DEBTS-R2.md` debt 2), so the staging bytes no
  longer match.
- NOTHING sent yet: no SENT marker in repo; all series unsent. No
  suite/sample wait for this series (asynth-only, zero new samples).
- Posts AFTER Series C merges (functional, not convention): patch 2/2
  appends to the `tests/fate/ac4.mak` that B creates and C extends,
  and both FATE tests run `-c:a ac4`. Do not post before C lands
  (stack order ... -> B -> C -> E).

## 1. Send Series E to ffmpeg-devel (this pack)

To: `ffmpeg-devel@ffmpeg.org`
Patches only — NO attachments (no samples, no .md docs, no mbox file).

In a scratch dir (NOT the project repo):

```sh
mkdir -p send && git mailsplit -osend doc/post/series-E-ts.mbox   # -> 0001 0002
cp doc/covers/COVER-E.txt send/0000                  # whole file: Subject line + blank + body
git send-email --to=ffmpeg-devel@ffmpeg.org \
  --from="Michael Rowlinson <mrowlinson@gmail.com>" \
  send/0000 send/0001 send/0002
```

Expected send order / threading:

1. `[PATCH 0/2] MPEG-TS mux descriptors for AC-4` (cover)
2. `[PATCH 1/2] lavf/mpegtsenc: write AC-4 descriptors` (2 files:
   mpegtsenc.c +26, Changelog +1: get_dvb_stream_type AC-4 case
   staying 0x06 private data, AC-4 registration descriptor in the
   PMT audio
   block, DVB AC-4 descriptor 0x7F/0x15 with channel_mode in the
   SYSTEM_B block + Opus-pattern overflow guard)
3. `[PATCH 2/2] fate: ac4 ts mux tests` (3 files:
   tests/fate/ac4-pmt-section.sh new, tests/fate/ac4.mak +43/-1
   append, tests/ref/fate/ac4-mpegts-probe new;
   fate-ac4-mpegts-pmt + fate-ac4-mpegts-probe, asynth-only)

Expect ZERO `git send-email` warnings (mbox fully ASCII — no 8bit
warnings possible). Verify the archive/outbox shows 0/2 + 1/2 + 2/2
threaded in order, then record the Message-IDs + date in the
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
→ B → C(10) → E(2, this send) → D(6) → (parser follow-ups).

Constraints: mail-1+24 h before A0 (M3 fate refs) and B2 (decoder
FATE); A0 before B2 (demux copy/pts); B2-before-C NOT required.
C-before-E REQUIRED (2/2 appends to C's ac4.mak; tests need the
encoder). D appends to the `ac4.mak` C creates and E extends.
