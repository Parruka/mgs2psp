/* xlme.c: the picture's loop on the Media Engine, the parts between the records and the translator (task #149; xlme.h).  The late walk
   is me13 b's (boot/xlive_chk_g.c lw_walk, lw_run, lw_seen, lw_q, lw_dma; me_step1_notes.md 2) moved here with its counts in the XmPic;
   the switches are psp/xl_psp.c xlive_chk_dma's, the callbacks its on_call, on_pass, on_flush and on_prim without the host's timing,
   the frame's end, list and start its xl_psp_frame and gl_list without the GE and the display (those stay on the main CPU). */
#include <string.h>
#include "xlme.h"
#include "hot39.h"   /* gl39 */
#include "me_rt.h"                      /* -DMGS2_ME_SAFE: ME_CALLP and the C library's calls on the ME */
#include "../runtime/host_time.h"       /* OPT_* and OPT2_* (only the bits: the switches come with each record, never from g_opt) */
#include "xlview.h"                     /* task #180: a transfer's copy (step 4b) */
#include "nge.h"                        /* native step 1 (MGS2_NGE) */

long xm_lw7_miss[8] H39, xm_lw3_miss[8] H39;
#ifndef __psp__
long xm_lw_ncmd[256];
#endif

/* ---- the late walk with the lanes ---- */
typedef struct { uint32_t left, cmd, cl, wl; } LwLoc;   /* runtime/ee_io.h VifLoc: the walk's state a lane leaves */
static const uint8_t *lw_mem, *lw_spr; static uint32_t lw_size;   /* the bytes the lanes read (lw_lanes.h lw_ptr) */
static XmView *lw_view;                 /* task #180: the walk's view of the transfer's copy, or 0 (lw_lanes.h lw_ptr, lw_tp) */
static inline const uint32_t *lw_tp(uint32_t a)   /* a lane's next slot or prim (lw_lanes.h, after tools/dev/me18/lw_view.py) */
{ return lw_view ? xv_q(lw_view, a) : (const uint32_t *)((a & 0x80000000u) ? lw_spr + (a & 0x3ff0) : lw_mem + (a & (lw_size - 1) & ~15u)); }
/* native step 1: the next native group's key (RAM offset) while a transfer with groups is walked, else none: lanes 3 and 4, which take
   runs of prims and slots, stop before it (lw_lanes.h), so the walk comes to the tag where the group is merged */
static uint32_t lw_stop = 0xffffffffu; static long lw_stop_hit;
#define LW_STOP(a) (!((a) & 0x80000000u) && ((a) & (lw_size - 1) & ~15u) >= lw_stop && (lw_stop_hit++, 1))
#define lw_ncmd xm_lw_ncmd
#define lw7_miss xm_lw7_miss
#define lw3_miss xm_lw3_miss
#include "lw_lanes.h"
#undef lw_ncmd
#undef lw7_miss
#undef lw3_miss
/* ---- native step 1: ship-mesh groups from records (xlme.h XmPic; ge/natp04.h the front end) ---- */
static long lw_nat_in, lw_nat_out, lw_nat_part;
NatSplit g_ns = { .ph_i = -1 };   /* gl34 (ge/natsplit.h); gl38: no held call picked */
/* n quadwords at guest address a in one piece, as lw_ptr, the view's reads counted apart: whole in the copy, or whole outside it */
static inline __attribute__((always_inline)) const uint32_t *lw_nptr(uint32_t a, uint32_t n)
{
  uint32_t k; XmView *v = lw_view; int i;
  if (!(a & 0x80000000u)) { k = a & (lw_size - 1) & ~15u; if (k + 16 * n > lw_size) return 0; if (!v) return (const uint32_t *)(lw_mem + k); }
  else { uint32_t o = a & 0x3ff0; if (o + 16 * n > 0x4000u) return 0; if (!v) return (const uint32_t *)(lw_spr + o); k = XV_SPR + o; }
  i = xv_find(v, k);
  if (i >= 0) { if (v->x[i].a1 - k < 16 * n) { lw_nat_part++; return 0; } lw_nat_in++; return (const uint32_t *)(v->arena + v->x[i].off + (k - v->x[i].a0)); }
  i = -i - 1;
  if (i < v->n && v->x[i].a0 - k < 16 * n) { lw_nat_part++; return 0; }
  lw_nat_out++;
  return (const uint32_t *)(k >= XV_SPR ? v->spr + (k - XV_SPR) : v->mem + k);
}
/* gl34 (the fast front end's groups, NATG_FAST): lw_nptr with a hint of its own for each kind of pointer (the block, the material and the
   three streams lie apart, so the view's one hint misses at nearly every pointer and the look-up searches the extents).  The same answer
   as lw_nptr whatever the hints hold: a hint only says where the search starts (an index out of range: the whole list), and the view's
   own hint is not touched.  lw_nh_chk > 0 (the host's switch MGS2_NAT_HCHK): each answer against lw_nptr's, lw_nh_n[1] the differing */
static int lw_nh[5]; long lw_nh_n[3]; int lw_nh_chk;
static inline __attribute__((always_inline)) const uint32_t *lw_nptr_h1(uint32_t a, uint32_t n, int s, long *cnt)
{
  uint32_t k; XmView *v = lw_view; const XvExt *x; int i, lo, hi, vn;
  if (!(a & 0x80000000u)) { k = a & (lw_size - 1) & ~15u; if (k + 16 * n > lw_size) return 0; if (!v) return (const uint32_t *)(lw_mem + k); }
  else { uint32_t o = a & 0x3ff0; if (o + 16 * n > 0x4000u) return 0; if (!v) return (const uint32_t *)(lw_spr + o); k = XV_SPR + o; }
  x = v->x; vn = v->n; i = lw_nh[s]; lo = 0; hi = vn;
  if ((unsigned)i < (unsigned)vn) {
    if (x[i].a0 <= k) {
      if (k < x[i].a1) goto in;
      if (i + 1 == vn || k < x[i + 1].a0) { i++; goto out; }   /* between the hint's extent and the next: i the first above k */
      lo = i + 1; }
    else hi = i;
  }
  while (lo < hi) { int m = (lo + hi) >> 1; if (x[m].a1 <= k) lo = m + 1; else hi = m; }
  i = lo;                                                       /* the first extent ending above k (vn: none) */
  if (i < vn && x[i].a0 <= k) { lw_nh[s] = i; goto in; }
  lw_nh[s] = i - 1;
out:
  if (i < vn && x[i].a0 - k < 16 * n) { cnt[2]++; return 0; }
  cnt[1]++;
  return (const uint32_t *)(k >= XV_SPR ? v->spr + (k - XV_SPR) : v->mem + k);
in:
  if (x[i].a1 - k < 16 * n) { cnt[2]++; return 0; }
  cnt[0]++;
  return (const uint32_t *)(v->arena + x[i].off + (k - x[i].a0));
}
static inline __attribute__((always_inline)) const uint32_t *lw_nptr_h(uint32_t a, uint32_t n, int s)
{
  long c[3] = { 0, 0, 0 }; const uint32_t *p = lw_nptr_h1(a, n, s, c);
  lw_nh_n[0]++;
  if (lw_nh_chk > 0) { const uint32_t *q = lw_nptr(a, n); lw_nh_n[2]++; if (p != q) lw_nh_n[1]++; return q; }   /* (lw_nptr counted the read) */
  lw_nat_in += c[0]; lw_nat_out += c[1]; lw_nat_part += c[2];
  return p;
}
#define NATP_PTR(a, n) lw_nptr((a), (n))
#define NATP_PTRH(a, n, s) lw_nptr_h((a), (n), (s))
#define NATP_SPAN(p, nw, a) xl_vif1_span((p), (nw), (a))
#include "natp04.h"
#include "natp06.h"   /* native step 2: the people's front end (nat_p06_group) */
/* the groups of buffer b from *cur whose key is at or below RAM offset a (all of them: a = 0xffffffff), in order, to the translator.
   Returns the quadwords of the packet to step over (x86 shadow: the last group's own packet lies at a), else 0 */
static inline __attribute__((always_inline)) uint32_t lw_nat(XmPic *x, int b, int *cur, uint32_t a)
{
  const NatGrp *g = x->nat_g[b]; int n = x->nat_n[b], i = *cur, i0 = i; uint32_t skip = 0, c0 = 0; const int ns = x->ns_on && x->nat_clk;
  if (i >= n || (g[i].key & (lw_size - 1) & ~15u) > a) return 0;
  lw_nh_chk = x->nh_chk;
  if (x->nat_clk) c0 = ME_CALLP(x->nat_clk)();
  if (ns) { uint32_t c1 = ME_CALLP(x->nat_clk)(); g_ns.cal_t += (uint32_t)(c1 - c0); g_ns.cal_n++; c0 = c1; }   /* gl34: a pair of reads */
  int nr = 0, nrt = 0, f1 = 0; long bad0 = 0;   /* gl36 (xlate.h xl_nr_*): the run's reuse, when its picture has the switch; gl38: F1 (xl_f1_*) */
  if (x->nat_ru[b] & 3) { int e = i + 1; while (e < n && (g[e].key & (lw_size - 1) & ~15u) <= a) e++;
    if ((x->nat_ru[b] & 1) && xl_nr_cap) nr = xl_nr_begin(g + i, e - i, x->nat_i0[b] + i);
    if ((x->nat_ru[b] & 2) && nr != 1) { f1 = xl_f1_begin(g + i, e - i); bad0 = x->nat_bad; } }
  do {
    if (ns && --g_ns.cd <= 0) { g_ns.cd = 24 + (int)((g_ns.n * 5) & 15); g_ns.n++; g_ns.clk = x->nat_clk; g_ns.last = ME_CALLP(x->nat_clk)(); }   /* gl34: this group timed by stage */
    if (nr) xl_nr_group(i - i0);
    xl_f1_j = i - i0;
    nge_walk_r(b, x->nat_rf[b], x->nat_i0[b], i); int rc = nat_p04_group(&g[i]); nge_walk_end();   /* native step 1: the group's reference (ge/nge.h),
                                                     by its index in the whole array: g starts at the pending groups (nat_i0) */
    if (g_ns.clk) { NS_MARK(NS_END); g_ns.clk = 0; }
    if (rc == NATP_CORE) { x->nat_core++; nrt++; } else if (rc == NATP_FED) x->nat_fed++; else x->nat_bad++;
    skip = (g[i].flags & NATG_PKT) && (g[i].key & (lw_size - 1) & ~15u) == a ? 7u + (g[i].flags & NATG_FULL) : 0;
    i++;
  } while (i < n && (g[i].key & (lw_size - 1) & ~15u) <= a);
  if (nr) xl_nr_end(nrt);
  if (f1) xl_f1_end(x->nat_bad == bad0);
  if (x->nat_clk) { uint32_t d = ME_CALLP(x->nat_clk)() - c0; x->nat_t += d;
    if (ns) { g_ns.seen += i - i0; g_ns.ch_nat[g_ns.ch_cur] += d; g_ns.ch_grp[g_ns.ch_cur] += i - i0; } }
  x->nat_runs++; *cur = i;
  return skip;
}
/* native step 2: the people's groups of buffer b from *cur whose key is at or below lim, in order, to the translator (lane 2), each with
   its reference (ge/nge.h nge_walk6r: np6_r[b][i], taken with the groups) */
#define LW_K6(g) ((g).key & (lw_size - 1) & ~15u)
static void lw_nat6(XmPic *x, int b, int *cur, uint32_t lim)
{
  const NatP6 *g = x->np6_g[b]; const int n = x->np6_n[b]; int i = *cur;
  if (i >= n || LW_K6(g[i]) > lim) return;
  do {
    nge_walk6r(x->np6_r[b], i); const int rc = nat_p06_group(&g[i]); nge_walk6_end();
    if (rc == NATP_CORE) x->np6_core++; else if (rc == NATP_FED) x->np6_fed++; else x->np6_bad++;
    i++;
  } while (i < n && LW_K6(g[i]) <= lim);
  x->np6_runs++; *cur = i;
}
static void lw_nat_end(XmPic *x, int b, int cur, int pcur, int plan)   /* the walk ends: a planned transfer's groups the tags did not reach, drawn now */
{
  lw_stop = 0xffffffffu;
  if (b >= 0 && plan && cur < x->nat_n[b]) { x->nat_late += x->nat_n[b] - cur; (void)lw_nat(x, b, &cur, 0xffffffffu); }
  if (b >= 0 && plan && pcur < x->np6_n[b]) { x->np6_late += x->np6_n[b] - pcur; lw_nat6(x, b, &pcur, 0xffffffffu); }   /* step 2 */
  x->np6_b = b; x->np6_used = b >= 0 ? pcur : 0;
  x->nat_b = b; x->nat_used = cur;
  x->nat_in += lw_nat_in; x->nat_out += lw_nat_out; x->nat_part += lw_nat_part; lw_nat_in = lw_nat_out = lw_nat_part = 0;
  x->nat_stop += lw_stop_hit; lw_stop_hit = 0;
}
static inline const uint32_t *lw_q(const XlvMem *m, uint32_t a)
{ if (m->view) return xv_q(m->view, a);   /* task #180 */
  return (const uint32_t *)((a & 0x80000000u) ? m->spr + (a & 0x3ff0) : m->mem + (a & (m->mem_size - 1) & ~15u)); }
static void lw_run(XlvMem *m, uint32_t a, uint32_t n, uint32_t chcr)   /* ge/xlive.c run(): VIF1, spans */
{
  while (n) {
    uint32_t spr = a & 0x80000000u, o = spr ? (a & 0x3ff0) : (a & (m->mem_size - 1) & ~15u), k = ((spr ? 0x4000 : m->mem_size) - o) / 16;
    const uint8_t *p;
    if (k > n) k = n;
    if (m->view) { uint32_t lim = 16 * k; p = xv_at(m->view, (spr ? XV_SPR : 0) + o, &lim, XV_SPAN); if (lim < 16 * k) { m->view->part[XV_SPAN]++; k = lim / 16; } }   /* task #180 */
    else p = (const uint8_t *)lw_q(m, a);
    if (m->seen) for (uint32_t i = 0; i < k; i++) ME_CALLP(m->seen)(m->ctx, 1, a + 16 * i, p + 16 * i, 0, chcr);
    m->qw += k;
    xl_vif1_span((const uint32_t *)p, 4 * k, a);
    a += 16 * k; n -= k;
  }
}
static void lw_seen(XmPic *x, uint32_t chcr, uint32_t tadr, uint32_t nx, int nt)   /* a lane's group: its nt tags (CNT, REF) and data */
{
  XlvMem *m = x->m;
  for (int i = 0; i < nt; i++) {
    const uint8_t *t = (const uint8_t *)lw_q(m, tadr); uint64_t tag; uint32_t n, id, addr, data, next;
    memcpy(&tag, t, 8); n = tag & 0xffff; id = (tag >> 28) & 7; addr = (uint32_t)(tag >> 32) & 0xfffffff0u;
    m->tags++; if (m->seen) ME_CALLP(m->seen)(m->ctx, 1, tadr, t, 1, chcr);
    if (id == 1) { data = tadr + 16; next = data + 16 * n; } else if (id == 3) { data = addr; next = tadr + 16; } else { x->lw_bad++; return; }
    if (m->seen) for (uint32_t j = 0; j < n; j++) ME_CALLP(m->seen)(m->ctx, 1, data + 16 * j, (const uint8_t *)lw_q(m, data + 16 * j), 0, chcr);
    m->qw += n; tadr = next;
  }
  if (tadr != nx) x->lw_bad++;
}
/* gl34 (ge/natsplit.h): the walk's time by chain.  lw_ch_to: the clock read, the time and the converter calls since the last read
   to the slot that ends; i: the slot that begins (-1: the transfer's end) */
static void lw_ch_to(XmPic *x, int i)
{
  NatSplit *s = &g_ns; uint32_t t = ME_CALLP(x->nat_clk)(); int c = s->ch_cur;
  s->ch_t[c] += (uint32_t)(t - s->ch_last); s->ch_calls[c] += x->calls - s->ch_c0; s->ch_n[c]++;
  s->ch_last = t; s->ch_c0 = x->calls;
  if (i >= 0) s->ch_cur = i;
}
static void lw_walk(XmPic *x, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int lanes, uint32_t opt)   /* ge/xlive.c walk(), VIF1 */
{
  XlvMem *m = x->m; uint32_t asp[2], nasp = 0, guard = 0;
  int nb = x->nat_b >= 0 ? x->nat_b : x->np6_b, ncur = 0, pcur = 0, nplan = nb >= 0;   /* native step 1; step 2: the people's (pcur) */
  const int non = (x->nat_n[0] | x->nat_n[1] | x->np6_n[0] | x->np6_n[1]) != 0 && (chcr & 0x40);
  m->transfers++; x->lw_tr++; lw_mem = m->mem; lw_spr = m->spr; lw_size = m->mem_size; lw_view = m->view;
  if (((chcr >> 2) & 3) == 0) { lw_run(m, madr & ~0x80000000u, qwc, chcr); if (non) lw_nat_end(x, nb, 0, 0, nplan); return; }   /* normal mode: RAM only */
  if (qwc) {
    uint32_t id = (chcr >> 28) & 7;
    lw_run(m, madr, qwc, chcr);
    if (id == 0 || id == 7 || ((chcr & 0x80) && (chcr >> 31))) { if (non) lw_nat_end(x, nb, 0, 0, nplan); return; }
  }
  for (;;) {
    const uint32_t *t = lw_q(m, tadr); uint64_t tag; uint32_t n, id, addr, data, next; int end = 0;
    ME_BEAT();
    if (non && !nasp && !(tadr & 0x80000000u)) {   /* native step 1: a top-level tag in packet memory: the groups at or below it first */
      uint32_t a = tadr & (lw_size - 1) & ~15u;
      if (a >= NAT_PKT0 && a < NAT_PKT1) {
        int b = NAT_BUF(a);
        if (nb < 0 && (x->nat_n[b] > 0 || x->np6_n[b] > 0)) nb = b;   /* (a walk with no plan: bound as xc_walk binds it) */
        if (b == nb) {   /* step 2: the people at or below both the tag and the next ship group first, then the ship's, then the
                            people interleaved (boot/nat_x86.c's order) */
          const uint32_t ks = ncur < x->nat_n[b] ? x->nat_g[b][ncur].key & (lw_size - 1) & ~15u : 0xffffffffu;
          lw_nat6(x, b, &pcur, a < ks ? a : ks);
          uint32_t sk = lw_nat(x, b, &ncur, a);
          lw_nat6(x, b, &pcur, a);
          lw_stop = ncur < x->nat_n[b] ? x->nat_g[b][ncur].key & (lw_size - 1) & ~15u : 0xffffffffu;
          if (pcur < x->np6_n[b] && LW_K6(x->np6_g[b][pcur]) < lw_stop) lw_stop = LW_K6(x->np6_g[b][pcur]);
          if (sk) { tadr += 16 * sk; guard += sk; continue; } } } }
    if ((chcr & 0x40) && (t[2] & 0xffffff00u) == 0x01000100u) {   /* vif1_chain_fast's lane block */
      uint32_t nx, cl = t[2] & 0xff; int nt = 0, ln = 0; LwLoc s; memset(&s, 0, sizeof s);
      if (cl == 1) {
        int l = lanes;
        if (opt & OPT_LANE_MATCH) {
          uint32_t room = (tadr & 0x80000000u) ? 0x4000u - (tadr & 0x3ff0) : lw_size - (tadr & (lw_size - 1) & ~15u), u;
          if (room < 112u) l = 0;
          else { u = t[t[3] == 0x6c188000u ? 10 : 6]; l &= u == 0x01000103u ? 1 : u == 0x01000101u ? 2 : 0; } }
        if (l & 1) { nt = lw_p04(t, tadr, &s, &nx); ln = 1; }
        if (!nt && (l & 2)) {
          const int ps = x->ns_on && x->nat_clk && --g_ns.pcd <= 0;   /* gl38 (ge/natsplit.h): this P06 group timed by stage */
          if (ps) { g_ns.clk = g_ns.pclk = x->nat_clk; g_ns.tbo = NS_PN; g_ns.last = ME_CALLP(x->nat_clk)(); }
          nt = lw_p06(t, tadr, &s, &nx); ln = 2;
          if (ps) { if (nt) { NS_MARK(NS_END); g_ns.pn++; g_ns.pcd = NS_P_EVERY - 3 + (int)((g_ns.pn * 3) & 7); } else g_ns.pcd = 1;
            g_ns.clk = 0; g_ns.tbo = 0; }
          if (nt && x->ns_on && x->nat_clk) g_ns.pseen++; } }
      else if (cl == 7) { if (lanes & 8) { nt = lw_p07(t, tadr, &s, &nx); ln = 4; } }
      else if ((cl == 3 || cl == 6 || cl == 12) && (lanes & 4) && (t[3] & 0xff00ffffu) == 0x6c008100u) { nt = lw_fx(t, tadr, &s, &nx); ln = 3; }
      if (nt) { x->lw_grp[ln]++; x->lw_tags[ln] += nt; lw_seen(x, chcr, tadr, nx, nt); tadr = nx; guard += (uint32_t)nt; continue; } }
    memcpy(&tag, t, 8); n = tag & 0xffff; id = (tag >> 28) & 7; addr = (uint32_t)(tag >> 32) & 0x7ffffff0u; addr |= (uint32_t)(tag >> 32) & 0x80000000u;
    m->tags++;
    if (m->seen) ME_CALLP(m->seen)(m->ctx, 1, tadr, (const uint8_t *)t, 1, chcr);
    if (chcr & 0x40) xl_vif1_span(t + 2, 2, tadr + 8);   /* TTE */
    switch (id) {   /* (no jump table: -fno-jump-tables) */
    case 0: data = addr; next = 0; end = 1; break;                    /* refe */
    case 1: data = tadr + 16; next = data + 16 * n; break;              /* cnt */
    case 2: data = tadr + 16; next = addr; break;                       /* next */
    case 3: case 4: data = addr; next = tadr + 16; break;               /* ref, refs */
    case 5: data = tadr + 16; if (nasp < 2) asp[nasp++] = data + 16 * n; next = addr; break;   /* call */
    case 6: data = tadr + 16; if (nasp) next = asp[--nasp]; else { next = 0; end = 1; } break;   /* ret */
    default: data = tadr + 16; next = 0; end = 1; break;                /* end */
    }
    lw_run(m, data, n, chcr);
    if (end || ((chcr & 0x80) && (tag >> 31) & 1)) break;
    tadr = next;
    if (++guard > 1000000) { m->guard_stops++; break; }
  }
  if (non) lw_nat_end(x, nb, ncur, pcur, nplan);
}
/* one transfer of a record: the late walk for one the runtime's walk would have fed to the translator (lanes: the lanes it would have
   run), as xlv_dma does it otherwise (its xl_flush included) */
void xm_walk(XmPic *p, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int lanes, uint32_t opt)
{
  const int ns = p->ns_on && p->nat_clk;   /* gl34: the walk's time by chain (ge/natsplit.h) */
  if (ns) { NatSplit *s = &g_ns; s->ch_last = ME_CALLP(p->nat_clk)(); s->ch_c0 = p->calls; s->ch_cur = ch == 1 && (unsigned)p->ch_i < NS_CH ? p->ch_i : NS_CH_OTHER; }
  if (p->lw_on && ch == 1 && lanes && p->m->fast) { lw_walk(p, chcr, madr, qwc, tadr, lanes, opt); if (ns) lw_ch_to(p, NS_CH_FLUSH); xl_flush(); }
  else xlv_dma(p->m, ch, chcr, madr, qwc, tadr);
  if (ns) lw_ch_to(p, -1);
}

/* ---- task #180 (step 4b): a transfer's copy (xlme.h xm_copy) ---- */
static int xc_merge(XmCopy *c, XcR *r, int n)   /* r sorted by k0 and joined where ranges meet or overlap; c->tmp: n entries of scratch */
{
  int w, i, m, ok = 1; XcR *s = r, *d = c->tmp; const int pp = c->xp & XP_PP;
  for (i = 1; i < n && ok; i++) ok = r[i - 1].k0 <= r[i].k0;
  for (w = 1; !ok && w < n; w *= 2) {   /* bottom-up merge sort */
    for (i = 0; i < n; i += 2 * w) {
      int a = i, am = i + w < n ? i + w : n, b = am, bm = i + 2 * w < n ? i + 2 * w : n, o = i;
      while (a < am && b < bm) d[o++] = s[b].k0 < s[a].k0 ? s[b++] : s[a++];
      while (a < am) d[o++] = s[a++];
      while (b < bm) d[o++] = s[b++];
    }
    c->xp_moves += n;
    if (pp) { XcR *x = s; s = d; d = x; }   /* gl40 XP_PP: the next pass reads what this one wrote */
    else { memcpy(r, d, (size_t)n * sizeof *r); c->xp_cpy += n; }   /* gl39's: the pass's result copied back */
  }
  for (i = 0, m = 0; i < n; i++)   /* (XP_PP: the join reads the pass's last result, s, wherever it lies, and writes r) */
    if (m && s[i].k0 <= r[m - 1].k1) { if (s[i].k1 > r[m - 1].k1) r[m - 1].k1 = s[i].k1; }
    else r[m++] = s[i];
  return m;
}
static void xc_add(XmCopy *c, int b, uint32_t k0, uint32_t k1)
{
  XcR *r = b ? c->rb : c->ra; int *n = b ? &c->nb : &c->na, cap = b ? c->cap : XC_CAPS(c);   /* (step 4d-3: ra's room) */
  c->xp_adds++;   /* gl40 probe */
  if (*n && k0 <= r[*n - 1].k1 && k1 >= r[*n - 1].k0) {   /* meets the last: joined (the walk goes on mostly in order) */
    if (k0 < r[*n - 1].k0) r[*n - 1].k0 = k0;
    if (k1 > r[*n - 1].k1) r[*n - 1].k1 = k1;
    return; }
  if (c->xp & XP_LOOK)   /* gl40 XP_LOOK: the XP_K - 1 entries before the last, newest first; the first one it meets is widened (the union kept) */
    for (int j = *n - 2, e = *n - XP_K; j >= 0 && j >= e; j--)
      if (k0 <= r[j].k1 && k1 >= r[j].k0) { if (k0 < r[j].k0) r[j].k0 = k0; if (k1 > r[j].k1) r[j].k1 = k1; return; }
  if (*n == cap) { *n = xc_merge(c, r, *n); if (*n == cap) { c->over = 1; return; } }
  r[*n].k0 = k0; r[*n].k1 = k1; (*n)++;
}
static void xc_run(XmCopy *c, int b, uint32_t a, uint32_t n)   /* n quadwords from guest address a, in ge/xlive.c run()'s pieces */
{
  while (n) {
    uint32_t spr = a & 0x80000000u, o = spr ? (a & 0x3ff0) : (a & (c->mem_size - 1) & ~15u), k = ((spr ? 0x4000 : c->mem_size) - o) / 16;
    if (k > n) k = n;
    xc_add(c, b && !spr, (spr ? XV_SPR : 0) + o, (spr ? XV_SPR : 0) + o + 16 * k);   /* (no image marks in the scratchpad) */
    a += 16 * k; n -= k;
  }
}
static void xc_tag(XmCopy *c, uint32_t a)   /* a tag and the margin after it, to the end of RAM or the scratchpad (where the lanes stop) */
{
  uint32_t spr = a & 0x80000000u, o = spr ? (a & 0x3ff0) : (a & (c->mem_size - 1) & ~15u), e = spr ? 0x4000 : c->mem_size, k1 = o + 16 + (c->rd_only ? 0 : XC_MARGIN);
  if (k1 > e) k1 = e;
  xc_add(c, 0, (spr ? XV_SPR : 0) + o, (spr ? XV_SPR : 0) + k1);
}
/* native step 1: a group the walk will draw from its record: its data kept, as the REF tags of its packet would have kept it (the
   front end's caps and ranges, ge/natp04.h: a group it drops adds nothing) */
static void xc_nat(XmCopy *c, const NatGrp *g)
{
  if (g->n[0] > 128u || g->n[1] > 128u || g->n[2] > 64u) return;
  if (g->flags & NATG_FULL) { xc_run(c, 1, g->blk & 0xfffffff0u, 24); c->nat_qw += 24; }
  xc_run(c, 1, g->mat & 0xfffffff0u, 8); c->nat_qw += 8;
  if (g->flags & NATG_INPL) return;   /* gl33: a static group's three streams stay in RAM (ge/natsnap.h NATG_INPL): the walk reads them
                                         there (lw_nptr's "outside the copy" path) */
  for (int k = 0; k < 3; k++) if (g->s[k] && g->n[k]) { xc_run(c, 1, g->s[k] & 0xfffffff0u, g->n[k]); c->nat_qw += g->n[k]; }
}
/* native step 2: a people group the walk will draw from its record: its data kept as its packet's REF tags would have kept it (the
   constant block, the materials sent, the six streams; nat_p06_group's caps: a group it drops adds nothing) */
static void xc_nat6(XmCopy *c, const NatP6 *g)
{
  for (int j = 0; j < 6; j++) if (g->n[j] > 128u) return;
  if (g->n[2] > 64u || g->n[3] > 64u || g->n[4] > 64u) return;
  if (g->flags & NP6G_FULL) { xc_run(c, 1, g->blk & 0xfffffff0u, 24); c->nat_qw += 24; }
  for (int j = 0; j < 3; j++) if (g->mat[j]) { xc_run(c, 1, g->mat[j] & 0xfffffff0u, 8); c->nat_qw += 8; }
  for (int k = 0; k < 6; k++) if (g->s[k] && g->n[k]) { xc_run(c, 1, g->s[k] & 0xfffffff0u, g->n[k]); c->nat_qw += g->n[k]; }
}
static int xc_walk(XmCopy *c, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr)   /* ge/xlive.c walk()'s chain rules */
{
  uint32_t asp[2], nasp = 0, guard = 0; int non = (c->nat_n[0] | c->nat_n[1] | c->np6_n[0] | c->np6_n[1]) != 0 && (chcr & 0x40);
  c->nat_b = -1; c->nat_cur = 0; c->np6_b = -1; c->np6_cur = 0;   /* (step 2: the people bound with the ship's buffer) */
  if (((chcr >> 2) & 3) == 0) { xc_run(c, 1, madr & ~0x80000000u, qwc); return 0; }   /* normal mode: RAM only */
  if (qwc) {
    uint32_t id = (chcr >> 28) & 7;
    xc_run(c, id == 0 || id == 3 || id == 4, madr, qwc);
    if (id == 0 || id == 7 || ((chcr & 0x80) && (chcr >> 31))) return 0;
  }
  for (;;) {
    const uint8_t *t = (tadr & 0x80000000u) ? c->spr + (tadr & 0x3ff0) : c->mem + (tadr & (c->mem_size - 1) & ~15u);
    uint64_t tag; uint32_t n, id, addr, data, next; int end = 0;
    memcpy(&tag, t, 8); n = tag & 0xffff; id = (tag >> 28) & 7; addr = (uint32_t)(tag >> 32) & 0x7ffffff0u; addr |= (uint32_t)(tag >> 32) & 0x80000000u;
    if (non && !nasp && !(tadr & 0x80000000u)) {   /* native step 1: the walk's rule (lw_walk) */
      uint32_t a = tadr & (c->mem_size - 1) & ~15u;
      if (a >= NAT_PKT0 && a < NAT_PKT1) {
        int b = NAT_BUF(a);
        if (c->nat_b < 0 && (c->nat_n[b] > 0 || c->np6_n[b] > 0)) c->nat_b = c->np6_b = b;
        if (b == c->nat_b) { const NatGrp *g = c->nat_g[b]; const NatP6 *q = c->np6_g[b];
          while (c->nat_cur < c->nat_n[b] && (g[c->nat_cur].key & (c->mem_size - 1) & ~15u) <= a) { xc_nat(c, &g[c->nat_cur]); c->nat_cur++; c->nat_grp++; }
          while (c->np6_cur < c->np6_n[b] && (q[c->np6_cur].key & (c->mem_size - 1) & ~15u) <= a) { xc_nat6(c, &q[c->np6_cur]); c->np6_cur++; c->np6_grp++; } } } }
    xc_tag(c, tadr);
    switch (id) {   /* (no jump table: -fno-jump-tables) */
    case 0: data = addr; next = 0; end = 1; break;
    case 1: data = tadr + 16; next = data + 16 * n; break;
    case 2: data = tadr + 16; next = addr; break;
    case 3: case 4: data = addr; next = tadr + 16; break;
    case 5: data = tadr + 16; if (nasp < 2) asp[nasp++] = data + 16 * n; next = addr; break;
    case 6: data = tadr + 16; if (nasp) next = asp[--nasp]; else { next = 0; end = 1; } break;
    default: data = tadr + 16; next = 0; end = 1; break;
    }
    xc_run(c, id == 0 || id == 3 || id == 4, data, n);
    if (end || ((chcr & 0x80) && (tag >> 31) & 1)) break;
    tadr = next;
    if (++guard > 1000000) return 4;
  }
  return 0;
}
static const uint32_t xc_keep[2][2] = { { 0x12c0000u, 0x1430000u }, { 0x1ed0000u, 0x1ee0000u } };   /* the texture sources the game rewrites */
/* gl40: xm_plan in two parts.  xc_ranges: the walk, the sorts and joins, the marks taken out, ra and rb joined into rb[0 .. *m): 0, or
   1 / 4 (as xm_plan's).  The probe's clock (c->clk) read at its start, after the walk and after the sorts of ra and rb. */
#define XC_CLK(c) ((c)->clk ? ME_CALLP((c)->clk)() : 0u)
static int xc_ranges(XmCopy *c, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int *mo, uint32_t *t2)
{
  int r, na, nb, nk = 0, i, j, m; uint32_t t0 = XC_CLK(c), t1;
  c->na = c->nb = 0; c->over = 0; c->xp_plans++;
  r = xc_walk(c, chcr, madr, qwc, tadr); t1 = XC_CLK(c); c->xp_tw += t1 - t0;
  if (r || (c->over && (r = 1))) { c->fails[r]++; *t2 = t1; return r; }
  if (c->na > c->na_max) c->na_max = c->na;
  if (c->nb > c->nb_max) c->nb_max = c->nb;
  if (c->nmk > c->nmk_max) c->nmk_max = c->nmk;
  c->xp_ra += c->na; c->xp_rb += c->nb; c->xp_mk += c->mk_full ? 0 : c->nmk;
  na = xc_merge(c, c->ra, c->na); nb = xc_merge(c, c->rb, c->nb);
  *t2 = XC_CLK(c); c->xp_ts += *t2 - t1;
  if (!c->mk_full && c->nmk) {   /* the marks outside the two ranges (tmp), then back in mk */
    int n0 = xc_merge(c, c->mk, c->nmk);
    for (i = 0; i < n0; i++) {
      uint32_t s = c->mk[i].k0, e = c->mk[i].k1;
      for (j = 0; j < 2 && s < e; j++) {
        if (xc_keep[j][1] <= s || xc_keep[j][0] >= e) continue;
        if (xc_keep[j][0] > s) { if (nk == XC_CAPS(c)) break; c->tmp[nk].k0 = s; c->tmp[nk].k1 = xc_keep[j][0]; nk++; }   /* (4d-3: mk's room) */
        s = xc_keep[j][1];
      }
      if (s < e && nk < XC_CAPS(c)) { c->tmp[nk].k0 = s; c->tmp[nk].k1 = e; nk++; }
    }
    memcpy(c->mk, c->tmp, (size_t)nk * sizeof *c->mk);
  }
  for (i = 0, j = 0, m = 0; i < nb; i++) {   /* rb less the marks, into tmp */
    uint32_t s = c->rb[i].k0, e = c->rb[i].k1; int jj;
    while (j < nk && c->mk[j].k1 <= s) j++;
    for (jj = j; s < e; ) {
      if (jj < nk && c->mk[jj].k0 < e) {
        if (c->mk[jj].k0 > s) { if (m == c->cap) { c->fails[1]++; return 1; } c->tmp[m].k0 = s; c->tmp[m].k1 = c->mk[jj].k0; m++; }
        c->marked += (c->mk[jj].k1 < e ? c->mk[jj].k1 : e) - (c->mk[jj].k0 > s ? c->mk[jj].k0 : s);
        if (c->mk[jj].k1 > s) s = c->mk[jj].k1;
        jj++; c->rb_cut++;
      } else { if (m == c->cap) { c->fails[1]++; return 1; } c->tmp[m].k0 = s; c->tmp[m].k1 = e; m++; s = e; }
    }
  }
  nb = m;
  for (i = 0, j = 0, m = 0; i < na || j < nb; ) {   /* ra and tmp joined, into rb */
    XcR x = j >= nb || (i < na && c->ra[i].k0 <= c->tmp[j].k0) ? c->ra[i++] : c->tmp[j++];
    if (m && x.k0 <= c->rb[m - 1].k1) { if (x.k1 > c->rb[m - 1].k1) c->rb[m - 1].k1 = x.k1; }
    else { if (m == c->cap) { c->fails[1]++; return 1; } c->rb[m++] = x; }
  }
  *mo = m;
  return 0;
}
static int xc_plan(XmCopy *c, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int *x0, int *nx)   /* gl39's xm_plan */
{
  int r, i, m = 0; uint32_t tot = 0, t2 = 0;
  r = xc_ranges(c, chcr, madr, qwc, tadr, &m, &t2);
  if (r) { if (c->clk) c->xp_tr += XC_CLK(c) - t2; return r; }
  for (i = 0; i < m; i++) tot += c->rb[i].k1 - c->rb[i].k0;
  if (c->next + m > c->ext_cap) { c->fails[2]++; r = 2; }
  else if (c->used + tot > c->arena_n) { c->fails[3]++; r = 3; }
  else {
    *x0 = c->next; *nx = m;
    for (i = 0; i < m; i++) {
      XvExt *x = &c->ext[c->next++]; uint32_t k0 = c->rb[i].k0, n = c->rb[i].k1 - k0;
      x->a0 = k0; x->a1 = c->rb[i].k1; x->off = c->used; c->used += n;
    }
    c->copies++; c->exts += m; c->bytes += tot; if (m > c->nx_max) c->nx_max = m;
  }
  if (c->clk) c->xp_tr += XC_CLK(c) - t2;
  return r;
}
/* gl40 XP_CHK: the plan as asked (c->xp's other bits; its result kept), then gl39's ranges made again from the same transfer (xp 0) and
   compared: the same result, and on 0 the same ranges as the plan's extents, in order.  The second walk's counts and outputs are put back
   (the native groups taken, nat_b and nat_cur, are the first plan's).  The image marks (mk, the host's) are joined in place by the first
   plan, as gl39's own plans again after a full half or arena (psp/xl_psp.c mq_chk_end): the second plan's marks are the same set.  A
   plan that failed for the extents' or the arena's room (2, 3) is compared by the second walk's result only (xp_room); the host plans it
   again once there is room, and that plan is compared in full. */
int xm_plan(XmCopy *c, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int *x0, int *nx)   /* step 4b-2: xm_copy's first part */
{
  const int xp = c->xp; int r, r0, m0 = 0, i, bad; uint32_t t2;
  if (!(xp & XP_CHK)) return xc_plan(c, chcr, madr, qwc, tadr, x0, nx);
  r = xc_plan(c, chcr, madr, qwc, tadr, x0, nx);
  {
    const int na = c->na, nb = c->nb, over = c->over, nab = c->nat_b, nac = c->nat_cur, nam = c->na_max, nbm = c->nb_max, nkm = c->nmk_max;
    const int pb = c->np6_b, pc = c->np6_cur; const long pg = c->np6_grp;   /* step 2 */
    const long ng = c->nat_grp, rc = c->rb_cut, pl = c->xp_plans, ad = c->xp_adds, mv = c->xp_moves, cp = c->xp_cpy, ra = c->xp_ra, rb = c->xp_rb, mk = c->xp_mk;
    const uint64_t nq = c->nat_qw, mr = c->marked, tw = c->xp_tw, ts = c->xp_ts; long fl[5];
    uint32_t (*const ck)(void) = c->clk;
    for (i = 0; i < 5; i++) fl[i] = c->fails[i];
    c->xp = 0; c->clk = 0;
    r0 = xc_ranges(c, chcr, madr, qwc, tadr, &m0, &t2);
    c->xp = xp; c->clk = ck;
    if (r == 2 || r == 3) { bad = r0 != 0; c->xp_room++; }
    else if (r) bad = r0 != r;
    else { bad = r0 != 0 || m0 != *nx;
      for (i = 0; !bad && i < m0; i++) bad = c->rb[i].k0 != c->ext[*x0 + i].a0 || c->rb[i].k1 != c->ext[*x0 + i].a1; }
    c->xp_chk++;
    if (bad && !c->xp_dif++) { c->xp_dif_w[0] = (uint32_t)r | (uint32_t)r0 << 8; c->xp_dif_w[1] = (uint32_t)(r ? 0 : *nx); c->xp_dif_w[2] = (uint32_t)m0; c->xp_dif_vb = -1; }
    c->na = na; c->nb = nb; c->over = over; c->nat_b = nab; c->nat_cur = nac; c->na_max = nam; c->nb_max = nbm; c->nmk_max = nkm;
    c->np6_b = pb; c->np6_cur = pc; c->np6_grp = pg;
    c->nat_grp = ng; c->rb_cut = rc; c->nat_qw = nq; c->marked = mr; c->xp_plans = pl; c->xp_adds = ad; c->xp_moves = mv; c->xp_cpy = cp;
    c->xp_ra = ra; c->xp_rb = rb; c->xp_mk = mk; c->xp_tw = tw; c->xp_ts = ts;
    for (i = 0; i < 5; i++) c->fails[i] = fl[i];
  }
  return r;
}
int xm_nat_room(XmCopy *c, uint32_t n, uint32_t *off)   /* native step 1 (xlme.h) */
{
  n = (n + 15u) & ~15u;
  if (c->used + n > c->arena_n) return 3;
  *off = c->used; c->used += n;
  return 0;
}
void xm_lay(XmCopy *c, int x0, int nx, int t, const XmLogE *log, int nlog)   /* step 4c: xm_fill's log part (an early copy's, at the FILL) */
{
  const XvExt *x = c->ext + x0; int i;
  for (i = nlog - 1; i >= 0 && log[i].t >= t; i--) {   /* the entries made since the transfer started, the newest first */
    uint32_t a = log[i].l << 6, e = a + 64; int lo = 0, hi = nx, k, in = 0;
    while (lo < hi) { int md = (lo + hi) >> 1; if (x[md].a1 <= a) lo = md + 1; else hi = md; }   /* the first extent that ends after a */
    for (k = lo; k < nx && x[k].a0 < e; k++) {   /* (RAM keys: the scratchpad's extents, keyed from XV_SPR, are never met) */
      uint32_t s = x[k].a0 > a ? x[k].a0 : a, f = x[k].a1 < e ? x[k].a1 : e;
      memcpy(c->arena + x[k].off + (s - x[k].a0), log[i].old + (s - a), f - s); c->laid_b += f - s; in = 1; }
    c->laid++; c->laid_lines += in;
  }
}
void xm_fill(XmCopy *c, int x0, int nx, int t, const XmLogE *log, int nlog)   /* step 4b-2: xm_copy's second part, at the deadline */
{
  const XvExt *x = c->ext + x0; int i;
  for (i = 0; i < nx; i++)
    memcpy(c->arena + x[i].off, x[i].a0 >= XV_SPR ? c->spr + (x[i].a0 - XV_SPR) : c->mem + x[i].a0, x[i].a1 - x[i].a0);
  c->fills++;
  xm_lay(c, x0, nx, t, log, nlog);
}
int xm_copy(XmCopy *c, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int *x0, int *nx)
{
  int r = xm_plan(c, chcr, madr, qwc, tadr, x0, nx);
  if (!r) xm_fill(c, *x0, *nx, 0, 0, 0);
  return r;
}

/* ---- the switches a record carries ---- */
void xm_init(XmPic *p)
{
  static const int start[XS_N] = { 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1 };   /* xlate.c, gsup2.c and texcache.c start them so (xl_psp.c's) */
  for (int i = 0; i < XS_N; i++) p->sw[i] = start[i];
  p->now = 0; p->calls = p->prims = p->lists = p->list_fail = 0; p->dig_frame = 0;
  p->lw_tr = p->lw_bad = 0; for (int i = 0; i < 5; i++) p->lw_tags[i] = p->lw_grp[i] = 0;
}
void xm_objects(XmPic *p, GuMap *map, TkCache *tk)
{
  p->map = map; p->tk = tk;
  p->sw[XS_GUMEMO] = p->sw[XS_GUCLUT] = p->sw[XS_LDID] = -1;
  p->sw[XS_TKBLK] = p->sw[XS_TKFLAGS] = p->sw[XS_UMEMO] = p->sw[XS_UFRONT] = -1;
}
void xm_switch(XmPic *p, uint32_t opt, uint32_t opt2)
{
  int *s = p->sw, v;
  if (p->follow & XM_F_REST) {
    if ((v = (opt & OPT_XL_VMUD) != 0) != s[XS_VMUD]) { xl_vm_mode(v); s[XS_VMUD] = v; }
    if ((v = (opt & OPT_XL_V48) != 0) != s[XS_V48]) { xl_ud_v48(v); s[XS_V48] = v; } }
  if (p->follow & XM_F_LANES) {
    if ((v = (opt & OPT_XL_EAGER) != 0) != s[XS_EAGER]) { xl_ud_eager(v); s[XS_EAGER] = v; }
    if ((v = (opt & OPT_XL_MARK) != 0) != s[XS_MARK]) { xl_ud_mark(v); s[XS_MARK] = v; }
    v = p->lanes & (((opt & OPT_XL_LANE) ? 1 : 0) | ((opt & OPT_XL_LANE2) ? 2 : 0) | ((opt & OPT_XL_LANE3) ? 4 : 0) | ((opt & OPT_XL_LANE4) ? 8 : 0));
    if (v != s[XS_LANE]) { xl_lane(v != 0); s[XS_LANE] = v; } }
  if (p->follow & XM_F_REST) {
#ifdef SB_NEED
    if ((v = (opt & OPT_XL_NEED) != 0) != s[XS_NEED]) { xl_need(v); gl_need = v; s[XS_NEED] = v; }
#endif
    if (p->map && (v = (opt & OPT_GU_MEMO) != 0) != s[XS_GUMEMO]) { gu_area_memo(p->map, v); s[XS_GUMEMO] = v; }
    if (p->map && (v = (opt & OPT_GU_CLUT) != 0) != s[XS_GUCLUT]) { gu_clut_memo(p->map, v); s[XS_GUCLUT] = v; } }
  if (p->follow & XM_F_STEP0) {
    if (p->map && (v = (opt2 & OPT2_GU_LDID) ? 1 + p->chk_li : 0) != s[XS_LDID]) { gu_ld_ident(p->map, v); s[XS_LDID] = v; } }
  if (p->follow & XM_F_REST) {
    if (p->tk && (v = (opt & OPT_TK_BLK) != 0) != s[XS_TKBLK]) { tk_blk(p->tk, v); s[XS_TKBLK] = v; }
    if (p->tk && (v = (opt & OPT_TK_FLAGS) != 0) != s[XS_TKFLAGS]) { tk_flags(p->tk, v); s[XS_TKFLAGS] = v; } }
  if (p->follow & XM_F_STEP0) {
    if (p->tk && (v = (opt2 & OPT2_TX_UMEMO) ? 1 + p->chk_um : 0) != s[XS_UMEMO]) { tk_umemo(p->tk, v); s[XS_UMEMO] = v; }
    if (p->tk && (v = (opt2 & OPT2_TX_FRONT) ? 1 + p->chk_uf : 0) != s[XS_UFRONT]) { tk_ufront(p->tk, v); s[XS_UFRONT] = v; } }
}

/* ---- the translator's callbacks ---- */
/* gl26: the walk's split (xlme.h xm_tsplit).  ts_d: a callback inside a timed one is not timed again (none is expected) */
static uint32_t (*ts_clk)(void) H39; static uint64_t *ts_t H39; static long *ts_n H39; static int ts_d H39;
void xm_tsplit(uint32_t (*clk)(void), uint64_t *t, long *n) { ts_clk = clk; ts_t = t; ts_n = n; ts_d = 0; }
static inline int ts_on(uint32_t *c0) { if (!ts_clk || ts_d++) return 0; *c0 = ME_CALLP(ts_clk)(); return 1; }   /* 0: not timed (gl26b: the
                                                                                   start exact, no mark in the clock's value) */
static inline void ts_off(int on, uint32_t c0, int b) { if (on) { ts_t[b] += ME_CALLP(ts_clk)() - c0; ts_n[b]++; ts_d = 0; } else if (ts_d) ts_d--; }
static inline long ts_hf(const GlFrame *f) { return f->hflush[1] + f->hflush[2] + f->hflush_log; }   /* gl26b: gl_call's flushes */
void xm_call(XmPic *p, const XlCall *c) { p->calls++; if (p->now) { uint32_t c0 = 0; int on = ts_on(&c0); long h0 = on ? ts_hf(p->glf) : 0;
    gl_call(p->glf, c); ts_off(on, c0, on && ts_hf(p->glf) != h0 ? XM_TS_CFL : c->lane >= 1 && c->lane <= 4 ? c->lane : XM_TS_L0); } }
void xm_pass(XmPic *p, const XlCall *c, int pass) { if (p->now) { uint32_t c0 = 0; int on = ts_on(&c0); gl_pass(p->glf, c, pass); ts_off(on, c0, XM_TS_PASS); } }
void xm_flush(XmPic *p) { GlFrame *f = p->glf; if (f->nheld) { uint32_t c0 = 0; int on = ts_on(&c0); f->hflush[0]++; gl_flush(f); ts_off(on, c0, XM_TS_FLUSH); } }   /* sb4: a transfer's end */
void xm_prim(XmPic *p, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n) { p->prims++; if (p->now) { uint32_t c0 = 0; int on = ts_on(&c0); gl_prim(p->glf, g, d, v, n); ts_off(on, c0, XM_TS_PRIM); } }
void xm_on_call(void *ctx, const XlCall *c) { xm_call((XmPic *)ctx, c); }
void xm_on_pass(void *ctx, const XlCall *c, int pass) { xm_pass((XmPic *)ctx, c, pass); }
void xm_on_flush(void *ctx) { xm_flush((XmPic *)ctx); }
void xm_on_prim(void *ctx, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n) { xm_prim((XmPic *)ctx, g, d, v, n); }
void xm_install(XmPic *p) { xl_on_call(xm_on_call, p); xl_on_pass(xm_on_pass, p); xl_on_flush(xm_on_flush, p); xl_on_prim(xm_on_prim, p); }
/* task #180: the GS map's n bytes at RAM key k (k + n within RAM) through a transfer's copy: in the copy, in RAM, or the pieces put
   together in one of four buffers (the map reads each at once) */
const uint8_t *xv_ram(XmView *v, uint32_t k, uint32_t n)
{
  uint32_t lim = n, done = 0; const uint8_t *p; uint8_t *d; int b;
  if (!n) return v->mem + k;
  p = xv_at(v, k, &lim, XV_RAM);
  if (lim >= n) return p;
  v->part[XV_RAM]++; b = v->tmp_i = (v->tmp_i + 1) & 3;
  if (n <= v->fix_n) { d = v->fix[b]; v->n_fix++; }   /* step 4d-2 */
  else {
    if (v->tmp_n[b] < n) { uint8_t *q = (uint8_t *)realloc(v->tmp[b], n); if (!q) return 0; v->tmp[b] = q; v->tmp_n[b] = n; }
    d = v->tmp[b]; v->n_tmp++; }
  while (done < n) { uint32_t l = n - done; const uint8_t *s = xv_at(v, k + done, &l, XV_RAM); memcpy(d + done, s, l); done += l; }
  return d;
}
const uint8_t *xm_ram(void *ctx, uint32_t ee, uint32_t n)
{
  const XlvMem *m = ((const XmPic *)ctx)->m;
  ee &= m->mem_size - 1;
  if (!m->mem || ee + n > m->mem_size) return 0;
  return m->view ? xv_ram(m->view, ee, n) : m->mem + ee;   /* task #180 */
}

/* ---- a frame's end, its list, a frame's start ---- */
void xm_end(XmPic *p) { gl_end(p->glf); p->now = 0; }
int xm_list(XmPic *p, int buf, int dig)
{
  GlFrame *f = p->glf; GxFrame *F = &p->gxf; int r;
  F->b = f->o.b; F->nb = (uint32_t)f->o.nb; F->v = f->o.v; F->nv = (uint32_t)f->o.nv; F->d = f->o.d;
  F->t = f->o.t; F->nt = (uint32_t)f->o.nt; F->tdata = f->tdata; F->tpal = f->tpal;
  gx_depth_map(f->map_lo, f->map_hi, 1.0f, &F->zs, &F->zo);
  p->gxl->dig_on = dig;
  r = gx_frame(p->gxl, p->gxh, F, buf);
  p->lists++; if (r) p->list_fail++;
  if (dig) { GxList *l = p->gxl; const uint32_t c = l->clk ? ME_CALLP(l->clk)() : 0;   /* gl38: the frame's digest timed apart */
    p->dig_frame = gl_digest(f, l);
#ifdef __psp__
    p->dig_c = gl_digest_c(f);   /* gl41: the content digest (the twin prints it from its own frame) */
#endif
    if (l->clk) l->t_k[GX_T_DIG] += ME_CALLP(l->clk)() - c; }
  return r;
}
void xm_begin(XmPic *p, uint32_t opt, uint32_t opt2)
{
  GlFrame *f = p->glf;
  if (p->follow & XM_F_CUT) f->h.cut = (opt & OPT_GL_CUT) != 0;
  f->h.clipfast = (opt & OPT_CLIP_FAST) != 0;
  f->stepc = (opt & OPT_XL_STEPC) != 0;        /* sb4: step c (renderer_notes 11.29) */
  f->stepc7 = (opt2 & OPT2_C7) != 0;           /* sb10 T2: and lane 4's P07 calls */
  f->h.c1 = (opt2 & OPT2_C1) != 0;             /* sb11 C1: the faces skinned and lit by the GE */
  f->h.c2 = (opt2 & OPT2_C2) != 0;             /* sb12 C2: the people skinned and lit by the GE */
  f->h.c3v = (opt2 & OPT2_C3V) != 0;           /* sb14 3v: df0 in two passes */
  f->h.c3z = (opt2 & OPT2_C3Z) != 0;           /* sb14 3z: df0's depth range by the box */
  f->h.c3w = (opt2 & OPT2_C3W) != 0;           /* sb14 3w: two-pass df0 vertices straight into the frame */
  f->h.c4 = (opt2 & OPT2_C4) != 0;             /* sb15 step 4: the sprites, quads and streaks drawn (task #180: as psp/xl_psp.c) */
  f->h.c4x = (opt2 & OPT2_C4X) != 0;           /* sb15 4x: the effect strips from their clip positions */
  gl_begin(f); p->now = 1;
}
