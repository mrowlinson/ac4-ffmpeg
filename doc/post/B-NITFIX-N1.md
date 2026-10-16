# B-NITFIX-N1 -- VERIFY-B count wart fix

Source: VERIFY-B N1 (`ff-B-series-verify/VERIFY-B.md:56-57`):
checklist said 2/2 = "12 files"; actual = 11.

Fix (this branch `ff/doc-B-nit`, from master `f827e56`):
`doc/post/B-POST-CHECKLIST.md:59`: `(2/2, 12 files)` -> `(2/2, 11 files)`.
One word, no other content touched.

Evidence (from `doc/post/series-B-decfate.mbox` on master):
1/2 = 3 files (Changelog, decoders.texi, general_contents.texi).
2/2 = 11 files (tests/Makefile + tests/fate/ac4.mak + 9 refs:
51, 714, copy-51, copy-stereo, crc-bad, delay, ims120, seek-eof,
stereo51). Total 14 = 5/5 files + 9/9 refs, consistent.

Sweep: `grep -rn "12 files" doc/` now clean (rc=1). Other B counts
re-checked vs mbox, all correct: 13067 B, 105-line mak, 10 decode
CMDs + 4 copy = 14 live, 16/16 parity (+2 deferred comments),
11 referenced suite names, 6/6 options, aggregate count 1,
Makefile include ac3/ac4/adpcm. Series-A counts untouched.

Re-verify at re-cut per VERIFY-B.
