/* asf.c: ASF spectral writer. See asf.h. */
#include "asf.h"

#include <assert.h>
#include <stdlib.h>

#include "huffman_tables.h"
#include "sfb_offsets.h"


int
asf_band_of(int line, const int *offs, int nsfb)
{
    if (offs == NULL)
        offs = sfb_offsets48(2048);
    assert(offs != NULL);
    if (line < offs[0] || line >= offs[nsfb])
        return -1;
    /* bisect_right(offs, line, 0, nsfb + 1) - 1 over sorted offs. */
    int lo = 0, hi = nsfb + 1;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (offs[mid] <= line)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo - 1;
}


int
asf_book_for_peak(int p)
{
    if (p <= 4)
        return 5;
    if (p <= 7)
        return 7;
    if (p <= 12)
        return 9;
    if (p <= ASF_MAX_Q)
        return 11;
    return -1;
}


static void
escape_params(int v, int *n, int *pay)
{
    assert(v >= 16);
    int k = 0;
    while ((1 << (k + 5)) <= v)
        k++;
    assert(k <= 8);
    *n = k;
    *pay = v - (1 << (k + 4));
}


void
asf_emit_pair(bitwriter_t *w, int book, int a, int b)
{
    const uint8_t *ln;
    const uint32_t *cw;
    int mod;
    switch (book) {
    case 5:
        ln = ASF_HCB_5_LEN;
        cw = ASF_HCB_5_CW;
        mod = 9;
        break;
    case 7:
        ln = ASF_HCB_7_LEN;
        cw = ASF_HCB_7_CW;
        mod = 8;
        break;
    case 9:
        ln = ASF_HCB_9_LEN;
        cw = ASF_HCB_9_CW;
        mod = 13;
        break;
    case 11:
        ln = ASF_HCB_11_LEN;
        cw = ASF_HCB_11_CW;
        mod = 17;
        break;
    default:
        assert(!"bad book");
        return;
    }
    if (book == 5) {
        assert(abs(a) <= 4 && abs(b) <= 4);
        int idx = (a + 4) * 9 + (b + 4);
        bw_put(w, cw[idx], ln[idx]);
        return;
    }
    int aa = abs(a), bb = abs(b);
    int ta = (book == 11 && aa >= 16) ? 16 : aa;
    int tb = (book == 11 && bb >= 16) ? 16 : bb;
    assert(ta < mod && tb < mod);
    int idx = ta * mod + tb;
    bw_put(w, cw[idx], ln[idx]);
    if (ta)
        bw_put(w, a < 0 ? 1u : 0u, 1);
    if (tb)
        bw_put(w, b < 0 ? 1u : 0u, 1);
    if (book == 11) {
        const int ms[2] = { a, b };
        for (int i = 0; i < 2; i++) {
            if (abs(ms[i]) >= 16) {
                int n, pay;
                escape_params(abs(ms[i]), &n, &pay);
                for (int j = 0; j < n; j++)
                    bw_put(w, 1, 1);
                bw_put(w, 0, 1);
                bw_put(w, (unsigned)pay, n + 4);
            }
        }
    }
}


void
asf_put_section(bitwriter_t *w, int book, int length, int nbits,
                int esc)
{
    assert(0 <= book && book <= 11 && length >= 1);
    bw_put(w, (unsigned)book, 4);
    int rem = length - 1;
    while (rem >= esc) {
        bw_put(w, (unsigned)esc, nbits);
        rem -= esc;
    }
    bw_put(w, (unsigned)rem, nbits);
}


#define ASF_MAX_SFB 64


typedef struct {
    int bands[ASF_MAX_SFB];
    int books[ASF_MAX_SFB];
    int nqb;
} group_plan_t;


static void
group_plan(const int *lines, const int *wins, const int *qs, int n,
           const int *offs, int max_sfb, group_plan_t *plan)
{
    int seen[ASF_MAX_SFB] = { 0 };
    int peaks[ASF_MAX_SFB] = { 0 };
    assert(max_sfb <= ASF_MAX_SFB);
    for (int i = 0; i < n; i++) {
        int ln = lines[i];
        int q = qs[i];
        assert(0 <= ln && ln < offs[max_sfb]);
        assert(0 < abs(q) && abs(q) <= ASF_MAX_Q);
        (void)wins;
        int b = asf_band_of(ln, offs, max_sfb);
        assert(b >= 0);
        seen[b] = 1;
        if (abs(q) > peaks[b])
            peaks[b] = abs(q);
    }
    plan->nqb = 0;
    for (int b = 0; b < max_sfb; b++) {
        if (!seen[b])
            continue;
        plan->bands[plan->nqb] = b;
        plan->books[plan->nqb] = asf_book_for_peak(peaks[b]);
        assert(plan->books[plan->nqb] > 0);
        plan->nqb++;
    }
}


static void
write_group_sections(bitwriter_t *w, const group_plan_t *plan,
                     int max_sfb, int sect_bits)
{
    int esc = (1 << sect_bits) - 1;
    int pos = 0;
    for (int i = 0; i < plan->nqb; i++) {
        int b = plan->bands[i];
        if (b > pos)
            asf_put_section(w, 0, b - pos, sect_bits, esc);
        asf_put_section(w, plan->books[i], 1, sect_bits, esc);
        pos = b + 1;
    }
    if (pos < max_sfb)
        asf_put_section(w, 0, max_sfb - pos, sect_bits, esc);
}


static void
write_group_spectral(bitwriter_t *w, const int *lines,
                     const int *wins, const int *qs, int n,
                     const group_plan_t *plan, const int *offs,
                     int nwins)
{
    for (int i = 0; i < plan->nqb; i++) {
        int b = plan->bands[i];
        int lo = offs[b], span = offs[b + 1] - lo;
        assert(span % 2 == 0);
        int *grids =
            calloc((size_t)nwins * (size_t)span, sizeof *grids);
        assert(grids != NULL);
        for (int j = 0; j < n; j++) {
            int ln = lines[j];
            if (ln < lo || ln >= lo + span)
                continue;
            int wn = wins ? wins[j] : 0;
            assert(0 <= wn && wn < nwins);
            grids[wn * span + (ln - lo)] = qs[j];
        }
        for (int wn = 0; wn < nwins; wn++) {
            for (int k = 0; k < span; k += 2) {
                asf_emit_pair(w, plan->books[i],
                              grids[wn * span + k],
                              grids[wn * span + k + 1]);
            }
        }
        free(grids);
    }
}


void
asf_write_sf_data(bitwriter_t *w, const int *lines, const int *qs,
                  int n, int ref, int max_sfb,
                  const int *refs_by_band, int has_refs,
                  const int *offs, int sect_bits)
{
    if (offs == NULL)
        offs = sfb_offsets48(2048);
    assert(offs != NULL && 0 <= ref && ref <= 255);
    group_plan_t plan;
    group_plan(lines, NULL, qs, n, offs, max_sfb, &plan);
    write_group_sections(w, &plan, max_sfb, sect_bits);
    write_group_spectral(w, lines, NULL, qs, n, &plan, offs, 1);
    if (!has_refs) {
        bw_put(w, (unsigned)ref, 8);
        for (int i = 0; i < plan.nqb - 1; i++)
            bw_put(w, ASF_HCB_SCALEFAC_CW[60],
                   ASF_HCB_SCALEFAC_LEN[60]);
    } else {
        assert(refs_by_band != NULL && plan.nqb > 0);
        for (int i = 0; i < plan.nqb; i++)
            assert(0 <= refs_by_band[plan.bands[i]] &&
                   refs_by_band[plan.bands[i]] <= 255);
        assert(refs_by_band[plan.bands[0]] == ref);
        bw_put(w, (unsigned)ref, 8);
        for (int i = 1; i < plan.nqb; i++) {
            int step = refs_by_band[plan.bands[i]] -
                refs_by_band[plan.bands[i - 1]];
            assert(-60 <= step && step <= 60);
            int idx = step + 60;
            bw_put(w, ASF_HCB_SCALEFAC_CW[idx],
                   ASF_HCB_SCALEFAC_LEN[idx]);
        }
    }
    bw_put(w, 0, 1);
}


void
asf_write_mixed_leg(bitwriter_t *w, const asf_group_t *groups,
                    int ngroups, int ref)
{
    assert(ngroups >= 1 && 0 <= ref && ref <= 255);
    group_plan_t *plans =
        malloc((size_t)ngroups * sizeof *plans);
    assert(plans != NULL);
    for (int g = 0; g < ngroups; g++) {
        const int *offs = groups[g].offs;
        if (offs == NULL)
            offs = sfb_offsets48(2048);
        group_plan(groups[g].lines, groups[g].wins, groups[g].qs,
                   groups[g].n, offs, groups[g].max_sfb,
                   &plans[g]);
    }
    for (int g = 0; g < ngroups; g++) {
        write_group_sections(w, &plans[g], groups[g].max_sfb,
                             groups[g].sect_bits);
    }
    for (int g = 0; g < ngroups; g++) {
        const int *offs = groups[g].offs;
        if (offs == NULL)
            offs = sfb_offsets48(2048);
        write_group_spectral(w, groups[g].lines, groups[g].wins,
                             groups[g].qs, groups[g].n, &plans[g],
                             offs, groups[g].nwins);
    }
    int total = 0;
    for (int g = 0; g < ngroups; g++)
        total += plans[g].nqb;
    if (total == 0) {
        bw_put(w, (unsigned)ref, 8);
        bw_put(w, 0, 1);
        free(plans);
        return;
    }
    for (int g = 0; g < ngroups; g++) {
        for (int i = 0; i < plans[g].nqb; i++) {
            int b = plans[g].bands[i];
            assert(0 <= groups[g].refs_by_band[b] &&
                   groups[g].refs_by_band[b] <= 255);
        }
    }
    int g0 = -1;
    for (int g = 0; g < ngroups; g++) {
        if (plans[g].nqb > 0) {
            g0 = g;
            break;
        }
    }
    assert(g0 >= 0);
    assert(groups[g0].refs_by_band[plans[g0].bands[0]] == ref);
    bw_put(w, (unsigned)ref, 8);
    int pg = -1, pb = -1;
    for (int g = 0; g < ngroups; g++) {
        for (int i = 0; i < plans[g].nqb; i++) {
            int b = plans[g].bands[i];
            if (pg >= 0) {
                int step = groups[g].refs_by_band[b] -
                    groups[pg].refs_by_band[pb];
                assert(-60 <= step && step <= 60);
                int idx = step + 60;
                bw_put(w, ASF_HCB_SCALEFAC_CW[idx],
                       ASF_HCB_SCALEFAC_LEN[idx]);
            }
            pg = g;
            pb = b;
        }
    }
    bw_put(w, 0, 1);
    free(plans);
}
