# FATE samples submission checklist (AC-4, combined single send)

Package: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-samples-pack-combined/`
(17 files: 10 `.ac4` + 4 `.pcm` + chromium-LICENSE.txt +
REQUEST.txt + this file; 15 attachments)

Merges the mail-1 pack (`ff-samples-pack/`: 7 synth files + 3 acquired
clips + license, ~266 KB) and the mail-2 T-pin set
(`ff-samples-pack2/`: stereo51/impulse/garbage/seek-REF, ~13 KB) into
ONE send. Owner sends; lanes never send. Both source packs stay
untouched (copies verified byte-identical, see below).

## Owner send steps

1. DONE (ff/doc-mail-combined): foot of REQUEST.txt carries
   `Michael Rowlinson <mrowlinson@gmail.com>`.
2. Mail REQUEST.txt body to **samples-request@ffmpeg.org** with the 15
   attachments (11 synth/self-made files + 3 acquired clips +
   chromium-LICENSE.txt; NOT the .md docs, NOT REQUEST.txt itself
   unless you want it attached — paste it as the message body).
   Attach list: clip-stereo.ac4, clip-stereo-ref.pcm,
   clip-51-real.ac4, clip-51-real-ref.pcm, clip-crc-bad.ac4,
   clip-crc-bad-ref.pcm, clip-gap-fail.ac4, clip-stereo51.ac4,
   clip-impulse.ac4, garbage.ac4, clip-stereo-seek-0.05.pcm,
   clip-ims120.ac4, clip-chanbased.ac4, clip-714.ac4,
   chromium-LICENSE.txt.
3. Wait for the upload-confirmation reply. Policy: suite files must be
   live **>=24 h before** the FATE patch post (Series B, B2
   `fate: add ac4 decoder tests`).
4. After confirmation, verify locally:
   `make fate-rsync SAMPLES=/path/to/fate-suite` then confirm the 12
   `ac4/clip-*` suite files exist in the suite (8 base + stereo51/
   impulse/garbage/seek-REF; ref-only pcm + license placement per
   maintainer).
5. Only then post Series B (base + T-pin slots together). (Series A
   decoder core needs no samples and can go any time.)

## Suite path + naming

- Target dir: `ac4/` (new directory; nothing existing to overwrite —
  never overwrite suite files per policy).
- Proposed names are in REQUEST.txt. If the maintainer renames
  anything (e.g. `clip-crc-bad.ac4` -> `crc-bad.ac4`), update
  `tests/fate/ac4.mak` sample paths at B2 implementation time.

## Timing vs patch post (SUBMIT-PLAN order)

- Series A (decoder core + wiring): no samples needed, post any time.
- Series B (docs + `fate-ac4-*` tests, base + T-pin slots): post only
  after this mail is confirmed + 24 h elapsed.
- Series C/D need no new samples (encoder uses generated asynth;
  mp4 sample TBD later if D3 wants one).

## Verification already done (this run, read-only + copy-verify)

- All 15 staged copies `cmp`-identical to their source packs (11 vs
  ff-samples-pack, 4 vs ff-samples-pack2); source packs untouched.
- sha256 of all 15 files re-checked: match REQUEST.txt rows (which
  carry forward the mail-1 FATE-PREP/FATE-MORE + samples-manifest
  pins and the mail-2 SAMPLES.md §B + pins.txt rows).
- Sizes: 271986 B (mail-1 eleven) + 13366 B (mail-2 four) = 285352 B
  (~279 KB); REQUEST.txt header/body/notes totals agree.
- clip-stereo-seek-0.05.pcm byte-identical to clip-crc-bad-ref.pcm
  (sha256 2ee1ade1…, confirmed via sha256sum on both files).
- Decode smoke (prior runs, prebuilt `c-ac4dec`): mail-1 stereo exit
  0 pcm + ref-identical; 51 exit 0 pcm + ref-identical; gap-fail
  exit 3 no-sync + no output file; crc-bad exit 0 pcm+crc1 +
  ref-identical. Mail-2 stereo51/impulse/seek exit 0 pcm +
  double-decode identical; garbage exit 3 no-sync, no output.
- Build provenance (ff-mak-samples/MAK-SAMPLES.md, read first-hand in
  the mail-2 run): stereo51 = 6ch x 2048 synth (L=440 R=560 Hz, rest
  digital zero); impulse = stereo 8192 bed + impulses at 0/4096
  (4096->6144 tap is the pin); garbage = Random(0xAC40) 1024 B;
  seek slice = tail of clip-stereo.ac4 decoded (emulates -ss 0.05).
- No `[OWNER` / `[TODO` / placeholder text in REQUEST.txt or this
  file (grep-verified); no follow-up/mail-1/mail-2 framing left in
  REQUEST.txt (single-send wording).

## Known gaps (NOT in this package)

Resolved by this mail: mail-1 trio (ims120 + chanbased + 714,
placement pending; 714 needs the maintainer call flagged in
REQUEST) + T-pin set (stereo51 + impulse + garbage + seek-REF).

- `fate-ac4-mono`: encoder rejects mono input; needs new shape.
  No public mono AC-4 exists (STILL-NONE).
- `fate-ac4-51-acpl4` (codec-4): no public codec-4 bytes exist
  (STILL-NONE).
- `fate-ac4-gap-fails` (aspx-unimpl silence shape): needs new codec
  shape; clip-gap-fail.ac4 here is a truncation/no-sync negative,
  adjacent but NOT that slot.
- framemd5 goldens live in-tree (`tests/ref/fate/`), recorded from
  the real wrapper at B2 — never from cleanroom bytes.
  Evidence-only refs (clip-stereo51-ref.pcm, clip-impulse-ref.pcm)
  stay in ff-mak-samples/, NOT mailed.
- garbage.ac4 suite REF: negproof wording was proven on different
  (A0-local) garbage bytes; B-time MUST re-confirm the REF string
  against these suite bytes (SAMPLES.md §D).
