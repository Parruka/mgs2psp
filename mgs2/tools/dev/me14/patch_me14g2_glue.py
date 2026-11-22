# patch_me14g2_glue.py <me14 boot/xlive_chk_g.c> <out>: task #149 step 2b, the split twin's glue on the ME loop's code (ge/xlme.c): the late
# walk with the lanes (me13 b's lw_walk, lw_run, lw_seen, lw_q, lw_lanes.h) moved out to xm_walk; the per-record switches (tr_switch) through
# xm_switch (the twin's xm.follow: the lanes, eager and mark with MGS2_SB_LANE_OPT, step 0's bits always, as tr_switch; MGS2_XM_FOLLOW=15:
# every switch, the PSP's); the callbacks' picture work through xm_call, xm_pass, xm_flush and xm_prim; a frame's end, its list and digest
# and a frame's start through xm_end, xm_list and xm_begin; the GS map's RAM reader xm_ram; gb_now is xm.now.  The twin's own checks and
# printing stay here.  Input: patch_me14g_glue.py's output.
import sys, re
s = open(sys.argv[1], encoding='utf-8').read()
def rep(old, new, n=1):
    global s
    c = s.count(old)
    if c != n: sys.exit('%d matches (want %d) for: %s' % (c, n, old[:100]))
    s = s.replace(old, new)
rep('#include "../ge/me_rt.h"     /* task #149: the picture heap, its counts */\n',
    '#include "../ge/me_rt.h"     /* task #149: the picture heap, its counts */\n'
    '#include "../ge/xlme.h"      /* task #149 step 2b: the ME loop\'s code (the late walk, the switches, the callbacks, a frame\'s end and start) */\n'
    'static XmPic xm;\n')
rep('static int lw_on; static long lw_tr, lw_tags[5], lw_grp[5], lw_bad, lw7_miss[8], lw3_miss[8];   /* me13 b: the late walk with the lanes (lw_walk) */',
    'static int lw_on;   /* me13 b: the late walk with the lanes (task #149 step 2b: ge/xlme.c xm_walk, its counts in xm) */')
rep('static GlFrame glf; static int gb_on, gb_now; static char ger_pre[200];',
    '#define gb_now xm.now   /* task #149 step 2b: the frame state the callbacks see is the ME loop\'s (ge/xlme.c) */\n'
    'static GlFrame glf; static int gb_on; static char ger_pre[200];')
# the late walk -> ge/xlme.c
a = s.index('/* me13 b (task #147): the late walk with the lanes.')
b_end = '  else xlv_dma(&M, e->ch, e->chcr, e->madr, e->qwc, e->tadr);\n}\n'
b = s.index(b_end, a) + len(b_end)
assert s.count('/* me13 b (task #147): the late walk with the lanes.') == 1 and s.count(b_end) == 1
s = s[:a] + ('/* me13 b (task #147): the late walk with the lanes; task #149 step 2b: moved to ge/xlme.c (xm_walk, lw_lanes.h there), the code the ME\n'
             '   runs: the transfers the runtime\'s walk would have fed to the translator walked with their record\'s lanes, the others by xlv_dma */\n'
             'static void lw_dma(const DfE *e) { xm_walk(&xm, e->ch, e->chcr, e->madr, e->qwc, e->tadr, e->lane, e->opt); }\n') + s[b:]
# the switches
a = s.index('static void tr_switch(uint32_t opt, uint32_t opt2)\n{\n')
b = s.index('\n}\n', a) + 3
s = s[:a] + ('static void tr_switch(uint32_t opt, uint32_t opt2)   /* task #149 step 2b: ge/xlme.c xm_switch (which switches follow: xm.follow) */\n'
             '{\n  xm_switch(&xm, opt, opt2);\n  if (lane_opt) g_xl_lane = xm.sw[XS_LANE];   /* (as tr_switch did: the runtime\'s lanes too) */\n}\n') + s[b:]
rep('static int trs_um = -1, trs_li = -1, trs_uf = -1;   /* me13 d */\n', '')
rep('  trs_um = trs_li = trs_uf = -1;   /* tr_switch sets the modes again on the new map and cache at the next transfer */\n',
    '  xm_objects(&xm, map, tkc);   /* the switches set again on the new map and cache at the next transfer (task #149 step 2b; me13 d: trs_*) */\n')
rep('    gu_free(map); if (!(map = gu_new(ram, 0)))', '    gu_free(map); if (!(map = gu_new(xm_ram, &xm)))')
# the callbacks
rep('  if (gb_now) gl_pass(&glf, c, pass);\n}', '  xm_pass(&xm, c, pass);   /* task #149 step 2b */\n}')
rep('  if (gb_now) gl_call(&glf, c);\n', '  xm_call(&xm, c);   /* task #149 step 2b: the ME loop\'s (ge/xlme.c) */\n')
rep('static void on_flush(void *ctx) { (void)ctx; if (glf.nheld) { glf.hflush[0]++; gl_flush(&glf); } }',
    'static void on_flush(void *ctx) { (void)ctx; xm_flush(&xm); }')
rep('  if (gb_now) gl_prim(&glf, g, d, v, n);\n}', '  xm_prim(&xm, g, d, v, n);   /* task #149 step 2b */\n}')
# a frame's end and its list
rep('  gl_end(&glf);\n  if (gx_on) gx_list(vb);', '  xm_end(&xm);   /* task #149 step 2b: gl_end */\n  if (gx_on) gx_list(vb);')
rep('''  GxFrame F = { glf.o.b, (uint32_t)glf.o.nb, glf.o.v, (uint32_t)glf.o.nv, glf.o.d, glf.o.t, (uint32_t)glf.o.nt, glf.tdata, glf.tpal, 1, 0 };
  gx_depth_map(glf.map_lo, glf.map_hi, 1.0f, &F.zs, &F.zo);
  gxl.dig_on = dg;
  if (gx_frame(&gxl, &gxh, &F, d) && ++gx_fail <= 50)''',
    '''  if (xm_list(&xm, d, dg) && ++gx_fail <= 50)   /* task #149 step 2b: the list and its digest by the ME loop (ge/xlme.c) */''')
rep('    uint64_t g = gl_digest(&glf, &gxl); gx_all = (gx_all ^ g) * 0x100000001b3ull; gx_n++;',
    '    uint64_t g = xm.dig_frame; gx_all = (gx_all ^ g) * 0x100000001b3ull; gx_n++;')
# a frame's start (the late path)
m = re.search(r'    gb_now = 1; glf\.h\.clipfast = \(e->opt & OPT_CLIP_FAST\) != 0;.*?\n    glf\.ram = df_snap \? df_live : M\.mem; gl_begin\(&glf\);', s, re.S)
assert m and m.group(0).count('\n') == 3, 'frame start'
s = s[:m.start()] + ('    glf.ram = df_snap ? df_live : M.mem; xm_begin(&xm, e->opt, e->opt2);   /* task #149 step 2b: the frame\'s switches (me13: as the normal hook\n'
                     '                                                                          sets them) and gl_begin by the ME loop (ge/xlme.c) */') + s[m.end():]
# set-up
rep('    M.seen = seen;\n    if (!(map = gu_new(ram, 0)))', '    M.seen = seen; xm.m = &M;   /* task #149 step 2b */\n    if (!(map = gu_new(xm_ram, &xm)))')
rep('    if (one_on) fprintf(stderr, "xlive mode: one walk (MGS2_XLIVE_ONE)\\n");\n',
    '''    xm.map = map; xm.tk = tkc; xm.glf = &glf; xm.gxl = &gxl; xm.gxh = &gxh;   /* task #149 step 2b: the ME loop (ge/xlme.c) */
    xm.follow = getenv("MGS2_XM_FOLLOW") ? (unsigned)strtoul(getenv("MGS2_XM_FOLLOW"), 0, 0) & XM_F_ALL : (lane_opt ? XM_F_LANES : 0) | XM_F_STEP0;
    xm.lanes = lane_opt ? lane_base : g_xl_lane; xm.lw_on = lw_on; xm.chk_um = um_chk; xm.chk_li = li_chk; xm.chk_uf = uf_chk;
    xm_init(&xm);
    xm.sw[XS_LANE] = g_xl_lane; xm.sw[XS_UMEMO] = xm.sw[XS_LDID] = xm.sw[XS_UFRONT] = -1;   /* as tr_switch's state started (me13 d) */
    { const char *e = getenv("MGS2_XL_EAGER"); if (e) xm.sw[XS_EAGER] = atoi(e) != 0; e = getenv("MGS2_XL_MARK"); if (e) xm.sw[XS_MARK] = atoi(e) != 0; }
    fprintf(stderr, "xlive mode: task #149 step 2b: the late walk, the switches (those that follow each record: %u, MGS2_XM_FOLLOW), the "
            "callbacks' picture work and a frame's end, list and start by the ME loop's code (ge/xlme.c)\\n", xm.follow);
    if (one_on) fprintf(stderr, "xlive mode: one walk (MGS2_XLIVE_ONE)\\n");
''')
# the late walk's report
rep('''"lane 3 misses %ld %ld %ld %ld %ld %ld\\n", lw_tr, lw_grp[1], lw_tags[1], lw_grp[2], lw_tags[2], lw_grp[3], lw_tags[3], lw_grp[4],
                     lw_tags[4], lw_bad, lw7_miss[0], lw7_miss[1], lw7_miss[2], lw7_miss[3], lw7_miss[4], lw7_miss[5], lw3_miss[0], lw3_miss[1],
                     lw3_miss[2], lw3_miss[3], lw3_miss[4], lw3_miss[5]);''',
    '''"lane 3 misses %ld %ld %ld %ld %ld %ld\\n", xm.lw_tr, xm.lw_grp[1], xm.lw_tags[1], xm.lw_grp[2], xm.lw_tags[2], xm.lw_grp[3], xm.lw_tags[3],
                     xm.lw_grp[4], xm.lw_tags[4], xm.lw_bad, xm_lw7_miss[0], xm_lw7_miss[1], xm_lw7_miss[2], xm_lw7_miss[3], xm_lw7_miss[4],
                     xm_lw7_miss[5], xm_lw3_miss[0], xm_lw3_miss[1], xm_lw3_miss[2], xm_lw3_miss[3], xm_lw3_miss[4], xm_lw3_miss[5]);''')
rep('static void totals(void)\n{\n  heap_line("xlive:");\n',
    'static void totals(void)\n{\n  heap_line("xlive:");\n'
    '  fprintf(stderr, "xlive xm: the ME loop\'s code: calls %ld, primitives %ld, lists %ld (did not fit %ld)\\n", xm.calls, xm.prims, xm.lists, xm.list_fail);\n')
open(sys.argv[2], 'w', encoding='utf-8').write(s)
print('g2 glue patched')
