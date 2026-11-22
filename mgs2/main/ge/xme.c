/* xme.c: the ME's loop (task #180 step 4c; xme.h).  The records' copies and walks are psp/xl_psp.c's 4b-2 (mp_fill_all, mp_late), the
   frame's end, list and start its xl_psp_frame and gl_list without the GE and the display, moved here so that one loop serves the ME
   (4d), its thread on the emulator (4c) and the main CPU's own turns.  Step 4d: the cache steps through the host's hook (xme.h). */
#include <string.h>
#include "xme.h"
#include "me_rt.h"                      /* -DMGS2_ME_SAFE: ME_CALLP */

static void xme_sum(XmeCtx *x)          /* psp/xl_psp.c sum(): the translator's 32-bit counters summed a walk at a time */
{
  const XlStats *S = xl_stats(); uint32_t q = (uint32_t)x->m->qw, w = (uint32_t)S->words, c = (uint32_t)S->calls, a = (uint32_t)S->ad_kicks;
  x->t_qw += q - x->l_qw; x->l_qw = q; x->t_words += w - x->l_words; x->l_words = w;
  x->t_calls += c - x->l_calls; x->l_calls = c; x->t_ad += a - x->l_ad; x->l_ad = a;
}
static void xme_cache(XmeCtx *x, int op, const void *p, uint32_t n)   /* step 4d (4d-4: counted) */
{
  if (!x->cache) return;
  if (op == XME_C_ALL) x->n_call++; else { x->n_cinv++; x->n_cinv_b += n; }
  ME_CALLP(x->cache)(x, op, p, n);
}
static void xme_rec_inv(XmeCtx *x, const XmeRec *r)    /* step 4d: before the copy, the record's extents (the main CPU's plan wrote
                                                          them) and their bytes in RAM and the scratchpad (xm_fill's sources) out of the
                                                          ME's cache, by range, or the whole cache at XME_C_BIG or more */
{
  const XvExt *e = x->fc.ext + r->x0; uint32_t n = (uint32_t)r->nx * (uint32_t)sizeof *e; int i;
  for (i = 0; i < r->nx && n < XME_C_BIG; i++) n += e[i].a1 - e[i].a0;
  if (n >= XME_C_BIG) { xme_cache(x, XME_C_ALL, 0, 0); return; }
  xme_cache(x, XME_C_INV, e, (uint32_t)r->nx * (uint32_t)sizeof *e);
  for (i = 0; i < r->nx; i++)
    xme_cache(x, XME_C_INV, e[i].a0 >= XV_SPR ? x->fc.spr + (e[i].a0 - XV_SPR) : x->fc.mem + e[i].a0, e[i].a1 - e[i].a0);
}
static void xme_rec(XmeCtx *x, const XmeRec *e)
{
  XmeRec *r;
  x->n_rec++;
  if (x->nrec == XME_NREC) { x->n_over++; return; }   /* (the main CPU walks at 64: not expected) */
  r = &x->rec[x->nrec++]; *r = *e;
  if (r->nx < 0) { x->n_ram++; return; }               /* walked from RAM */
  if (x->cache) xme_rec_inv(x, r);
  xm_fill(&x->fc, r->x0, r->nx, 0, 0, 0);              /* the copy from RAM as it is now (the log comes at the FILL) */
  if (r->now) x->n_now++; else x->n_early++;
}
static void xme_fill(XmeCtx *x, int nlog)              /* the log over the early copies since the last FILL: they hold the bytes as their
                                                          transfers started */
{
  if (nlog > 0) xme_cache(x, XME_C_INV, x->log, (uint32_t)nlog * (uint32_t)sizeof *x->log);   /* step 4d: the main CPU's log entries */
  for (int i = 0; i < x->nrec; i++) {
    XmeRec *r = &x->rec[i];
    if (r->nx < 0 || r->now) continue;
    XS(x)->at = XME_AT_LAY + (uint32_t)i; xm_lay(&x->fc, r->x0, r->nx, r->t, x->log, nlog); r->now = 1; x->n_lay++;
  }
  x->n_fill++;
}
static void xme_walk(XmeCtx *x, XmeWait w)             /* psp/xl_psp.c 4b-2 mp_late's walks */
{
  XmPic *p = x->p; XlvMem *m = x->m; XmView *v = x->v;
  xm_tsplit(!x->ws_on ? 0 : w == x->w_me ? x->clk_me : w && w == x->w_thr ? x->clk : 0, x->t_ws, x->n_ws);   /* gl26: the split's clock, this
                                                                                                    CPU's (0: none; gl27: none unless ws_on) */
  xme_cache(x, XME_C_ALL, 0, 0);                       /* step 4d: the walk reads image data left in RAM and records walked from RAM */
  for (int i = 0; i < x->nrec; i++) {
    const XmeRec *r = &x->rec[i];
    XS(x)->at = XME_AT_WALK + (uint32_t)i;
    if (r->restart) { xme_sum(x); gu_zero_empty(p->map, 0); xl_reset(); x->l_words = x->l_calls = x->l_ad = 0; x->n_restart++; }
    xm_switch(p, r->opt, r->opt2);
    if (r->nx >= 0) { v->x = x->fc.ext + r->x0; v->n = r->nx; v->last = 0; m->view = v; }
    xm_walk(p, r->ch, r->chcr, r->madr, r->qwc, r->tadr, r->lanes, r->opt);
    m->view = 0; xme_sum(x); XS(x)->heart++; x->n_walked++;
  }
  xm_tsplit(0, 0, 0);   /* gl26: no clock outside a walk */
  x->nrec = 0; x->n_walk++;
}
static uint32_t xme_released(const XmeCtx *x) { return XS(x)->ge_done; }
/* step 4d-2: the picture heap's hooks on the ME (me_rt.c mh_grow, mh_give_back, mh_resize call them when nothing fits): the request goes
   to the main CPU through the ctx and the ME waits for the answer (the host's wait, why 3; the heartbeat moves; the place word stays the
   entry's), served at the main CPU's waits, transfers and hooks (psp/xl_psp.c mq_heap_serve).  The ME's cache is written back and
   dropped first: a resize may copy the region's bytes, a give-back frees it. */
static XmeCtx *xme_x; static XmeWait xme_w;   /* the loop's, set by the host before the loop starts (xme_bind; step 4d-4: the ME writes
                                                  none of its turn-owned memory outside its turns) */
void xme_bind(XmeCtx *x, XmeWait w) { xme_x = x; xme_w = w; }
static uintptr_t xme_rq(int op, void *p, uint32_t n, uint32_t *got)
{
  XmeCtx *x = xme_x; volatile XmeSh *s = XS(x); uint32_t q;
  xme_cache(x, XME_C_ALL, 0, 0);
  s->rq_op = op; s->rq_p = p; s->rq_n = n; s->rq_got = 0; s->rq_r = 0;
  XME_BARRIER(); q = s->rq_seq + 1; s->rq_seq = q; x->n_rq++;
  while (s->rq_done != q) { s->heart++; ME_CALLP(xme_w)(x, 3); }
  XME_BARRIER();
  if (got) *got = s->rq_got;
  return (uintptr_t)s->rq_r;
}
static void *xme_more(size_t want, size_t *got) { uint32_t g = 0; void *r = (void *)xme_rq(XME_RQ_MORE, 0, (uint32_t)want, &g); *got = g; return r; }
static void xme_less(void *r) { (void)xme_rq(XME_RQ_LESS, r, 0, 0); }
static void *xme_resize(void *r, size_t want) { return (void *)xme_rq(XME_RQ_RESIZE, r, (uint32_t)want, 0); }
void xme_heap_ask(int resize) { me_heap_grow(xme_more, xme_less); me_heap_resize(resize ? xme_resize : 0); }
/* gl21: the clock of the CPU that runs this call: the ME runs the loop with its own wait (x->w_me) and reads its Count; the main CPU (its
   thread or itself) the system's microseconds.  Chosen by the wait each call carries, never by a word the other CPU switches */
#define XME_CLK(x, w) ((w) == (x)->w_me ? ME_CALLP((x)->clk_me)() : ME_CALLP((x)->clk)())
static void xme_hook(XmeCtx *x, const XmeHook *h, XmeWait w)
{
  XmPic *p = x->p;
  x->n_hook++;
  if (h->stop) { p->now = 0; return; }                 /* the GE stopped: no more frames */
  if (h->end) {
    XmeRes *s; int r;
    uint32_t c0 = XME_CLK(x, w), c1;   /* gl20: the waits and the end's own time apart */
    while ((int32_t)(XS(x)->listed - xme_released(x)) >= (int32_t)XME_NRES) { x->n_resfull++; XS(x)->heart++; XS(x)->at = XME_AT_RESFULL; ME_CALLP(w)(x, 1); }
    c1 = XME_CLK(x, w); x->t_k[XME_T_GEW] += c1 - c0;
    XS(x)->at = XME_AT_END; xm_end(p); XS(x)->at = XME_AT_LIST; r = xm_list(p, h->d, h->dig);
    x->t_k[XME_T_LIST] += XME_CLK(x, w) - c1;
    s = &x->res[XS(x)->listed % XME_NRES];
    s->fvb = h->vb - 1; s->d = h->d; s->r = r; s->nt = (int)p->glf->o.nt; s->dig = h->dig; s->list = p->gxl->w;
    s->dig_list = p->gxl->dig; s->dig_frame = h->dig ? p->dig_frame : 0; s->c = p->gxl->c;
    xme_cache(x, XME_C_ALL, 0, 0);                     /* step 4d: the GE reads the list, the frame memory and the textures from RAM */
    XME_BARRIER(); XS(x)->listed++; x->n_list++;
  }
  if (h->begin) {   /* the GE must have finished every list before: the frame's arena, the list buffer, the cache's last textures */
    uint32_t c0 = XME_CLK(x, w); int two = XS(x)->ge2 && x->fm[1];   /* gl23: or every list but the last, with two frame memories */
    while ((int32_t)(XS(x)->listed - xme_released(x)) > two) { x->n_gewait++; XS(x)->heart++; XS(x)->at = XME_AT_GEWAIT; ME_CALLP(w)(x, 1); }
    x->t_k[XME_T_GEW] += XME_CLK(x, w) - c0;
    if (two) {   /* gl23: the other frame memory and list; the textures of the frame just ended kept while the GE may draw it */
      if ((int32_t)(XS(x)->listed - xme_released(x)) > 0) x->n_ge2++;
      tk_guard(p->tk, x->tk_fb);
      x->fm_k ^= 1; gb_arena(&p->glf->o, x->fm[x->fm_k], x->fm_arena, x->fm_nb, x->fm_nv); p->gxl = x->fl[x->fm_k];
    } else tk_guard(p->tk, TK_NO_GUARD);
    x->tk_fb = tk_now(p->tk);
    XS(x)->at = XME_AT_BEGIN; xm_begin(p, h->opt, h->opt2); x->n_begin++;
  }
  XS(x)->at = XME_AT_TKFRAME; tk_frame(p->tk);
}
int xme_entry(XmeCtx *x, const XmeE *e, XmeWait w)
{
  int k = e->kind, ret = 0;   /* (no switch: -fno-jump-tables keeps it a chain anyway) */
  XS(x)->at = (uint32_t)k;
  if (k == XE_REC) xme_rec(x, &e->r);
  else if (k == XE_FILL) xme_fill(x, e->nlog);
  else if (k == XE_WALK) xme_walk(x, w);
  else if (k == XE_HOOK) xme_hook(x, &e->h, w);
  else if (k == XE_THIN) {   /* step 4d-3: the slot as the main CPU wrote it (it writes it back before the post) */
    xme_cache(x, XME_C_INV, x->thin_r, (uint32_t)((const char *)(x->thin_d + 512) - (const char *)x->thin_r));
    gl_thin(x->p->glf, x->thin_r, x->thin_n, x->thin_keep3, x->thin_d, x->thin_nd); x->n_thin++; }
  else if (k == XE_SYNC) x->n_sync++;   /* step 4d: the main CPU makes the picture next (gl24b: the whole cache written back in xme_run,
                                           after this count and the entry's time: no line of the ME's own left dirty through the main
                                           CPU's turn) */
  else if (k == XE_STOP) ret = 1;
  return ret;   /* (the place word goes back to 0 in xme_run, after the tail: a take-back that sees 0 sees every entry before whole) */
}
void xme_run(XmeCtx *x, XmeWait w)
{
  volatile XmeSh *s = XS(x);   /* step 4d-4: the shared words through the uncached alias (xme_bind set the heap's requests' ctx and wait) */
  for (;;) {
    uint32_t t = s->tail; const XmeE *e;
    if (t == s->head) {   /* empty: idle until the main CPU posts (the thread: it signals when it sees idle, and the wait has a time limit;
                             the ME: its wait looks at head every 5 us); only the shared words are written while idle */
      uint32_t c0 = XME_CLK(x, w);   /* gl20 */
      s->heart++; s->idle = 1; s->at = XME_AT_IDLE;
      if (s->tail == s->head) ME_CALLP(w)(x, 0);
      s->at = 0; s->idle = 0;
      { uint64_t v = ((uint64_t)s->t_idle_hi << 32 | s->t_idle_lo) + (uint32_t)(XME_CLK(x, w) - c0);   /* gl24: the shared words only */
        s->t_idle_q++; XME_BARRIER(); s->t_idle_lo = (uint32_t)v; s->t_idle_hi = (uint32_t)(v >> 32); XME_BARRIER(); s->t_idle_q++; }
      continue; }
    XME_BARRIER(); e = &x->e[t & (XME_N - 1)];
    xme_cache(x, XME_C_INV, e, sizeof *e);   /* step 4d-3: the entry as the main CPU wrote it (written back before head moved) */
    if (e->kind == XE_HOOK && x->hang_vb >= 0 && e->h.vb >= x->hang_vb) {   /* the hang test: between entries (warm), or inside the next
                                                                               walk (hang_in: cold) */
      if (x->hang_in) { x->hang_vb = -1; x->hang_arm = 1; }
      else { xme_cache(x, XME_C_ALL, 0, 0); s->hung = 1; s->at = XME_AT_HANG; for (;;) ME_CALLP(w)(x, 2); } }
    if (e->kind == XE_WALK && x->hang_arm && x->nrec > 0) {   /* hang_in's stop: at the walk's first record, its cache not written back */
      x->hang_arm = 0; s->hung = 1; s->at = XME_AT_WALK; for (;;) ME_CALLP(w)(x, 2); }
    { uint32_t k = (uint32_t)e->kind & 7u, c0 = XME_CLK(x, w); int st = xme_entry(x, e, w);   /* gl20: the entry's own time */
      x->t_k[k] += XME_CLK(x, w) - c0;
      if (k == XE_SYNC || k == XE_STOP) xme_cache(x, XME_C_ALL, 0, 0);   /* gl24b: the hand-over's write-back the ME's last cached write */
      if (st) { XME_BARRIER(); s->tail = t + 1; XME_BARRIER(); s->at = 0; s->stopped = 1; return; } }
    XME_BARRIER(); s->tail = t + 1; XME_BARRIER(); s->at = 0; s->heart++;
  }
}
