#!/usr/bin/env python3
"""gl25: on top of gl24b (tree argv[1]).  With two copy areas the walk posted at a hook with records (H1, the end of the full step) may
run through the lite step and the next full step: the main CPU no longer waits for it at the start of the lite step's hook (H2, no
records).  It waits only at the next hook WITH records, before that hook's FILL and WALK (the lchk3 check: the RAM a late walk reads
stays unchanged until then), or when its copies need the half that walk holds.
1. two_seq[2]: the walk that holds each half; two_poll(): the halves whose walk is done emptied without a wait (tail passed its seq);
   with the current half held and the other free, the step moves to the other (no copy of the step is in a held half).
2. two_wait(h): wait only for the walk that holds half h (the older walk, when two are out).
3. mq_frame: in two-area turns a hook without records does not wait; a hook with records waits for the walks before (as gl24).
4. Counters: hooks without a wait, waits for an older walk, half moves (report and turn line).
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

patch('psp/xl_psp.c', [
 ("static int two_wx; static uint32_t two_wb;   /* gl24b: the extents and arena bytes planned since the last walk posted (the walk peaks) */\n",
  "static int two_wx; static uint32_t two_wb;   /* gl24b: the extents and arena bytes planned since the last walk posted (the walk peaks) */\n"
  "static uint32_t two_seq[2]; static long two_nowait, two_older, two_move, two_t1[3];   /* gl25: the walk that holds each half; counts */\n"),
 # two_post: the seq of the walk that holds the halves
 ("  two_keep(); two_pend[two_h] = 1; if (two_spill) two_pend[two_h ^ 1] = 1;\n",
  "  two_keep(); two_pend[two_h] = 1; if (two_spill) two_pend[two_h ^ 1] = 1;\n"
  "  two_seq[two_h] = mq_walk_seq; if (two_spill) two_seq[two_h ^ 1] = mq_walk_seq;   /* gl25 */\n"),
 # two_poll / two_wait after two_post
 ("static void mq_tstart(long vb)   /* an ME turn begins",
  "static void mq_wait(uint32_t seq, int why);   /* (gl25: two_wait) */\n"
  "static void two_poll(void)   /* gl25: the halves whose walk is done emptied, without a wait; the current half held and the other free: the\n"
  "                                step moves there (a held half holds no copy of this step: its copies wait until the half is free) */\n"
  "{\n"
  "  if (!two) return;\n"
  "  for (int h = 0; h < 2; h++) if (two_pend[h] && mq_done(two_seq[h])) { two_empty(h); if (h == two_h) two_load(h); }\n"
  "  if (two_pend[two_h] && !two_pend[two_h ^ 1]) { two_load(two_h ^ 1); two_move++; }\n"
  "  if (mq_walk_due && mq_done(mq_walk_seq)) mq_walk_due = 0;\n"
  "}\n"
  "static void two_wait(int h, int why)   /* gl25: only the walk that holds half h (with two walks out, the older one for the older half) */\n"
  "{\n"
  "  if (two_seq[h] != mq_walk_seq) two_older++;\n"
  "  mq_wait(two_seq[h], why);\n"
  "  two_poll();\n"
  "}\n"
  "static void mq_tstart(long vb)   /* an ME turn begins"),
 ("  two_t0[0] = two_steps; two_t0[1] = two_spills; two_t0[2] = two_nohalf;   /* gl24 */\n",
  "  two_t0[0] = two_steps; two_t0[1] = two_spills; two_t0[2] = two_nohalf;   /* gl24 */\n"
  "  two_t1[0] = two_nowait; two_t1[1] = two_older; two_t1[2] = two_move;   /* gl25 */\n"),
 # turn line
 ("\"before %.1f ms\\n\", mq_tvb0, vb, tt,",
  "\"before %.1f ms; gl25: hooks without a wait %ld, waits for an older walk %ld, half moves %ld\\n\", mq_tvb0, vb, tt,"),
 ("          two_nohalf - two_t0[2], w[MW_HOOK]);\n",
  "          two_nohalf - two_t0[2], w[MW_HOOK], two_nowait - two_t1[0], two_older - two_t1[1], two_move - two_t1[2]);\n"),
 # mq_chk_end: the current half's walk only
 ("  if (!two || two_pend[two_h]) mq_arena_wait();   /* (no plan while the ME walks the records before; gl24: in the half it walks) */\n",
  "  if (!two) mq_arena_wait();   /* (no plan while the ME walks the records before; gl24: in the half it walks) */\n"
  "  else { two_poll(); if (two_pend[two_h]) two_wait(two_h, MW_ARENA); }   /* gl25: only the walk that holds this half */\n"),
 # spill: the other half's walk only
 ("    if (two_pend[two_h ^ 1]) { mq_arena_wait(); if (mq_tbk_cold != tb || !mp_nrec) return; }\n",
  "    two_poll(); if (two_pend[two_h ^ 1]) { two_wait(two_h ^ 1, MW_ARENA); if (mq_tbk_cold != tb || !mp_nrec) return; }   /* gl25 */\n"),
 # mq_frame: no wait at a hook without records
 ("  mq_arena_waitw(two ? MW_HOOK : MW_ARENA);   /* gl24: with two areas the walk before ran while this step's game code did */\n",
  "  if (!two) mq_arena_wait();\n"
  "  else if (mp_nrec) mq_arena_waitw(MW_HOOK);   /* gl24: with two areas the walk before ran while this step's game code did; gl25: only a\n"
  "                                                 hook with records waits for it, before its FILL and WALK (lchk3: the RAM a late walk reads\n"
  "                                                 is unchanged until the next hook with records) */\n"
  "  else { two_poll(); two_nowait++; }   /* gl25: a hook without records (the lite step's): no wait */\n"),
 # report
 ("\"before %.1f s\\n\", two_steps, two_spills, two_nohalf, two_full_n, mq_wus[MW_HOOK] / 1e6);\n",
  "\"before %.1f s; gl25: hooks without a wait %ld, waits for an older walk %ld, half moves %ld\\n\", two_steps, two_spills, two_nohalf,\n"
  "                       two_full_n, mq_wus[MW_HOOK] / 1e6, two_nowait, two_older, two_move);\n"),
])
print('gl25 applied to', T)
