# patch_me14_glue.py <sb14 boot/xlive_chk.c> <out>: task #149, the plain x86 twin's glue for the ME-safe picture code (me14/ge built with
# -DMGS2_ME_SAFE): the picture heap (ge/me_rt.c) given its region before the first picture allocation (MGS2_ME_HEAP_MB, default 256; or
# MGS2_ME_HEAP_KB with the PSP's hooks: more from the C library's heap when nothing fits, at least MGS2_ME_HEAP_GROW_KB, default 32, given
# back when empty, and a region one block fills resized by realloc, off with MGS2_ME_HEAP_RESIZE=0, as psp/xl_psp.c does), the
# thinning's scan run by the host at the frame's first walked transfer and handed in with gl_thin (glf.ft_on), the heap's line in the totals.
import sys
s = open(sys.argv[1], encoding='utf-8').read()
def rep(old, new, n=1):
    global s
    c = s.count(old)
    if c != n: sys.exit('%d matches (want %d) for: %s' % (c, n, old[:100]))
    s = s.replace(old, new)
rep('#include "../ge/fxthin.h"    /* task #114 */\n',
    '#include "../ge/fxthin.h"    /* task #114 */\n#include "../ge/me_rt.h"     /* task #149: the picture heap, its counts */\n')
rep('static void setup(void);\nvoid xlive_chk_dma(',
    'static size_t x_grow = 32 << 10;   /* task #149: MGS2_ME_HEAP_KB\'s hooks, as psp/xl_psp.c\'s me_more and me_less */\n'
    'static void *x_more(size_t want, size_t *got)\n'
    '{\n  size_t n = want < x_grow ? x_grow : (want + 4095) & ~(size_t)4095; void *p = malloc(n); if (p) *got = n; return p;\n}\n'
    'static void x_less(void *r) { free(r); }\n'
    'static void *x_resize(void *r, size_t want) { return realloc(r, want); }\n'
    'static void setup(void);\n'
    'static int thin_due = 1;   /* task #149: a frame began (gl_begin) and the thinning\'s scan has not run for it yet */\n'
    'static void heap_line(const char *pre)   /* task #149: the picture heap (ge/me_rt.c) and the faults me_rt counts */\n'
    '{\n'
    '  const MeHeapStats *h = me_heap_stats();\n'
    '  fprintf(stderr, "%s picture heap: %zu KB in %d regions now (%zu KB added now, %zu KB at most: the hook gave %ld regions, took back %ld, resized %ld, "\n'
    '          "refused %ld), peak %zu KB in use (extent %zu KB), now %zu KB; blocks peak %ld; allocations %ld, frees %ld, "\n'
    '          "reallocs %ld (moved %ld); refused %ld (first %zu bytes); check %d; me_rt faults: overlapping copies %ld, depth by zk %ld\\n", pre,\n'
    '          h->size >> 10, h->pools, h->added >> 10, h->added_peak >> 10, h->grows, h->returned, h->resized, h->grow_fails, h->peak >> 10, h->extent >> 10, h->used >> 10, h->blocks_peak, h->allocs, h->frees, h->reallocs, h->moved, h->fails,\n'
    '          h->fail_first, me_heap_check(), me_faults.overlap, me_faults.zk);\n'
    '}\n'
    'void xlive_chk_dma(')
rep("""M.mem still 0: the digest runs from vb 30,800 stopped there at the rain's first +25f0 call.  The same pointer otherwise */\n""",
    """M.mem still 0: the digest runs from vb 30,800 stopped there at the rain's first +25f0 call.  The same pointer otherwise */\n"""
    """  if (thin_due && glf.ft_on) { thin_due = 0; gl_thin(&glf, glf.ft->r, fx_thin_scan(glf.ft, glf.ram)); }   /* task #149: the thinning's scan
                                         on the host's side, at the frame's first walked transfer (it was gl_call's at the frame's first effect call) */\n""")
rep('  if (from >= 0) {\n    M.seen = seen;\n',
    '  if (from >= 0) {\n'
    '    { size_t hn = (size_t)(getenv("MGS2_ME_HEAP_MB") ? atol(getenv("MGS2_ME_HEAP_MB")) : 256) << 20; static uint8_t *hb;   /* task #149 */\n'
    '      const char *hk = getenv("MGS2_ME_HEAP_KB"), *gk = getenv("MGS2_ME_HEAP_GROW_KB");\n'
    '      if (hk) hn = (size_t)atol(hk) << 10;\n'
    '      if (gk) x_grow = (size_t)atol(gk) << 10;\n'
    '      if (!(hb = malloc(hn)) || me_heap_init(hb, hn)) { fprintf(stderr, "xlive: no memory for the picture heap\\n"); exit(1); }\n'
    '      if (hk) { const char *z = getenv("MGS2_ME_HEAP_RESIZE"); me_heap_grow(x_more, x_less); if (!z || atoi(z)) me_heap_resize(x_resize); }\n'
    '      fprintf(stderr, "xlive mode: the picture code built ME-safe (task #149), its heap a region of %zu KB (MGS2_ME_HEAP_MB or _KB)%s\\n", hn >> 10,\n'
    '              hk ? "; more from the C library\'s heap when nothing fits, given back when empty, resized by realloc (MGS2_ME_HEAP_KB: the PSP\'s hooks)" : ""); }\n'
    '    M.seen = seen;\n')
rep('if (fx_thin_on(&ft)) { glf.ft = &ft; fprintf', 'if (fx_thin_on(&ft)) { glf.ft = &ft; glf.ft_on = 1; fprintf')
rep('glf.ram = M.mem; gl_begin(&glf);', 'glf.ram = M.mem; gl_begin(&glf); thin_due = 1;')
rep('static void totals(void)\n{\n', 'static void totals(void)\n{\n  heap_line("xlive:");\n')
open(sys.argv[2], 'w', encoding='utf-8').write(s)
print('glue patched')
