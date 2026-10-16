/* SNF spectral-noise-fill table, pipe2-clean-etsi transcription.
 * Sources (public ETSI deliverables, fetched 2026-10-02):
 *   P1 V1.4.1 ts_10319001v010401p0.zip sha 8c827f84...e3ea4
 *   URLs + full shas: tmp/wave05/WEB-e0.md. Generator:
 *   tools/snf_gen.py (aspx gate family: count + descriptor).
 * Layout below is own; numeric values transcribed by the
 * generator; zero ETSI prose or code copied. DO NOT HAND-EDIT:
 * regenerate with tools/snf_gen.py.
 * Values: RANDOM_NOISE_TABLE (Annex C.11), binary32.
 */
#ifndef AC4_SNF_TABLES_H
#define AC4_SNF_TABLES_H

#define SNF_RANDOM_NOISE_N 256
extern const float SNF_RANDOM_NOISE[SNF_RANDOM_NOISE_N];

#endif
