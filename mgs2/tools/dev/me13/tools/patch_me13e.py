# patch_me13e.py <xlive_chk.c>: me13 e (task #147, the take-back's x86 check, me_handoff_design.md 4.6), on me13 d's boot/xlive_chk.c:
# MGS2_XLIVE_TBK=<bits> says what the take-back keeps: 1 the translator's lasting state (ge_e/xlate.c xl_keep_save / xl_keep_load: the GS
# registers, the GIF and VIF state, VU1's memory and its micro program's sources, the GS front end), 2 the GS map (what the GS's memory
# holds: the uploads, CLUT loads and drawn areas so far).  0 (the default) is me13 d's cold start.  Whatever is not kept starts afresh;
# the texture cache, the live frame and the GE list always do.  Also: a new map gets the copy set's upload marks (cs_mark), which me13 d's
# take-back left off (from X on, the upload sources first met after X were then copied rather than read from RAM: the same bytes, as no
# code writes those while in flight [C: me12/me13 census], so me13 d's frames stand).  Built into me13/xe only (mk_e.sh).
import sys
p = sys.argv[1]
s = open(p).read()
def rep(old, new, cnt=1):
    global s
    n = s.count(old)
    if n != cnt: sys.exit('patch: %d matches (want %d) for: %s' % (n, cnt, old[:90]))
    s = s.replace(old, new)

rep('static long tb_vb = -1; static int tb_done;\n',
    'static long tb_vb = -1; static int tb_done;\n'
    'static int tb_keep;   /* me13 e: MGS2_XLIVE_TBK */\n'
    'void xl_keep_save(void); void xl_keep_load(void);   /* me13 e: ge_e/xlate.c */\n')
rep('''  GlFrame k = glf;
  gu_free(map); if (!(map = gu_new(ram, 0))) { fprintf(stderr, "me13: take-back: no memory for the map\\n"); exit(1); }
  xl_uploads(map); gu_keys(map, 1); gu_zero_empty(map, 1);
''', '''  GlFrame k = glf;
  if (tb_keep & 1) xl_keep_save();   /* me13 e */
  if (!(tb_keep & 2)) {
    gu_free(map); if (!(map = gu_new(ram, 0))) { fprintf(stderr, "me13: take-back: no memory for the map\\n"); exit(1); }
    xl_uploads(map); gu_keys(map, 1); gu_zero_empty(map, 1);
    if (cs_up) gu_on_data(map, cs_mark, 0); }   /* me13 e: the copy set's upload marks, as setup's */
''')
rep('''  xl_reset();
  trs_um = trs_li = trs_uf = -1;   /* tr_switch sets the modes again on the new map and cache at the next transfer */
  tb_done = 1;
  fprintf(stderr, "me13 tb: vb %ld: the picture started afresh (GS map, texture cache, live frame, GE list, translator)\\n", vb);
''', '''  xl_reset();
  if (tb_keep & 1) xl_keep_load();   /* me13 e */
  trs_um = trs_li = trs_uf = -1;   /* tr_switch sets the modes again on the new map and cache at the next transfer */
  tb_done = 1;
  fprintf(stderr, "me13 tb: vb %ld: the picture started afresh (texture cache, live frame, GE list%s%s), kept:%s%s%s\\n", vb,
          tb_keep & 2 ? "" : ", GS map", tb_keep & 1 ? "" : ", translator", tb_keep & 1 ? " the translator's state" : "",
          tb_keep == 3 ? " and" : "", tb_keep & 2 ? " the GS map" : tb_keep & 1 ? "" : " nothing");
''')
rep('''        fprintf(stderr, "xlive mode: me13 d: the take-back: the picture started afresh at the first frame from vb %ld (MGS2_XLIVE_TB)\\n", tb_vb); }
''', '''        fprintf(stderr, "xlive mode: me13 d: the take-back: the picture started afresh at the first frame from vb %ld (MGS2_XLIVE_TB)\\n", tb_vb);
        tb_keep = getenv("MGS2_XLIVE_TBK") ? atoi(getenv("MGS2_XLIVE_TBK")) & 3 : 0;   /* me13 e */
        fprintf(stderr, "xlive mode: me13 e: the take-back keeps %s (MGS2_XLIVE_TBK=%d)\\n", tb_keep == 3 ? "the translator's state and the GS map" :
                tb_keep == 1 ? "the translator's state" : tb_keep == 2 ? "the GS map" : "nothing", tb_keep); }
''')
open(p, 'w').write(s)
print('patched')
