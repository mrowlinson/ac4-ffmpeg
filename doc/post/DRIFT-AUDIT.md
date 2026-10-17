# DRIFT-AUDIT: FF dec-lib/src + lavc-dec/core vs plain C port

Date: 2026-10-17. Seat: ff-drift-audit R2 (TB-only; MBP down at resume,
Dolby-binary checks UNRUN).
Bases: ff clone @ 224ec0b (branch ff/doc-drift-audit) vs plain repo
dolby-ac4-cleanroom/src @ 7609dfcd7 (clean tree, read-only).
Method: per-file diff + normalized diff (comments/blank/ws stripped;
scratch normdiff.py) + symbol inventories + env-knob set-diff +
rebuild + differential drivers. Normdiff treats comment-only hunks as
equal; every remaining replace hunk was read first-hand.

## Verdict scale

- IDENTICAL: byte- or normalized-identical, no behavior delta.
- INTENTIONAL: delta matches a cited design doc / in-code scope note.
- DRIFT: unexplained delta, needs a fix (fix spec inline).
- UNRUN: could not execute locally (reason noted).

## A. dec-lib/src vs plain C port

| # | Item | Finding | Verdict |
|---|------|---------|---------|
| A1 | aspx_noise, aspx_tables, huffman_tables, imdct, qmf, qwin_tables, sfb_offsets, snf_tables (.c+.h) | `diff` 0 lines each (16 files) | IDENTICAL |
| A2 | entropy.c | 1 line: trailing-whitespace only (plain:1205 vs src:1204) | IDENTICAL |
| A3 | entropy.h | 0 lines | IDENTICAL |
| A4 | acpl.c/h: file-statics (prev_q/prev_have, IW_* scratch) -> per-instance acpl_state_t param | Same arithmetic, mechanical rename; caller reset preserved (plain ac4dec.c:5890 `acpl_reset()`; src dec_lib.c:6512 `acpl_reset(&c->acpl)` at open) | INTENTIONAL: dec-lib/Makefile header ("except acpl.c/h (state threaded)") |
| A5 | dec_lib.c vs ac4dec.c: file-statics -> heap ctx (_ac4d_self macro field map, e.g. e3_cmp_on/e3_dngain/e_drc_cli) | Same latch/compute order; e3_cmp_on values identical, storage moved | INTENTIONAL: dec-lib/Makefile header ("dec_lib.c (ctx conversion + streaming split)") |
| A6 | DRC apply leg missing in FF (plain ac4dec.c ~7874-8085: SET-A transmitted-gains + curve/follower, drc_floor/drc_scale/drc_chw fit tables) | Plain leg is default-OFF (gated `AC4DEC_DRC=1` or --drc); FF ac4d_open rejects drc_on/AC4DEC_DRC with AC4D_UNSUPPORTED ("Whole-stream legs unsupported in streaming v1 (H1)", src dec_lib.c:6444-6452); shim maps to `drc-unimpl` | INTENTIONAL (H1 scope). Parity-neutral at defaults. No fix; optional future: offline DRC post-pass |
| A7 | Knobs in plain, absent in src: AC4DEC_DRC_TA, AC4DEC_DRC_G0, AC4DEC_DRC_D0, AC4DEC_CMPLEAD (DRC-leg params) | All consumed only by the A6 leg | INTENTIONAL (same H1 strip). Knob set-diff: src-not-plain = {} (no novel knobs) |
| A8 | AC4DEC_HEIGHTS side file (plain ac4dec.c:6615: separate 4ch heights .pcm) | FF drops the side file; mains chain untouched ("Heights artifact dropped in streaming (side file)", src dec_lib.c:7478) | INTENTIONAL (streaming v1 scope). Mains bytes unaffected |
| A9 | e4_sr[] grid table deleted; out_sr = base_fs x sf_mult (N5) | Plain still rates via e4_sr round(spf*fps) labels (ac4dec.c:8448 VER fps, :9158 out_sr); FF latches sf_mult + Table E.4 rate (src dec_lib.c:6652, ac4_src.h s5.11 counts) | INTENTIONAL: COMBO-RERUN-7 (N5-CORRECT, pins 14/14), in-code N5 notes. FIX SPEC (plain-side): port N5 to plain ac4dec.c out_sr + VER fps (grate!=13 rows diverge today) |
| A10 | e4_spf[] internal grid | Identical 14 values both sides | IDENTICAL |
| A11 | drc_film_light_l/g fallback curve tables | Identical values (src:5105-5106, core:3465-3466) | IDENTICAL |

## B. lavc-dec/core vs dec-lib/src (in-tree port)

Master cite: lavc-dec/README.md (port-delta list + pinned defaults +
provenance pin dec-lib 8e27f25) + COMBO-RERUN-7 ("lavc copy = dec-lib
minus CLI/shim + LGPL header per vendor note").

| # | Item | Finding | Verdict |
|---|------|---------|---------|
| B1 | File headers -> FFmpeg LGPL + guard remap (AC4_* -> AVCODEC_*_H, `#endif /* */`) on all 12 headers | Mechanical | INTENTIONAL (README re-vendor rule; fate-source) |
| B2 | 8 table files *_tables.c/h -> *_tab.c/h (+ #includes, OBJS) | Values bit-identical (normdiff: 0 del / 0 ins; sole replace = the #include line; sfb_offsets + aspx_noise: 0 hunks) | INTENTIONAL (README: dodges libavcodec CLEANFILES + .gitignore wipe; ac3tab precedent) |
| B3 | ac4d.h: log/log_u callback deleted; guard AC4D_H -> AVCODEC_AC4D_H; H1 wording trimmed | API otherwise identical | INTENTIONAL (README lib-contract list) |
| B4 | ac4_stages.h not ported (141 lines) | Unreferenced dead header (zero #includes anywhere); build + gates green without it | INTENTIONAL (README x3) |
| B5 | dec_lib.c: 48 static fns dropped (TS/MP4 demux, moov/moof, chain_walk, shim main + report/fmt/info, DRC dump/select, e3_shf*, e9_* debug) + 11 externals (sym diff): acpl_probe_1ch, ac4d_logf2, drc_outref_eff, drc_select_mode, e3_drc_curve_dump, e3_drc_dump, e3_shfgain, e3_shfgain_n, e3_shfparse, e9_debug, e9_show | CLI/shim/demux/debug surface only; zero DSP-entry loss (core-not-src function scan: empty) | INTENTIONAL (README: CLI strip + env/dump/probe strip) |
| B6 | dec_lib.c kept-code edits: 28 normalized replaces, 3467 del, 0 added | Every hunk read: includes (B2), calloc->av_calloc/free->av_free (x4), DRC gate minus env (default off), ce_on minus nocmp (default 0), SPLF fprintf->`;`, KEEP67 x3 (default zero), compand knob block -> hardcoded defaults (nocmp/unity/inv/census/dump/fullint/unshift/prewarm/onsetavg/floorskip/useflat/frameoff/frameof2/avgall/gainhold/hotfill=1/fade0=0), unity/inv/avgall/hotfill/gainhold/unshift folds (all default-valued), M4CURVE file-load -> builtin, M4 gate -> always-on | INTENTIONAL, equivalent under default env (README pinned-defaults list matches hunk-by-hunk: M4 on, M4CURVE builtin, SHF off, SYNK 5, KEEP67 zeroing, compand defaults + hotfill on) |
| B7 | entropy.c: 334 lines deleted (AC4DEC_DQOFF block, AC4DEC_DEBUG prints, ac4e_sgn_* encoder globals), 3 replaces (2 include renames + dqe fold) | dqe derives from AC4DEC_DQOFF, default 0.0 -> `pow(aq+0)` == core `pow(aq)`; same 11 statics both sides | INTENTIONAL (default-equiv strip) |
| B8 | imdct.c: 97 del (KBDNPTS/NOCOSCACHE env, ola_note, cos-cache malloc path), 4 replaces + 1 added line | KBDNPTS->table alphas, NOCOSCACHE->cached kernels (defaults); malloc/free->av_malloc/av_free; the 1 added line = `#include "libavutil/mem.h"` | INTENTIONAL |
| B9 | qmf.c: SYNK env block -> `k < 5` | Default pinned (README lists SYNK 5) | INTENTIONAL |
| B10 | acpl.c: ACPLPROBE/ZDUMP env blocks + acpl_probe_1ch + dump_frame field/init stripped (64 del, 1 include-rename replace) | Forensics-only; dump_frame field removed with its uses (no dangling refs) | INTENTIONAL |
| B11 | calloc/malloc/free -> av_* (dec_lib.c x5 sites, imdct.c x2); unistd.h dropped | Same lifetimes; core builds (in-tree-only by design) | INTENTIONAL (README allocator conversion) |
| B12 | SHF short-window rolloff block dropped | Env-gated experimental (AC4DEC_SHF unset -> mode -1 -> no-op); core == default path ("SHF off" per README) | INTENTIONAL |
| B13 | Comment scrub + ASCII map | Core ASCII-clean (zero non-ASCII lines); DSP comments keep table/clause cites | INTENTIONAL |
| B14 | lavc-dec/ac4dec.c wrapper (853 lines, FF-only, no plain counterpart) | Thin AVCodec wrapper; calls public ac4d_* API only (README: "UNCHANGED by the Q1 port") | Out of diff scope; covered by wrapper gates (F2: UNRUN, see C5) |

## C. Parity-relevant suites (rebuilt + run this seat)

All builds out-of-tree in NVME scratch; plain repo untouched.

| # | Suite | Result |
|---|-------|--------|
| C1 | dec-lib rebuild (`make clean all test`) | BUILD OK, ac4_src_test ALL PASS, zero warnings shown |
| C2 | parity-run.py: fresh shim vs plain oracle c-ac4dec (100 vectors, scratch parity-clean.log) | IDENT 90/100. ALL 10 DIFFs = N5 rate/count divergence (A9): oracle FMT sr=46034/51200 (e4_sr grid) vs shim 48000 (base_fs x sf_mult), identical frame counts + notes; byte ratios exactly 46034/48000 or 51200/48000 (s5.11 Table-70 counts follow the label). Zero unexplained diffs. (First run 60/100 was seat-contaminated: concurrent shim --out to parity/a.pcm; reran clean.) |
| C3 | ngis-gate.sh (ASan/UBSan rebuild) | PASS (pin + repro75 + corner100) |
| C4 | fuzzfix-gate.sh (ASan/UBSan rebuild) | PASS (4 dnsplice repros + opt repro + grinds 205-210 all PASS, ~53.7k feeds each) |
| C5 | f2-gap-trunc.sh | UNRUN: needs an in-tree ffmpeg binary carrying this audit core; none exists locally (prebuilt trees predate it); building one is out of audit scope |
| C6 | fate-n5/validate.py --shim fresh | PASS 14/14 (gpac PCM pin 8b41f7cf, 5727312 B, wav-sr, sf4/fs1 contrasts) |
| C7 | fate-b2/validate.py --shim fresh | PASS (stereo/51/crc-bad/stereo51/impulse pins + ref.pcm byte-ident + gap-trunc/garbage neg controls) |
| C8 | diffdrain src-vs-core differential (new: scratch diffdrain.c + av-shim; links src/*.c with -DAC4D_LIB_ONLY vs core/*.c) | stereo 3-frame PCM IDENT (16384 B) + log IDENT; 5.1 3-frame PCM IDENT (49152 B, f1 -3 both, matches shim rej 1); 50/50 harness mutant packets PCM+rc IDENT; long-stream gpac 70-frame PCM IDENT (1345344 B, sha b892472f..., all rc=0 both) |
| C9 | Dolby-binary probes (DRP/DME on MBP) | UNRUN: MBP TB link down at R2 resume (ssh + ping 169.254.103.231 fail); venue rule forbids Studio runs; prior seat VENUE-OK (HB1) retained for provenance only |

## D. Fix specs

1. (plain-side, from A9) Port N5 rate semantics to
   dolby-ac4-cleanroom/src/ac4dec.c: replace `e4_sr[grate]` in out_sr
   (line ~9158) and VER fps (line ~8448) with
   `(gfs==0?44100:48000) x sf_mult` latched from the TOC bed-gis row
   (mirror src dec_lib.c:6644-6660 + fate-n5 pins). Until then, any
   grate!=13 vector rates differently between plain and FF (label +
   counts), and parity-run FMT compares will flag it. Owner call:
   port or record plain-wontfix (FF is spec-correct per Table E.4).
2. No FF-side fixes: zero DRIFT items. The B5 dropped externals
   (drc_select_mode et al.) are intentionally absent from the
   streaming core; do NOT re-add (H1 scope + lavc README reserved-
   surface deletion).

## E. Reproduce

```
S=/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-drift-audit
make -C $S/ff/dec-lib clean all test
python3 $S/ff/fate-n5/validate.py --shim $S/ff/dec-lib/build/shim --tmp $S/n5-out
python3 $S/ff/fate-b2/validate.py --shim $S/ff/dec-lib/build/shim
sh $S/ff/dec-lib/tests/ngis-gate.sh $S/ngis-gate-bld2
python3 $S/normdiff.py $S/ff/dec-lib/src/dec_lib.c $S/ff/lavc-dec/core/dec_lib.c
```
