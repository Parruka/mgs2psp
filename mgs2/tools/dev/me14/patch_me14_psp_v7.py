# patch_me14_psp.py <sb14 psp/xl_psp.c> <out>: task #149 step 2 on the PSP's main CPU: psp/xl_psp.c for the picture files built ME-safe
# (me14/ge, -DMGS2_ME_SAFE): the picture heap given its region before the first picture allocation (MGS2_ME_HEAP_KB in mgs2.cfg, default
# ME_HEAP_KB), the heap's hooks for more room when nothing fits (a region from newlib's heap, at least ME_HEAP_GROW_KB, or the request's
# own size) and for taking a region back once it is empty (the main CPU's side only), the thinning's scan run by the host at the frame's first walked transfer and handed in with gl_thin (glf.ft_on; timed as
# HOST_GL, as it was inside gl_call), and the heap's line in log.txt.  The main CPU's path keeps its own callbacks and timing.
# v6: each event keeps the request.  v7: after a resize of a region that ended at newlib's top chunk, the free top is given back at once
# (malloc_trim): newlib's realloc takes the whole new size from the top there, and the break rose by 132 KB for 64 KB of growth at
# vb 10,848 in me14p6 (v6's block made and freed first, to grow the top by the shortfall alone, came from a free chunk elsewhere).
import sys
s = open(sys.argv[1], encoding='utf-8').read()
def rep(old, new, n=1):
    global s
    c = s.count(old)
    if c != n: sys.exit('%d matches (want %d) for: %s' % (c, n, old[:100]))
    s = s.replace(old, new)
rep('#include "../ge/gsup.h"\n',
    '#include "../ge/gsup.h"\n#include "../ge/me_rt.h"   /* task #149: the picture files built ME-safe: their heap (me_heap_init), its counts */\n'
    '#include <unistd.h>       /* sbrk(0), for the heap\'s events */\n'
    '#include <malloc.h>       /* newlib\'s mallinfo and malloc_usable_size, for the resize hook */\n'
    '#ifndef ME_HEAP_KB\n#define ME_HEAP_KB 768              /* the picture heap\'s first region (KB) without MGS2_ME_HEAP_KB in mgs2.cfg: what the\n'
    '                                       picture\'s set-up takes (734 KB in 13 blocks before the first frame: me14p2 on the\n'
    '                                       emulator) and a margin; the hooks below give the rest and take it back */\n#endif\n'
    '#ifndef ME_HEAP_GROW_KB\n#define ME_HEAP_GROW_KB 32         /* the least region the hook gives when nothing fits (KB); a larger request gets its own\n'
    '                                       size */\n#endif\n'
    'static uint8_t *me_hb; static size_t me_hn;   /* its first region */\n'
    '/* the picture heap\'s hook (the main CPU only): a region from newlib\'s heap when no free block fits a request */\n'
    'static size_t me_grow = (size_t)ME_HEAP_GROW_KB << 10;   /* MGS2_ME_HEAP_GROW_KB in mgs2.cfg (0: each region the request\'s own size) */\n'
    'static long me_vb;   /* the step the picture is in (xl_psp_frame), for the hooks\' events */\n'
    '/* the hooks\' events for the log (op: + a region given, ! refused, - taken back, = resized in place, > resized and moved, ? resize\n'
    '   refused), with newlib\'s break after each */\n'
    'static struct { long vb; uint32_t at, from, n, w, brk; char op; } me_ev[48]; static int me_nev, me_nev_all;\n'
    'static void me_ev_add(char op, void *at, void *from, size_t n, size_t w)\n'
    '{\n  me_nev_all++; if (me_nev == 48) return;\n'
    '  me_ev[me_nev].vb = me_vb; me_ev[me_nev].op = op; me_ev[me_nev].at = (uint32_t)(uintptr_t)at; me_ev[me_nev].from = (uint32_t)(uintptr_t)from;\n'
    '  me_ev[me_nev].n = (uint32_t)n; me_ev[me_nev].w = (uint32_t)w; me_ev[me_nev].brk = (uint32_t)(uintptr_t)sbrk(0); me_nev++;\n}\n'
    'static void *me_more(size_t want, size_t *got)\n'
    '{\n  size_t n = want < me_grow ? me_grow : (want + 63) & ~(size_t)63;\n'
    '  void *p = malloc(n); if (p) *got = n; me_ev_add(p ? \'+\' : \'!\', p, 0, p ? n : want, want); return p;\n}\n'
    '/* the other hook: a region the hook gave goes back to newlib\'s heap once no block in it is in use */\n'
    'static void me_less(void *r) { free(r); me_ev_add(\'-\', r, 0, 0, 0); }\n'
    '/* the third: a region whose one block a realloc grows, resized by newlib\'s realloc (in place at the top of its heap, as gl16\'s tables grew).\n'
    '   newlib\'s realloc grows a block that ends at its top chunk in place only when the top already holds the growth; otherwise it takes the\n'
    '   whole new size from a top grown by sbrk and joins it to the block, so the break rises by the new size, not the growth, and the rest\n'
    '   stays free at the top (me14p6: 72 and 132 KB for 32 and 64 KB of growth of the texture cache\'s table at vb 10,340 and 10,848; a block\n'
    '   made and freed first, to grow the top by the shortfall alone, came from a free chunk elsewhere instead: me14p7).  So after a resize of\n'
    '   a region that ended at the top chunk (its block ends malloc_usable_size - 4 bytes on: newlib\'s next chunk; the top runs to the break)\n'
    '   while the top was short, the free top is given back at once (malloc_trim): the break then stands where a growth in place would have\n'
    '   left it.  me_trim counts the times memory went back. */\n'
    'static long me_trim;\n'
    'static void *me_resize(void *r, size_t want)\n'
    '{\n  struct mallinfo mi = mallinfo(); size_t have = malloc_usable_size(r);\n'
    '  int top = want > have && (uint8_t *)r + have - 4 + mi.keepcost == (uint8_t *)sbrk(0) && mi.keepcost < want - have + 64;\n'
    '  void *p = realloc(r, want);\n'
    '  if (top && p) me_trim += malloc_trim(0);\n'
    '  me_ev_add(!p ? \'?\' : p == r ? \'=\' : \'>\', p ? p : r, r, want, want); return p;\n}\n')
rep('static int gl_now, gl_full;              /* a frame is being made; the step that ended had full draw-pipeline calls */\n',
    'static int gl_now, gl_full;              /* a frame is being made; the step that ended had full draw-pipeline calls */\n'
    'static int thin_due;                     /* task #149: a frame began and the thinning\'s scan has not run for it yet */\n')
rep('if (fx_thin_on(&gl_ft)) { glf.ft = &gl_ft; glf.ram = M.mem; }',
    'if (fx_thin_on(&gl_ft)) { glf.ft = &gl_ft; glf.ram = M.mem; glf.ft_on = 1; }')
rep('      gl_begin(&glf); gl_now = 1;\n', '      gl_begin(&glf); gl_now = 1; thin_due = 1;   /* task #149 */\n')
rep('      M.mem = mem; M.mem_size = XL_GSZ; M.spr = spr; M.seen = 0; M.fast = 1;\n',
    '      M.mem = mem; M.mem_size = XL_GSZ; M.spr = spr; M.seen = 0; M.fast = 1;\n'
    '      { const char *e = mgs2_getenv("MGS2_ME_HEAP_KB"); me_hn = (size_t)(e && atoi(e) >= 64 ? atoi(e) : ME_HEAP_KB) << 10;   /* task #149 */\n'
    '        if (!(me_hb = malloc(me_hn)) || me_heap_init(me_hb, me_hn)) {\n'
    '          state = -1; fprintf(stderr, "translator: no memory for the picture heap (%u KB), off\\n", (unsigned)(me_hn >> 10)); return; }\n'
    '        { const char *g = mgs2_getenv("MGS2_ME_HEAP_GROW_KB"); if (g) me_grow = (size_t)atoi(g) << 10; }\n'
    '        me_heap_grow(me_more, me_less);\n'
    '        { const char *z = mgs2_getenv("MGS2_ME_HEAP_RESIZE"); if (!z || atoi(z)) me_heap_resize(me_resize); }\n'
    '        fprintf(stderr, "translator: the picture code built ME-safe (task #149): its heap a region of %u KB at %08x (MGS2_ME_HEAP_KB), more "\n'
    '                "from newlib\'s heap when nothing fits (regions of at least %u KB: MGS2_ME_HEAP_GROW_KB), given back when empty, %s\\n",\n'
    '                (unsigned)(me_hn >> 10), (unsigned)(uintptr_t)me_hb, (unsigned)(me_grow >> 10),\n'
    '                mgs2_getenv("MGS2_ME_HEAP_RESIZE") && !atoi(mgs2_getenv("MGS2_ME_HEAP_RESIZE")) ? "not resized (MGS2_ME_HEAP_RESIZE=0)"\n'
    '                : "resized by realloc when one block fills it"); }\n')
rep('void xl_psp_frame(long vb)\n{\n', 'void xl_psp_frame(long vb)\n{\n  me_vb = vb;   /* task #149: the heap hooks\' events */\n')
rep('  ntransfer++;\n',
    '  ntransfer++;\n'
    '#ifdef MGS2_GL\n'
    '  if (thin_due && glf.ft_on) {   /* task #149: the thinning\'s scan on the host\'s side, at the frame\'s first walked transfer (it was gl_call\'s\n'
    '                                    at the frame\'s first effect call); the ME will not read the game\'s RAM for it */\n'
    '    thin_due = 0; HOST_ENTER(HOST_GL); gl_thin(&glf, glf.ft->r, fx_thin_scan(glf.ft, glf.ram)); HOST_LEAVE(HOST_GL); }\n'
    '#endif\n')
rep('  if (!state) { fprintf(f, "translator: never ran (OPT_XL off)\\n"); return; }\n',
    '  if (!state) { fprintf(f, "translator: never ran (OPT_XL off)\\n"); return; }\n'
    '  { const MeHeapStats *h = me_heap_stats();   /* task #149 */\n'
    '    fprintf(f, "picture heap: %u KB in %d regions now (the first %u KB; %u KB added now, %u KB at most: the hook gave %ld regions, took back "\n'
    '            "%ld, resized %ld, refused %ld), peak %u KB in use (extent %u KB "\n'
    '            "in the first), now %u KB; blocks peak %ld; allocations %ld, frees %ld, reallocs %ld (moved %ld); refused %ld (first %u bytes); check %d; "\n'
    '            "depth by zk %ld\\n", (unsigned)(h->size >> 10), h->pools, (unsigned)((h->size - h->added) >> 10), (unsigned)(h->added >> 10),\n'
    '            (unsigned)(h->added_peak >> 10), h->grows, h->returned, h->resized, h->grow_fails, (unsigned)(h->peak >> 10), (unsigned)(h->extent >> 10),\n'
    '            (unsigned)(h->used >> 10), h->blocks_peak, h->allocs, h->frees,\n'
    '            h->reallocs, h->moved, h->fails, (unsigned)h->fail_first, me_heap_check(), me_faults.zk);\n'
    '    fprintf(f, "picture heap: the hooks\' %d events (vb: + KB given at, ! refused, - taken back, = resized in place, > resized and moved "\n'
    '            "from, ? resize refused; newlib\'s break after it; its free top given back after a resize %ld times)", me_nev_all, me_trim);\n'
    '    for (int k = 0; k < me_nev; k++) {\n'
    '      fprintf(f, "%s vb %ld: %c", k ? ";" : ":", me_ev[k].vb, me_ev[k].op);\n'
    '      if (me_ev[k].op != \'-\') fprintf(f, "%u.%u KB ", me_ev[k].n >> 10, (me_ev[k].n & 1023) * 10 >> 10);\n'
    '      if (me_ev[k].op == \'+\' || me_ev[k].op == \'!\') fprintf(f, "(for %u B) ", me_ev[k].w);\n'
    '      fprintf(f, "at %08x", me_ev[k].at);\n'
    '      if (me_ev[k].op == \'>\') fprintf(f, " from %08x", me_ev[k].from);\n'
    '      fprintf(f, " (%08x)", me_ev[k].brk); }\n'
    '    fprintf(f, "\\n"); }\n')
open(sys.argv[2], 'w', encoding='utf-8').write(s)
print('psp glue patched')
