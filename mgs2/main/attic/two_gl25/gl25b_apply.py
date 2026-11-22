#!/usr/bin/env python3
"""gl25b: on top of gl25 (tree argv[1]), from lchk4 (emulator, full route to vb 31,400, build_gl24f/emu/log.txt 83162c4f):
a hook walk's RAM reads first changed 1 hook after it only at vb 1 (boot) and vb 3815 (inside xc_keep), 2 hooks after it only at vb
20692 (011a09f0-011a53c0, the talks) and vb 26008 (00c04140-00c04200, the deck's start; its next hook with records 60 hooks later).
1. In two-area turns a hook waits for the walk before if it has records OR that walk is two hooks old (posted at the hook before the
   last): a walk never runs past the second hook after its own, even where hooks without records follow one another.
2. ge/xlme.c xc_keep: 0x0c04000-0x0c05000 and 0x11a0000-0x11a6000 added (always copied, as the texture sources the game rewrites):
   with them every range lchk4 saw change within two hooks is copied, not read late.
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
 ("static uint32_t two_seq[2]; static long two_nowait, two_older, two_move, two_t1[3];   /* gl25: the walk that holds each half; counts */\n",
  "static uint32_t two_seq[2]; static long two_nowait, two_older, two_move, two_t1[3];   /* gl25: the walk that holds each half; counts */\n"
  "static long mq_walk_hk = -1;   /* gl25b: mp_hooks at the hook that posted the last hook walk */\n"),
 ("  else if (mp_nrec) mq_arena_waitw(MW_HOOK);   /* gl24: with two areas the walk before ran while this step's game code did; gl25: only a\n"
  "                                                 hook with records waits for it, before its FILL and WALK (lchk3: the RAM a late walk reads\n"
  "                                                 is unchanged until the next hook with records) */\n",
  "  else if (mp_nrec || (mq_walk_due && mp_hooks != mq_walk_hk))   /* gl24: with two areas the walk before ran while this step's game code did */\n"
  "    mq_arena_waitw(MW_HOOK);   /* gl25: a hook with records waits for it, before its FILL and WALK; gl25b: so does the second hook after\n"
  "                                  the walk's own (lchk4: the RAM a hook walk reads stays unchanged for two hooks, xc_keep's ranges apart) */\n"),
 ("  if (mp_nrec) mq_late(0, 0);\n  memset(&e, 0, sizeof e); e.kind = XE_HOOK; e.h.vb = (int32_t)vb;\n",
  "  if (mp_nrec) { mq_late(0, 0); if (mq_walk_due) mq_walk_hk = mp_hooks; }   /* gl25b */\n  memset(&e, 0, sizeof e); e.kind = XE_HOOK; e.h.vb = (int32_t)vb;\n"),
])
patch('ge/xlme.c', [
 ("static const uint32_t xc_keep[2][2] = { { 0x12c0000u, 0x1430000u }, { 0x1ed0000u, 0x1ee0000u } };   /* the texture sources the game rewrites */\n",
  "#define XC_NKEEP 4\n"
  "static const uint32_t xc_keep[XC_NKEEP][2] = { { 0x0c04000u, 0x0c05000u }, { 0x11a0000u, 0x11a6000u }, { 0x12c0000u, 0x1430000u },\n"
  "                                               { 0x1ed0000u, 0x1ee0000u } };   /* the texture sources the game rewrites (in order; gl25b: the\n"
  "                                               first two, from lchk4: rewritten within two hooks of a walk that reads them) */\n"),
 ("      for (j = 0; j < 2 && s < e; j++) {\n",
  "      for (j = 0; j < XC_NKEEP && s < e; j++) {\n"),
])
print('gl25b applied to', T)
