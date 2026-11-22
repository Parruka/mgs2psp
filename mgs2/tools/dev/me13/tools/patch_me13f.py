# patch_me13f.py <xlive_chk_e.c copy>: me13 f (task #147, the take-back's x86 check, me_handoff_design.md 4.6), on me13 e's
# boot/xlive_chk_e.c: MGS2_XLIVE_TBK bit 4 keeps the live frame's depth window too: the range its depth map covers (map_lo, map_hi: the
# last frame's when a frame has none) and the effects' class 3 window (h.zlo, h.zhi: the last frame's range), which gl_end carries from
# each frame to the next (ge/gelive.c) and gl_init starts empty.  me13 e's TBK=3 left one frame different at 24,001 (deck) and 33,001
# (indoors): the first after the take-back, with more effect triangles drawn as the PS2's polygon (aux 29,856 / 208 bytes against
# 29,312 / 192).  Built into me13/xf only (mk_f.sh).
import sys
p = sys.argv[1]
s = open(p).read()
def rep(old, new, cnt=1):
    global s
    n = s.count(old)
    if n != cnt: sys.exit('patch: %d matches (want %d) for: %s' % (n, cnt, old[:90]))
    s = s.replace(old, new)

rep('  glf.memo_check = k.memo_check; glf.zchk = k.zchk; glf.hold_max = k.hold_max; glf.ft = k.ft; glf.h.cut = k.h.cut; glf.ram = k.ram;\n',
    '  glf.memo_check = k.memo_check; glf.zchk = k.zchk; glf.hold_max = k.hold_max; glf.ft = k.ft; glf.h.cut = k.h.cut; glf.ram = k.ram;\n'
    '  if (tb_keep & 4) { glf.map_lo = k.map_lo; glf.map_hi = k.map_hi; glf.h.zlo = k.h.zlo; glf.h.zhi = k.h.zhi; glf.zlo = k.zlo; glf.zhi = k.zhi; }   /* me13 f */\n')
rep('''  fprintf(stderr, "me13 tb: vb %ld: the picture started afresh (texture cache, live frame, GE list%s%s), kept:%s%s%s\\n", vb,
          tb_keep & 2 ? "" : ", GS map", tb_keep & 1 ? "" : ", translator", tb_keep & 1 ? " the translator's state" : "",
          tb_keep == 3 ? " and" : "", tb_keep & 2 ? " the GS map" : tb_keep & 1 ? "" : " nothing");
''', '''  fprintf(stderr, "me13 tb: vb %ld: the picture started afresh (texture cache, live frame, GE list%s%s), kept (TBK %d):%s%s%s\\n", vb,
          tb_keep & 2 ? "" : ", GS map", tb_keep & 1 ? "" : ", translator", tb_keep, tb_keep & 1 ? " the translator's state" : "",
          tb_keep & 2 ? " the GS map" : "", tb_keep & 4 ? " the live frame's depth window" : "");
''')
rep('tb_keep = getenv("MGS2_XLIVE_TBK") ? atoi(getenv("MGS2_XLIVE_TBK")) & 3 : 0;   /* me13 e */',
    'tb_keep = getenv("MGS2_XLIVE_TBK") ? atoi(getenv("MGS2_XLIVE_TBK")) & 7 : 0;   /* me13 e; f: bit 4 */')
rep('''        fprintf(stderr, "xlive mode: me13 e: the take-back keeps %s (MGS2_XLIVE_TBK=%d)\\n", tb_keep == 3 ? "the translator's state and the GS map" :
                tb_keep == 1 ? "the translator's state" : tb_keep == 2 ? "the GS map" : "nothing", tb_keep); }
''', '''        fprintf(stderr, "xlive mode: me13 f: the take-back keeps (MGS2_XLIVE_TBK=%d):%s%s%s%s\\n", tb_keep, tb_keep & 1 ? " the translator's state" : "",
                tb_keep & 2 ? " the GS map" : "", tb_keep & 4 ? " the live frame's depth window" : "", tb_keep ? "" : " nothing"); }
''')
open(p, 'w').write(s)
print('patched')
