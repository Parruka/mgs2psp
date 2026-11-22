# gl24 debug (not for delivery): MGS2_ME_PIC_LCHK=1 (the emulator only): the hook's walk made at once, every read it makes from RAM
# (outside its copies) recorded by the view, hashed after the walk and again at the next hook (the latest time a walk one step late
# would read them); the ranges that changed logged.  Usage: python3 -I lchk_apply.py <tree with ge/ and psp/>
import sys, hashlib
t = sys.argv[1]
def load(p, want):
    s = open(t + '/' + p).read(); h = hashlib.md5(s.encode()).hexdigest()
    assert h.startswith(want), (p, h); return s
def rep(s, old, new):
    assert s.count(old) == 1, (s.count(old), old[:80]); return s.replace(old, new)

# 1. ge/xlview.h: the record
v = load('ge/xlview.h', 'c3624a05')
v = rep(v, """  long n_fix, n_tmp;                 /* the reads put together in the fixed buffers, in tmp */
} XmView;
""", """  long n_fix, n_tmp;                 /* the reads put together in the fixed buffers, in tmp */
  uint32_t *rl; int rl_n, rl_cap; long rl_over;   /* gl24 debug (MGS2_ME_PIC_LCHK): the reads from RAM, (k0, k1, what) each; 0: none */
} XmView;
/* gl24 debug: a read from RAM recorded, joined to the last when of the same kind and meeting it */
static inline __attribute__((always_inline)) void xv_rl(XmView *v, uint32_t k0, uint32_t k1, int what)
{
  uint32_t *r = v->rl; int n = v->rl_n;
  if (n && r[3 * n - 1] == (uint32_t)what && k0 <= r[3 * n - 2] && k1 >= r[3 * n - 3]) {
    if (k0 < r[3 * n - 3]) r[3 * n - 3] = k0;
    if (k1 > r[3 * n - 2]) r[3 * n - 2] = k1;
    return; }
  if (n == v->rl_cap) { v->rl_over++; return; }
  r[3 * n] = k0; r[3 * n + 1] = k1; r[3 * n + 2] = (uint32_t)what; v->rl_n = n + 1;
}
""")
v = rep(v, """  v->out[what]++;
  return k >= XV_SPR ? v->spr + (k - XV_SPR) : v->mem + k;
""", """  v->out[what]++;
  if (v->rl) {   /* gl24 debug: a tag's read counts the 416 bytes the lanes may read on from it (xlme.h XC_MARGIN), to the next extent */
    uint32_t e = k + *lim;
    if (what == XV_TAG) { uint32_t m = k + 16 + 416, top = k >= XV_SPR ? XV_SPR + 0x4000 : v->mem_size;
      if (i < v->n && v->x[i].a0 < m) m = v->x[i].a0;
      if (m > top) m = top;
      if (m > e) e = m; }
    xv_rl(v, k, e, what); }
  return k >= XV_SPR ? v->spr + (k - XV_SPR) : v->mem + k;
""")
open(t + '/ge/xlview.h', 'w').write(v)

# 2. psp/xl_psp.c: the check
s = load('psp/xl_psp.c', '168e807d')
s = rep(s, """static XmPic xmp ME_LN; static XmCopy xmc; static XmView mp_view ME_LN;""", """static XmPic xmp ME_LN; static XmCopy xmc; static XmView mp_view ME_LN;"""
        + """\n/* gl24 debug (MGS2_ME_PIC_LCHK): its state (lc_check below) */\n#define LC_N 16384
#define LC_T 512
typedef struct { uint32_t k0, k1, what; uint64_t h; } LcP;
static int lc_on, lc_pn, lc_logn, lc_tn; static LcP *lc_p; static uint32_t *lc_rl;
static long lc_walks, lc_chk[XV_NK], lc_bad[XV_NK], lc_over, lc_skip, lc_t_over, lc_walk_bad, lc_rmax; static long lc_vb_take;
static uint64_t lc_chkb[XV_NK], lc_badb[XV_NK], lc_wb, lc_wbmax, lc_bwmax; static uint32_t lc_blk[513];
static struct { uint32_t k0, k1, what, n; long vb0, vb1; } lc_t[LC_T];
""")
s = rep(s, """static void mq_arena_wait(void)   /* the walks posted last are done: the arena and its extents are the main CPU's again */
""", """/* gl24 debug (MGS2_ME_PIC_LCHK=1, the emulator only; not for delivery): the hook's walk made at once (the main CPU waits for it), its reads
   from RAM outside its copies recorded by the view (ge/xlview.h xv_rl), hashed after the walk and again at the next hook: the latest time
   a walk one step late (two copy areas) would read them.  The ranges that changed are logged */
static const char *const lc_wn[XV_NK] = { "tag", "run", "lane", "map" };
static uint64_t lc_hash(uint32_t k0, uint32_t k1)
{
  const uint8_t *p = k0 >= XV_SPR ? M.spr + (k0 - XV_SPR) : M.mem + k0; uint32_t n = k1 - k0, i = 0; uint64_t h = 0xcbf29ce484222325ull;
  for (; i + 4 <= n; i += 4) { uint32_t w; memcpy(&w, p + i, 4); h = (h ^ w) * 0x100000001b3ull; }
  for (; i < n; i++) h = (h ^ p[i]) * 0x100000001b3ull;
  return h;
}
static void lc_take(void)   /* after the hook's walk: its reads from RAM hashed as the walk saw them (the main CPU waited) */
{
  int n = mp_view.rl_n; uint64_t b = 0;
  lc_walks++; lc_over += mp_view.rl_over; if (n > lc_rmax) lc_rmax = n; lc_vb_take = me_vb;
  for (int i = 0; i < n; i++) {
    LcP *q; if (lc_pn == LC_N) { lc_skip++; continue; }
    q = &lc_p[lc_pn++]; q->k0 = lc_rl[3 * i]; q->k1 = lc_rl[3 * i + 1]; q->what = lc_rl[3 * i + 2]; q->h = lc_hash(q->k0, q->k1);
    b += q->k1 - q->k0; }
  lc_wb += b; if (b > lc_wbmax) lc_wbmax = b;
  mp_view.rl = 0; mp_view.rl_n = 0; mp_view.rl_over = 0;
}
static void lc_check(long vb)   /* at the next hook: the ranges hashed again */
{
  uint64_t bw = 0; int bad = 0;
  for (int i = 0; i < lc_pn; i++) {
    const LcP *q = &lc_p[i]; int w = (int)q->what, j;
    lc_chk[w]++; lc_chkb[w] += q->k1 - q->k0;
    if (lc_hash(q->k0, q->k1) == q->h) continue;
    lc_bad[w]++; lc_badb[w] += q->k1 - q->k0; bw += q->k1 - q->k0; bad = 1;
    lc_blk[q->k0 >= XV_SPR ? 512 : (q->k0 >> 16) & 511] += q->k1 - q->k0;
    for (j = 0; j < lc_tn; j++) if (lc_t[j].k0 == q->k0 && lc_t[j].k1 == q->k1 && lc_t[j].what == q->what) break;
    if (j < lc_tn) { lc_t[j].n++; lc_t[j].vb1 = lc_vb_take; }
    else if (lc_tn < LC_T) { lc_t[lc_tn].k0 = q->k0; lc_t[lc_tn].k1 = q->k1; lc_t[lc_tn].what = q->what; lc_t[lc_tn].n = 1;
      lc_t[lc_tn].vb0 = lc_t[lc_tn].vb1 = lc_vb_take; lc_tn++; }
    else lc_t_over++;
    if (lc_logn < 300) { lc_logn++; fprintf(stderr, "lchk (gl24 debug): the walk at vb %ld read %s bytes %08x-%08x (%u) from RAM; changed by the hook at "
                                           "vb %ld\\n", lc_vb_take, lc_wn[w], (unsigned)q->k0, (unsigned)q->k1, (unsigned)(q->k1 - q->k0), vb); }
  }
  lc_walk_bad += bad; if (bw > lc_bwmax) lc_bwmax = bw;
  lc_pn = 0;
}
static void lc_report(FILE *f)
{
  if (!lc_on) return;
  fprintf(f, "lchk (gl24 debug): hook walks recorded %ld; reads from RAM recorded per walk at most %ld (not recorded: %ld; not checked: %ld); "
          "bytes read from RAM a walk %.1f KB on average, %llu KB at most\\n", lc_walks, lc_rmax, lc_over, lc_skip,
          lc_walks ? (double)lc_wb / 1024.0 / lc_walks : 0.0, (unsigned long long)(lc_wbmax >> 10));
  for (int w = 0; w < XV_NK; w++)
    fprintf(f, "lchk (gl24 debug): %s reads from RAM: checked %ld (%llu KB), changed by the next hook %ld (%llu KB)\\n", lc_wn[w], lc_chk[w],
            (unsigned long long)(lc_chkb[w] >> 10), lc_bad[w], (unsigned long long)(lc_badb[w] >> 10));
  fprintf(f, "lchk (gl24 debug): walks with a change %ld; changed bytes in one walk at most %llu; distinct changed ranges %d (more: %ld)\\n",
          lc_walk_bad, (unsigned long long)lc_bwmax, lc_tn, lc_t_over);
  for (int j = 0; j < lc_tn; j++)
    fprintf(f, "lchk (gl24 debug): changed range %s %08x-%08x (%u bytes): %u times, vb %ld .. %ld\\n", lc_wn[lc_t[j].what], (unsigned)lc_t[j].k0,
            (unsigned)lc_t[j].k1, (unsigned)(lc_t[j].k1 - lc_t[j].k0), (unsigned)lc_t[j].n, lc_t[j].vb0, lc_t[j].vb1);
  for (int j = 0; j < 513; j++) if (lc_blk[j])
    fprintf(f, "lchk (gl24 debug): changed bytes in the 64 KB block %s%07x: %u\\n", j == 512 ? "scratchpad " : "", j == 512 ? 0u : (unsigned)j << 16,
            (unsigned)lc_blk[j]);
}
static void mq_arena_wait(void)   /* the walks posted last are done: the arena and its extents are the main CPU's again */
""")
s = rep(s, """  if ((e = mgs2_getenv("MGS2_ME_PIC_TBK_MS")) && atoi(e) > 0) mq_tbk_us = (uint32_t)atoi(e) * 1000u;
""", """  if ((e = mgs2_getenv("MGS2_ME_PIC_LCHK")) && atoi(e) && mp_want == 4) {   /* (gl24 debug) */
    lc_rl = malloc((size_t)LC_N * 12); lc_p = malloc((size_t)LC_N * sizeof *lc_p); lc_on = lc_rl && lc_p;
    if (!lc_on) { free(lc_rl); free(lc_p); lc_rl = 0; lc_p = 0; }
    fprintf(stderr, "lchk (gl24 debug): %s\\n", lc_on ? "on: the hook's walk made at once, its reads from RAM checked at the next hook" : "no memory: off"); }
  if ((e = mgs2_getenv("MGS2_ME_PIC_TBK_MS")) && atoi(e) > 0) mq_tbk_us = (uint32_t)atoi(e) * 1000u;
""")
s = rep(s, """  mq_probe();
  mq_arena_wait();
  if (!mq_m4 && mq_turn""", """  mq_probe();
  mq_arena_wait();
  if (lc_on && lc_pn) lc_check(vb);   /* (gl24 debug) */
  if (!mq_m4 && mq_turn""")
s = rep(s, """  if (mp_nrec) mq_late(0, 0);
  memset(&e, 0, sizeof e); e.kind = XE_HOOK; e.h.vb = (int32_t)vb;
""", """  if (mp_nrec) {
    if (lc_on && mq_m4 && !mq_self) {   /* (gl24 debug) */
      mp_view.rl_cap = LC_N; mp_view.rl_n = 0; mp_view.rl_over = 0; mp_view.rl = lc_rl;
      mq_late(0, 1); lc_take(); }
    else mq_late(0, 0); }
  memset(&e, 0, sizeof e); e.kind = XE_HOOK; e.h.vb = (int32_t)vb;
""")
s = rep(s, """static void mq_report(FILE *f)
{
""", """static void mq_report(FILE *f)
{
  lc_report(f);   /* (gl24 debug) */
""")
open(t + '/psp/xl_psp.c', 'w').write(s)
print('ok')
