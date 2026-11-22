# patch_me14g_glue.py <me13 boot/xlive_chk_g.c> <out>: task #149, step 1's split twin (me13 g) with the ME-safe picture code: the picture
# heap (as patch_me14_glue.py), and the thinning's scan on the host's side: at the frame's first queued transfer the host scans the game's
# RAM as it is then (fx_thin_scan) and the ranges go with the transfer's record; the late walk hands them in (gl_thin) before it walks
# that transfer.  The take-back keeps glf.ft_on as it keeps glf.ft.  Without MGS2_XLIVE_DEFER the scan runs before the walk, as in xa.
import sys
s = open(sys.argv[1], encoding='utf-8').read()
def rep(old, new, n=1):
    global s
    c = s.count(old)
    if c != n: sys.exit('%d matches (want %d) for: %s' % (c, n, old[:100]))
    s = s.replace(old, new)
rep('#include "../ge/fxthin.h"    /* task #114 */\n',
    '#include "../ge/fxthin.h"    /* task #114 */\n#include "../ge/me_rt.h"     /* task #149: the picture heap, its counts */\n')
rep('int lane; } DfE;', 'int lane; int thin_set, nthin; XcThin thin[16]; } DfE;')
rep('static void setup(void);\nvoid xlive_chk_dma(',
    'static void setup(void);\n'
    'static int thin_due = 1;   /* task #149: a frame began and the thinning\'s scan has not run for it yet (the host\'s side) */\n'
    'static long thin_scans;\n'
    'static void heap_line(const char *pre)   /* task #149: the picture heap (ge/me_rt.c) and the faults me_rt counts */\n'
    '{\n'
    '  const MeHeapStats *h = me_heap_stats();\n'
    '  fprintf(stderr, "%s picture heap: region %zu KB, peak %zu KB in use (extent %zu KB), now %zu KB; blocks peak %ld; allocations %ld, frees %ld, "\n'
    '          "reallocs %ld (moved %ld); refused %ld (first %zu bytes); check %d; me_rt faults: overlapping copies %ld, depth by zk %ld; "\n'
    '          "thinning scans by the host %ld\\n", pre, h->size >> 10, h->peak >> 10, h->extent >> 10, h->used >> 10, h->blocks_peak, h->allocs,\n'
    '          h->frees, h->reallocs, h->moved, h->fails, h->fail_first, me_heap_check(), me_faults.overlap, me_faults.zk, thin_scans);\n'
    '}\n'
    'void xlive_chk_dma(')
rep('q->lane = one ? (lane_opt ? lane_m(g_opt) : g_xl_lane) : 0;   /* me13 b, c */\n',
    'q->lane = one ? (lane_opt ? lane_m(g_opt) : g_xl_lane) : 0;   /* me13 b, c */\n'
    '    q->thin_set = 0;   /* task #149: the frame\'s first transfer carries the host\'s scan of the game\'s RAM as it is now */\n'
    '    if (thin_due && glf.ft_on) { thin_due = 0; thin_scans++; q->thin_set = 1; q->nthin = fx_thin_scan(glf.ft, mem); memcpy(q->thin, glf.ft->r, sizeof q->thin); }\n')
rep('  if (one_on && one) { one_tr++; g_xl_one = 1;',
    '  if (thin_due && glf.ft_on) { thin_due = 0; thin_scans++; gl_thin(&glf, glf.ft->r, fx_thin_scan(glf.ft, mem)); }   /* task #149 (no queue) */\n'
    '  if (one_on && one) { one_tr++; g_xl_one = 1;')
rep('  if (from >= 0) {\n    M.seen = seen;\n',
    '  if (from >= 0) {\n'
    '    { size_t hn = (size_t)(getenv("MGS2_ME_HEAP_MB") ? atol(getenv("MGS2_ME_HEAP_MB")) : 256) << 20; static uint8_t *hb;   /* task #149 */\n'
    '      if (!(hb = malloc(hn)) || me_heap_init(hb, hn)) { fprintf(stderr, "xlive: no memory for the picture heap\\n"); exit(1); }\n'
    '      fprintf(stderr, "xlive mode: the picture code built ME-safe (task #149), its heap a region of %zu MB (MGS2_ME_HEAP_MB)\\n", hn >> 20); }\n'
    '    M.seen = seen;\n')
rep('if (fx_thin_on(&ft)) { glf.ft = &ft; fprintf', 'if (fx_thin_on(&ft)) { glf.ft = &ft; glf.ft_on = 1; fprintf')
rep('glf.ram = M.mem; gl_begin(&glf);', 'glf.ram = M.mem; gl_begin(&glf); thin_due = 1;')
rep('  df_rgb = h.fb;\n', '  df_rgb = h.fb; if (h.fb) thin_due = 1;   /* task #149: the host decides here that a frame begins */\n')
rep('    tr_switch(e->opt, e->opt2);   /* me13 c */\n',
    '    tr_switch(e->opt, e->opt2);   /* me13 c */\n'
    '    if (e->thin_set) gl_thin(&glf, e->thin, e->nthin);   /* task #149: the host\'s scan, before the walk reaches the frame\'s effects */\n')
rep('glf.ft = k.ft;', 'glf.ft = k.ft; glf.ft_on = k.ft_on;')
rep('static void totals(void)\n{\n', 'static void totals(void)\n{\n  heap_line("xlive:");\n')
open(sys.argv[2], 'w', encoding='utf-8').write(s)
print('g glue patched')
