# Torture TOC-gate trio: upload manifest (pins + provenance, zero bytes)

Date: 2026-10-16. Pack staged in scratch ONLY —
`/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-torture-upload-prep/pack/`
(3 `.ac4` + `SHA256SUMS`). This repo carries pins + provenance,
zero sample bytes. Mail body draft: `doc/mail-torture-trio/REQUEST.txt`;
send/verify steps: `doc/mail-torture-trio/CHECKLIST.md`.

## Pins

| Suite path | sha256 | bytes | Recipe (from clip-stereo.ac4) |
|---|---|---|---|
| `ac4/t17-rate14.ac4` | `118aa558b7577772d1520eb12f46b22635e3c7277a132f262e3880932a121c1b` | 1042 | f1 rate:=14 (raw[2] FB->FD), CRC re-stamped |
| `ac4/t17b-rate14-f0.ac4` | `705f1f92d35394d8a1883693218dc9dbe355a044a87a132b32134c8f00ddf90a` | 1042 | f0 rate:=14 (raw[2] FB->FD), CRC re-stamped |
| `ac4/t18-rate15.ac4` | `97d0a7ba29dcdfc224d360216fd07d6e3c781181b713c6a8c8677cfd6219041e` | 1042 | f1 rate:=15 (raw[2] FB->FF), CRC re-stamped |

Total: 3126 B. Suite names follow the staged mak convention
(`fate-b2/ac4.mak` torture block: "upload to fate-suite as
`ac4/tNN-*.ac4`"); shape matches the existing `ac4/` clips
(lowercase, hyphenated, `.ac4`).

## Provenance

- Base: `clip-stereo.ac4` (1042 B, 3x AC41 frames at off 0/348/695)
  from the combined decoder mail: own synth sine tones encoded with
  our cleanroom c-ac4enc. No third-party content in the trio.
- Surgery: deterministic `build.py` (ff-parser-torture): TOC-head
  rate field rewrite + CRC16 (poly 0x8005, MSB-first, init 0) re-stamp
  over lenbytes+raw. Regen reproduces all three byte-identically.
- Released for unrestricted redistribution as part of the FFmpeg
  FATE suite (see REQUEST.txt).

## Expected behavior (oracle + parser, first-hand in prior runs)

- t17: oracle exit 0, 2f pcm (`-v debug` FRM i=0,2: f1 skipped at
  decode); parser emits f0+f2 = 2 (TOC gate rejects f1 at parse).
- t17b: oracle exit 0, 1f pcm (warn no-ig emit-all, fri0 fs0 ig0
  hasI0 rej2); parser emits f1+f2 = 2 (TOC gate rejects f0).
- t18: oracle exit 0, 2f pcm (same shape as t17); parser emits 2.
- Demux contrast (NEGPROOF cases 28/29): demux passes all 3 pkts
  through, exit 0 — the reject is parser/TOC-level, decode-phase only.

## Mak slots (future, not landed)

- Staged: `fate-b2/ac4.mak` parser-torture block, "2 pkts" bucket
  (comment-only; landing rule excludes it).
- D4 parser FATE posts self-contained (no samples); these bytes stage
  the future parser-torture harness asserts (emit-2 each).
- If the maintainer renames any file, update the mak paths at
  implementation time.

## Verification (this run, first-hand)

- 3/3 pack copies `cmp`-identical to `ff-parser-torture/samples/`
  AND to `ff-torture-nits/samples/` (two independent sources).
- `sha256sum -c SHA256SUMS`: 3/3 OK; pack pins `diff`-identical to
  the source `SHA256SUMS` trio rows and the TORTURE.md table rows.
- Regen: scratch-local `build.py` rerun (read-only base clip),
  3/3 outputs `cmp`-identical to pack bytes.
- Sizes: 1042 x3 = 3126 B, match pins, REQUEST.txt, CHECKLIST.
- `git status` on this branch shows doc files only.

## Known gaps (NOT in this pack)

- Remaining 28 torture vectors (t01-t16, t19-t29 + t14b): a later
  batch proposes them with the parser-FATE harness post.
- Parser-torture FATE mechanism still TBD (harness or
  demux+framecrc packet counts); framemd5-style goldens, if any,
  get recorded from the real parser at implementation, never here.
