/* ASPX_NOISE[512][2] noise values table (D41 transcription).
 * Authorized: owner D3a un-defer 2026-10-13 (reopen of
 * spec/aspx-noise-DEFERRED.md: full-decode noise synthesis).
 * Sources (public ETSI deliverables, fetched 2026-10-02):
 *   P1 V1.4.1 ts_10319001v010401p0.zip sha 8c827f84...e3ea4
 *   URLs + full shas: tmp/wave05/WEB-e0.md. Generator:
 *   tools/aspx_noise_gen.py (e0 gate family; C99 hex-float,
 *   exact float32). Layout below is own; numeric values
 *   transcribed by the generator; zero ETSI prose or code
 *   copied. DO NOT HAND-EDIT: regenerate with the tool.
 */
#ifndef AC4_ASPX_NOISE_H
#define AC4_ASPX_NOISE_H

extern const float ASPX_NOISE[512][2];
#define ASPX_NOISE_NROWS 512

#endif
