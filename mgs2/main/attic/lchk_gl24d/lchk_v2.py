# gl24 debug v2 on top of lchk_apply.py (v1): the 4 KB pages each walk reads from RAM (complete: no overflow), the ranges joined across
# kinds (a bit mask), the walks whose ranges overflowed named.  Usage: python3 -I lchk_v2.py <tree>
import sys
t = sys.argv[1]
def R(s, old, new):
    assert s.count(old) == 1, (s.count(old), old[:70]); return s.replace(old, new)
v = open(t + '/ge/xlview.h').read()
v = R(v, r"""  uint32_t *rl; int rl_n, rl_cap; long rl_over;   /* gl24 debug (MGS2_ME_PIC_LCHK): the reads from RAM, (k0, k1, what) each; 0: none */""",
 r"""  uint32_t *rl; int rl_n, rl_cap; long rl_over;   /* gl24 debug (MGS2_ME_PIC_LCHK): the reads from RAM, (k0, k1, kinds' bits) each; 0: none */
  uint32_t *pg;                      /* gl24 debug: the 4 KB pages those reads touched, a bit each (RAM's 8192, then the scratchpad's 4) */""")
i0 = v.index('/* gl24 debug: a read from RAM recorded'); i1 = v.index('}\n', v.index('r[3 * n] = k0;')) + 2
v = v[:i0] + r"""/* gl24 debug: a read from RAM recorded: its pages marked; joined to one of the last two ranges it meets (their kinds' bits joined) */
static inline __attribute__((always_inline)) void xv_rl(XmView *v, uint32_t k0, uint32_t k1, int what)
{
  uint32_t *r = v->rl; int n = v->rl_n, j;
  { uint32_t b = k0 >= XV_SPR ? 8192 + ((k0 - XV_SPR) >> 12) : k0 >> 12, e = k0 >= XV_SPR ? 8192 + ((k1 - 1 - XV_SPR) >> 12) : (k1 - 1) >> 12;
    for (; b <= e && b < 8196; b++) v->pg[b >> 5] |= 1u << (b & 31); }
  for (j = n - 1; j >= 0 && j >= n - 2; j--)
    if (k0 <= r[3 * j + 1] && k1 >= r[3 * j]) {
      if (k0 < r[3 * j]) r[3 * j] = k0;
      if (k1 > r[3 * j + 1]) r[3 * j + 1] = k1;
      r[3 * j + 2] |= 1u << what; return; }
  if (n == v->rl_cap) { v->rl_over++; return; }
  r[3 * n] = k0; r[3 * n + 1] = k1; r[3 * n + 2] = 1u << what; v->rl_n = n + 1;
}
""" + v[i1:]
open(t + '/ge/xlview.h', 'w').write(v)
s = open(t + '/psp/xl_psp.c').read()
s = R(s, r"""static struct { uint32_t k0, k1, what, n; long vb0, vb1; } lc_t[LC_T];""",
 r"""static struct { uint32_t k0, k1, what, n; long vb0, vb1; } lc_t[LC_T];
static uint32_t *lc_pg; static uint64_t *lc_pgh; static uint16_t *lc_pgl, *lc_pgc; static int lc_pgn, lc_pglog; static long lc_pg_chk, lc_pg_bad, lc_over_walks;""")
s = R(s, r"""static void lc_take(void)   /* after the hook's walk: its reads from RAM hashed as the walk saw them (the main CPU waited) */
{""", r"""static uint64_t lc_page(int p) { uint32_t k0 = p >= 8192 ? XV_SPR + ((uint32_t)(p - 8192) << 12) : (uint32_t)p << 12; return lc_hash(k0, k0 + 4096); }
static void lc_take(void)   /* after the hook's walk: its reads from RAM hashed as the walk saw them (the main CPU waited) */
{""")
s = R(s, r"""  lc_walks++; lc_over += mp_view.rl_over; if (n > lc_rmax) lc_rmax = n; lc_vb_take = me_vb;""",
 r"""  lc_walks++; lc_over += mp_view.rl_over; if (n > lc_rmax) lc_rmax = n; lc_vb_take = me_vb;
  if (mp_view.rl_over) { lc_over_walks++; if (lc_over_walks <= 20) fprintf(stderr, "lchk (gl24 debug): the walk at vb %ld: %ld reads from RAM not recorded (their pages are)\n", me_vb, mp_view.rl_over); }
  lc_pgn = 0;
  for (int w = 0; w < 257; w++) { uint32_t m = lc_pg[w]; lc_pg[w] = 0;
    while (m) { int b = __builtin_ctz(m), p = 32 * w + b; m &= m - 1; if (p < 8196) { lc_pgl[lc_pgn++] = (uint16_t)p; lc_pgh[p] = lc_page(p); } } }""")
s = R(s, r"""  lc_walk_bad += bad; if (bw > lc_bwmax) lc_bwmax = bw;
  lc_pn = 0;""", r"""  lc_walk_bad += bad; if (bw > lc_bwmax) lc_bwmax = bw;
  lc_pn = 0;
  for (int i = 0; i < lc_pgn; i++) { int p = lc_pgl[i]; lc_pg_chk++;
    if (lc_page(p) == lc_pgh[p]) continue;
    lc_pg_bad++; if (lc_pgc[p] < 65535) lc_pgc[p]++;
    if (lc_pglog < 200) { lc_pglog++; fprintf(stderr, "lchk (gl24 debug): the walk at vb %ld read from the page %s%07x; it changed by the hook at vb %ld\n", lc_vb_take,
                                             p >= 8192 ? "scratchpad " : "", p >= 8192 ? (unsigned)(p - 8192) << 12 : (unsigned)p << 12, vb); } }
  lc_pgn = 0;""")
s = R(s, r"""    const LcP *q = &lc_p[i]; int w = (int)q->what, j;
    lc_chk[w]++; lc_chkb[w] += q->k1 - q->k0;
    if (lc_hash(q->k0, q->k1) == q->h) continue;
    lc_bad[w]++; lc_badb[w] += q->k1 - q->k0; bw += q->k1 - q->k0; bad = 1;""",
 r"""    const LcP *q = &lc_p[i]; int w = __builtin_ctz(q->what), j, ch = lc_hash(q->k0, q->k1) != q->h;
    for (int b = 0; b < XV_NK; b++) if ((q->what >> b) & 1) { lc_chk[b]++; lc_chkb[b] += q->k1 - q->k0; if (ch) { lc_bad[b]++; lc_badb[b] += q->k1 - q->k0; } }
    if (!ch) continue;
    bw += q->k1 - q->k0; bad = 1;""")
s = R(s, r"""  for (int j = 0; j < 513; j++) if (lc_blk[j])""", r"""  fprintf(f, "lchk (gl24 debug): walks with reads not recorded %ld; the pages read (4 KB): checked %ld, changed by the next hook %ld\n", lc_over_walks, lc_pg_chk, lc_pg_bad);
  for (int p = 0, m = 0; p < 8196 && m < 200; p++) if (lc_pgc[p]) { m++;
    fprintf(f, "lchk (gl24 debug): page %s%07x changed %u times\n", p >= 8192 ? "scratchpad " : "", p >= 8192 ? (unsigned)(p - 8192) << 12 : (unsigned)p << 12, (unsigned)lc_pgc[p]); }
  for (int j = 0; j < 513; j++) if (lc_blk[j])""")
s = R(s, r"""lc_wn[lc_t[j].what]""", r"""lc_wn[__builtin_ctz(lc_t[j].what)]""")
s = R(s, r"""    lc_rl = malloc((size_t)LC_N * 12); lc_p = malloc((size_t)LC_N * sizeof *lc_p); lc_on = lc_rl && lc_p;""",
 r"""    lc_rl = malloc((size_t)LC_N * 12); lc_p = malloc((size_t)LC_N * sizeof *lc_p); lc_pg = calloc(257, 4); lc_pgh = malloc(8196 * 8);
    lc_pgl = malloc(8196 * 2); lc_pgc = calloc(8196, 2); lc_on = lc_rl && lc_p && lc_pg && lc_pgh && lc_pgl && lc_pgc;""")
s = R(s, r"""      mp_view.rl_cap = LC_N; mp_view.rl_n = 0; mp_view.rl_over = 0; mp_view.rl = lc_rl;""",
 r"""      mp_view.rl_cap = LC_N; mp_view.rl_n = 0; mp_view.rl_over = 0; mp_view.pg = lc_pg; mp_view.rl = lc_rl;""")
open(t + '/psp/xl_psp.c', 'w').write(s); print('v2 ok')
