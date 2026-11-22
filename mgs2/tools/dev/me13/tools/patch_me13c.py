# patch_me13c.py <xlive_chk.c>: me13 c (task #147): the per-transfer switches at the transfer's late walk in the late path (tr_switch)
import sys
fn = sys.argv[1]; s = open(fn).read()
def rep(old, new, cnt=1):
    global s
    n = s.count(old); assert n == cnt, (old[:90], n); s = s.replace(old, new)
old_blk = '''  if (lane_opt) { int m = lane_base & (((g_opt & OPT_XL_LANE) ? 1 : 0) | ((g_opt & OPT_XL_LANE2) ? 2 : 0) | ((g_opt & OPT_XL_LANE3) ? 4 : 0) |
                                     ((g_opt & OPT_XL_LANE4) ? 8 : 0));   /* gl11: as psp/xl_psp.c xlive_chk_dma's first lines */
    if (m != g_xl_lane) { xl_lane(m != 0); g_xl_lane = m; }
    { static int eg = 1, mk = 1; int e = (g_opt & OPT_XL_EAGER) != 0, k = (g_opt & OPT_XL_MARK) != 0;   /* sb8: as psp/xl_psp.c's */
      if (e != eg) { xl_ud_eager(e); eg = e; }
      if (k != mk) { xl_ud_mark(k); mk = k; } } }
  { static int um = -1, li = -1;   /* sb9: g_opt2's step 0 bits at each transfer, as psp/xl_psp.c's */
    int u = (g_opt2 & OPT2_TX_UMEMO) ? 1 + um_chk : 0, l = (g_opt2 & OPT2_GU_LDID) ? 1 + li_chk : 0;
    if (tkc && u != um) { tk_umemo(tkc, u); um = u; }
    { static int uf = -1; int f = (g_opt2 & OPT2_TX_FRONT) ? 1 + uf_chk : 0; if (tkc && f != uf) { tk_ufront(tkc, f); uf = f; } }   /* sb10 T1 */
    if (map && l != li) { gu_ld_ident(map, l); li = l; } }
'''
rep(old_blk, '''  if (!dfr) tr_switch(g_opt, g_opt2);   /* me13 c: the late path switches at the transfer's late walk (df_exec) */
''')
rep('''static void setup(void);
void xlive_chk_dma(''', '''/* the per-transfer switches (gl11 the lanes, sb8 eager and mark, sb9 and sb10 step 0's bits), as psp/xl_psp.c xlive_chk_dma's first
   lines: at each transfer in today's walk; me13 c: in the late path at the transfer's late walk (df_exec), from the g_opt and g_opt2
   the transfer carries, as the ME's walk would take them from the transfer's record, so that turns (MGS2_OPT_CYCLE) switch the late
   walk at the transfer where they switch the game's */
static int lane_m(uint32_t opt)
{ return lane_base & (((opt & OPT_XL_LANE) ? 1 : 0) | ((opt & OPT_XL_LANE2) ? 2 : 0) | ((opt & OPT_XL_LANE3) ? 4 : 0) | ((opt & OPT_XL_LANE4) ? 8 : 0)); }
static void tr_switch(uint32_t opt, uint32_t opt2)
{
  if (lane_opt) { int m = lane_m(opt);   /* gl11 */
    if (m != g_xl_lane) { xl_lane(m != 0); g_xl_lane = m; }
    { static int eg = 1, mk = 1; int e = (opt & OPT_XL_EAGER) != 0, k = (opt & OPT_XL_MARK) != 0;   /* sb8 */
      if (e != eg) { xl_ud_eager(e); eg = e; }
      if (k != mk) { xl_ud_mark(k); mk = k; } } }
  { static int um = -1, li = -1;   /* sb9: g_opt2's step 0 bits */
    int u = (opt2 & OPT2_TX_UMEMO) ? 1 + um_chk : 0, l = (opt2 & OPT2_GU_LDID) ? 1 + li_chk : 0;
    if (tkc && u != um) { tk_umemo(tkc, u); um = u; }
    { static int uf = -1; int f = (opt2 & OPT2_TX_FRONT) ? 1 + uf_chk : 0; if (tkc && f != uf) { tk_ufront(tkc, f); uf = f; } }   /* sb10 T1 */
    if (map && l != li) { gu_ld_ident(map, l); li = l; } }
}
static void setup(void);
void xlive_chk_dma(''')
rep('''q->opt = g_opt; q->lane = one ? g_xl_lane : 0;   /* me13 b */''',
    '''q->opt = g_opt; q->opt2 = g_opt2; q->lane = one ? (lane_opt ? lane_m(g_opt) : g_xl_lane) : 0;   /* me13 b, c */''')
rep('''static void df_exec(DfE *e)
{
  if (!e->kind) { struct timespec a, b;
''', '''static void df_exec(DfE *e)
{
  if (!e->kind) { struct timespec a, b;
    tr_switch(e->opt, e->opt2);   /* me13 c */
''')
rep('''transfer carries g_opt
   and, when the runtime's walk would have fed it to the translator (the one walk), the lanes then on, g_xl_lane; else lane 0) */''',
    '''transfer carries g_opt
   and g_opt2 (me13 c: the per-transfer switches, tr_switch) and, when the runtime's walk would have fed it to the translator (the one
   walk), the lanes then on (g_xl_lane, or with MGS2_SB_LANE_OPT g_opt's lane bits); else lane 0) */''')
open(fn, 'w').write(s); print('patched', fn)
