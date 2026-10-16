# DSI drift vs C-r3 wire bytes — verdict DOC-ONLY

Date: 2026-10-16. Scratch: `ff-dsi-drift` (probes + exports kept there).
Scope: parent-master `dsi/` (0f6c6e2; `dsi/` identical to 9c7fb2e) vs
r3-tip vendored DSI (`ac4/C-encoder-split-r3@065976d`,
`libavcodec/ac4dsi.{c,h}`, `ac4dsi_priv.h`, `ac4dsi_putbits.{c,h}`).
Vendor base: `ff/dsi-lavcshape-merged` (6ae9514). Drift since vendor =
`ff/dsi-minors2` (355d1e0) only.

## Verdict: DOC-ONLY — no wire follow-up

minors2 touches zero wire bytes. Proof (all first-hand, TMPDIR=scratch):

1. lavcshape→master = exactly the 7 minors2 files; `dsi_goldens.h`
   byte-identical.
2. Suites re-run: master `test`/`test-werror`/`test-asan` ALL PASS, 4/4
   goldens byte-exact both suites (tally 920/1065/1804); lavcshape
   `test` ALL PASS, same 4/4 pins. Same inputs → same pinned bytes.
3. Kept bodies mechanical-diff master-vs-r3: 3 validators
   (taglen/v1_check/stream_check) BODY-IDENTICAL modulo linkage;
   stream_init/pres_init_v1/set_layout_mask/mode/strerror/layout_map
   IDENTICAL.
4. putbits emission master-vs-r3 differs only in the capbits expression
   (MINOR-1) + comments/include path. Probe: old vs new identical for
   all caps 0..2^20 and both INT_MAX knees ≤ INT_MAX; differ only
   above INT_MAX (unreachable: in-tree DSI buffer is 128 B).
5. End-to-end repro with the r3 wrapper's exact inputs
   (stream_init + 128000/256000 bps, mask 0x3/0x60f): parent-master
   oracle AND putbits emit the r3-pinned bytes exactly, both layouts:
   - stereo (32 B): `20ba0140003e801fffffffe00112f880000042000002501000000310995b8080`
   - 5.1 (32 B): `20ba0140007d001fffffffe00112f98000004800008e501000008f10995b8080`
6. Vendor delta itself is by design: LGPL prepend + oracle/shim strip
   from `.c`/`.h` + comments + `put_bits.h` include path. No emission
   change.

## Copyright L3 (this branch)

r3-tip headers lack the Copyright line C1/C2 carry (7 files: C3
`ac4enc.c`/`ac4enc.h` + 5 DSI). This branch fixes the parent-side half:
canonical LGPL block with ` * Copyright (c) 2026 Michael Rowlinson` L3
on the 5 vendored DSI files (titles = r3-tip verbatim, so the next
re-vendor is header-stable). Stale "no license header yet" NOTE in
`ac4dsi.h` updated (DSI-15 remainder resolved for the vendored set).

C3 wrapper half is ffmpeg-side only — at the next recut, insert one
line as new L3 of `libavcodec/ac4enc.c` and `libavcodec/ac4enc.h`:
` * Copyright (c) 2026 Michael Rowlinson` (after the title line).
Re-vendor rule from here on: keep parent DSI headers verbatim (never
re-prepend); oracle/shim strip unchanged.

## Gates

- Wire goldens byte-exact re-proven (item 2 + 5 above; suites re-run on
  this branch post-header, comments-only — still ALL PASS).
- LGPL sweep: vendored set 5/5 headed with L3 → 0 headerless; r3-tip
  ffmpeg stack re-swept, still 0 headerless.
- Build clean (`test-werror`, `-Werror`, ASan).
