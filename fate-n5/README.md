# fate-n5: N5 rate-semantics FATE stage (dec-lib fix)

N5 (`ff/dec-n5fix-r2`): dec-lib output rate = base_fs x sf_mult on all fri
rows + s5.11 converter (internal spf -> Table-70 output counts locked
to sequence_counter). Staged against the fixed dec-lib (this branch).
Lands in-tree as FATE decode asserts once the Series-A wrapper +
suite samples exist (fate-ac4-n5-rate slot family, TBD at B-time).

## Files

- `pins.txt` — pins validated vs fixed dec-lib output (this branch).
- `validate.py` — runnable pin gate (`python3 validate.py`, exit 0 =
  all pins match lib output). Needs scratch samples (see --help).

## Per-item verdicts

APPLIED = on this branch. SUITE-GATED = needs suite samples and/or
the real wrapper (not attempted, per brief).

| # | Item | Verdict |
|---|---|---|
| 1 | gpac FMT sr=48000 (was 46034), 298 frames x 1601/1602, note=pcm | APPLIED |
| 2 | gpac PCM pin 8b41f7cf (5727312 B = 477276 x 6ch x 2B) | APPLIED |
| 3 | gpac WAV header sr field = 48000 | APPLIED |
| 4 | g-fri2-sf4 FMT sr=192000 fail/lead-trunc (sf_mult honored) | APPLIED |
| 5 | g-fri2-fs1 FMT sr=48000 fail/lead-trunc (sf=1 contrast) | APPLIED |
| 6 | In-tree fate-ac4-n5-rate stanza + framemd5 golden | SUITE-GATED (needs wrapper; re-record rule below) |
| 7 | A0 packet-duration phase (round) vs Table 70 (floor): 1-sample | A0 FOLLOW-UP (not dec-lib; see report) |

## Gates (this branch)

- `dec-lib` unit `make test` PASS (Table 70 exact, VER fps preserved).
- `python3 fate-n5/validate.py` PASS (14 checks).
- Full dec parity re-run: only N5-expected diffs (fri 0-12 rate/count
  changes), zero unexpected (see branch report).

## Re-record rule (binding at B-time)

Every pin here is lib-derived. In-tree framemd5 goldens MUST be
re-recorded from the real wrapper at B-time; never commit staged
bytes as goldens. The linear SRC kernel is a placeholder (counts are
spec-pinned; values are not DRP-bit-exact) — goldens re-recorded
after a kernel upgrade will move.
