# patch_none.py (task #158): MGS2_GL_STREAKS=0 = the rain's streak calls left out whole (Ffm 13:27 "remove the rain"), in /home/claude/sb17
import hashlib, sys
T = '/home/claude/sb17/'
def rmw(f, md5, subs):
    p = T + f; s = open(p, encoding='latin-1').read()
    assert hashlib.md5(s.encode('latin-1')).hexdigest().startswith(md5), f
    for a, b in subs:
        assert s.count(a) == 1, (f, a[:60], s.count(a))
        s = s.replace(a, b)
    open(p, 'w', encoding='latin-1').write(s)
rmw('ge/fxthin.h', 'f6eed2d1', [
  ('  int keep[FX_KINDS];                 /* 1 strip in keep[k] of kind k\'s calls drawn (1: all) */',
   '  int keep[FX_KINDS];                 /* 1 strip in keep[k] of kind k\'s calls drawn (1: all; the streaks\' 0: none, task #158) */'),
  ('  long tab_blocks, tab_empty, tab_bad, tab_ranges;',
   '  long line_none;                     /* task #158: streak calls of the rain\'s ranges left out whole (MGS2_GL_STREAKS=0) */\n  long tab_blocks, tab_empty, tab_bad, tab_ranges;'),
  ('/* shares 1..8 (streaks: task #154) */', '/* shares 1..8 (streaks: task #154; 0: none, task #158) */'),
  ('/* 1: a share above 1 */', '/* 1: a share above 1, or the streaks none */')])
rmw('ge/fxthin.c', '0cf2e355', [
  ('t->keep[2] = clamp_share(keep_streaks);', 't->keep[2] = keep_streaks == 0 ? 0 : clamp_share(keep_streaks);   /* task #158: 0 none */'),
  ('return t->keep[0] > 1 || t->keep[1] > 1 || t->keep[2] > 1; }', 'return t->keep[0] > 1 || t->keep[1] > 1 || t->keep[2] != 1; }'),
  ('        if (t->keep[k] > 1) {\n', '        if (t->keep[k] != 1) {   /* (task #158: the streaks\' 0, none, takes their ranges too) */\n')])
rmw('ge/gelive.c', '0d752682', [
  ('int gl_need, gl_need_chk; long gl_need_n, gl_need_diff;   /* sb1 (gelive.h) */\nvoid gl_call(GlFrame *f, const XlCall *c)\n',
   'int gl_need, gl_need_chk; long gl_need_n, gl_need_diff;   /* sb1 (gelive.h) */\n'
   '/* task #158 (Ffm 2026-11-03 13:27, "remove the rain"): MGS2_GL_STREAKS=0 leaves a streak call whose data lies in one of the rain\'s ranges\n'
   '   out whole, as C4 off leaves out every streak call (before its decode); the other streak calls (spark.c, usp.c, the opening\'s without a\n'
   '   block) are drawn */\n'
   'static int gl_line_none(const XcHost *h, uint32_t src)\n{\n'
   '  for (int k = 0; k < h->nthin; k++) if (h->thin[k].line && !h->thin[k].keep && src - h->thin[k].lo < h->thin[k].hi - h->thin[k].lo) return 1;\n'
   '  return 0;\n}\nvoid gl_call(GlFrame *f, const XlCall *c)\n'),
  ('    f->ft_done = 1; f->h.nthin = fx_thin_scan(f->ft, f->ram, f->h.c4); f->h.thin = f->ft->r; }\n',
   '    f->ft_done = 1; f->h.nthin = fx_thin_scan(f->ft, f->ram, f->h.c4); f->h.thin = f->ft->r; }\n'
   '  if (c->entry == VC_LINE && f->h.nthin && gl_line_none(&f->h, c->src)) { f->n.calls--; f->n.other++; f->ft->line_none++; return; }   /* task #158 */\n')])
rmw('psp/xl_psp.c', '9b814fc6', [
  ('the rain\'s streaks 1 in %d (with C4) "', 'the rain\'s streaks 1 in %d (with C4; 0: none) "'),
  ('      if (t->keep[2] > 1)   /* task #154 */\n        fprintf(f, "picture: thinned streaks (1 in %d): actors %ld; their tables\' blocks %ld, entries 0 %ld, blocks not right %ld; ranges %ld; "\n'
   '                "calls thinned %ld, their strips %ld, left out %ld (%ld segments)\\n", t->keep[2], t->found[2], t->tab_blocks, t->tab_empty,\n'
   '                t->tab_bad, t->tab_ranges, x->thin_lcalls, x->thin_lstrips, x->thin_ldrop, x->thin_lseg); } }',
   '      if (t->keep[2] != 1)   /* task #154 (0: none, task #158) */\n        fprintf(f, "picture: thinned streaks (1 in %d): actors %ld; their tables\' blocks %ld, entries 0 %ld, blocks not right %ld; ranges %ld; "\n'
   '                "calls thinned %ld, their strips %ld, left out %ld (%ld segments); calls left out whole %ld\\n", t->keep[2], t->found[2], t->tab_blocks, t->tab_empty,\n'
   '                t->tab_bad, t->tab_ranges, x->thin_lcalls, x->thin_lstrips, x->thin_ldrop, x->thin_lseg, t->line_none); } }')])
rmw('boot/xlive_chk.c', '9a67e3c9', [
  ('      if (t->keep[2] > 1)   /* task #154 */\n        fprintf(stderr, "xlive gb: thinned streaks (1 in %d): actors %ld; their tables\' blocks %ld, entries 0 %ld, blocks not right %ld; "\n'
   '                "ranges %ld; calls thinned %ld, their strips %ld, left out %ld (%ld segments)\\n", t->keep[2], t->found[2], t->tab_blocks,\n'
   '                t->tab_empty, t->tab_bad, t->tab_ranges, x->thin_lcalls, x->thin_lstrips, x->thin_ldrop, x->thin_lseg);',
   '      if (t->keep[2] != 1)   /* task #154 (0: none, task #158) */\n        fprintf(stderr, "xlive gb: thinned streaks (1 in %d): actors %ld; their tables\' blocks %ld, entries 0 %ld, blocks not right %ld; "\n'
   '                "ranges %ld; calls thinned %ld, their strips %ld, left out %ld (%ld segments); calls left out whole %ld\\n", t->keep[2], t->found[2], t->tab_blocks,\n'
   '                t->tab_empty, t->tab_bad, t->tab_ranges, x->thin_lcalls, x->thin_lstrips, x->thin_ldrop, x->thin_lseg, t->line_none);'),
  ('          if (ft.keep[2] > 1) fprintf(stderr, "xlive mode: the rain\'s streaks drawn 1 strip in %d with C4 (MGS2_GL_STREAKS)\\n", ft.keep[2]); } }',
   '          if (ft.keep[2] != 1) fprintf(stderr, "xlive mode: the rain\'s streaks drawn 1 strip in %d with C4 (MGS2_GL_STREAKS; 0: none)\\n", ft.keep[2]); } }')])
print('patched')
