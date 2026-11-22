/* xlme.c: the picture's loop on the Media Engine, the parts between the records and the translator (task #149; xlme.h).  The late walk
   is me13 b's (boot/xlive_chk_g.c lw_walk, lw_run, lw_seen, lw_q, lw_dma; me_step1_notes.md 2) moved here with its counts in the XmPic;
   the switches are psp/xl_psp.c xlive_chk_dma's, the callbacks its on_call, on_pass, on_flush and on_prim without the host's timing,
   the frame's end, list and start its xl_psp_frame and gl_list without the GE and the display (those stay on the main CPU). */
#include <string.h>
#include "xlme.h"
#include "me_rt.h"                      /* -DMGS2_ME_SAFE: ME_CALLP and the C library's calls on the ME */
#include "../runtime/host_time.h"       /* OPT_* and OPT2_* (only the bits: the switches come with each record, never from g_opt) */
#include "xlview.h"                     /* task #180: a transfer's copy (step 4b) */

long xm_lw7_miss[8], xm_lw3_miss[8];
#ifndef __psp__
long xm_lw_ncmd[256];
#endif

/* ---- the late walk with the lanes ---- */
typedef struct { uint32_t left, cmd, cl, wl; } LwLoc;   /* runtime/ee_io.h VifLoc: the walk's state a lane leaves */
static const uint8_t *lw_mem, *lw_spr; static uint32_t lw_size;   /* the bytes the lanes read (lw_lanes.h lw_ptr) */
static XmView *lw_view;                 /* task #180: the walk's view of the transfer's copy, or 0 (lw_lanes.h lw_ptr, lw_tp) */
static inline const uint32_t *lw_tp(uint32_t a)   /* a lane's next slot or prim (lw_lanes.h, after tools/dev/me18/lw_view.py) */
{ return lw_view ? xv_q(lw_view, a) : (const uint32_t *)((a & 0x80000000u) ? lw_spr + (a & 0x3ff0) : lw_mem + (a & (lw_size - 1) & ~15u)); }
#define lw_ncmd xm_lw_ncmd
#define lw7_miss xm_lw7_miss
#define lw3_miss xm_lw3_miss
#include "lw_lanes.h"
#undef lw_ncmd
#undef lw7_miss
#undef lw3_miss
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
static void lw_walk(XmPic *x, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int lanes, uint32_t opt)   /* ge/xlive.c walk(), VIF1 */
{
  XlvMem *m = x->m; uint32_t asp[2], nasp = 0, guard = 0;
  m->transfers++; x->lw_tr++; lw_mem = m->mem; lw_spr = m->spr; lw_size = m->mem_size; lw_view = m->view;
  if (((chcr >> 2) & 3) == 0) { lw_run(m, madr & ~0x80000000u, qwc, chcr); return; }   /* normal mode: RAM only */
  if (qwc) {
    uint32_t id = (chcr >> 28) & 7;
    lw_run(m, madr, qwc, chcr);
    if (id == 0 || id == 7 || ((chcr & 0x80) && (chcr >> 31))) return;
  }
  for (;;) {
    const uint32_t *t = lw_q(m, tadr); uint64_t tag; uint32_t n, id, addr, data, next; int end = 0;
    ME_BEAT();
    if ((chcr & 0x40) && (t[2] & 0xffffff00u) == 0x01000100u) {   /* vif1_chain_fast's lane block */
      uint32_t nx, cl = t[2] & 0xff; int nt = 0, ln = 0; LwLoc s; memset(&s, 0, sizeof s);
      if (cl == 1) {
        int l = lanes;
        if (opt & OPT_LANE_MATCH) {
          uint32_t room = (tadr & 0x80000000u) ? 0x4000u - (tadr & 0x3ff0) : lw_size - (tadr & (lw_size - 1) & ~15u), u;
          if (room < 112u) l = 0;
          else { u = t[t[3] == 0x6c188000u ? 10 : 6]; l &= u == 0x01000103u ? 1 : u == 0x01000101u ? 2 : 0; } }
        if (l & 1) { nt = lw_p04(t, tadr, &s, &nx); ln = 1; }
        if (!nt && (l & 2)) { nt = lw_p06(t, tadr, &s, &nx); ln = 2; } }
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
}
/* one transfer of a record: the late walk for one the runtime's walk would have fed to the translator (lanes: the lanes it would have
   run), as xlv_dma does it otherwise (its xl_flush included) */
void xm_walk(XmPic *p, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int lanes, uint32_t opt)
{
  if (p->lw_on && ch == 1 && lanes && p->m->fast) { lw_walk(p, chcr, madr, qwc, tadr, lanes, opt); xl_flush(); }
  else xlv_dma(p->m, ch, chcr, madr, qwc, tadr);
}

/* ---- task #180 (step 4b): a transfer's copy (xlme.h xm_copy) ---- */
static int xc_merge(XcR *r, int n, XcR *t)   /* r sorted by k0 and joined where ranges meet or overlap; t: n entries of scratch */
{
  int w, i, m, ok = 1;
  for (i = 1; i < n && ok; i++) ok = r[i - 1].k0 <= r[i].k0;
  for (w = 1; !ok && w < n; w *= 2) {   /* bottom-up merge sort */
    for (i = 0; i < n; i += 2 * w) {
      int a = i, am = i + w < n ? i + w : n, b = am, bm = i + 2 * w < n ? i + 2 * w : n, o = i;
      while (a < am && b < bm) t[o++] = r[b].k0 < r[a].k0 ? r[b++] : r[a++];
      while (a < am) t[o++] = r[a++];
      while (b < bm) t[o++] = r[b++];
    }
    memcpy(r, t, (size_t)n * sizeof *r);
  }
  for (i = 0, m = 0; i < n; i++)
    if (m && r[i].k0 <= r[m - 1].k1) { if (r[i].k1 > r[m - 1].k1) r[m - 1].k1 = r[i].k1; }
    else r[m++] = r[i];
  return m;
}
static void xc_add(XmCopy *c, int b, uint32_t k0, uint32_t k1)
{
  XcR *r = b ? c->rb : c->ra; int *n = b ? &c->nb : &c->na, cap = b ? c->cap : XC_CAPS(c);   /* (step 4d-3: ra's room) */
  if (*n && k0 <= r[*n - 1].k1 && k1 >= r[*n - 1].k0) {   /* meets the last: joined (the walk goes on mostly in order) */
    if (k0 < r[*n - 1].k0) r[*n - 1].k0 = k0;
    if (k1 > r[*n - 1].k1) r[*n - 1].k1 = k1;
    return; }
  if (*n == cap) { *n = xc_merge(r, *n, c->tmp); if (*n == cap) { c->over = 1; return; } }
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
static int xc_walk(XmCopy *c, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr)   /* ge/xlive.c walk()'s chain rules */
{
  uint32_t asp[2], nasp = 0, guard = 0;
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
int xm_plan(XmCopy *c, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int *x0, int *nx)   /* step 4b-2: xm_copy's first part */
{
  int r, na, nb, nk = 0, i, j, m; uint32_t tot = 0;
  c->na = c->nb = 0; c->over = 0;
  if ((r = xc_walk(c, chcr, madr, qwc, tadr)) || (c->over && (r = 1))) { c->fails[r]++; return r; }
  if (c->na > c->na_max) c->na_max = c->na;
  if (c->nb > c->nb_max) c->nb_max = c->nb;
  if (c->nmk > c->nmk_max) c->nmk_max = c->nmk;
  na = xc_merge(c->ra, c->na, c->tmp); nb = xc_merge(c->rb, c->nb, c->tmp);
  if (!c->mk_full && c->nmk) {   /* the marks outside the two ranges (tmp), then back in mk */
    int n0 = xc_merge(c->mk, c->nmk, c->tmp);
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
  for (i = 0; i < m; i++) tot += c->rb[i].k1 - c->rb[i].k0;
  if (c->next + m > c->ext_cap) { c->fails[2]++; return 2; }
  if (c->used + tot > c->arena_n) { c->fails[3]++; return 3; }
  *x0 = c->next; *nx = m;
  for (i = 0; i < m; i++) {
    XvExt *x = &c->ext[c->next++]; uint32_t k0 = c->rb[i].k0, n = c->rb[i].k1 - k0;
    x->a0 = k0; x->a1 = c->rb[i].k1; x->off = c->used; c->used += n;
  }
  c->copies++; c->exts += m; c->bytes += tot; if (m > c->nx_max) c->nx_max = m;
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
static uint32_t (*ts_clk)(void); static uint64_t *ts_t; static long *ts_n; static int ts_d;
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
  if (dig) p->dig_frame = gl_digest(f, p->gxl);
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
