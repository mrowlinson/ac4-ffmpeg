/* Huffman codebook tables, E0 transcription (JOBS#32).
 * Sources (public ETSI deliverables, fetched 2026-10-02):
 *   P1 V1.4.1 ts_10319001v010401p0.zip sha 8c827f84...e3ea4
 *   P2 V1.3.1 ts_10319002v010301p0.zip sha 44105a06...eef9c6
 *   URLs + full shas: tmp/wave05/WEB-e0.md. Generator:
 *   tools/huffman_gen.py (gates on prefix-free + survey pins).
 * Layout below is own; numeric values transcribed by the
 * generator; zero ETSI prose or code copied. DO NOT HAND-EDIT:
 * regenerate with tools/huffman_gen.py.
 * Order: 0-10 ASF 1-11, 11 SCALEFAC, 12 SNF, 13-36 ACPL
 * (dtype ALPHA/BETA/BETA3/GAMMA x COARSE/FINE x F0/DF/DT),
 * 37-48 AJOC (DRY/WET x COARSE/FINE x F0/DF/DT),
 * 49-52 DE (ABS_0/DIFF_0/ABS_1/DIFF_1), 53 DRC.
 * Codeword convention: low LEN bits, MSB first on the wire.
 */
#ifndef AC4_HUFFMAN_TABLES_H
#define AC4_HUFFMAN_TABLES_H

#include <stdint.h>

typedef struct {
    const char *name;
    const uint8_t *len;
    const uint32_t *cw;
    int entries;
    int dim;
    int is_unsigned;
    int mod, mod2, mod3;
    int off;
    int maxlen;
} huffman_book_t;

extern const uint8_t ASF_HCB_1_LEN[81];
extern const uint32_t ASF_HCB_1_CW[81];
extern const uint8_t ASF_HCB_2_LEN[81];
extern const uint32_t ASF_HCB_2_CW[81];
extern const uint8_t ASF_HCB_3_LEN[81];
extern const uint32_t ASF_HCB_3_CW[81];
extern const uint8_t ASF_HCB_4_LEN[81];
extern const uint32_t ASF_HCB_4_CW[81];
extern const uint8_t ASF_HCB_5_LEN[81];
extern const uint32_t ASF_HCB_5_CW[81];
extern const uint8_t ASF_HCB_6_LEN[81];
extern const uint32_t ASF_HCB_6_CW[81];
extern const uint8_t ASF_HCB_7_LEN[64];
extern const uint32_t ASF_HCB_7_CW[64];
extern const uint8_t ASF_HCB_8_LEN[64];
extern const uint32_t ASF_HCB_8_CW[64];
extern const uint8_t ASF_HCB_9_LEN[169];
extern const uint32_t ASF_HCB_9_CW[169];
extern const uint8_t ASF_HCB_10_LEN[169];
extern const uint32_t ASF_HCB_10_CW[169];
extern const uint8_t ASF_HCB_11_LEN[289];
extern const uint32_t ASF_HCB_11_CW[289];
extern const uint8_t ASF_HCB_SCALEFAC_LEN[121];
extern const uint32_t ASF_HCB_SCALEFAC_CW[121];
extern const uint8_t ASF_HCB_SNF_LEN[22];
extern const uint32_t ASF_HCB_SNF_CW[22];
extern const uint8_t ACPL_HCB_ALPHA_COARSE_F0_LEN[17];
extern const uint32_t ACPL_HCB_ALPHA_COARSE_F0_CW[17];
extern const uint8_t ACPL_HCB_ALPHA_COARSE_DF_LEN[33];
extern const uint32_t ACPL_HCB_ALPHA_COARSE_DF_CW[33];
extern const uint8_t ACPL_HCB_ALPHA_COARSE_DT_LEN[33];
extern const uint32_t ACPL_HCB_ALPHA_COARSE_DT_CW[33];
extern const uint8_t ACPL_HCB_ALPHA_FINE_F0_LEN[33];
extern const uint32_t ACPL_HCB_ALPHA_FINE_F0_CW[33];
extern const uint8_t ACPL_HCB_ALPHA_FINE_DF_LEN[65];
extern const uint32_t ACPL_HCB_ALPHA_FINE_DF_CW[65];
extern const uint8_t ACPL_HCB_ALPHA_FINE_DT_LEN[65];
extern const uint32_t ACPL_HCB_ALPHA_FINE_DT_CW[65];
extern const uint8_t ACPL_HCB_BETA_COARSE_F0_LEN[5];
extern const uint32_t ACPL_HCB_BETA_COARSE_F0_CW[5];
extern const uint8_t ACPL_HCB_BETA_COARSE_DF_LEN[9];
extern const uint32_t ACPL_HCB_BETA_COARSE_DF_CW[9];
extern const uint8_t ACPL_HCB_BETA_COARSE_DT_LEN[9];
extern const uint32_t ACPL_HCB_BETA_COARSE_DT_CW[9];
extern const uint8_t ACPL_HCB_BETA_FINE_F0_LEN[9];
extern const uint32_t ACPL_HCB_BETA_FINE_F0_CW[9];
extern const uint8_t ACPL_HCB_BETA_FINE_DF_LEN[17];
extern const uint32_t ACPL_HCB_BETA_FINE_DF_CW[17];
extern const uint8_t ACPL_HCB_BETA_FINE_DT_LEN[17];
extern const uint32_t ACPL_HCB_BETA_FINE_DT_CW[17];
extern const uint8_t ACPL_HCB_BETA3_COARSE_F0_LEN[9];
extern const uint32_t ACPL_HCB_BETA3_COARSE_F0_CW[9];
extern const uint8_t ACPL_HCB_BETA3_COARSE_DF_LEN[17];
extern const uint32_t ACPL_HCB_BETA3_COARSE_DF_CW[17];
extern const uint8_t ACPL_HCB_BETA3_COARSE_DT_LEN[17];
extern const uint32_t ACPL_HCB_BETA3_COARSE_DT_CW[17];
extern const uint8_t ACPL_HCB_BETA3_FINE_F0_LEN[17];
extern const uint32_t ACPL_HCB_BETA3_FINE_F0_CW[17];
extern const uint8_t ACPL_HCB_BETA3_FINE_DF_LEN[33];
extern const uint32_t ACPL_HCB_BETA3_FINE_DF_CW[33];
extern const uint8_t ACPL_HCB_BETA3_FINE_DT_LEN[33];
extern const uint32_t ACPL_HCB_BETA3_FINE_DT_CW[33];
extern const uint8_t ACPL_HCB_GAMMA_COARSE_F0_LEN[21];
extern const uint32_t ACPL_HCB_GAMMA_COARSE_F0_CW[21];
extern const uint8_t ACPL_HCB_GAMMA_COARSE_DF_LEN[41];
extern const uint32_t ACPL_HCB_GAMMA_COARSE_DF_CW[41];
extern const uint8_t ACPL_HCB_GAMMA_COARSE_DT_LEN[41];
extern const uint32_t ACPL_HCB_GAMMA_COARSE_DT_CW[41];
extern const uint8_t ACPL_HCB_GAMMA_FINE_F0_LEN[41];
extern const uint32_t ACPL_HCB_GAMMA_FINE_F0_CW[41];
extern const uint8_t ACPL_HCB_GAMMA_FINE_DF_LEN[81];
extern const uint32_t ACPL_HCB_GAMMA_FINE_DF_CW[81];
extern const uint8_t ACPL_HCB_GAMMA_FINE_DT_LEN[81];
extern const uint32_t ACPL_HCB_GAMMA_FINE_DT_CW[81];
extern const uint8_t AJOC_HCB_DRY_COARSE_F0_LEN[51];
extern const uint32_t AJOC_HCB_DRY_COARSE_F0_CW[51];
extern const uint8_t AJOC_HCB_DRY_COARSE_DF_LEN[51];
extern const uint32_t AJOC_HCB_DRY_COARSE_DF_CW[51];
extern const uint8_t AJOC_HCB_DRY_COARSE_DT_LEN[101];
extern const uint32_t AJOC_HCB_DRY_COARSE_DT_CW[101];
extern const uint8_t AJOC_HCB_DRY_FINE_F0_LEN[101];
extern const uint32_t AJOC_HCB_DRY_FINE_F0_CW[101];
extern const uint8_t AJOC_HCB_DRY_FINE_DF_LEN[101];
extern const uint32_t AJOC_HCB_DRY_FINE_DF_CW[101];
extern const uint8_t AJOC_HCB_DRY_FINE_DT_LEN[201];
extern const uint32_t AJOC_HCB_DRY_FINE_DT_CW[201];
extern const uint8_t AJOC_HCB_WET_COARSE_F0_LEN[21];
extern const uint32_t AJOC_HCB_WET_COARSE_F0_CW[21];
extern const uint8_t AJOC_HCB_WET_COARSE_DF_LEN[21];
extern const uint32_t AJOC_HCB_WET_COARSE_DF_CW[21];
extern const uint8_t AJOC_HCB_WET_COARSE_DT_LEN[41];
extern const uint32_t AJOC_HCB_WET_COARSE_DT_CW[41];
extern const uint8_t AJOC_HCB_WET_FINE_F0_LEN[41];
extern const uint32_t AJOC_HCB_WET_FINE_F0_CW[41];
extern const uint8_t AJOC_HCB_WET_FINE_DF_LEN[41];
extern const uint32_t AJOC_HCB_WET_FINE_DF_CW[41];
extern const uint8_t AJOC_HCB_WET_FINE_DT_LEN[81];
extern const uint32_t AJOC_HCB_WET_FINE_DT_CW[81];
extern const uint8_t DE_HCB_ABS_0_LEN[32];
extern const uint32_t DE_HCB_ABS_0_CW[32];
extern const uint8_t DE_HCB_DIFF_0_LEN[63];
extern const uint32_t DE_HCB_DIFF_0_CW[63];
extern const uint8_t DE_HCB_ABS_1_LEN[61];
extern const uint32_t DE_HCB_ABS_1_CW[61];
extern const uint8_t DE_HCB_DIFF_1_LEN[121];
extern const uint32_t DE_HCB_DIFF_1_CW[121];
extern const uint8_t DRC_HCB_LEN[255];
extern const uint32_t DRC_HCB_CW[255];

extern const huffman_book_t huffman_books[54];
#define HUFFMAN_NBOOKS 54
#define HUFFMAN_ASF_BASE 0
#define HUFFMAN_SCALE_FAC 11
#define HUFFMAN_SNF 12
#define HUFFMAN_ACPL_BASE 13
#define HUFFMAN_AJOC_BASE 37
#define HUFFMAN_DE_BASE 49
#define HUFFMAN_DRC 53
static inline int huffman_asf(int n) { return (n) - 1; }
static inline int huffman_acpl(int dt, int qm, int ht) {
    return 13 + ((dt) * 2 + (qm)) * 3 + (ht);
}
static inline int huffman_ajoc(int dt, int qm, int ht) {
    return 37 + ((dt) * 2 + (qm)) * 3 + (ht);
}
static inline int huffman_de(int absdif, int m) {
    return 49 + (m) * 2 + (absdif);
}
/* Returns entry index or -1. getbit yields next wire bit. */
int hcb_decode(const huffman_book_t *b,
               int (*getbit)(void *ctx), void *ctx);

#endif
