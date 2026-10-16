# fate-b2: B2 FATE stage applied at decoder landing

B2 (`fate: add ac4 decoder tests`, SUBMIT-PLAN-V2 Series B) staged
against merged dec-lib (d505b36). Lands in-tree as
`tests/fate/ac4.mak` once the Series-A wrapper + suite samples exist.

## Files

- `ac4.mak` — staged decode/demux mak, stage v3n
  (`ff-B2-prep/stage/mak/ac4-decode-demux.mak`) + torture-30
  nits G1 (finding-3 comment fix) + G2 (t17b pin, 31 samples)
  (sha256 66c97754...efc18, 425 lines). Landing: see
  `## Landing (APPEND+MERGE, never copy)` below. ENC tail
  re-added at C4 (see mak tail + ac4.mak.v3-full in stage).
- `pins.txt` — pins validated vs dec-lib output bytes (this branch).
- `validate.py` — runnable pin gate (`python3 validate.py`, exit 0 =
  all pins match lib output). Needs scratch samples (see --help).
- `samples-manifest.md` — blocked-5 acquired-sample pins + provenance
  (ims120, chanbased, 714-caveat; bytes-ready, mail pending).
- `torture-trio-manifest.md` — TOC-gate trio pins + provenance
  (t17/t17b/t18; pack-ready, mail drafted, send at clock end).

## Per-item verdicts

APPLIED = on this branch. SUITE-GATED = needs suite samples and/or
the real wrapper (not attempted, per brief).

| # | Item | Verdict |
|---|---|---|
| 1 | fate-ac4-stereo stanza + REF pin 5fe8ef4c (byte-identical) | APPLIED |
| 2 | fate-ac4-mono stanza (SAMPLE-TBD comment carried) | APPLIED as TBD |
| 3 | fate-ac4-seek stanza; slice==2nd-half evidence recorded | APPLIED; REF record SUITE-GATED |
| 4 | fate-ac4-51 stanza + PCM pin 354f6e0c + 6ch | APPLIED; framemd5 golden SUITE-GATED |
| 5 | fate-ac4-ims120 stanza (SAMPLE-TBD carried) | APPLIED as TBD; sample SUITE-GATED (blocked: no Part-2 tool) |
| 6 | fate-ac4-714 stanza (SAMPLE-TBD carried) | APPLIED as TBD; sample SUITE-GATED (blocked: tool downmixes) |
| 7 | fate-ac4-gap-fails stanza (D4 redefine carried) | APPLIED; sample+golden SUITE-GATED (blocked: no codec-4 knob) |
| 8 | fate-ac4-stereo51 stanza + pin d91a4274 + c2-c5-zero | APPLIED; 2nd-mail upload SUITE-GATED |
| 9 | fate-ac4-delay stanza + pin bab2cb61 (5 frames) | APPLIED; 2nd-mail upload SUITE-GATED |
| 10 | fate-ac4-crc-bad stanza + pin 2ee1ade1 + crc1 note | APPLIED; suite upload SUITE-GATED |
| 11 | fate-ac4-crc-bad-note stanza (REF=crc1 placeholder) | APPLIED; wording SUITE-GATED |
| 12 | fate-ac4-gap-trunc stanza; lib exit-3/no-sync confirmed | APPLIED; wrapper asserts SUITE-GATED |
| 13 | Deferred slots (delay-1536, float), comment-only | APPLIED |
| 14 | copy-stereo/copy-51 stanzas | APPLIED; goldens SUITE-GATED |
| 15 | seek-eof stanza | APPLIED; golden SUITE-GATED |
| 16 | probe-garbage stanza (REF pinned string) | APPLIED; suite-bytes re-confirm SUITE-GATED |
| 17 | F6 gate split + A1 wiring + B tail (in-mak) | APPLIED |
| 18 | Parser block comments + demux contrast | APPLIED; mechanism + 31-sample upload SUITE-GATED |
| 19 | tests/Makefile include line | APPLIED (already in-tree :157; landing = append+merge, no include step) |
| 20 | M3 fate-refs (demux.mak/seek.mak + 3 refs) | N/A already landed (ffmpeg-clone 2f7e33f, verified) |
| 21 | Owner samples mail + 24h clock | SUITE-GATED (owner action) |
| 22 | `fate-ac4` full + `--disable-everything` run | SUITE-GATED (needs wrapper) |

Out of scope: B1 decoder docs (rides A2-tail; A2 not landed).

## Gates (this branch)

- dec-lib parity 100/100 vs c-ac4dec oracle (run.py on worktree
  shim build; see scratch ff-B2-stage/parity-run.py output).
- `python3 fate-b2/validate.py` PASS (23 checks: 5 pins
  byte-identical, props, negatives).

## Landing (APPEND+MERGE, never copy) — F1 fix

NEVER plain-copy this file over in-tree `tests/fate/ac4.mak`: that
file already exists (E-track TS-mux tests) and the tests/Makefile
include line is already present (line 157). A copy would CLOBBER
the E tests, and both files define a single-colon `fate-ac4:`
target — make keeps the last, silently dropping the other set.

Land instead (mechanical, byte-verbatim stanza move, zero content
change; validated by the ff/doc-b2f1 dry-run):

1. Anchor: the single `fate-ac4:` aggregate line of in-tree
   `tests/fate/ac4.mak` (E-track tail).
2. Insert fate-b2/ac4.mak lines 151-369 (16 test stanzas + F6
   gates + F6 comment, byte-verbatim, single-blank separators)
   immediately BEFORE the aggregate line. Do NOT land lines
   1-150 (draft header, staging-only), 371-420 (parser staging
   block), or 425 (this file's own `fate-ac4:` line).
3. Land the B2 wiring with the other wiring lines, just above the
   aggregate: `# --- B2 wiring (decode/demux only; ENC terms
   re-added at C4) ---` + line 423 (`FATE_SAMPLES_AVCONV +=
   $(FATE_AC4-yes) $(FATE_AC4_MD5-yes) $(FATE_AC4_REMUX-yes)`).
4. Extend the ONE aggregate with the B2 vars (E vars first):
   `fate-ac4: $(FATE_AC4_MPEGTS_PMT-yes)
   $(FATE_AC4_MPEGTS_PROBE-yes) $(FATE_AC4-yes)
   $(FATE_AC4_MD5-yes) $(FATE_AC4_REMUX-yes)`.
5. Verify: `grep -c '^fate-ac4:'` = 1; E stanza + all 16 B2 tests
   present; no `overriding recipe` from `make -p`.

## Re-record rule (from APPLY-CHECKLIST §3, binding at B-time)

Every pin here is lib-derived. In-tree framemd5 goldens MUST be
re-recorded from the real wrapper at B-time; never commit staged
bytes as goldens.
