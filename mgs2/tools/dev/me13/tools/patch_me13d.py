# patch_me13d.py <xlive_chk.c>: me13 d (task #147, the hand-off's build order step 1, the take-back's x86 check; me_handoff_design.md 4.6):
# MGS2_XLIVE_TB=<vb>: at the first frame that begins at or after <vb> the picture is started afresh, as the main CPU would after a stopped
# ME: a new GS map and texture cache (the pool's memory kept), the live frame and the GE list set up again, the translator reset; the
# thinning keeps its state (it stays on the main CPU).  Then everything runs on as before (the late path), so the frames from <vb> on
# compared with today's show what a cold start changes.  tr_switch's last values move to file scope (trs_*) so that the switches are set
# again on the new map and cache.
import sys
p = sys.argv[1]
s = open(p).read()
def rep(old, new, cnt=1):
    global s
    n = s.count(old)
    if n != cnt: sys.exit('patch: %d matches (want %d) for: %s' % (n, cnt, old[:90]))
    s = s.replace(old, new)

rep('''  { static int um = -1, li = -1;   /* sb9: g_opt2's step 0 bits */
    int u = (opt2 & OPT2_TX_UMEMO) ? 1 + um_chk : 0, l = (opt2 & OPT2_GU_LDID) ? 1 + li_chk : 0;
    if (tkc && u != um) { tk_umemo(tkc, u); um = u; }
    { static int uf = -1; int f = (opt2 & OPT2_TX_FRONT) ? 1 + uf_chk : 0; if (tkc && f != uf) { tk_ufront(tkc, f); uf = f; } }   /* sb10 T1 */
    if (map && l != li) { gu_ld_ident(map, l); li = l; } }
}
''', '''  {   /* sb9: g_opt2's step 0 bits (me13 d: the last values set, trs_*, at file scope) */
    int u = (opt2 & OPT2_TX_UMEMO) ? 1 + um_chk : 0, l = (opt2 & OPT2_GU_LDID) ? 1 + li_chk : 0;
    if (tkc && u != trs_um) { tk_umemo(tkc, u); trs_um = u; }
    { int f = (opt2 & OPT2_TX_FRONT) ? 1 + uf_chk : 0; if (tkc && f != trs_uf) { tk_ufront(tkc, f); trs_uf = f; } }   /* sb10 T1 */
    if (map && l != trs_li) { gu_ld_ident(map, l); trs_li = l; } }
}
/* me13 d: the take-back (MGS2_XLIVE_TB=<vb>), at the first frame beginning at or after <vb>: the picture started afresh as the PSP's set-up
   makes it (setup's GX branch: map, cache, live frame, list), the translator reset; the thinning (glf.ft) and the frame buffers kept */
static long tb_vb = -1; static int tb_done;
static void tb_reset(long vb)
{
  GlFrame k = glf;
  gu_free(map); if (!(map = gu_new(ram, 0))) { fprintf(stderr, "me13: take-back: no memory for the map\\n"); exit(1); }
  xl_uploads(map); gu_keys(map, 1); gu_zero_empty(map, 1);
  tk_free(tkc); if (!(tkc = tk_new(map, gx_pool_n / 8 * 7)) || tk_pool(tkc, gx_pool, gx_pool_n)) { fprintf(stderr, "me13: take-back: no texture cache\\n"); exit(1); }
  if (gl_init(&glf, tkc, map, 0, GL_PSP_MAXTEX) || gb_arena(&glf.o, gx_arena, GL_PSP_ARENA, GL_PSP_NB, GL_PSP_NV)) {
    fprintf(stderr, "me13: take-back: no memory for the live frame\\n"); exit(1); }
  glf.memo_check = k.memo_check; glf.zchk = k.zchk; glf.hold_max = k.hold_max; glf.ft = k.ft; glf.h.cut = k.h.cut; glf.ram = k.ram;
  gx_init(&gxl, (uint32_t *)gx_lb, gx_listn / 4, gx_lb, gx_listn); gxl.ret = 1;
  xl_reset();
  trs_um = trs_li = trs_uf = -1;   /* tr_switch sets the modes again on the new map and cache at the next transfer */
  tb_done = 1;
  fprintf(stderr, "me13 tb: vb %ld: the picture started afresh (GS map, texture cache, live frame, GE list, translator)\\n", vb);
}
''')
rep('static int lane_m(uint32_t opt)\n', 'static int trs_um = -1, trs_li = -1, trs_uf = -1;   /* me13 d */\nstatic int lane_m(uint32_t opt)\n')
rep('''  if (e->fb) { gb_now = 1; glf.h.clipfast = (e->opt & OPT_CLIP_FAST) != 0; glf.stepc = (e->opt & OPT_XL_STEPC) != 0;
    glf.stepc7''', '''  if (e->fb) { if (tb_vb >= 0 && !tb_done && e->vb >= tb_vb) tb_reset(e->vb);   /* me13 d */
    gb_now = 1; glf.h.clipfast = (e->opt & OPT_CLIP_FAST) != 0; glf.stepc = (e->opt & OPT_XL_STEPC) != 0;
    glf.stepc7''')
rep('''      lw_on = M.fast && (!getenv("MGS2_XLIVE_LW") || atoi(getenv("MGS2_XLIVE_LW")));   /* me13 b */
''', '''      lw_on = M.fast && (!getenv("MGS2_XLIVE_LW") || atoi(getenv("MGS2_XLIVE_LW")));   /* me13 b */
      if (getenv("MGS2_XLIVE_TB") && (tb_vb = atol(getenv("MGS2_XLIVE_TB"))) >= 0) {   /* me13 d */
        if (!gx_on) { fprintf(stderr, "me13: MGS2_XLIVE_TB needs MGS2_XLIVE_GX\\n"); exit(1); }
        fprintf(stderr, "xlive mode: me13 d: the take-back: the picture started afresh at the first frame from vb %ld (MGS2_XLIVE_TB)\\n", tb_vb); }
''')
open(p, 'w').write(s)
print('patched')
