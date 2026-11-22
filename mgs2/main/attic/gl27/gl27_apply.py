import sys
p = sys.argv[1] + '/psp/xl_psp.c'; s = open(p).read()
def rep(old, new):
    global s
    n = s.count(old)
    assert n == 1, (n, old[:80])
    s = s.replace(old, new)
# 1. declarations, before gl_show's users
rep("""static long gl_built, gl_shown, gl_listfail, gl_shots;""",
"""static long gl_built, gl_shown, gl_listfail, gl_shots;""")
i = s.index("static void gl_show(void)")
i = s.rindex("/* the GE has drawn the frame", 0, i)
decl = """/* gl27: the screen flip.  gl_show hands a drawn frame to the display for its next vblank (PSP_DISPLAY_SETBUF_NEXTFRAME); until then
   the display still scans the buffer of the frame before, and that buffer is the one the next list draws into (from a clear to black).
   When the ME's next result was ready at once (TWO, GE2), mq_service gave the GE that list in the same vblank, and the GE cleared and drew
   the buffer on the screen: the black marks seen on the PSP in gl26 [I].  MGS2_GL_FLIPWAIT: 0 off (gl26; the cases counted), 1 the next
   list waits for a vblank after gl_show, 2 in every other ME turn and the main CPU turn after it (as GE2=2), and always from the vblank
   MGS2_GL_FLIPWAIT_FROM on.  flip_force: the shots' path (no wait, as gl26).  Counts by turn: 0 the ME's, 1 the main CPU's */
static uint32_t gl_show_vc;   /* the display's vblank count when gl_show ran */
static int flip_cfg, flip_on, flip_w, flip_force; static long flip_from = -1; static uint32_t flip_t0;
static long flip_hold[2], flip_race[2], flip_race_dir; static uint64_t flip_us[2];
static long flip_h0[2], flip_r0[2], flip_mvb0 = -1; static uint64_t flip_u0[2];   /* at a turn's start */
"""
s = s[:i] + decl + s[i:]
rep("""  sceDisplaySetFrameBuf((void *)(0x44000000u + (gl_ge_buf ? GL_FB1 : GL_FB0)), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
  gl_ge = 0; gl_shown++; gl_shown_buf = gl_ge_buf; gl_shown_vb = gl_ge_vb;""",
"""  sceDisplaySetFrameBuf((void *)(0x44000000u + (gl_ge_buf ? GL_FB1 : GL_FB0)), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
  gl_show_vc = sceDisplayGetVcount();   /* gl27 */
  gl_ge = 0; gl_shown++; gl_shown_buf = gl_ge_buf; gl_shown_vb = gl_ge_vb;""")
# 2. gl_list's own lists: counted only
rep("""  sceKernelDcacheWritebackAll();         /* the GE reads the list, the frame and the textures from RAM */
  sceGuStart(GU_DIRECT, gl_dl); sceGuCallList(gxl.w); sceGuFinish();""",
"""  sceKernelDcacheWritebackAll();         /* the GE reads the list, the frame and the textures from RAM */
  if (gl_shown && sceDisplayGetVcount() == gl_show_vc) flip_race_dir++;   /* gl27: counted only */
  sceGuStart(GU_DIRECT, gl_dl); sceGuCallList(gxl.w); sceGuFinish();""")
# 3. the shots' path
rep("""    if (mq_on) { mq_drain(); mq_service(); mq_ge_finish(); if (gl_ok != 1) return; }""",
"""    if (mq_on) { flip_force = 1; mq_drain(); mq_service(); mq_ge_finish(); flip_force = 0; if (gl_ok != 1) return; }   /* gl27: no flip
                                                                                                                        wait here */""")
# 4. mq_service's submit
rep("""    uint32_t k = mq_cons++ % XME_NRES;
    XME_BARRIER(); sceKernelDcacheWritebackRange(&mq->res[k], sizeof mq->res[k]); XME_BARRIER(); mq_submit(&XR(mq)[k]); }""",
"""    int fb = mq_self ? 1 : 0;
    if (gl_shown && !flip_force && sceDisplayGetVcount() == gl_show_vc) {   /* gl27: the frame before is not on the screen yet */
      if (flip_on) { if (!flip_w) { flip_w = 1; flip_t0 = sceKernelGetSystemTimeLow(); flip_hold[fb]++; } return; }
      flip_race[fb]++; }
    if (flip_w) { flip_us[fb] += sceKernelGetSystemTimeLow() - flip_t0; flip_w = 0; }
    uint32_t k = mq_cons++ % XME_NRES;
    XME_BARRIER(); sceKernelDcacheWritebackRange(&mq->res[k], sizeof mq->res[k]); XME_BARRIER(); mq_submit(&XR(mq)[k]); }""")
# 5. per turn lines
rep("""static void mq_tstart(long vb)   /* an ME turn begins (the main CPU's cache holds the counts as they are) */
{
""",
"""static void flip_line(const char *who, long vb0, long vb, int b)   /* gl27: a turn's flip counts */
{
  fprintf(stderr, "%s vb %ld-%ld gl27 display: flip wait %s; lists held for the screen %ld (%.1f ms); lists drawn into the shown buffer %ld\\n",
          who, vb0, vb, flip_on ? "on" : "off", flip_hold[b] - flip_h0[b], (double)(flip_us[b] - flip_u0[b]) / 1000.0, flip_race[b] - flip_r0[b]);
}
static void flip_t0set(int b) { flip_h0[b] = flip_hold[b]; flip_r0[b] = flip_race[b]; flip_u0[b] = flip_us[b]; }
static void mq_tstart(long vb)   /* an ME turn begins (the main CPU's cache holds the counts as they are) */
{
  if (flip_mvb0 >= 0) flip_line("main turn", flip_mvb0, vb, 1);   /* gl27: the main CPU's turn before */
  flip_t0set(0);
""")
rep("""  { double s[XM_TS_N], in = 0; long n[XM_TS_N];   /* gl26: the walk's split */""",
"""  flip_line("me turn", mq_tvb0, vb, 0); flip_t0set(1); flip_mvb0 = vb;   /* gl27 */
  if (mq->ws_on) { double s[XM_TS_N], in = 0; long n[XM_TS_N];   /* gl26: the walk's split (gl27: only with MGS2_ME_SPLIT) */""")
# 6. turn selection
rep("""    mq->cache = 0; mq->clk = mq_clk_us; mq_heap_own(); mq_self = 1; mq_tend(vb); two_start(0); }""",
"""    mq->cache = 0; mq->clk = mq_clk_us; mq_heap_own(); mq_self = 1; mq_tend(vb); two_start(0); flip_on = flip_sel(vb); }""")
rep("""    xme_heap_ask(me_rs_on); mq_self = 0; mq_tstart(vb); two_start(two_on(vb)); }   /* gl24: two areas or one, by ME turn */""",
"""    xme_heap_ask(me_rs_on); mq_self = 0; mq_tstart(vb); two_start(two_on(vb)); flip_on = flip_sel(vb); }   /* gl24: two areas or one, by ME
                                                                                                             turn; gl27: the flip wait */""")
rep("""/* step 4d, mode 4, after the hook's FILL, WALK and HOOK (no record waits): the hang test's stop found, the turns changed */
static void mq_turn_hook(long vb)""",
"""static int flip_sel(long vb)   /* gl27: the flip wait in the turn that begins at vb */
{
  return flip_cfg == 1 || (flip_cfg == 2 && ((flip_from >= 0 && vb >= flip_from) || !mq_turn || ((vb / (2 * mq_turn)) & 1)));
}
/* step 4d, mode 4, after the hook's FILL, WALK and HOOK (no record waits): the hang test's stop found, the turns changed */
static void mq_turn_hook(long vb)""")
# 7. config
rep("""  mq->w_thr = mq_wait_thr;   /* gl26: the loop's thread times the walk's split (the emulator), set once */""",
"""  mq->w_thr = mq_wait_thr;   /* gl26: the loop's thread times the walk's split (the emulator), set once */
  mq->ws_on = (e = mgs2_getenv("MGS2_ME_SPLIT")) && atoi(e) > 0;   /* gl27: before the ME starts */
  fprintf(stderr, "me picture walk split (gl26's timers, MGS2_ME_SPLIT): %s\\n", mq->ws_on ? "on" : "off");
  flip_cfg = (e = mgs2_getenv("MGS2_GL_FLIPWAIT")) ? atoi(e) : 0; flip_from = (e = mgs2_getenv("MGS2_GL_FLIPWAIT_FROM")) ? atol(e) : -1;
  flip_on = flip_cfg != 0;
  fprintf(stderr, "picture display (gl27): the next list waits for the screen to show the frame before (MGS2_GL_FLIPWAIT): %s", flip_cfg == 2 ?
          "in every other ME turn and the main CPU turn after it (on in odd pairs, as GE2=2)" : flip_cfg == 1 ? "always" : "never (counted)");
  if (flip_cfg == 2 && flip_from >= 0) fprintf(stderr, ", always from vb %ld", flip_from);
  fprintf(stderr, "\\n");""")
# 8. report
rep("""  if (mq_tn) { double s[XM_TS_N], in = 0, wk = (double)mq_tkall[XE_WALK] / (mq_tdiv ? mq_tdiv : 1);   /* gl26: the walk's split over the ME's""",
"""  fprintf(f, "picture display (gl27): flip wait MGS2_GL_FLIPWAIT %d (from vb %ld): ME turns: lists held for the screen %ld (%.2f s), lists drawn "
          "into the shown buffer %ld; main CPU turns: held %ld (%.2f s), drawn into the shown buffer %ld; lists of the main CPU's own picture "
          "(no loop) drawn into the shown buffer %ld\\n", flip_cfg, flip_from, flip_hold[0], flip_us[0] / 1e6, flip_race[0], flip_hold[1],
          flip_us[1] / 1e6, flip_race[1], flip_race_dir);
  if (mq_tn && mq->ws_on) { double s[XM_TS_N], in = 0, wk = (double)mq_tkall[XE_WALK] / (mq_tdiv ? mq_tdiv : 1);   /* gl26: the walk's split over the ME's""")
open(p, 'w').write(s)
print('ok')
