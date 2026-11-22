# patch_me13g.py <xlive_chk_f.c copy>: me13 g (task #147, the take-back's x86 check, me_handoff_design.md 4.6), on me13 f's
# boot/xlive_chk_f.c: MGS2_XLIVE_TBK bit 8 keeps the texture cache too (its entries, memos and pool, on the kept or the new map).  me13 e's
# TBK=3 left 578 of the codec route's 2,000 frames from vb 36,000 different in their textures' or palettes' bytes, every GE list equal.
# Built into me13/xg only (mk_g.sh).
import sys
p = sys.argv[1]
s = open(p).read()
def rep(old, new, cnt=1):
    global s
    n = s.count(old)
    if n != cnt: sys.exit('patch: %d matches (want %d) for: %s' % (n, cnt, old[:90]))
    s = s.replace(old, new)

rep('''  tk_free(tkc); if (!(tkc = tk_new(map, gx_pool_n / 8 * 7)) || tk_pool(tkc, gx_pool, gx_pool_n)) { fprintf(stderr, "me13: take-back: no texture cache\\n"); exit(1); }
''', '''  if (!(tb_keep & 8)) {   /* me13 g: bit 8 keeps the texture cache */
    tk_free(tkc); if (!(tkc = tk_new(map, gx_pool_n / 8 * 7)) || tk_pool(tkc, gx_pool, gx_pool_n)) { fprintf(stderr, "me13: take-back: no texture cache\\n"); exit(1); } }
''')
rep('tb_keep = getenv("MGS2_XLIVE_TBK") ? atoi(getenv("MGS2_XLIVE_TBK")) & 7 : 0;   /* me13 e; f: bit 4 */',
    'tb_keep = getenv("MGS2_XLIVE_TBK") ? atoi(getenv("MGS2_XLIVE_TBK")) & 15 : 0;   /* me13 e; f: bit 4; g: bit 8 */')
rep('''tb_keep & 4 ? " the live frame's depth window" : "", tb_keep ? "" : " nothing"); }''',
    '''tb_keep & 4 ? " the live frame's depth window" : "", tb_keep & 8 ? " the texture cache" : "", tb_keep ? "" : " nothing"); }''')
rep('''fprintf(stderr, "xlive mode: me13 f: the take-back keeps (MGS2_XLIVE_TBK=%d):%s%s%s%s\\n"''',
    '''fprintf(stderr, "xlive mode: me13 g: the take-back keeps (MGS2_XLIVE_TBK=%d):%s%s%s%s%s\\n"''')
rep('''          tb_keep & 2 ? " the GS map" : "", tb_keep & 4 ? " the live frame's depth window" : "");
''', '''          tb_keep & 2 ? " the GS map" : "", tb_keep & 4 ? " the live frame's depth window" : "");
  if (tb_keep & 8) fprintf(stderr, "me13 tb: vb %ld: and the texture cache kept\\n", vb);   /* me13 g */
''')
open(p, 'w').write(s)
print('patched')
