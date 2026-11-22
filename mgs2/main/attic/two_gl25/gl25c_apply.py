#!/usr/bin/env python3
"""gl25c: on top of gl25b (tree argv[1]); the review's two counter findings (gl25 review workflow wf_bf60e660-a50):
1. two_poll reads the queue's tail once for the halves and mq_walk_due (a walk ending between two reads left walk_due 0 with a half
   still marked held, so the next records hook skipped two_done: 'no half free' and 'half moves' over-counted).
2. two_post polls first (a FILL or copy wait that already passed the older walk frees its half before the new walk takes one).
3. mq_arena_waitw with no walk due still polls the halves (two areas).
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
 ("  if (!two) return;\n"
  "  for (int h = 0; h < 2; h++) if (two_pend[h] && mq_done(two_seq[h])) { two_empty(h); if (h == two_h) two_load(h); }\n"
  "  if (two_pend[two_h] && !two_pend[two_h ^ 1]) { two_load(two_h ^ 1); two_move++; }\n"
  "  if (mq_walk_due && mq_done(mq_walk_seq)) mq_walk_due = 0;\n",
  "  uint32_t t;\n"
  "  if (!two) return;\n"
  "  t = XS(mq)->tail;   /* gl25c: one read for the halves and walk_due */\n"
  "  for (int h = 0; h < 2; h++) if (two_pend[h] && (int32_t)(t - two_seq[h]) > 0) { two_empty(h); if (h == two_h) two_load(h); }\n"
  "  if (two_pend[two_h] && !two_pend[two_h ^ 1]) { two_load(two_h ^ 1); two_move++; }\n"
  "  if (mq_walk_due && (int32_t)(t - mq_walk_seq) > 0) mq_walk_due = 0;\n"),
 ("  if (two_wx > mq_ext_max) { mq_ext_max = two_wx; mq_ext_max_vb = me_vb; }   /* gl24b: this walk's extents and bytes */\n",
  "  two_poll();   /* gl25c: halves whose walk a FILL or copy wait already passed */\n"
  "  if (two_wx > mq_ext_max) { mq_ext_max = two_wx; mq_ext_max_vb = me_vb; }   /* gl24b: this walk's extents and bytes */\n"),
 ("static void two_post(void)   /* a walk posted",
  "static void two_poll(void);   /* (gl25c: two_post) */\nstatic void two_post(void)   /* a walk posted"),
 ("  if (!mq_walk_due) return;\n  mq_wait(mq_walk_seq, why);\n",
  "  if (!mq_walk_due) { two_poll(); return; }   /* gl25c */\n  mq_wait(mq_walk_seq, why);\n"),
])
print('gl25c applied to', T)
