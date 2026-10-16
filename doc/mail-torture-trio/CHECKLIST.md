# Torture-trio upload checklist (t17/t18 + t17b, READY-AT-CLOCK-END)

Package: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-torture-upload-prep/pack/`
(3 `.ac4` + `SHA256SUMS`, 3126 B). Pins + provenance:
`fate-b2/torture-trio-manifest.md` (this branch). Mail body:
`doc/mail-torture-trio/REQUEST.txt` (this branch).

Owner sends; lanes never send. Never push, never upload from a lane.

## Upload steps

1. Pack verify (DONE this run, see manifest): 3/3 `cmp`-identical to
   both sources, `sha256sum -c` 3/3 OK, regen byte-identical, sizes
   1042x3. Re-run before sending if the pack aged:
   `cd pack && sha256sum -c SHA256SUMS`.
2. Mail REQUEST.txt body to **samples-request@ffmpeg.org** with the 3
   attachments (`t17-rate14.ac4`, `t17b-rate14-f0.ac4`,
   `t18-rate15.ac4`; NOT the .md docs, NOT REQUEST.txt itself
   unless you want it attached — paste it as the message body).
3. Wait for the upload-confirmation reply. Policy: suite files must
   be live **>=24 h before** the FATE post that references them
   (future parser-torture harness, not in the current submit set).
4. After confirmation, verify locally:
   `make fate-rsync SAMPLES=/path/to/fate-suite`, then confirm
   `ac4/t17-rate14.ac4`, `ac4/t17b-rate14-f0.ac4`,
   `ac4/t18-rate15.ac4` exist in the suite and
   `sha256sum -c` against the manifest pins passes 3/3.
5. Only then post the FATE change that references them. D4 parser
   FATE is self-contained and needs no wait; the torture-harness
   post waits on this clock.

## Suite path + naming

- Target dir: `ac4/` (from the decoder mail; these names clobber
  nothing — never overwrite suite files per policy).
- Proposed names are in REQUEST.txt (`ac4/tNN-*.ac4`, matching the
  staged mak comment and the existing lowercase-hyphen `.ac4`
  clip shape). If the maintainer renames anything, update the
  staged `fate-b2/ac4.mak` torture block at implementation time.

## Timing vs patch post

- Decoder mail (15 files): sent; its 24h clock runs separately.
- This trio: READY-AT-CLOCK-END — pack verified, mail drafted, send
  at clock end (or as the owner directs).
- No submit-set item blocks on this trio (parser torture is
  OWNER-GATES item 8, future parser-FATE implementation).

## Verification already done (this run, read-only + copy-verify)

- 3/3 staged copies `cmp`-identical to `ff-parser-torture/samples/`
  and to `ff-torture-nits/samples/`; source dirs untouched.
- sha256 of all 3 re-checked: match manifest rows, source
  SHA256SUMS rows, and TORTURE.md table rows.
- Sizes 1042x3 = 3126 B; REQUEST.txt header/body/notes agree.
- Regen: scratch-local `build.py` rerun, 3/3 byte-identical.
- No `[OWNER` / `[TODO` / placeholder text in REQUEST.txt, this
  file, or the manifest (grep-verified at commit).
- No sample bytes on the branch (`git status` shows doc files
  only; pack lives in scratch).

## Known gaps (NOT in this package)

- Remaining 28 torture vectors ride a later batch (see manifest).
- Parser-torture FATE mechanism TBD; goldens recorded from the real
  parser at implementation, never from cleanroom bytes.
