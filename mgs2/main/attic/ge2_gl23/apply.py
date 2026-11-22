# gl23: two frames on the GE side (MGS2_ME_PIC_GE2 0 off, 1 on, 2 every other ME turn): the ME may begin frame N+1 while the GE draws N.
# The frame memory (arena + list) doubled, the texture cache keeps the textures of the frame in flight (tk_guard).  Edits ge/xme.h,
# ge/xme.c, ge/texcache.h, ge/texcache.c, psp/xl_psp.c in the tree given (argv[1]).
import sys, os
T = sys.argv[1]
def patch(rel, edits):
    p = os.path.join(T, rel); s = open(p).read()
    for a, b in edits:
        c = s.count(a); assert c == 1, (rel, a[:80], c); s = s.replace(a, b)
    open(p, 'w').write(s)

# ---- texcache: the guard ----
patch('ge/texcache.h', [(
"void tk_frame(TkCache *c);     /* a frame has ended */\n",
"void tk_frame(TkCache *c);     /* a frame has ended */\n"
"/* gl23: the frame count now (tk_frame's), and a guard: entries used at or after frame g are kept too (the frame the GE may still be\n"
"   drawing while the next one is made: psp/xl_psp.c MGS2_ME_PIC_GE2); TK_NO_GUARD (the default) none, as before */\n"
"#define TK_NO_GUARD 0x7fffffffL\n"
"long tk_now(const TkCache *c);\n"
"void tk_guard(TkCache *c, long g);\n")])
patch('ge/texcache.c', [
("  GuMap *map; uint32_t budget; long frame;\n",
 "  GuMap *map; uint32_t budget; long frame, guard;   /* gl23: guard (tk_guard) */\n"),
("  c->map = map; c->budget = budget; c->freel = c->head = c->tail = -1; c->nbucket = 4096; c->blk = 1; c->flags = 1; c->t16 = 1; c->um = 1;\n",
 "  c->map = map; c->budget = budget; c->freel = c->head = c->tail = -1; c->nbucket = 4096; c->blk = 1; c->flags = 1; c->t16 = 1; c->um = 1;\n"
 "  c->guard = TK_NO_GUARD;   /* gl23 */\n"),
("    if (c->e[i].frame < c->frame - 1) { drop(c, i); p = p_alloc(c, size); }\n",
 "    if (c->e[i].frame < c->frame - 1 && c->e[i].frame < c->guard) { drop(c, i); p = p_alloc(c, size); }   /* gl23: and not guarded */\n"),
("    if (c->e[i].frame < c->frame - 1) drop(c, i);\n",
 "    if (c->e[i].frame < c->frame - 1 && c->e[i].frame < c->guard) drop(c, i);   /* gl23: and not guarded */\n"),
("void tk_frame(TkCache *c) { c->frame++; c->st.frame_bytes = 0; c->st.frame_n = 0; }\n",
 "void tk_frame(TkCache *c) { c->frame++; c->st.frame_bytes = 0; c->st.frame_n = 0; }\n"
 "long tk_now(const TkCache *c) { return c->frame; }   /* gl23 */\n"
 "void tk_guard(TkCache *c, long g) { c->guard = g; }\n"),
])

# ---- xme.h: the shared switch, the two frame memories ----
patch('ge/xme.h', [
("  uint32_t me_ctl;            /* step 4d-4: the real ME's control block",
 "  uint32_t ge2;               /* gl23: 1: a frame may begin while the GE draws the one before (two frames on the GE side; the main CPU's,\n"
 "                                 set at an ME turn's start, 0 in its own turns) */\n"
 "  uint32_t me_ctl;            /* step 4d-4: the real ME's control block"),
("  XmeWait w_me;               /* gl21: the ME's wait (xme_me_wait): XME_CLK reads clk_me when the loop's wait is this one */\n",
 "  XmeWait w_me;               /* gl21: the ME's wait (xme_me_wait): XME_CLK reads clk_me when the loop's wait is this one */\n"
 "  void *fm[2]; GxList *fl[2]; /* gl23: the two frame memories (each: the arena, then its list's block) and their lists; fm[1] 0: one only */\n"
 "  uint32_t fm_arena, fm_nb, fm_nv;   /* gl23: the arena's bytes, batches and vertices (gb_arena) */\n"),
("  long n_sync, n_rq;          /* step 4d: hand-overs; heap requests */\n",
 "  long n_sync, n_rq;          /* step 4d: hand-overs; heap requests */\n"
 "  int fm_k; long tk_fb, n_ge2;   /* gl23: the frame memory in use; the texture cache's frame at the last frame's start; frames begun\n"
 "                                    while the GE still had the one before */\n"),
])

# ---- xme.c: the frame's start ----
patch('ge/xme.c', [(
"""  if (h->begin) {   /* the GE must have finished every list before: the frame's arena, the list buffer, the cache's last textures */
    uint32_t c0 = XME_CLK(x, w);
    while ((int32_t)(XS(x)->listed - xme_released(x)) > 0) { x->n_gewait++; XS(x)->heart++; XS(x)->at = XME_AT_GEWAIT; ME_CALLP(w)(x, 1); }
    x->t_k[XME_T_GEW] += XME_CLK(x, w) - c0;
""",
"""  if (h->begin) {   /* the GE must have finished every list before: the frame's arena, the list buffer, the cache's last textures */
    uint32_t c0 = XME_CLK(x, w); int two = XS(x)->ge2 && x->fm[1];   /* gl23: or every list but the last, with two frame memories */
    while ((int32_t)(XS(x)->listed - xme_released(x)) > two) { x->n_gewait++; XS(x)->heart++; XS(x)->at = XME_AT_GEWAIT; ME_CALLP(w)(x, 1); }
    x->t_k[XME_T_GEW] += XME_CLK(x, w) - c0;
    if (two) {   /* gl23: the other frame memory and list; the textures of the frame just ended kept while the GE may draw it */
      if ((int32_t)(XS(x)->listed - xme_released(x)) > 0) x->n_ge2++;
      tk_guard(p->tk, x->tk_fb);
      x->fm_k ^= 1; gb_arena(&p->glf->o, x->fm[x->fm_k], x->fm_arena, x->fm_nb, x->fm_nv); p->gxl = x->fl[x->fm_k];
    } else tk_guard(p->tk, TK_NO_GUARD);
    x->tk_fb = tk_now(p->tk);
"""),
])

# ---- xl_psp.c ----
patch('psp/xl_psp.c', [
# the second frame memory and list
("static TkCache *tk; static GlFrame glf ME_LN; static GxList gxl ME_LN; static GxHost gxh ME_LN; static GxFrame gxf;",
 "static TkCache *tk; static GlFrame glf ME_LN; static GxList gxl ME_LN, gxl2 ME_LN; static GxHost gxh ME_LN; static GxFrame gxf;"),
("static uint32_t gl_texkb = GL_PSP_TEXKB, gl_vol_n; static uint8_t *gl_vol; static void *gl_pool; static int gl_hold;",
 "static uint32_t gl_texkb = GL_PSP_TEXKB, gl_vol_n; static uint8_t *gl_vol, *gl_vol2; static void *gl_pool; static int gl_hold;   /* gl23: gl_vol2 */"),
("  gx_init(&gxl, (uint32_t *)(gl_vol + GL_PSP_ARENA), GL_PSP_LIST / 4, gl_vol + GL_PSP_ARENA, GL_PSP_LIST); gxl.ret = 1;   /* one block */\n",
 "  gx_init(&gxl, (uint32_t *)(gl_vol + GL_PSP_ARENA), GL_PSP_LIST / 4, gl_vol + GL_PSP_ARENA, GL_PSP_LIST); gxl.ret = 1;   /* one block */\n"
 "  if ((e = mgs2_getenv(\"MGS2_ME_PIC_GE2\")) && atoi(e) > 0) {   /* gl23: the second frame memory (the heap: the volatile block is full) */\n"
 "    if ((gl_vol2 = memalign(64, gl_vol_n))) { memset(gl_vol2, 0, gl_vol_n); sceKernelDcacheWritebackInvalidateRange(gl_vol2, gl_vol_n);\n"
 "      gx_init(&gxl2, (uint32_t *)(gl_vol2 + GL_PSP_ARENA), GL_PSP_LIST / 4, gl_vol2 + GL_PSP_ARENA, GL_PSP_LIST); gxl2.ret = 1; }\n"
 "    fprintf(stderr, \"picture (gl23): the second frame memory %s (%u KB at %08x)\\n\", gl_vol2 ? \"made\" : \"NOT made: no memory (one frame on the GE side)\",\n"
 "            (unsigned)gl_vol_n >> 10, (unsigned)(uintptr_t)gl_vol2); }\n"),
# the switch's state and the per-turn count
("static long ge_tn0; static uint64_t ge_td0, ge_tq0;   /* the ME turn's start (mq_tstart) */\n",
 "static long ge_tn0; static uint64_t ge_td0, ge_tq0;   /* the ME turn's start (mq_tstart) */\n"
 "static int ge2_cfg, ge2; static long ge2_tn0;   /* gl23: MGS2_ME_PIC_GE2 (0 off, 1 on, 2 every other ME turn); this turn's; n_ge2 at its start */\n"),
# buckets by ge2 now (gl23): ME turns with one frame, ME turns with two, main CPU turns
("  uint32_t now = sceKernelGetSystemTimeLow(); int b = mq_self ? 2 : ge_fast ? 1 : 0;\n",
 "  uint32_t now = sceKernelGetSystemTimeLow(); int b = mq_self ? 2 : ge2 ? 1 : 0;   /* gl23: by GE2 (gl22: by GEFAST) */\n"),
("  { int b = ge_fast ? 1 : 0; ge_tn0 = ge_n[b]; ge_td0 = ge_draw[b]; ge_tq0 = ge_note[b]; }   /* gl22 */\n",
 "  { int b = ge2 ? 1 : 0; ge_tn0 = ge_n[b]; ge_td0 = ge_draw[b]; ge_tq0 = ge_note[b]; ge2_tn0 = mq->n_ge2; }   /* gl22; gl23 by GE2 */\n"),
("\"%ld times, mean %.2f ms; gl22: idle in this turn %.1f; GE fast %d: lists %ld, drawing %.1f ms, seen %.1f ms after its finish\\n\"",
 "\"%ld times, mean %.2f ms; gl22: idle in this turn %.1f; GE2 %d: lists %ld, drawing %.1f ms, seen %.1f ms after its finish; begun while the \"\n"
 "          \"GE drew the frame before %ld\\n\""),
("          tt - (all - ms[XME_T_IDLE]), ge_fast, ge_n[ge_fast ? 1 : 0] - ge_tn0, (double)(ge_draw[ge_fast ? 1 : 0] - ge_td0) / 1000.0,\n"
 "          (double)(ge_note[ge_fast ? 1 : 0] - ge_tq0) / 1000.0);\n",
 "          tt - (all - ms[XME_T_IDLE]), ge2, ge_n[ge2 ? 1 : 0] - ge_tn0, (double)(ge_draw[ge2 ? 1 : 0] - ge_td0) / 1000.0,\n"
 "          (double)(ge_note[ge2 ? 1 : 0] - ge_tq0) / 1000.0, mq->n_ge2 - ge2_tn0);\n"),
# the config
("          ge_fast_cfg == 2 ? \"in every other ME turn (off, on, ...)\" : ge_fast_cfg == 1 ? \"always\" : \"never (the main CPU tells it, as gl21)\");\n",
 "          ge_fast_cfg == 2 ? \"in every other ME turn (off, on, ...)\" : ge_fast_cfg == 1 ? \"always\" : \"never (the main CPU tells it, as gl21)\");\n"
 "  ge2_cfg = (e = mgs2_getenv(\"MGS2_ME_PIC_GE2\")) ? atoi(e) : 0;   /* gl23 */\n"
 "  if (gl_vol2) { mq->fm[0] = gl_vol; mq->fm[1] = gl_vol2; mq->fl[0] = &gxl; mq->fl[1] = &gxl2;\n"
 "    mq->fm_arena = GL_PSP_ARENA; mq->fm_nb = GL_PSP_NB; mq->fm_nv = GL_PSP_NV; }\n"
 "  fprintf(stderr, \"me picture GE (gl23): two frames on the GE side (the ME begins a frame while the GE draws the one before; MGS2_ME_PIC_GE2): \"\n"
 "          \"%s\\n\", !gl_vol2 ? \"off (one frame memory)\" : ge2_cfg == 2 ? \"in every other ME turn (off, on, ...)\" : ge2_cfg == 1 ? \"in every ME turn\" : \"off\");\n"),
# the turns: off in the main CPU's, set at an ME turn's start
("    mq->cache = 0; mq->clk = mq_clk_us; mq_heap_own(); mq_self = 1; mq_tend(vb); }\n",
 "    XS(mq)->ge2 = 0;   /* gl23: the main CPU's turns: one frame on the GE side (the ME has handed over: it reads nothing until the next turn) */\n"
 "    mq->cache = 0; mq->clk = mq_clk_us; mq_heap_own(); mq_self = 1; mq_tend(vb); }\n"),
("    if (ge_fast_cfg == 2) ge_fast = (int)((vb / (2 * mq_turn)) & 1);   /* gl22: off, on, off, ... by ME turn */\n",
 "    if (ge_fast_cfg == 2) ge_fast = (int)((vb / (2 * mq_turn)) & 1);   /* gl22: off, on, off, ... by ME turn */\n"
 "    ge2 = gl_vol2 && (ge2_cfg == 1 || (ge2_cfg == 2 && ((vb / (2 * mq_turn)) & 1)));   /* gl23: off, on, off, ... by ME turn */\n"
 "    XS(mq)->ge2 = (uint32_t)ge2;\n"),
# the report's names
("  { static const char *nm[3] = { \"ME turns, the main CPU tells the ME\", \"ME turns, the finish tells the ME (GEFAST)\", \"main CPU turns\" };   /* gl22 */\n",
 "  { static const char *nm[3] = { \"ME turns, one frame on the GE side\", \"ME turns, two frames on the GE side (GE2)\", \"main CPU turns\" };   /* gl22, gl23 */\n"),
("    fprintf(f, \"me picture GE (gl22): finishes counted by the callback %u, lists seen finished with no callback %ld\\n\", (unsigned)ge_fin_n, ge_nofin); }\n",
 "    fprintf(f, \"me picture GE (gl22): finishes counted by the callback %u, lists seen finished with no callback %ld\\n\", (unsigned)ge_fin_n, ge_nofin);\n"
 "    if (mq) fprintf(f, \"me picture GE (gl23): frames begun while the GE drew the frame before: %ld\\n\", mq->n_ge2); }\n"),
])
print('gl23 patch applied')
