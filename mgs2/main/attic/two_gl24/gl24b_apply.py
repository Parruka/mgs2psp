#!/usr/bin/env python3
"""gl24b: fixes from the gl24 review, applied on top of gl24_apply.py's output (tree given as argv[1]).
1. mq_idle: the seqlock read yields and is bounded (the loop thread at a lower priority could never finish its write).
2. ge/xme.c: the hand-over's (SYNC, STOP) whole-cache write-back after the entry's own counts and time, so no ME-private line
   stays dirty in the ME's cache through the main CPU's turn (the real cause of gl23's stale turn lines: n_sync++ after it).
3. mq_chk_end: a lone record larger than a half gets the whole arena (every walk is done) instead of a walk from RAM.
4. Warm take-back (mode 4): one area again (two_start(0)).
5. The walk peaks (a walk's extents and arena bytes) measured per walk with two areas; the report prints the whole arena's sizes.
"""
import sys, os
T = sys.argv[1]

def patch(rel, pairs):
    p = os.path.join(T, rel); s = open(p).read()
    for old, new in pairs:
        n = s.count(old)
        if n != 1: sys.exit('%s: %d matches for %r' % (rel, n, old[:80]))
        s = s.replace(old, new, 1)
    open(p, 'w').write(s)

patch('ge/xme.c', [
 ("  else if (k == XE_SYNC) { xme_cache(x, XME_C_ALL, 0, 0); x->n_sync++; }   /* step 4d: the main CPU makes the picture next */\n"
  "  else if (k == XE_STOP) { xme_cache(x, XME_C_ALL, 0, 0); ret = 1; }\n",
  "  else if (k == XE_SYNC) x->n_sync++;   /* step 4d: the main CPU makes the picture next (gl24b: the whole cache written back in xme_run,\n"
  "                                           after this count and the entry's time: no line of the ME's own left dirty through the main\n"
  "                                           CPU's turn) */\n"
  "  else if (k == XE_STOP) ret = 1;\n"),
 ("      x->t_k[k] += XME_CLK(x, w) - c0;\n",
  "      x->t_k[k] += XME_CLK(x, w) - c0;\n"
  "      if (k == XE_SYNC || k == XE_STOP) xme_cache(x, XME_C_ALL, 0, 0);   /* gl24b: the hand-over's write-back the ME's last cached write */\n"),
])

patch('ge/xme.h', [
 ("written only by the loop (q odd while\n"
  "                                 it writes them).  gl20-23 kept it in t_k[XME_T_IDLE]: the ME's idle waits run in the main CPU's turns too,\n"
  "                                 and that cached write brought the line of the ME's own counts into its cache while the main CPU counted\n"
  "                                 in it, so the ME's next turn went on from its old counts (gl23's turn lines: hooks 0, lists < 0) */",
  "written only by the loop (q odd while\n"
  "                                 it writes them).  gl20-23 kept it in t_k[XME_T_IDLE], a cached write in the main CPU's turns (its line\n"
  "                                 holds tk_fb and fm_k).  gl23's stale turn lines (hooks 0, lists < 0) came from n_sync++ and t_k[XE_SYNC]\n"
  "                                 written after the hand-over's write-back: gl24b writes the cache back after them (ge/xme.c xme_run) */"),
])

patch('psp/xl_psp.c', [
 # 5: per-walk peaks with two areas
 ("static long two_steps, two_spills, two_nohalf, two_full_n, two_t0[3];\n",
  "static long two_steps, two_spills, two_nohalf, two_full_n, two_t0[3];\n"
  "static int two_wx; static uint32_t two_wb;   /* gl24b: the extents and arena bytes planned since the last walk posted (the walk peaks) */\n"),
 ("  two = on && two_ah; two_spill = 0;\n",
  "  two = on && two_ah; two_spill = 0; two_wx = 0; two_wb = 0;\n"),
 ("  two_keep(); two_pend[two_h] = 1; if (two_spill) two_pend[two_h ^ 1] = 1;\n",
  "  if (two_wx > mq_ext_max) { mq_ext_max = two_wx; mq_ext_max_vb = me_vb; }   /* gl24b: this walk's extents and bytes */\n"
  "  if (two_wb > mq_arena_max) { mq_arena_max = two_wb; mq_arena_max_vb = me_vb; }\n"
  "  two_wx = 0; two_wb = 0;\n"
  "  two_keep(); two_pend[two_h] = 1; if (two_spill) two_pend[two_h ^ 1] = 1;\n"),
 ("  if (xmc.next > mq_ext_max) { mq_ext_max = xmc.next; mq_ext_max_vb = me_vb; }   /* step 4d: for 4d-3's sizes */\n"
  "  if (xmc.used > mq_arena_max) { mq_arena_max = xmc.used; mq_arena_max_vb = me_vb; }\n",
  "  if (!two && xmc.next > mq_ext_max) { mq_ext_max = xmc.next; mq_ext_max_vb = me_vb; }   /* step 4d: for 4d-3's sizes (gl24b: one area only;\n"
  "                                                                                 two areas: two_post) */\n"
  "  if (!two && xmc.used > mq_arena_max) { mq_arena_max = xmc.used; mq_arena_max_vb = me_vb; }\n"),
 # 1: mq_idle
 ("  volatile XmeSh *x = XS(mq); uint32_t q, l, h;\n"
  "  do { q = x->t_idle_q; l = x->t_idle_lo; h = x->t_idle_hi; } while ((q & 1) || q != x->t_idle_q);\n"
  "  return (uint64_t)h << 32 | l;\n",
  "  volatile XmeSh *x = XS(mq); uint32_t q, l, h; int n = 0;\n"
  "  for (;;) {   /* gl24b: yield while q is odd (the emulator's loop thread runs below the main thread: it finishes its write only while\n"
  "                  the main thread sleeps), and give up after about 20 ms (a loop stopped inside its write: the time is only a count) */\n"
  "    q = x->t_idle_q; XME_BARRIER(); l = x->t_idle_lo; h = x->t_idle_hi; XME_BARRIER();\n"
  "    if (!(q & 1) && q == x->t_idle_q) break;\n"
  "    if (++n > 4) sceKernelDelayThread(10);\n"
  "    if (n > 2000) break;\n"
  "  }\n"
  "  return (uint64_t)h << 32 | l;\n"),
 # 4: warm take-back
 ("  while (XS(mq)->tail != XS(mq)->head) { uint32_t t = XS(mq)->tail; xme_entry(mq, &mq->e[t & (XME_N - 1)], mq_wait_self); XS(mq)->tail = t + 1; mq_selfrun++; n++; }\n"
  "  fprintf(stderr, \"me picture loop: TAKE-BACK at vb %ld (warm, mode 4)",
  "  while (XS(mq)->tail != XS(mq)->head) { uint32_t t = XS(mq)->tail; xme_entry(mq, &mq->e[t & (XME_N - 1)], mq_wait_self); XS(mq)->tail = t + 1; mq_selfrun++; n++; }\n"
  "  two_start(0);   /* gl24b: every walk done, the main CPU's own walks from RAM from now on: one area (the counts stay the patch's) */\n"
  "  fprintf(stderr, \"me picture loop: TAKE-BACK at vb %ld (warm, mode 4)"),
 # 5: the plans through mq_plan (three in mq_chk_end)
 ("static void mq_chk_end(void)   /* xlive_chk_end in mode 3 */\n",
  "static int mq_plan(const MpRec *e, int *x0, int *nx)   /* gl24b: xm_plan, counting a walk's extents and arena bytes with two areas */\n"
  "{\n"
  "  uint32_t u0 = xmc.used; int r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, x0, nx);\n"
  "  if (two && !r) { two_wx += *nx; two_wb += xmc.used - u0; }\n"
  "  return r;\n"
  "}\n"
  "static void mq_chk_end(void)   /* xlive_chk_end in mode 3 */\n"),
 ("  e = &mp_rec[mp_nrec - 1];\n"
  "  r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx);\n"
  "  if (two && (r == 2 || r == 3) && !two_spill) {",
  "  e = &mp_rec[mp_nrec - 1];\n"
  "  r = mq_plan(e, &x0, &nx);\n"
  "  if (two && (r == 2 || r == 3) && !two_spill) {"),
 ("      r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx); }\n"
  "  }\n",
  "      r = mq_plan(e, &x0, &nx); }\n"
  "  }\n"
  # 3: the lone record
  "  if (two && (r == 2 || r == 3) && mp_nrec == 1 && !two_pend[0] && !two_pend[1]) {   /* gl24b: one record larger than a half, every walk\n"
  "                                                                                   done: the whole arena (gl23 copied it there too) */\n"
  "    xmc.fails[r]--; two_h = 0; xmc.next = 0; xmc.used = 0; xmc.ext_cap = two_xn; xmc.arena_n = two_an; two_spill = 1; two_full_n++;\n"
  "    r = mq_plan(e, &x0, &nx);\n"
  "  }\n"),
 ("    mp_rec[0] = keep; mp_nrec = 1; e = &mp_rec[0];\n"
  "    r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx);\n",
  "    mp_rec[0] = keep; mp_nrec = 1; e = &mp_rec[0];\n"
  "    r = mq_plan(e, &x0, &nx);\n"),
 # 5: the report's sizes: the whole arena
 ("mq_ext_max, mq_ext_max_vb, xmc.ext_cap, (unsigned)(mq_arena_max >> 10),\n"
  "            mq_arena_max_vb, (unsigned)(xmc.arena_n >> 10),",
  "mq_ext_max, mq_ext_max_vb, two_an ? two_xn : xmc.ext_cap, (unsigned)(mq_arena_max >> 10),\n"
  "            mq_arena_max_vb, (unsigned)((two_an ? two_an : xmc.arena_n) >> 10),"),
])
print('gl24b applied to', T)
