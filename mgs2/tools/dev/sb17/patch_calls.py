# patch_calls.py (task #158): the rain's streak calls 1 in N, whole calls by their block (cfg MGS2_GL_STREAK_CALLS), in /home/claude/sb17.
# Originals in scratchpad c4x/pre_calls/.  Asserts each file's md5 before the edit.
import hashlib
T = '/home/claude/sb17/'
def edit(rel, md5, pairs):
    p = T + rel; s = open(p).read()
    assert hashlib.md5(s.encode()).hexdigest().startswith(md5), (rel, hashlib.md5(s.encode()).hexdigest()[:8])
    for old, new in pairs:
        assert s.count(old) == 1, (rel, old[:60], s.count(old)); s = s.replace(old, new)
    open(p, 'w').write(s); print(rel, hashlib.md5(s.encode()).hexdigest()[:8])

edit('ge/xconv.h', '0e78b2ac', [
 ('typedef struct { uint32_t lo, hi; int keep, line; } XcThin;',
  'typedef struct { uint32_t lo, hi; int keep, line, kcall; } XcThin;   /* (task #158 kcall: a streak range\'s calls 1 in kcall, by block) */')])

edit('ge/fxthin.h', '43a68d8a', [
 ('   blocks, marked for the streaks (XcThin.line).  Owners from the x86 census (MGS2_FX_WHO=1, stagec_notes.md 4.9). */',
  '   blocks, marked for the streaks (XcThin.line).  Owners from the x86 census (MGS2_FX_WHO=1, stagec_notes.md 4.9).\n\n'
  '   Task #158 (Ffm 2026-11-03 13:27): thinning the streaks by strips saves no time (each call is decoded and its vertices clipped before\n'
  '   the strips are dropped), so they can also be drawn 1 call in N (cfg MGS2_GL_STREAK_CALLS): a call is left out whole, before its decode,\n'
  '   unless its block\'s place in the range, (XlCall.src - lo) / FX_STREAK_BLOCK, is a multiple of N.  Each call is 32 drops spread over the\n'
  '   whole screen (x86 probe, deck route: a call\'s on-screen box spans 1.72 x 1.76 of the 2 x 2 clip square, median) and a block keeps its\n'
  '   place from frame to frame, so the same drops stay drawn. */'),
 ('#define FX_CACHE 512 ',
  '#define FX_STREAK_BLOCK 0x1290u   /* task #158: a streak block\'s place: its data (4,736 bytes) and the next block\'s header, in a row */\n'
  '#define FX_CACHE 512 '),
 ('  int keep[FX_KINDS];                 /* 1 strip in keep[k] of kind k\'s calls drawn (1: all; the streaks\' 0: none, task #158) */',
  '  int keep[FX_KINDS];                 /* 1 strip in keep[k] of kind k\'s calls drawn (1: all; the streaks\' 0: none, task #158) */\n'
  '  int keep_calls;                     /* task #158: 1 in keep_calls of the rain\'s streak calls drawn, by block (1: all) */'),
 ('  long line_none;                     /* task #158: streak calls of the rain\'s ranges left out whole (MGS2_GL_STREAKS=0) */',
  '  long line_none;                     /* task #158: streak calls of the rain\'s ranges left out whole (MGS2_GL_STREAKS=0, or\n'
  '                                         MGS2_GL_STREAK_CALLS above 1) */'),
 ('int fx_thin_on(const FxThin *t);                                  /* 1: a share above 1, or the streaks none */',
  'void fx_thin_calls(FxThin *t, int keep_calls);                    /* task #158: the streaks\' calls 1 in keep_calls (1..8), after init */\n'
  'int fx_thin_on(const FxThin *t);                                  /* 1: a share above 1, or the streaks none */')])

edit('ge/fxthin.c', '1d8f5562', [
 ('  t->keep[0] = clamp_share(keep_sheets); t->keep[1] = clamp_share(keep_splash); t->keep[2] = keep_streaks == 0 ? 0 : clamp_share(keep_streaks);   /* task #158: 0 none */\n}',
  '  t->keep[0] = clamp_share(keep_sheets); t->keep[1] = clamp_share(keep_splash); t->keep[2] = keep_streaks == 0 ? 0 : clamp_share(keep_streaks);   /* task #158: 0 none */\n'
  '  t->keep_calls = 1;\n}\n'
  'void fx_thin_calls(FxThin *t, int keep_calls) { t->keep_calls = clamp_share(keep_calls); }   /* task #158 */'),
 ('int fx_thin_on(const FxThin *t) { return t->keep[0] > 1 || t->keep[1] > 1 || t->keep[2] != 1; }',
  'int fx_thin_on(const FxThin *t) { return t->keep[0] > 1 || t->keep[1] > 1 || t->keep[2] != 1 || t->keep_calls > 1; }'),
 ('    t->r[t->n].lo = lo; t->r[t->n].hi = hi; t->r[t->n].keep = t->keep[2]; t->r[t->n].line = 1; t->n++; t->tab_ranges++;',
  '    t->r[t->n].lo = lo; t->r[t->n].hi = hi; t->r[t->n].keep = t->keep[2]; t->r[t->n].line = 1; t->r[t->n].kcall = t->keep_calls; t->n++; t->tab_ranges++;'),
 ('        if (t->keep[k] != 1) {   /* (task #158: the streaks\' 0, none, takes their ranges too) */',
  '        if (t->keep[k] != 1 || (k == 2 && t->keep_calls > 1)) {   /* (task #158: the streaks\' 0, none, and their calls 1 in N take\n'
  '                                                                       their ranges too) */'),
 ('              else { t->r[t->n].lo = obj; t->r[t->n].hi = nx; t->r[t->n].keep = t->keep[k]; t->r[t->n].line = 0; t->n++; t->found[k]++; }',
  '              else { t->r[t->n].lo = obj; t->r[t->n].hi = nx; t->r[t->n].keep = t->keep[k]; t->r[t->n].line = 0; t->r[t->n].kcall = 1; t->n++;\n'
  '                     t->found[k]++; }')])

edit('ge/gelive.c', 'c25a25d0', [
 ('''   out whole, as C4 off leaves out every streak call (before its decode); the other streak calls (spark.c, usp.c, the opening's without a
   block) are drawn */
static int gl_line_none(const XcHost *h, uint32_t src)
{
  for (int k = 0; k < h->nthin; k++) if (h->thin[k].line && !h->thin[k].keep && src - h->thin[k].lo < h->thin[k].hi - h->thin[k].lo) return 1;
  return 0;
}''',
  '''   out whole, as C4 off leaves out every streak call (before its decode); the other streak calls (spark.c, usp.c, the opening's without a
   block) are drawn.  MGS2_GL_STREAK_CALLS=N (fxthin.h) leaves out the calls whose block's place in the range is not a multiple of N */
static int gl_line_none(const XcHost *h, uint32_t src)
{
  for (int k = 0; k < h->nthin; k++) { const XcThin *r = &h->thin[k];
    if (r->line && src - r->lo < r->hi - r->lo && (!r->keep || (r->kcall > 1 && (src - r->lo) / FX_STREAK_BLOCK % (uint32_t)r->kcall))) return 1; }
  return 0;
}''')])

edit('psp/xl_psp.c', '8b2e812e', [
 ('    fx_thin_init(&gl_ft, rn, sp, sk); }',
  '    fx_thin_init(&gl_ft, rn, sp, sk);\n'
  '    if ((e = mgs2_getenv("MGS2_GL_STREAK_CALLS"))) fx_thin_calls(&gl_ft, atoi(e)); }   /* task #158: their calls 1 in N, by block */'),
 ('''               "(MGS2_GL_RAIN, MGS2_GL_SPLASH, MGS2_GL_STREAKS; 1: as the PS2)\\n", gl_ft.keep[0], gl_ft.keep[1], gl_ft.keep[2]);''',
  '''               "(MGS2_GL_RAIN, MGS2_GL_SPLASH, MGS2_GL_STREAKS; 1: as the PS2)\\n", gl_ft.keep[0], gl_ft.keep[1], gl_ft.keep[2]);
  if (gl_ft.keep_calls > 1) fprintf(stderr, "picture: the rain's streak calls drawn 1 in %d, whole calls by their block (with C4) "
                                    "(MGS2_GL_STREAK_CALLS)\\n", gl_ft.keep_calls);'''),
 ('''      if (t->keep[2] != 1)   /* task #154 (0: none, task #158) */
        fprintf(f, "picture: thinned streaks (1 in %d): actors %ld; their tables' blocks %ld, entries 0 %ld, blocks not right %ld; ranges %ld; "
                "calls thinned %ld, their strips %ld, left out %ld (%ld segments); calls left out whole %ld\\n", t->keep[2], t->found[2], t->tab_blocks, t->tab_empty,''',
  '''      if (t->keep[2] != 1 || t->keep_calls > 1)   /* task #154 (0: none, calls 1 in N: task #158) */
        fprintf(f, "picture: thinned streaks (1 in %d, calls 1 in %d): actors %ld; their tables' blocks %ld, entries 0 %ld, blocks not right %ld; ranges %ld; "
                "calls thinned %ld, their strips %ld, left out %ld (%ld segments); calls left out whole %ld\\n", t->keep[2], t->keep_calls, t->found[2], t->tab_blocks, t->tab_empty,''')])

edit('boot/xlive_chk.c', 'fb7e0571', [
 ('''      if (t->keep[2] != 1)   /* task #154 (0: none, task #158) */
        fprintf(stderr, "xlive gb: thinned streaks (1 in %d): actors %ld; their tables' blocks %ld, entries 0 %ld, blocks not right %ld; "
                "ranges %ld; calls thinned %ld, their strips %ld, left out %ld (%ld segments); calls left out whole %ld\\n", t->keep[2], t->found[2], t->tab_blocks,''',
  '''      if (t->keep[2] != 1 || t->keep_calls > 1)   /* task #154 (0: none, calls 1 in N: task #158) */
        fprintf(stderr, "xlive gb: thinned streaks (1 in %d, calls 1 in %d): actors %ld; their tables' blocks %ld, entries 0 %ld, blocks not right %ld; "
                "ranges %ld; calls thinned %ld, their strips %ld, left out %ld (%ld segments); calls left out whole %ld\\n", t->keep[2], t->keep_calls, t->found[2], t->tab_blocks,'''),
 ('''          getenv("MGS2_GL_SPLASH") ? atoi(getenv("MGS2_GL_SPLASH")) : 1, getenv("MGS2_GL_STREAKS") ? atoi(getenv("MGS2_GL_STREAKS")) : 1);''',
  '''          getenv("MGS2_GL_SPLASH") ? atoi(getenv("MGS2_GL_SPLASH")) : 1, getenv("MGS2_GL_STREAKS") ? atoi(getenv("MGS2_GL_STREAKS")) : 1);
        if (getenv("MGS2_GL_STREAK_CALLS")) fx_thin_calls(&ft, atoi(getenv("MGS2_GL_STREAK_CALLS")));   /* task #158 */'''),
 ('''          if (ft.keep[2] != 1) fprintf(stderr, "xlive mode: the rain's streaks drawn 1 strip in %d with C4 (MGS2_GL_STREAKS; 0: none)\\n", ft.keep[2]); } }''',
  '''          if (ft.keep[2] != 1) fprintf(stderr, "xlive mode: the rain's streaks drawn 1 strip in %d with C4 (MGS2_GL_STREAKS; 0: none)\\n", ft.keep[2]);
          if (ft.keep_calls > 1) fprintf(stderr, "xlive mode: the rain's streak calls drawn 1 in %d, whole calls by block, with C4 "
                                         "(MGS2_GL_STREAK_CALLS)\\n", ft.keep_calls); } }''')])
