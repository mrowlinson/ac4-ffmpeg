# B2 blocked-5 samples manifest: 3 acquired, bytes-ready, mail pending

Date: 2026-10-16. Base: GATES-EXEC blocked-5 acquisition, re-verified
first-hand this run (fresh fetch, re-strip, re-carve, decode smoke).
Status: bytes staged in scratch ONLY —
`/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-sample-manifest/`.
This repo carries pins + provenance, zero sample bytes.

## Pins

| Suite clip | sha256 | bytes | Source URL | License |
|---|---|---|---|---|
| `ac4/clip-ims120.ac4` | `0cf487d03255297eb934e6a0da894ac19e39d1e7fd9bb3cbb82c8d6918c6347f` | 48697 | `https://raw.githubusercontent.com/chromium/chromium/main/media/test/data/ac4-ims.ac4` | BSD-3-Clause (Chromium) |
| `ac4/clip-chanbased.ac4` | `dd8139c37d749e3178dece55f37ad6c400abb1131b3fbcab753c7b56c7ad4477` | 138694 | `https://raw.githubusercontent.com/chromium/chromium/main/media/test/data/ac4-channel-based-coding.ac4` | BSD-3-Clause (Chromium) |
| `ac4/clip-714.ac4` | `d2a1f4c96ef1fb833ec5b0a2be425c6a94b795d68595d304994ac5d8870381c7` | 4681 | carved from `https://samples.ffmpeg.org/A-codecs/ac-4/esc-tve-uhd-ac4.ts` (see recipe) | BROADCAST-CONTENT — maintainer call (flag below) |

License text (Chromium files): `https://raw.githubusercontent.com/chromium/chromium/main/LICENSE`
(sha256 `368cca11…c709b`, 1536 B, fetched first-hand, redistribution
permitted with notice). Copy on file in scratch (`lic-ROOT.txt`).

## Slots (fate-b2/ac4.mak)

- `clip-ims120.ac4` → `fate-ac4-ims120` (SAMPLE-TBD fill at B-time).
  Shape first-hand: `cm=120 ch=6`, 48 kHz, 2 presentations,
  41 frames real pcm, AC40 no-crc.
- `clip-chanbased.ac4` → `fate-ac4-51` D4 input note (prefer over
  `clip-51-real.ac4` once equivalence confirmed at implementation).
  Shape first-hand: `cm=14` 6ch 44.1 kHz, 88 frames real pcm,
  hot LFE (slot peak 32768, rail touches — the T4 file).
- `clip-714.ac4` → `fate-ac4-714` (SAMPLE-TBD fill at B-time).
  Shape first-hand: `cm=253 ch=8`, 25 fps (`sr=51200`),
  3/3 real pcm, dialnorm −23, AC41.

## 714 carve recipe (reproduced first-hand, byte-identical)

1. Source `esc-tve-uhd-ac4.ts` (174076686 B,
   sha256 `c4ca0d9825f7d7ee7b13a071d705a663f6c068e4f0684f1d84ea4593bc55ef9f`;
   local copy hash-verified, no re-download).
2. Strip: `ff-corpus/_probe/ts_ac4.py IN.TS OUTDIR` → `pid4004.ac4`
   (pid 4004, 985 PES, 5085225 B,
   sha256 `cd909a79c28390450b81d5875b89172919ecdb200240b127aaf366ec02d4bf7a`;
   3-way identical: corpus `_probe` + gates `strip714` + this run).
3. Size-walk sync frames from offset 0 (offsets match
   gates `offsets.txt` exactly); carve frames 17–19 =
   bytes [25501, 30182) → 4681 B.
4. Config-alignment check: naive frames 0–2 decode
   `status=fail note=aspx-nocfg` (config starts at frame 17);
   frames 17–19 decode 3/3 real pcm `ch=8`, exit 0,
   98304 B pcm (= 3×2048×8×2).

## Maintainer flag — 714 (broadcast content)

`clip-714.ac4` is a 4.7 KB excerpt of broadcast program content
(DVB TVE UHD capture). Suite acceptance is a maintainer call at
mail time; owner decides whether to propose the excerpt at all.
Do NOT upload without that call.

## Mail pending (all three)

- Chromium pair: owner proposes `ac4/clip-ims120.ac4` +
  `ac4/clip-chanbased.ac4` in a samples mail; maintainer
  confirms BSD-3 acceptance. Non-blocking for `fate-ac4-51`
  either way (mak keeps `clip-51-real.ac4` until equivalence).
- 714: same mail path only if owner + maintainer accept the
  broadcast-content flag above.
- Nothing here advances POST without owner action
  (trio still needs mail 1; every file here needs a suite mail).

## Verification (this run, first-hand)

- Fresh `curl` of both Chromium files + LICENSE: sizes and
  sha256 match pins; bytes identical to corpus `streams/` copies.
- Re-strip + re-carve of 714: sha256 match; naive-vs-aligned
  behavior reproduced.
- Decode smoke (`c-ac4dec decode`, read-only, TMPDIR in scratch):
  all 3 exit 0, real non-trivial pcm
  (ims 1007616 B / 9331 uniq words, cbc 2162688 B / 41837 uniq,
  714 98304 B / 2089 uniq).
- Gates: hashes reproduce; decodes smoke-clean; `git status`
  on this branch shows doc files only — no sample bytes committed.

Deferred (unchanged, per GATES-EXEC): `clip-mono.ac4` (no public
mono AC-4 exists) and `clip-51-acpl4.ac4` (no public codec-4 bytes)
stay SAMPLE-TBD.
