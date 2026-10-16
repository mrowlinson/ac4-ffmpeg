# dsi: standalone AC-4 DSI serializer (pure C)

Emits one `ac4_dsi_v1` payload (ETSI TS 103 190-2 Annex E) exactly as it
must appear in the MP4 `dac4` box: payload bytes only, no box header.
No dependencies, no allocation; the caller supplies the output buffer.
The ffmpeg tree is untouched; this directory is self-contained.

Layout source: `DSI-CONTRACT.md` (ff-dsi-contract scratch), verified
against 3 corpus MP4s. Field order also cross-checked against the
cleanroom `dac4_probe.py` parse order (read-only).

## Files

- `ac4dsi.h` / `ac4dsi.c` — serializer (`ac4dsi_serialize` + init helpers).
- `ac4dsi_priv.h` — shared validation + wire constants for the two
  emitters (private, not installed).
- `ac4dsi_putbits.h` / `ac4dsi_putbits.c` — PutBitContext-based
  serializer (`ac4dsi_serialize_putbits`, DSI-11). Same contract as
  `ac4dsi_serialize`, byte-identical output; only the `.c` needs lavc
  headers. Lib-level only: C-encoder wrapper adoption is a follow-up.
- `dsi_goldens.h` — golden vectors + shape builders shared by both
  suites (they can never drift apart on inputs).
- `dsi_test.c` — golden byte-exact tests + unit negatives + mask-API
  tests (hand-rolled oracle).
- `dsi_putbits_test.c` — differential tests (putbits vs oracle) built
  against the REAL lavc headers; needs a configured ffmpeg tree, see
  `LAVCBASE`/`LAVCSRC` in the Makefile (defaults to the `ff-C` scratch
  build; override for an in-tree-configured ffmpeg).
- `Makefile` — `make test` builds with `-Wall -Wextra -Wpedantic` and runs;
  `make test-werror` adds `-Werror`, `make test-asan` runs the suite under
  ASan+UBSan. `dsi_test` covers the deprecated string shim, so that one
  TU compiles with `-Wno-deprecated-declarations` (scoped, commented).

## API

```c
ac4dsi_stream_t s;  ac4dsi_stream_init(&s);
s.bit_rate = 64000;                 /* caller MUST set target */
ac4dsi_pres_t p;    ac4dsi_pres_init_v1(&p);   /* stereo defaults */
uint8_t out[64]; size_t len;
int rc = ac4dsi_serialize(&s, &p, 1, out, sizeof(out), &len);
```

- `presentation_version == 1`: v1 block synthesized (config 0x1F, one
  channel-coded substream group). Layout helper (DSI-12 mask form,
  pass `avctx->ch_layout.u.mask` straight in):
  `ac4dsi_pres_set_layout_mask(&p, AC4DSI_CHMASK_STEREO | AC4DSI_CHMASK_5POINT1)`
  (0x3 / 0x60f, pinned to the lavc `AV_CH_LAYOUT_*` bits; the 0x3f BACK
  variant is rejected — no wire vectors pin it. If a future encoder
  wrapper accepts BACK, the table must grow it). Legacy
  `ac4dsi_pres_set_layout(&p, "stereo" | "5.1")` shim kept for CLI/test.
- `presentation_version >= 2`: `opaque`/`opaque_len` copied verbatim
  under pv/pres_bytes framing (opaque round-trip, never parsed).
  Rationale: our encoder TOC emits pv=1 only, and the MP4 muxer wraps
  encoder bytes verbatim, so this path is currently exercised by
  forward-compat shapes only (believer's pv2 entry pins it byte-exact).
  It also serves future demux-roundtrip work (re-emit a parsed pv2+
  block untouched). Kept deliberately; harmless when unused.
- `language_tag` is wire ISO 639-1 ("en"). lavc metadata is usually
  639-2 ("eng"): map 639-2 -> 639-1 before wiring (see header).
- `presentation_version == 0`: rejected (legacy v0, do not emit).
- Bitrate mode rule: `ac4dsi_mode_for_wait_frames()` (Table E.8).
- Errors: `AC4DSI_EINVAL` (bad/unsupported field), `AC4DSI_ENOSPC`
  (buffer too small; `*out_len` still reports the needed size).

## Open items (parameters with documented defaults)

Per contract section 3, encoder-owner decisions that pin FATE goldens:

| item | default | wild vectors |
|---|---|---|
| program id | off (`b_program_id=0`, our encoder TOC) | on, id 300 |
| presentation id | present, `=0` (our encoder TOC) | 10 |
| content tag | on, classifier 0, `"en"` | `"en"` |

`ch_mode` 11-14 (back/top modifiers) are rejected: no wire vectors pin
those bits, so emitting them would be unverifiable.

## Gates

- 4/4 goldens byte-exact (`dashif` 34 B, `livesim2` 34 B, `believer`
  52 B with pv2-opaque + v1 entries, `uuid` 50 B with program uuid).
- Boundary pins: 63-char tag (max legal, bytes verbatim), 300 B opaque
  pres_bytes escape (0xFF + u16), exact-fit / off-by-one, ch_mode 11-14
  loop, out_len NULL, EINVAL-keeps-len.
- Warning-clean: `-Wall -Wextra -Wpedantic`, `-Werror` via `make test-werror`.
- Unit negatives pass (NULLs, range violations, pv0, bsv1+prog,
  uuid-without-prog, 64-char tag, small-buffer ENOSPC with size).
- ASan+UBSan clean (`make test-asan`); 4 extra shapes (stereo/5.1/uuid/no-content-type)
  re-parsed exact by the independent cleanroom probe.
- DSI-11: putbits emitter byte-identical to the oracle on all 4 goldens
  x every cap 0..need+1, tag63/escape shapes, max-opaque (65790),
  maxed-v1, 4-pres mix, full ch_mode 0..31 sweep, mask-layout sweep,
  and 3000 deterministic fuzz cases (rc/len/bytes incl. ENOSPC
  partials), all against the real lavc headers + libavutil link.
- DSI-12: mask table pins (stereo 0/1/0x1, 5.1 1/4/0x47), exotic
  rejects (incl. 5.1-BACK 0x3f, 7.1, quad, mono, all single-bit masks),
  fail-leaves-untouched, string-shim equivalence.
