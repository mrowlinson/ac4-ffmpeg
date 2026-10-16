# DEE-banner provenance reconciliation

Date: 2026-10-16. Flag: `ff-gates-exec/GATES-EXEC.md` §5 obs 1 —
`c-ac4enc --help` banners as "Dolby Encoding Engine ... 6.5.4" while
FATE-PREP/REQUEST.txt describe pack clips as "encoded with our own
cleanroom AC-4 encoder ... no Dolby bytes". Reconciled before any
mail cites it. Plain repo read-only throughout; parent untouched
except this branch.

## Findings (all first-hand)

Banner (verified `./c-ac4enc --help`, exit 0, TMPDIR in scratch):

    Dolby AC-4 encoder.
    ...
    This tool belongs to the Dolby Encoding Engine version 6.5.4-dme+b56bc97e
    Interface version: 1.2.0 (Jul 27 2026):

- Lives in: `dolby-ac4-cleanroom/encoder/help_text.py`
  (`IMS_HELP[5]`, `AC4_HELP[6]`, `AJOC_HELP[4]`). Module header:
  verbatim byte-exact DEE `--help` pulls, "do NOT hand-edit".
- Emitted by: `_print_dee_help` (`encoder/cli.py:24-42`, "Print the
  DEE-verbatim help body") via the frozen prebuilt `c-ac4enc`
  (Mach-O arm64).
- Pinned by plain-repo tests:
  `tests/test_ajoc_help.py::test_banner_iface` asserts the exact
  banner string. Rewording/stripping it breaks the plain suite and
  its byte-exact CLI-fidelity design.
- REQUEST.txt (scratch `ff-samples-pack/REQUEST.txt:35-39`, UNSENT)
  claimed: "encoded with our own cleanroom AC-4 encoder. No
  third-party content, no Dolby bytes." FATE-PREP.md:9-11 agrees
  ("Encoded with the cleanroom `c-ac4enc`"). The clips ARE
  cleanroom-encoded (FATE-PREP §§1-2: synth sine WAVs through
  `c-ac4enc`); the tension is only the tool's self-banner.

## Upstream expectation (first-hand)

Not "zero occurrences of Dolby": the ffmpeg tree carries
descriptive marks pervasively — `CODEC_LONG_NAME("Dolby E")`
(`libavcodec/codec_desc.c:3420`), `AV_CODEC_ID_DOLBY_E`, Dolby
Vision code/comments, a dolby.com URL in `doc/encoders.texi:183`.
The rule is no false origin + no copied Dolby text/code. Our
ffmpeg additions follow the descriptive pattern only:

- `CODEC_LONG_NAME("Dolby AC-4")` (`libavcodec/ac4enc.c:611`,
  `lavc-dec/ac4dec.c:805`) — mirrors the "Dolby E" precedent.
- `doc/encoders.texi`: "Dolby AC-4 audio encoder, natively
  implemented into FFmpeg." (origin-asserting, M5).
- Zero `Encoding Engine` / `6.5.4` / ODK hits in any ffmpeg
  branch history or our `ac4*` files; `enc-lib` deliberately
  excludes the CLI/banner layer (`enc_help_text.c`,
  `enc-lib/VENDOR.md:22-24`).

## Decision: docs-side reconciliation (banner untouched)

- Banner NOT reworded/stripped: plain repo is read-only for this
  task, the text is do-not-hand-edit fidelity data pinned by
  tests, and rebuilding the frozen `c-ac4enc` is out of scope.
  "Strip" = the DEE banner text appears nowhere in the submit
  surface (audited below), with this file as the justification.
- REQUEST.txt aligned (scratch working file, owner sends from
  it). New §Provenance:

      encoded with our own cleanroom AC-4 encoder (c-ac4enc, an
      independent implementation; no Dolby SDK, code, or bytes
      involved). No third-party content.

  Names the tool (transparency), scopes the no-Dolby claim to
  SDK/code/bytes (unassailable, matches cover-letter language:
  "No Dolby SDK, reference code, or disassembly was used").
  Clip claims otherwise unchanged (still true: synth PCM +
  cleanroom encode).

## Submit-surface audit (post-fix)

| Surface | Result |
|---|---|
| `REQUEST.txt` | aligned wording above; no banner text |
| FATE-PREP.md | "cleanroom `c-ac4enc`" — consistent, unchanged |
| Parent master + this branch | no `Encoding Engine`/`6.5.4` outside this file's quoted evidence; `dolby` hits = repo-name paths, wave-note comments, `CODEC_LONG_NAME`, cover denials/patent notes only |
| Our ffmpeg `ac4*` + docs diff | 3 descriptive hits only (long_name, texi line, parity-source repo-name comment `ac4enc.c:342`) |
| Covers (`doc/covers/*`) | denials ("No Dolby SDK...") + patent notes only |
| Banner re-verify | `--help` head re-run post-fix: text unchanged, as documented |

## Owner

No decision needed. Mails still UNSENT; owner-name fill done on
ff/doc-mail2 (this branch). If a maintainer ever asks about the
`c-ac4enc` banner, point here: independent reimplementation whose
CLI help reproduces reference wording for compatibility; no
Dolby SDK/code/bytes in the tool chain or the clips.
