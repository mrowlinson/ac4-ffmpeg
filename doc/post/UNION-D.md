# UNION-D — one gated Series-D mbox (D-seat content + stack mechanics + T3)

Seat ff-union-D. Base master `6905822` (stack docs merged; D mbox held
for this seat). Inputs: D-seat `a840073` (M19-M21 r2 + A0 companion)
vs stack `3b65f39` (DAG mechanics) + T3 spec (STACK-DAG.md, for D).
Deliverable: `doc/post/series-D-mp4.mbox` (162643 B, 6 patches),
`doc/covers/COVER-D.txt` (content refresh, still 0/6), send-doc D
sizes, this record. Branch `ff/doc-union-D`.

## Divergence resolutions

- M19 (Changelog policy): D-seat lines kept, re-anchored to the stack:
  `- AC-4 parser` after E's TS-descriptors line (D 1/6),
  `- AC-4 audio in MP4/MOV (ac-4 sample entry with dac4 box)` after
  the parser line (D 3/6). D stays stack-dependent by necessity
  (no dual-applying hunk exists; D-seat proof stands).
- M20 (multi-frame): landed verbatim (mov_ac4_is_multiframe helper +
  EINVAL call site + D 5/6 negative). movenc.c bytes identical to
  D-seat tip; parser.c + both test C files likewise.
- M21 (one TOC walker): SPLIT by seat ownership. The A0 union belongs
  to ff-union-B-A0 (A0 frozen wrt D), so the D-seat A0 companion
  (need_parsing + demuxer conversion + golden re-pins) does NOT ride
  here. Union D lands the parser half: D 1/6 creates header-only
  `libavcodec/ac4_toc.h` (D-seat bytes, comment-only honesty edit:
  parser uses it now, raw demuxer converges in a follow-up) and the
  parser adopts it (output-domain params, reconciled branches).
  D 4/6 builder fix + 12 vectors ride. Zero golden churn: demuxer
  untouched, no keyflag/param flips, no re-pins in D.
  Parent seam: if B-A0's A0 creates ac4_toc.h too, drop D 1/6's
  header-create hunk at merge (parser + messages already assume the
  shared header; demuxer-convergence follow-up then just lands).
- Stack mechanics kept: configure/Makefile/parsers.c hunks, mak
  appendix shape + aggregate order (PARSE MP4 TSDEMUX RT), E -strict
  -2 probe, T1/T2/T5 goldens. D 2/6 + D 6/6 test bytes identical to
  both inputs (From-hash-only deltas).
- T3 CLAIMED by D (first right per STACK-DAG; C only if D defers):
  new FATE_AC4_RT section riding D 5/6 (series stays 6/6, no cover
  renumber). GEN synth stereo -> raw AC-4 -> decode -> framemd5
  (`tests/ref/fate/ac4-roundtrip`, 150 lines); first frame dts/pts
  4096 pins the encoder initial_padding vs decoder skip offset.
  C seat: do NOT land a second T3 (dup); parent drops any C-side one.

## Gates (all green, out-of-tree build/, TMPDIR scratch, suite mirror read-only)

- DAG stack-am: 24/24 `git am --whitespace=error` on fresh pristine
  3e6cd5e (master A0/A/B/C/E + union D); tip tree identical to the
  built tree. `git show --check` clean all 6; subjects 6/6 `[PATCH
  n/6]`; signoffs 6/6; 0 non-ASCII; 0 placeholders.
- Build: configure rc=0, `make -j18` rc=0, 34 warnings (all
  baseline-file, 0 ac4).
- `fate-ac4` 29/29 rc=0 (28 stack + T3). D: parser/mp4-roundtrip/
  ts-demux/roundtrip + aliases parse/mp4/ts/rt all green. GEN +
  golden deterministic (byte-identical re-runs).
- Fixed during union: T3 section must precede the aggregate (make
  expands prereqs at parse time); mak EOF blank-line hygiene.

## Parent notes

- B-A0 seam (above) is the only merge-time decision.
- D-PREFLIGHT-CHECKLIST.md still describes the r3 source line; needs
  a union refresh pass at release (same for B/C).
- No TB sends made (suite mirror + local GEN only). Nothing pushed.
