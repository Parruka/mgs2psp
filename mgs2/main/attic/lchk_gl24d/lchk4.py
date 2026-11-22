#!/usr/bin/env python3
"""lchk4 (debug, over lchk3 in /home/claude/d24): a hook walk's RAM reads hashed again at EVERY hook after it, until the next hook with
records (or 64 hooks); each range's first change reported with its distance in hooks. A walk whose turn ended at its own hook (the
hand-over drains it there) is dropped. Gives the longest a walk may lag without reading changed RAM."""
import sys
p = sys.argv[1]; s = open(p).read()
def rep(o, n):
    global s
    c = s.count(o); assert c == 1, (c, o[:70]); s = s.replace(o, n, 1)
rep("static struct { uint32_t k0, k1, what, n; long vb0, vb1; } lc_t[LC_T];\n",
    "static struct { uint32_t k0, k1, what, n; long vb0, vb1; } lc_t[LC_T];\n"
    "static int lc_d, lc_tu0; static long lc_crossed, lc_wl[10], lc_dw[5], lc_dwr[5], lc_dn[5], lc_d4log; static uint64_t lc_db[5];   /* lchk4 */\n")
rep("  lc_walks++; lc_over += mp_view.rl_over;",
    "  lc_d = 0; lc_tu0 = (int)mq_turns;   /* lchk4 */\n  lc_walks++; lc_over += mp_view.rl_over;")
rep("static void lc_report(FILE *f)\n{\n  if (!lc_on) return;\n",
    "static void lc_check4(long vb, int rec)   /* lchk4: at every hook after the walk */\n"
    "{\n"
    "  int first = 0;\n"
    "  if ((int)mq_turns != lc_tu0) { lc_crossed++; lc_pn = 0; lc_pgn = 0; return; }   /* the hand-over drained the walk at its own hook */\n"
    "  lc_d++;\n"
    "  { int b = lc_d > 5 ? 4 : lc_d - 1;\n"
    "    for (int i = 0; i < lc_pn; i++) {\n"
    "      LcP *q = &lc_p[i]; if (q->what & 0x80) continue;\n"
    "      if (lc_hash(q->k0, q->k1) == q->h) continue;\n"
    "      q->what |= 0x80; lc_dn[b]++; lc_db[b] += q->k1 - q->k0;\n"
    "      if (!first) { first = 1; lc_dw[b]++; if (rec) lc_dwr[b]++; }\n"
    "      if (lc_d4log < 400) { lc_d4log++; fprintf(stderr, \"lchk4: the walk at vb %ld read %s bytes %08x-%08x (%u) from RAM; changed by the hook at vb %ld, \"\n"
    "        \"%d hooks after it%s\\n\", lc_vb_take, lc_wn[__builtin_ctz(q->what & 0x7f)], (unsigned)q->k0, (unsigned)q->k1, (unsigned)(q->k1 - q->k0), vb, lc_d,\n"
    "        rec ? \" (a hook with records)\" : \"\"); } } }\n"
    "  if (rec || lc_d >= 64) { lc_wl[lc_d > 9 ? 9 : lc_d]++; lc_pn = 0; lc_pgn = 0; }\n"
    "}\n"
    "static void lc_report(FILE *f)\n{\n  if (!lc_on) return;\n"
    "  fprintf(f, \"lchk4: walks dropped at their turn's hand-over %ld; hooks from a walk to the next hook with records (9 = 9 or more, 64 at most):\", lc_crossed);\n"
    "  for (int i = 1; i < 10; i++) fprintf(f, \" %d:%ld\", i, lc_wl[i]);\n"
    "  fprintf(f, \"\\n\");\n"
    "  for (int b = 0; b < 5; b++) fprintf(f, \"lchk4: first changes %s%d hooks after the walk: walks %ld (seen at a hook with records %ld), ranges %ld, %llu KB\\n\",\n"
    "    b == 4 ? \">= \" : \"\", b + 1, lc_dw[b], lc_dwr[b], lc_dn[b], (unsigned long long)(lc_db[b] >> 10));\n")
rep("  if (lc_on && lc_pn && mp_nrec) lc_check(vb);",
    "  if (lc_on && lc_pn) lc_check4(vb, mp_nrec != 0);   /* lchk4 */\n  if (0) lc_check(vb);")
open(p, 'w').write(s); print('lchk4 applied')
