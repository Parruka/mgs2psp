# patch_me13.py <xlive_chk.c>: me13 (task #147, the ME hand-off's build order step 1a) on top of sb14's harness + me12.diff:
# me11 b's copy set switches (MGS2_XLIVE_SNAPP, MGS2_XLIVE_SNAPR, now with a..b ranges) and the deadline copy with the guard log
# (MGS2_XLIVE_DL, MGS2_XLIVE_G0)
import sys
p = sys.argv[1]
s = open(p).read()
def rep(old, new, cnt=1):
    global s
    n = s.count(old)
    if n != cnt: sys.exit('patch: %d matches (want %d) for: %s' % (n, cnt, old[:90]))
    s = s.replace(old, new)

# 1. me11 b: the packets' own upload granules and those in the listed regions of 64 KB
rep('static int ov_on, ov_kmask, ov_up; static uint8_t *ov_save; static size_t ov_cap;\n',
    'static int ov_on, ov_kmask, ov_up; static uint8_t *ov_save; static size_t ov_cap;\n'
    'static int ov_pk, ov_nrg; static uint8_t ov_rg[512];   /* me11 b: the packets\' upload granules, and those in the listed 64 KB regions */\n')
rep('int acc = g < n && (ov_up || !CS_UP(a + g));',
    'int acc = g < n && (ov_up || !CS_UP(a + g) || (ov_pk && k < 2) || ov_rg[((a + g) >> 16) & 511]);')

# 2. me13: the deadline copy and the guards
ME13 = r'''/* me13 (task #147, the ME hand-off's build order step 1a; me_handoff_design.md 4.2-4.3): MGS2_XLIVE_DL=<vb> (with MGS2_XLIVE_DEFER=2
   and MGS2_XLIVE_SNAPK, the overlay walk) takes each transfer's copy at its deadline, the hook that ends the transfer's step, as the ME
   would: the guard log laid over the game's RAM, the transfer's chain walked again and its spans copied from RAM as it is then.  From the
   hook at <vb> on the late walk reads that copy; before it the copy taken as the transfer came (the boot's frames, which the main CPU
   draws).  The guards: the stores of the G0 functions (their code ranges from MGS2_XLIVE_G0=<file>: offset, size and name per line, from
   nm -S of this binary; reported by the game code built with GEN_DG_TRACE through rt_dgt_wr / rt_dgt_wrh, by the store's return address)
   and the runtime's own writes into game RAM (rt_me_guard: the scratchpad DMA to RAM, the SIF's IOP-to-EE copies, the disc reads; boot.c
   built with ME_GUARD_RT and me13's runtime headers).  From the step's first transfer to its deadline a guarded store logs the 64 old
   bytes of each line it touches, once per line and lseq (the number of the step's transfers started before the store); at the deadline
   the transfer numbered tseq gets the entries with lseq >= tseq laid over RAM, the oldest entry of a line last so that it wins: the bytes
   as they were when that transfer started.  The copy taken as the transfer came (me11's) is compared with the deadline copy range by
   range and byte by byte ("me13 dl" lines): a byte that differs is a write no guard logged. */
static int dl_on; static long dl_from;
static int dl_lseq;                            /* the step's transfers started so far (0: no frame in flight) */
#define DL_TMAX 8192
static DfE *dl_q[DL_TMAX];                     /* the step's queued transfers, in the order they came */
static const uint8_t *dl_spr;
typedef struct { uint32_t l; int32_t lseq; uint8_t rt, pad[3]; uint8_t old[64]; } DlE;
static DlE *dl_log; static long dl_ln, dl_lcap;
#define DL_H (1 << 21)
typedef struct { uint32_t l; int32_t lseq; uint32_t gen; } DlH;
static DlH *dl_h; static uint32_t dl_gen = 1;
typedef struct { uint32_t a, e; char name[40]; long st, fl; } DlR;
static DlR dl_g0[64]; static int dl_ng0; static uint32_t dl_g0lo = 0xffffffffu, dl_g0hi;
typedef struct { long g0st, g0fl, rtst, rtfl, rtb, ent, entrt, entchg, steps, tr, trdl, trdiff, rngdiff, bdiff, kb[4], laid, maxent, maxtr; } DlS;
static DlS dl_w, dl_all; static long dl_prn, dl_rprn;
static uint8_t *dl_save; static size_t dl_scap;
static int dl_g0find(uintptr_t w)   /* the G0 function holding the return address w (an offset in the executable), or -1 */
{
  if (w < dl_g0lo || w >= dl_g0hi) return -1;
  int lo = 0, hi = dl_ng0 - 1;
  while (lo <= hi) { int m = (lo + hi) / 2; if (w < dl_g0[m].a) hi = m - 1; else if (w >= dl_g0[m].e) lo = m + 1; else return m; }
  return -1;
}
static void dl_log_lines(uint32_t a, uint32_t n, int rt)
{
  uint32_t l0, l1;
  a &= GSZ - 1; l0 = a >> 6; l1 = (a + (n ? n : 1) - 1) >> 6; if (l1 >= GSZ / 64) l1 = GSZ / 64 - 1;
  for (uint32_t l = l0; l <= l1; l++) {
    uint64_t key = (uint64_t)l << 20 ^ (uint64_t)(uint32_t)dl_lseq;
    uint32_t h = (uint32_t)((key * 0x9E3779B97F4A7C15ull) >> 43) & (DL_H - 1);
    while (dl_h[h].gen == dl_gen && (dl_h[h].l != l || dl_h[h].lseq != dl_lseq)) h = (h + 1) & (DL_H - 1);
    if (dl_h[h].gen == dl_gen) continue;   /* this line is in the log for this lseq already */
    if (dl_ln >= DL_H / 2) { fprintf(stderr, "me13: the guard log is full (%ld lines) at vb %ld\n", dl_ln, cur_vb); exit(3); }
    dl_h[h].gen = dl_gen; dl_h[h].l = l; dl_h[h].lseq = dl_lseq;
    if (dl_ln == dl_lcap) { dl_lcap = dl_lcap ? 2 * dl_lcap : 4096; if (!(dl_log = realloc(dl_log, (size_t)dl_lcap * sizeof *dl_log))) exit(1); }
    DlE *e = &dl_log[dl_ln++]; e->l = l; e->lseq = dl_lseq; e->rt = (uint8_t)rt; memcpy(e->old, df_live + 64 * (size_t)l, 64);
    if (rt) dl_w.entrt++; }
}
static inline void dl_store(uint32_t a, int n, uintptr_t w)   /* a reported game store (before it writes) */
{
  int f = dl_g0find(w); if (f < 0) return;
  dl_g0[f].st++; dl_w.g0st++;
  if (dl_lseq) { dl_g0[f].fl++; dl_w.g0fl++; dl_log_lines(a, (uint32_t)n, 0); }
}
void rt_me_guard(uint32_t a, uint32_t n)       /* the runtime, before it writes n bytes of game RAM at a */
{
  if (!dl_on) return;
  dl_w.rtst++;
  if (dl_lseq) { dl_w.rtfl++; dl_w.rtb += n; dl_log_lines(a, n, 1);
    if (dl_rprn < 40) { dl_rprn++; fprintf(stderr, "me13 rt: vb %ld: the runtime writes %u bytes at %07x with %d transfer%s in flight\n", cur_vb, n,
                                             (unsigned)(a & (GSZ - 1)), dl_lseq, dl_lseq > 1 ? "s" : ""); } }
}
static long dl_lay(int tseq)   /* the entries with lseq >= tseq laid over RAM, the newest first; each live line saved before */
{
  uint8_t *live = (uint8_t *)(uintptr_t)df_live; long k = 0;
  if ((size_t)dl_ln * 64 > dl_scap) { dl_scap = (size_t)dl_ln * 128 + 4096; if (!(dl_save = realloc(dl_save, dl_scap))) exit(1); }
  for (long i = dl_ln - 1; i >= 0; i--) { DlE *e = &dl_log[i]; if (e->lseq < tseq) continue;
    memcpy(dl_save + 64 * (size_t)k, live + 64 * (size_t)e->l, 64); k++; memcpy(live + 64 * (size_t)e->l, e->old, 64); }
  return k;
}
static void dl_unlay(int tseq, long k)   /* RAM put back, the lines in the reverse order of dl_lay */
{
  uint8_t *live = (uint8_t *)(uintptr_t)df_live;
  for (long i = 0; i < dl_ln; i++) { DlE *e = &dl_log[i]; if (e->lseq < tseq) continue;
    k--; memcpy(live + 64 * (size_t)e->l, dl_save + 64 * (size_t)k, 64); }
  if (k) { fprintf(stderr, "me13: dl_unlay out of step (%ld)\n", k); exit(3); }
}
static void dl_cmp(const DfE *q, const uint8_t *lg, size_t ln, long vb, int tseq)   /* the deadline copy against the copy as it came */
{
  if (ln == q->ln && !memcmp(lg, q->lg, ln)) return;
  dl_w.trdiff++;
  const uint8_t *x = q->lg, *xe = q->lg + q->ln, *y = lg, *ye = lg + ln;
  while (x < xe && y < ye) { uint32_t xa, xn, xk, ya, yn, yk;
    memcpy(&xa, x, 4); memcpy(&xn, x + 4, 4); memcpy(&xk, x + 8, 4); memcpy(&ya, y, 4); memcpy(&yn, y + 4, 4); memcpy(&yk, y + 8, 4);
    if (xa != ya || xn != yn || xk != yk) { dl_w.rngdiff++;
      if (dl_prn < 200) { dl_prn++; fprintf(stderr, "me13 dl walk: vb %ld transfer %d of %d ch %d: range %07x+%u kind %u as it came, %07x+%u kind %u at the deadline\n",
                                            vb, tseq, dl_lseq, q->ch, xa, xn, xk, ya, yn, yk); }
      return; }
    long c = 0; uint32_t f = 0xffffffffu;
    for (uint32_t i = 0; i < xn; i++) if (x[12 + i] != y[12 + i]) { c++; if (f == 0xffffffffu) f = i; }
    if (c) { dl_w.bdiff += c; dl_w.kb[xk & 3] += c;
      if (dl_prn < 200) { dl_prn++; fprintf(stderr, "me13 dl bytes: vb %ld transfer %d of %d ch %d: %ld bytes differ in %07x+%u kind %u%s, the first at %07x\n",
                                            vb, tseq, dl_lseq, q->ch, c, xa, xn, xk, CS_UP(xa + (f & ~15u)) ? " (upload source)" : "", xa + f); } }
    x += 12 + xn; y += 12 + yn; }
  if (x < xe || y < ye) { dl_w.rngdiff++;
    if (dl_prn < 200) { dl_prn++; fprintf(stderr, "me13 dl walk: vb %ld transfer %d of %d ch %d: the walks end differently\n", vb, tseq, dl_lseq, q->ch); } }
}
static void dl_deadline(long vb)   /* the hook that ends the step: each of its transfers copied as the ME would at its deadline */
{
  int n = dl_lseq; DfSnap keep = ds_w; long keep_step = ds_step;   /* df_copy's counters stay those of the copy as it came */
  for (long i = 0; i < dl_ln; i++) if (memcmp(dl_log[i].old, df_live + 64 * (size_t)dl_log[i].l, 64)) dl_w.entchg++;
  dl_w.ent += dl_ln; if (dl_ln > dl_w.maxent) dl_w.maxent = dl_ln; if (n > dl_w.maxtr) dl_w.maxtr = n;
  if (n) dl_w.steps++;
  for (int t = 0; t < n; t++) { DfE *q = dl_q[t]; size_t ln; long k = dl_lay(t + 1);
    dl_w.laid += k;
    df_walk(df_live, dl_spr, q->chcr, q->madr, q->qwc, q->tadr);
    uint8_t *lg = df_copy(df_live, &ln);
    dl_unlay(t + 1, k);
    dl_cmp(q, lg, ln, vb, t + 1); dl_w.tr++;
    if (vb >= dl_from) { free(q->lg); q->lg = lg; q->ln = ln; dl_w.trdl++; } else free(lg); }
  ds_w = keep; ds_step = keep_step;
  dl_lseq = 0; dl_ln = 0; if (!++dl_gen) { memset(dl_h, 0, (size_t)DL_H * sizeof *dl_h); dl_gen = 1; }
}
static void dl_line(const char *what, const DlS *s)
{
  fprintf(stderr, "me13 dl %s: steps with transfers %ld, transfers %ld (the deadline copy read by %ld); differing from the copy as they came: "
          "transfers %ld, walks %ld, bytes %ld (tags %ld, after tags %ld, REF %ld, normal %ld); guards: G0 stores %ld, %ld of them with a frame in "
          "flight; runtime writes %ld, %ld in flight (%ld bytes); log lines %ld (the runtime's %ld; changed by the deadline %ld), the most in a "
          "step %ld; lines laid %ld; the most transfers in a step %ld\n", what, s->steps, s->tr, s->trdl, s->trdiff, s->rngdiff, s->bdiff,
          s->kb[0], s->kb[1], s->kb[2], s->kb[3], s->g0st, s->g0fl, s->rtst, s->rtfl, s->rtb, s->ent, s->entrt, s->entchg, s->maxent, s->laid, s->maxtr);
}
static void dl_add(void)
{
  DlS *a = &dl_all, *w = &dl_w;
  a->g0st += w->g0st; a->g0fl += w->g0fl; a->rtst += w->rtst; a->rtfl += w->rtfl; a->rtb += w->rtb; a->ent += w->ent; a->entrt += w->entrt;
  a->entchg += w->entchg; a->steps += w->steps; a->tr += w->tr; a->trdl += w->trdl; a->trdiff += w->trdiff; a->rngdiff += w->rngdiff;
  a->bdiff += w->bdiff; for (int k = 0; k < 4; k++) a->kb[k] += w->kb[k]; a->laid += w->laid;
  if (w->maxent > a->maxent) a->maxent = w->maxent; if (w->maxtr > a->maxtr) a->maxtr = w->maxtr;
  memset(w, 0, sizeof *w);
}
static void dl_g0load(const char *fn)
{
  FILE *f = fopen(fn, "r"); char ln[256];
  if (!f) { fprintf(stderr, "me13: no G0 file %s\n", fn); exit(1); }
  while (fgets(ln, sizeof ln, f) && dl_ng0 < 64) { unsigned long a, n; char nm[64];
    if (sscanf(ln, "%lx %lx %63s", &a, &n, nm) != 3) continue;
    DlR *r = &dl_g0[dl_ng0++]; r->a = (uint32_t)a; r->e = (uint32_t)(a + n); snprintf(r->name, sizeof r->name, "%s", nm); r->st = r->fl = 0; }
  fclose(f);
  for (int i = 1; i < dl_ng0; i++) for (int j = i; j > 0 && dl_g0[j].a < dl_g0[j - 1].a; j--) { DlR t = dl_g0[j]; dl_g0[j] = dl_g0[j - 1]; dl_g0[j - 1] = t; }
  for (int i = 0; i < dl_ng0; i++) { if (dl_g0[i].a < dl_g0lo) dl_g0lo = dl_g0[i].a; if (dl_g0[i].e > dl_g0hi) dl_g0hi = dl_g0[i].e;
    if (i && dl_g0[i].a < dl_g0[i - 1].e) { fprintf(stderr, "me13: G0 ranges overlap\n"); exit(1); } }
}
static void dl_report(void)
{
  dl_line("totals", &dl_all);
  for (int i = 0; i < dl_ng0; i++) fprintf(stderr, "me13 g0: %s (%x-%x): stores %ld, with a frame in flight %ld\n", dl_g0[i].name, dl_g0[i].a, dl_g0[i].e,
                                           dl_g0[i].st, dl_g0[i].fl);
}
'''
rep('''void rt_dgt_wr(uint32_t a, int n) { if (wt_on) wt_note(a, (uint32_t)n, (uintptr_t)__builtin_return_address(0) - (uintptr_t)&__executable_start); }
uint32_t rt_dgt_wrh(uint32_t a, int n) { if (wt_on) wt_note(a, (uint32_t)n, (uintptr_t)__builtin_return_address(0) - (uintptr_t)&__executable_start); return a; }''',
ME13 + '''void rt_dgt_wr(uint32_t a, int n) { uintptr_t w = (uintptr_t)__builtin_return_address(0) - (uintptr_t)&__executable_start;
  if (wt_on) wt_note(a, (uint32_t)n, w);
  if (dl_on) dl_store(a, n, w); }
uint32_t rt_dgt_wrh(uint32_t a, int n) { uintptr_t w = (uintptr_t)__builtin_return_address(0) - (uintptr_t)&__executable_start;
  if (wt_on) wt_note(a, (uint32_t)n, w);
  if (dl_on) dl_store(a, n, w);
  return a; }''')

# 3. each transfer: its place in the step (after the copy as it came)
rep('''    if (df_snap) { df_walk(mem, spr, chcr, madr, qwc, tadr); if (wt_on) wt_kick(); q->lg = df_copy(mem, &q->ln);''',
    '''    if (df_snap) { df_walk(mem, spr, chcr, madr, qwc, tadr); if (wt_on) wt_kick(); q->lg = df_copy(mem, &q->ln);
      if (dl_on) { if (dl_lseq >= DL_TMAX) { fprintf(stderr, "me13: too many transfers in a step\\n"); exit(3); } dl_q[dl_lseq++] = q; dl_spr = spr; }   /* me13 */''')

# 4. the hook: the deadline first; the lines every 100 vblanks and at the end
rep('''  struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
  double game = df_ms(&df_tafter, &t0); int last;''',
    '''  struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
  double game = df_ms(&df_tafter, &t0); int last;
  if (dl_on) dl_deadline(vb);   /* me13: before the queue runs (its walks lay copies over RAM) */''')
rep('''      cs_line(w, &cs_w, df_w.tsteps); cs_add(); }''',
    '''      cs_line(w, &cs_w, df_w.tsteps); cs_add(); }
    if (dl_on) { dl_line(w, &dl_w); dl_add(); }''')
rep('''if (wt_on) wt_report(); }''', '''if (wt_on) wt_report(); if (dl_on) dl_report(); }''')

# 5. the set-up
rep('''        gu_on_data(map, cs_mark, 0);   /* me10: marks the upload sources, then the texture check's on_data if it is on */''',
    '''        if (ov_on) { ov_pk = getenv("MGS2_XLIVE_SNAPP") && atoi(getenv("MGS2_XLIVE_SNAPP"));   /* me11 b; me13: a..b ranges too */
          if (getenv("MGS2_XLIVE_SNAPR")) { const char *q = getenv("MGS2_XLIVE_SNAPR");
            while (*q) { char *e; unsigned long r = strtoul(q, &e, 16), r2 = r; if (e == q) break;
              if (e[0] == '.' && e[1] == '.') { const char *q2 = e + 2; r2 = strtoul(q2, &e, 16); if (e == q2) break; }
              for (unsigned long x = r; x <= r2 && x < 512; x++) if (!ov_rg[x]) { ov_rg[x] = 1; ov_nrg++; }
              q = *e ? e + 1 : e; } }
          if (ov_pk || ov_nrg) fprintf(stderr, "xlive mode: me11 b: upload granules copied too:%s%s %d regions of 64 KB (MGS2_XLIVE_SNAPP, MGS2_XLIVE_SNAPR)\\n",
                                       ov_pk ? " the packets' own," : "", ov_nrg ? " those in" : "", ov_nrg); }
        if (ov_on && getenv("MGS2_XLIVE_DL")) {   /* me13 */
          dl_on = 1; dl_from = atol(getenv("MGS2_XLIVE_DL")); if (!(dl_h = calloc(DL_H, sizeof *dl_h))) exit(1);
          if (getenv("MGS2_XLIVE_G0")) dl_g0load(getenv("MGS2_XLIVE_G0"));
          fprintf(stderr, "xlive mode: me13: each transfer copied at its deadline (the hook ending its step) with the guard log laid over RAM, "
                  "read by the late walk from vb %ld; guards: %d G0 functions (MGS2_XLIVE_G0) and the runtime's writes (MGS2_XLIVE_DL)\\n", dl_from, dl_ng0); }
        gu_on_data(map, cs_mark, 0);   /* me10: marks the upload sources, then the texture check's on_data if it is on */''')
open(p, 'w').write(s)
print('patched', p)
