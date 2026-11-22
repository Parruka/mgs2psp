# gl24: two copy areas (MGS2_ME_PIC_TWO) and the ME's idle time in the shared words (the turn lines' counts).  On gl23's sources.
# Usage: python3 -I gl24_apply.py <tree>
import sys, hashlib
t = sys.argv[1]
def load(p, want):
    s = open(t + '/' + p).read(); h = hashlib.md5(s.encode()).hexdigest()
    assert h.startswith(want), (p, h); return s
def R(s, old, new):
    assert s.count(old) == 1, (s.count(old), old[:90]); return s.replace(old, new)

# ---- ge/xme.h: the idle time in the shared words ----
h = load('ge/xme.h', '0d19b28a')
h = R(h, r"""  uint32_t me_ctl;            /* step 4d-4: the real ME's control block (psp/xme_me.h XmeMeCtl, its uncached address: its waits move
                                 hle_me_start's heartbeat too and see an exit request); 0: the loop is the emulator's thread */
} XmeSh;""", r"""  uint32_t me_ctl;            /* step 4d-4: the real ME's control block (psp/xme_me.h XmeMeCtl, its uncached address: its waits move
                                 hle_me_start's heartbeat too and see an exit request); 0: the loop is the emulator's thread */
  uint32_t t_idle_q, t_idle_lo, t_idle_hi;   /* gl24: the loop's idle waits' time in its clock's ticks, written only by the loop (q odd while
                                 it writes them).  gl20-23 kept it in t_k[XME_T_IDLE]: the ME's idle waits run in the main CPU's turns too,
                                 and that cached write brought the line of the ME's own counts into its cache while the main CPU counted
                                 in it, so the ME's next turn went on from its old counts (gl23's turn lines: hooks 0, lists < 0) */
} XmeSh;""")
h = R(h, r"""  uint64_t t_k[XME_T_N];      /* gl20: the loop's own time in clk's ticks: XME_T_IDLE its idle waits, 1-7 each entry kind's whole time""",
         r"""  uint64_t t_k[XME_T_N];      /* gl20: the loop's own time in clk's ticks: XME_T_IDLE unused (gl24: XmeSh t_idle), 1-7 each entry kind's whole time""")
open(t + '/ge/xme.h', 'w').write(h)

# ---- ge/xme.c: the idle wait writes only the shared words ----
c = load('ge/xme.c', 'db696d94')
c = R(c, r"""      s->at = 0; s->idle = 0; x->t_k[XME_T_IDLE] += XME_CLK(x, w) - c0; continue; }""",
         r"""      s->at = 0; s->idle = 0;
      { uint64_t v = ((uint64_t)s->t_idle_hi << 32 | s->t_idle_lo) + (uint32_t)(XME_CLK(x, w) - c0);   /* gl24: the shared words only */
        s->t_idle_q++; XME_BARRIER(); s->t_idle_lo = (uint32_t)v; s->t_idle_hi = (uint32_t)(v >> 32); XME_BARRIER(); s->t_idle_q++; }
      continue; }""")
open(t + '/ge/xme.c', 'w').write(c)

# ---- psp/xl_psp.c ----
s = load('psp/xl_psp.c', '168e807d')
# the wait kinds: the hook's wait for the walk before (gl24)
s = R(s, r"""enum { MW_NOW, MW_FILL, MW_ARENA, MW_RAM, MW_RING, MW_THIN, MW_DRAIN, MW_PROBE, MW_SYNC, MW_N };""",
         r"""enum { MW_NOW, MW_FILL, MW_ARENA, MW_RAM, MW_RING, MW_THIN, MW_DRAIN, MW_PROBE, MW_SYNC, MW_HOOK, MW_N };""")
s = R(s, r""""a heartbeat (a hook: entries waiting, none since the hook before)", "a hand-over to the main CPU (SYNC)" };""",
         r""""a heartbeat (a hook: entries waiting, none since the hook before)", "a hand-over to the main CPU (SYNC)",
                                            "the walk before at a hook (gl24: two copy areas)" };""")
# state and the halves' functions, before mq_tstart
s = R(s, r"""static void mq_tstart(long vb)   /* an ME turn begins (the main CPU's cache holds the counts as they are) */
{""", r"""/* gl24: two copy areas (MGS2_ME_PIC_TWO: 0 off, 1 in every ME turn, 2 in every other ME turn: off, on, ...).  The arena and the extents
   in two halves: a step's copies go to one half while the ME walks the step before from the other, so the step's first copy plan no
   longer waits for that walk; the main CPU waits for it at the next hook instead (MW_HOOK), so a walk reads RAM at the latest as the next
   hook comes (the late-read check gl24d: what the walks read from RAM outside their copies did not change by then, the boot's refill and
   one opening step apart).  A step whose copies do not fit in its half takes the other half once the walk there is done (a spill); the
   step after then waits for its walk at its first plan, as gl23.  Records keep their extents' places (x0), so the ME needs nothing new */
static int two_cfg, two, two_h, two_pend[2], two_spill, two_xh, two_xn, two_next[2]; static uint32_t two_ah, two_an, two_used[2];
static long two_steps, two_spills, two_nohalf, two_full_n, two_t0[3];
static void two_load(int h) { two_h = h; xmc.next = two_next[h]; xmc.used = two_used[h]; xmc.ext_cap = (h + 1) * two_xh; xmc.arena_n = (uint32_t)(h + 1) * two_ah; }
static void two_keep(void) { two_next[two_h] = xmc.next; two_used[two_h] = xmc.used; }
static void two_empty(int h) { two_next[h] = h * two_xh; two_used[h] = (uint32_t)h * two_ah; two_pend[h] = 0; }
static void two_start(int on)   /* every walk done (an ME turn's start, a hand-over, a take-back): two areas, or gl23's one */
{
  two = on && two_ah; two_spill = 0;
  if (two) { two_empty(0); two_empty(1); two_load(0); }
  else { two_pend[0] = two_pend[1] = 0; xmc.next = 0; xmc.used = 0; if (two_an) { xmc.ext_cap = two_xn; xmc.arena_n = two_an; } }
}
static int two_on(long vb) { return mq_turn && (two_cfg == 1 || (two_cfg == 2 && ((vb / (2 * mq_turn)) & 1))); }
static void two_done(void)   /* every walk posted is done: the halves they held empty (the step's own copies stay) */
{
  for (int h = 0; h < 2; h++) if (two_pend[h]) { two_empty(h); if (h == two_h) two_load(h); }
}
static void two_post(void)   /* a walk posted with the records so far: their halves its until it is done; the next copies in the other
                                 half when that one is empty, else (a spill before) in this one after the wait */
{
  two_keep(); two_pend[two_h] = 1; if (two_spill) two_pend[two_h ^ 1] = 1;
  two_spill = 0; two_steps++;
  if (!two_pend[two_h ^ 1]) two_load(two_h ^ 1); else two_nohalf++;
}
static void mq_tstart(long vb)   /* an ME turn begins (the main CPU's cache holds the counts as they are) */
{""")
# the idle time from the shared words
s = R(s, r"""  for (int k = 0; k < XME_T_N; k++) mq_tk0[k] = mq->t_k[k];
  for (int k = 0; k < MW_N; k++) mq_tw0[k] = mq_wus[k];""", r"""  for (int k = 0; k < XME_T_N; k++) mq_tk0[k] = k == XME_T_IDLE ? mq_idle() : mq->t_k[k];   /* gl24: the idle time from the shared words */
  for (int k = 0; k < MW_N; k++) mq_tw0[k] = mq_wus[k];
  two_t0[0] = two_steps; two_t0[1] = two_spills; two_t0[2] = two_nohalf;   /* gl24 */""")
s = R(s, r"""  for (int k = 0; k < XME_T_N; k++) { uint64_t d = mq->t_k[k] - mq_tk0[k]; mq_tkall[k] += d;""",
         r"""  for (int k = 0; k < XME_T_N; k++) { uint64_t d = (k == XME_T_IDLE ? mq_idle() : mq->t_k[k]) - mq_tk0[k]; mq_tkall[k] += d;""")
s = R(s, r"""static void mq_tend(long vb)   /* an ME turn ends, after the hand-over (the ME's cache written back, the main CPU's dropped) */""",
         r"""static uint64_t mq_idle(void)   /* gl24: the loop's idle time (XmeSh t_idle_*: q odd while the loop writes them) */
{
  volatile XmeSh *x = XS(mq); uint32_t q, l, h;
  do { q = x->t_idle_q; l = x->t_idle_lo; h = x->t_idle_hi; } while ((q & 1) || q != x->t_idle_q);
  return (uint64_t)h << 32 | l;
}
static void mq_tend(long vb)   /* an ME turn ends, after the hand-over (the ME's cache written back, the main CPU's dropped) */""")
s = R(s, r"""static uint64_t mq_tk0[XME_T_N],""", r"""static uint64_t mq_idle(void);
static uint64_t mq_tk0[XME_T_N],""")
# the turn line: gl24's numbers
s = R(s, r"""          "%ld times, mean %.2f ms; gl22: idle in this turn %.1f; GE2 %d: lists %ld, drawing %.1f ms, seen %.1f ms after its finish; begun while the "
          "GE drew the frame before %ld\n",""", r"""          "%ld times, mean %.2f ms; gl22: idle in this turn %.1f; GE2 %d: lists %ld, drawing %.1f ms, seen %.1f ms after its finish; begun while the "
          "GE drew the frame before %ld; gl24 TWO %d: walks posted %ld, spills %ld, no half free %ld, the main CPU waited at hooks for the walk "
          "before %.1f ms\n",""")
s = R(s, r"""          (double)(ge_note[ge2 ? 1 : 0] - ge_tq0) / 1000.0, mq->n_ge2 - ge2_tn0);
  mq_tvb0 = -1;""", r"""          (double)(ge_note[ge2 ? 1 : 0] - ge_tq0) / 1000.0, mq->n_ge2 - ge2_tn0, two, two_steps - two_t0[0], two_spills - two_t0[1],
          two_nohalf - two_t0[2], w[MW_HOOK]);
  mq_tvb0 = -1;""")
# the cfg
s = R(s, r"""  ge2_cfg = (e = mgs2_getenv("MGS2_ME_PIC_GE2")) ? atoi(e) : 0;   /* gl23 */""",
         r"""  ge2_cfg = (e = mgs2_getenv("MGS2_ME_PIC_GE2")) ? atoi(e) : 0;   /* gl23 */
  two_cfg = (e = mgs2_getenv("MGS2_ME_PIC_TWO")) ? atoi(e) : 0;   /* gl24: the halves (the arena's at 64 bytes) */
  two_xn = xmc.ext_cap; two_an = xmc.arena_n; two_xh = two_xn / 2; two_ah = (two_an / 2) & ~63u; two = 0;
  fprintf(stderr, "me picture copies (gl24): two copy areas (a step's copies in one half while the ME walks the step before from the other; "
          "MGS2_ME_PIC_TWO): %s; each %u KB and %d extents\n", !mq_turn && two_cfg ? "off (no turns)" : two_cfg == 2 ? "in every other ME turn "
          "(off, on, ...)" : two_cfg == 1 ? "in every ME turn" : "off", (unsigned)(two_ah >> 10), two_xh);""")
# the arena wait: two areas keep the step's own copies
s = R(s, r"""static void mq_arena_wait(void)   /* the walks posted last are done: the arena and its extents are the main CPU's again */
{
  if (!mq_walk_due) return;
  mq_wait(mq_walk_seq, MW_ARENA);
  if (xmc.next > mq_ext_max) { mq_ext_max = xmc.next; mq_ext_max_vb = me_vb; }   /* step 4d: for 4d-3's sizes */
  if (xmc.used > mq_arena_max) { mq_arena_max = xmc.used; mq_arena_max_vb = me_vb; }
  mq_walk_due = 0; xmc.next = 0; xmc.used = 0;
}""", r"""static void mq_arena_waitw(int why)   /* the walks posted last are done: the arena and its extents are the main CPU's again (gl24: with two
                                         areas, the halves those walks held: the step's own copies stay) */
{
  if (!mq_walk_due) return;
  mq_wait(mq_walk_seq, why);
  if (xmc.next > mq_ext_max) { mq_ext_max = xmc.next; mq_ext_max_vb = me_vb; }   /* step 4d: for 4d-3's sizes */
  if (xmc.used > mq_arena_max) { mq_arena_max = xmc.used; mq_arena_max_vb = me_vb; }
  mq_walk_due = 0;
  if (two) two_done(); else { xmc.next = 0; xmc.used = 0; }
}
static void mq_arena_wait(void) { mq_arena_waitw(MW_ARENA); }""")
# take-backs: one area
s = R(s, r"""  xmc.next = 0; xmc.used = 0; mq_walk_due = 0; mq_thin_due = 0; g_g0_on = 0;""",
         r"""  xmc.next = 0; xmc.used = 0; mq_walk_due = 0; mq_thin_due = 0; g_g0_on = 0; two_start(0);   /* (gl24) */""")
s = R(s, r"""  g_g0_on = 0; g_xl_img = g_xl_end = 0; mp_nrec = 0; mp_unfilled = 0; mp_nlog = 0; mq_walk_due = 0; mq_thin_due = 0; thin_due = 0;""",
         r"""  g_g0_on = 0; g_xl_img = g_xl_end = 0; mp_nrec = 0; mp_unfilled = 0; mp_nlog = 0; mq_walk_due = 0; mq_thin_due = 0; thin_due = 0;
  two = 0;   /* (gl24) */""")
# mq_late: the walk's halves
s = R(s, r"""  mq_walk_seq = mq_post(&e); mq_walk_due = 1;
  mp_nrec = 0; mp_late_runs++; mp_chn = 0;""", r"""  mq_walk_seq = mq_post(&e); mq_walk_due = 1;
  if (two) two_post();   /* gl24 */
  mp_nrec = 0; mp_late_runs++; mp_chn = 0;""")
# mq_chk_end: no wait at the plan while the other half is walked; a spill
s = R(s, r"""  mq_arena_wait();                       /* (no plan while the ME walks the records before) */
  if (mq_tbk_cold != tb || !mp_nrec) return;   /* (a take-back: the record dropped) */
  e = &mp_rec[mp_nrec - 1];
  r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx);
  if ((r == 2 || r == 3) && mp_nrec > 1) {   /* no room: the records before laid and walked now, the copy planned again */
    MpRec keep = *e; mp_drain_room++; mp_nrec--; mq_late(3, 1);
    if (mq_tbk_cold != tb) return;
    mp_rec[0] = keep; mp_nrec = 1; e = &mp_rec[0];
    r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx);
  }""", r"""  if (!two || two_pend[two_h]) mq_arena_wait();   /* (no plan while the ME walks the records before; gl24: in the half it walks) */
  if (mq_tbk_cold != tb || !mp_nrec) return;   /* (a take-back: the record dropped) */
  e = &mp_rec[mp_nrec - 1];
  r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx);
  if (two && (r == 2 || r == 3) && !two_spill) {   /* gl24: the half full: the other half, once the walk there is done (a spill) */
    if (two_pend[two_h ^ 1]) { mq_arena_wait(); if (mq_tbk_cold != tb || !mp_nrec) return; }
    if (two) { xmc.fails[r]--; two_keep(); two_load(two_h ^ 1); two_spill = 1; two_spills++;
      r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx); }
  }
  if ((r == 2 || r == 3) && mp_nrec > 1) {   /* no room: the records before laid and walked now, the copy planned again */
    MpRec keep = *e; mp_drain_room++; mp_nrec--; mq_late(3, 1);
    if (mq_tbk_cold != tb) return;
    if (two) { two_h = 0; xmc.next = 0; xmc.used = 0; xmc.ext_cap = two_xn; xmc.arena_n = two_an; two_spill = 1; two_full_n++; }   /* gl24:
                                                                   every walk done: the whole arena for the rest of the step */
    mp_rec[0] = keep; mp_nrec = 1; e = &mp_rec[0];
    r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx);
  }""")
# the hook: the walk before waited for as MW_HOOK
s = R(s, r"""  mq_probe();
  mq_arena_wait();
  if (!mq_m4 && mq_turn""", r"""  mq_probe();
  mq_arena_waitw(two ? MW_HOOK : MW_ARENA);   /* gl24: with two areas the walk before ran while this step's game code did */
  if (!mq_m4 && mq_turn""")
# the turns
s = R(s, r"""    XS(mq)->ge2 = 0;   /* gl23: the main CPU's turns: one frame on the GE side (the ME has handed over: it reads nothing until the next turn) */
    mq->cache = 0; mq->clk = mq_clk_us; mq_heap_own(); mq_self = 1; mq_tend(vb); }""",
         r"""    XS(mq)->ge2 = 0;   /* gl23: the main CPU's turns: one frame on the GE side (the ME has handed over: it reads nothing until the next turn) */
    mq->cache = 0; mq->clk = mq_clk_us; mq_heap_own(); mq_self = 1; mq_tend(vb); two_start(0); }""")
s = R(s, r"""    XS(mq)->ge2 = (uint32_t)ge2;
    xme_heap_ask(me_rs_on); mq_self = 0; mq_tstart(vb); }""", r"""    XS(mq)->ge2 = (uint32_t)ge2;
    xme_heap_ask(me_rs_on); mq_self = 0; mq_tstart(vb); two_start(two_on(vb)); }   /* gl24: two areas or one, by ME turn */""")
s = R(s, r"""      if (!mq_self) { xme_heap_ask(me_rs_on); mq_tstart(me_vb); }
      fprintf(stderr, "me picture loop (task #180 step 4d-4): on:""", r"""      if (!mq_self) { xme_heap_ask(me_rs_on); mq_tstart(me_vb); two_start(two_on(me_vb)); }
      fprintf(stderr, "me picture loop (task #180 step 4d-4): on:""")
s = R(s, r"""  if (mq_m4) { mq->cache = mq_self ? 0 : mq_cache_thr; if (!mq_self) { xme_heap_ask(me_rs_on); mq_tstart(me_vb); } }""",
         r"""  if (mq_m4) { mq->cache = mq_self ? 0 : mq_cache_thr; if (!mq_self) { xme_heap_ask(me_rs_on); mq_tstart(me_vb); two_start(two_on(me_vb)); } }""")
# the run's summary
s = R(s, r"""static void mq_report(FILE *f)
{""", r"""static void mq_report(FILE *f)
{
  if (two_cfg) fprintf(f, "me picture copies (gl24): two copy areas: walks posted %ld, spills into the other half %ld, steps with no half free "
                       "after their walk %ld, steps given the whole arena after a walk at once %ld; the main CPU waited at hooks for the walk "
                       "before %.1f s\n", two_steps, two_spills, two_nohalf, two_full_n, mq_wus[MW_HOOK] / 1e6);""")
open(t + '/psp/xl_psp.c', 'w').write(s)
print('gl24 ok')
