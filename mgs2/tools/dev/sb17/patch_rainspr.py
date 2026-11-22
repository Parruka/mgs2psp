# patch_rainspr.py (task #161): the rain's sprites left out whole with C4 (cfg MGS2_GL_RAIN_SPRITES=0), in /home/claude/sb17.  Owners and
# where they keep their blocks from the x86 census (sb15/x11 MGS2_FX_WHO=3, both routes; no block two actors point at): body_sph.c +0x78,
# charaspread.c +0xa8, rain_gas_pers.c +0x184, line_sph.c a table of 0x50-byte entries from +0xa0 (seen up to +0xb40: 35).  Off (unset or 1):
# every path as before.  Originals in scratchpad c4x/pre_rainspr/.  Asserts each file's md5 before the edit.
import hashlib
T = '/home/claude/sb17/'
def edit(rel, md5, pairs):
    p = T + rel; s = open(p).read()
    assert hashlib.md5(s.encode()).hexdigest().startswith(md5), (rel, hashlib.md5(s.encode()).hexdigest()[:8])
    for old, new in pairs:
        assert s.count(old) == 1, (rel, old[:60], s.count(old)); s = s.replace(old, new)
    open(p, 'w').write(s); print(rel, hashlib.md5(s.encode()).hexdigest()[:8])

edit('ge/fxthin.h', '4f913fe4', [
 ("   place from frame to frame, so the same drops stay drawn. */",
  "   place from frame to frame, so the same drops stay drawn.\n\n"
  "   Task #161 (deck_costs.md, 2026-11-03): with C4 on, the rain's sprites (P04+3740) can be left out whole, before their decode (cfg\n"
  "   MGS2_GL_RAIN_SPRITES=0; 1 or unset: drawn).  Their actors by the x86 census (sb15 MGS2_FX_WHO=3, both routes: no block two actors\n"
  "   point at): body_sph.c (its block's pointer at +0x78), charaspread.c (+0xa8), rain_gas_pers.c (+0x184), line_sph.c (a table of 0x50-byte\n"
  "   entries from +0xa0, the pointer first; seen up to +0xb40, 35 entries, read only inside the actor's own block).  The scan keeps their\n"
  "   blocks sorted and joined (a block that starts at the last one's end + its header joins it); a sprite call whose XlCall.src lies in one\n"
  "   is left out (fx_drop_has).  The x86 check (MGS2_THIN_CHK=1) counts the sprite and quad calls by the census's owner, left out or not. */"),
 ("#define FX_KINDS 3      /* 0: the sheets of drops close to the camera (d_splash_camera.c), 1: the splash at the feet (ft_splsh.c), 2: the\n"
  "                           rain's streaks (rain.c, rain_slow.c; task #154) */",
  "#define FX_KINDS 4      /* 0: the sheets of drops close to the camera (d_splash_camera.c), 1: the splash at the feet (ft_splsh.c), 2: the\n"
  "                           rain's streaks (rain.c, rain_slow.c; task #154), 3: the rain's sprites (task #161: drawn or left out whole) */\n"
  "#define FX_DROPS 512    /* task #161: the rain's sprite blocks a frame (more are drawn: counted in spr_over) */"),
 ("  long tab_blocks, tab_empty, tab_bad, tab_ranges;   /* task #154: the streak actors' tables: blocks taken, entries 0, entries whose\n"
  "                                         block's header did not look right (not thinned), ranges made of the blocks taken */",
  "  long tab_blocks, tab_empty, tab_bad, tab_ranges;   /* task #154: the streak actors' tables: blocks taken, entries 0, entries whose\n"
  "                                         block's header did not look right (not thinned), ranges made of the blocks taken */\n"
  "  struct { uint32_t lo, hi; } d[FX_DROPS]; int nd;   /* task #161: this frame's rain sprite blocks, sorted and joined (keep[3] 0) */\n"
  "  long spr_none, spr_blocks, spr_bad, spr_over, spr_ranges;   /* task #161: sprite calls left out whole; blocks taken; pointers whose\n"
  "                                         block's header did not look right (drawn); blocks over FX_DROPS (drawn); ranges after joining */"),
 ("void fx_thin_calls(FxThin *t, int keep_calls);                    /* task #158: the streaks' calls 1 in keep_calls (1..8), after init */",
  "void fx_thin_calls(FxThin *t, int keep_calls);                    /* task #158: the streaks' calls 1 in keep_calls (1..8), after init */\n"
  "void fx_thin_rainspr(FxThin *t, int keep);                        /* task #161: 0: the rain's sprites left out whole (with C4); 1: drawn */\n"
  "int fx_drop_has(const FxThin *t, uint32_t src);                   /* task #161: 1: src lies in one of this frame's rain sprite blocks */"),
 ("int fx_thin_on(const FxThin *t);                                  /* 1: a share above 1, or the streaks none */",
  "int fx_thin_on(const FxThin *t);                                  /* 1: a share above 1, or the streaks none (or the rain's sprites out) */"),
])

edit('ge/fxthin.c', 'f1a77723', [
 ("static const struct { const char *s; int k; uint32_t obj; } NAME[] = {   /* the file name, its kind, where the actor keeps its DG object */\n"
  "  { \"d_splash_camera.c\", 0, 0x40 }, { \"ft_splsh.c\", 1, 0x860 }, { \"rain.c\", 2, 0 }, { \"rain_slow.c\", 2, 0 } };",
  "static const struct { const char *s; int k; uint32_t obj, n, stride; } NAME[] = {   /* the file name, its kind, where the actor keeps its\n"
  "                                         DG object (task #161: the first of n pointers, stride bytes apart) */\n"
  "  { \"d_splash_camera.c\", 0, 0x40, 1, 0 }, { \"ft_splsh.c\", 1, 0x860, 1, 0 }, { \"rain.c\", 2, 0, 0, 0 }, { \"rain_slow.c\", 2, 0, 0, 0 },\n"
  "  { \"body_sph.c\", 3, 0x78, 1, 0 }, { \"charaspread.c\", 3, 0xa8, 1, 0 }, { \"rain_gas_pers.c\", 3, 0x184, 1, 0 },\n"
  "  { \"line_sph.c\", 3, 0xa0, 35, 0x50 } };   /* (task #161) */"),
 ("  t->keep_calls = 1;\n}\nvoid fx_thin_calls(FxThin *t, int keep_calls) { t->keep_calls = clamp_share(keep_calls); }   /* task #158 */\n"
  "int fx_thin_on(const FxThin *t) { return t->keep[0] > 1 || t->keep[1] > 1 || t->keep[2] != 1 || t->keep_calls > 1; }",
  "  t->keep_calls = 1; t->keep[3] = 1;\n}\nvoid fx_thin_calls(FxThin *t, int keep_calls) { t->keep_calls = clamp_share(keep_calls); }   /* task #158 */\n"
  "void fx_thin_rainspr(FxThin *t, int keep) { t->keep[3] = keep ? 1 : 0; }   /* task #161 */\n"
  "int fx_thin_on(const FxThin *t) { return t->keep[0] > 1 || t->keep[1] > 1 || t->keep[2] != 1 || t->keep_calls > 1 || !t->keep[3]; }"),
 ("int fx_thin_scan(FxThin *t, const uint8_t *ram, int lines)\n{\n  t->n = 0; t->scans++;",
  "/* task #161: a rain sprite actor's blocks into t->d (a table's entries only inside the actor's own block) */\n"
  "static void fx_rainspr(FxThin *t, const uint8_t *ram, uint32_t a, int q)\n{\n"
  "  uint32_t lim = 0x1000;   /* (the walk checked the actor's first 0x1000 bytes) */\n"
  "  if (NAME[q].n > 1) { const uint32_t ae = blk_end(ram, a); if (!ae) { t->bad++; return; } if (ae - a < lim) lim = ae - a; }\n"
  "  t->found[3]++;\n"
  "  for (uint32_t i = 0; i < NAME[q].n; i++) {\n"
  "    const uint32_t o = NAME[q].obj + NAME[q].stride * i;\n"
  "    if (o + 4 > lim) break;\n"
  "    const uint32_t p = RD(a + o);\n"
  "    if (!p) continue;\n"
  "    const uint32_t e = blk_end(ram, p);\n"
  "    if (!e) { t->spr_bad++; continue; }\n"
  "    if (t->nd == FX_DROPS) { t->spr_over++; continue; }\n"
  "    t->d[t->nd].lo = p; t->d[t->nd].hi = e; t->nd++; t->spr_blocks++;\n"
  "  }\n}\n"
  "/* task #161: the blocks in address order (a shell sort: no library call), neighbours joined as fx_streaks joins them */\n"
  "static void fx_drops_join(FxThin *t)\n{\n"
  "  static const int gap[] = { 109, 41, 19, 5, 1 };\n"
  "  for (int g = 0; g < 5; g++) for (int i = gap[g]; i < t->nd; i++) {\n"
  "    const uint32_t lo = t->d[i].lo, hi = t->d[i].hi; int j = i;\n"
  "    while (j >= gap[g] && t->d[j - gap[g]].lo > lo) { t->d[j] = t->d[j - gap[g]]; j -= gap[g]; }\n"
  "    t->d[j].lo = lo; t->d[j].hi = hi; }\n"
  "  int m = 0;\n"
  "  for (int i = 0; i < t->nd; i++) {\n"
  "    if (m && t->d[i].lo <= t->d[m - 1].hi + 0x10) { if (t->d[i].hi > t->d[m - 1].hi) t->d[m - 1].hi = t->d[i].hi; }\n"
  "    else t->d[m++] = t->d[i]; }\n"
  "  t->nd = m; t->spr_ranges += m;\n}\n"
  "int fx_drop_has(const FxThin *t, uint32_t src)   /* task #161 */\n{\n"
  "  int lo = 0, hi = t->nd - 1;\n"
  "  while (lo <= hi) { const int m = (lo + hi) >> 1;\n"
  "    if (src < t->d[m].lo) hi = m - 1; else if (src >= t->d[m].hi) lo = m + 1; else return 1; }\n"
  "  return 0;\n}\n"
  "int fx_thin_scan(FxThin *t, const uint8_t *ram, int lines)\n{\n  t->n = 0; t->nd = 0; t->scans++;"),
 ("      if (!ok_addr(a, 0x1000) || ++steps > WALK_MAX) { t->bad++; t->actors += steps; return t->n; }",
  "      if (!ok_addr(a, 0x1000) || ++steps > WALK_MAX) { t->bad++; t->actors += steps; fx_drops_join(t); return t->n; }"),
 ("          if (k == 2) { if (lines) fx_streaks(t, ram, a); }",
  "          if (k == 3) { if (lines) fx_rainspr(t, ram, a, q); }   /* task #161 (lines: C4 on, the sprites are drawn) */\n"
  "          else if (k == 2) { if (lines) fx_streaks(t, ram, a); }"),
 ("      a = RD(a);\n    }\n  }\n  t->actors += steps;\n  return t->n;\n}",
  "      a = RD(a);\n    }\n  }\n  t->actors += steps;\n  fx_drops_join(t);   /* task #161 */\n  return t->n;\n}"),
])

edit('ge/gelive.c', 'bd3864f5', [
 ("  if (f->ft && !f->ft_done && (c->entry == XL_FX || c->entry == XL_FX2 || c->entry == VC_LINE)) {   /* task #114: the frame's thinned",
  "  if (f->ft && !f->ft_done && (c->entry == XL_FX || c->entry == XL_FX2 || c->entry == VC_LINE || (c->entry == VC_SPR && !f->ft->keep[3]))) {   /* task #114: the frame's thinned"),
 ("  if (c->entry == VC_LINE && f->h.nthin && gl_line_none(&f->h, c->src)) { f->n.calls--; f->n.other++; f->ft->line_none++; return; }   /* task #158 */",
  "  if (c->entry == VC_LINE && f->h.nthin && gl_line_none(&f->h, c->src)) { f->n.calls--; f->n.other++; f->ft->line_none++; return; }   /* task #158 */\n"
  "  if (c->entry == VC_SPR && f->ft && f->ft->nd && fx_drop_has(f->ft, c->src)) { f->n.calls--; f->n.other++; f->ft->spr_none++; return; }   /* task #161 */"),
])

edit('psp/xl_psp.c', '81a79761', [
 ("    if ((e = mgs2_getenv(\"MGS2_GL_STREAK_CALLS\"))) fx_thin_calls(&gl_ft, atoi(e)); }   /* task #158: their calls 1 in N, by block */",
  "    if ((e = mgs2_getenv(\"MGS2_GL_STREAK_CALLS\"))) fx_thin_calls(&gl_ft, atoi(e));   /* task #158: their calls 1 in N, by block */\n"
  "    if ((e = mgs2_getenv(\"MGS2_GL_RAIN_SPRITES\"))) fx_thin_rainspr(&gl_ft, atoi(e)); }   /* task #161: 0: the rain's sprites left out */"),
 ("  if (gl_ft.keep_calls > 1) fprintf(stderr, \"picture: the rain's streak calls drawn 1 in %d, whole calls by their block (with C4) \"\n"
  "                                    \"(MGS2_GL_STREAK_CALLS)\\n\", gl_ft.keep_calls);",
  "  if (gl_ft.keep_calls > 1) fprintf(stderr, \"picture: the rain's streak calls drawn 1 in %d, whole calls by their block (with C4) \"\n"
  "                                    \"(MGS2_GL_STREAK_CALLS)\\n\", gl_ft.keep_calls);\n"
  "  if (!gl_ft.keep[3]) fprintf(stderr, \"picture: the rain's sprites (body_sph.c, charaspread.c, line_sph.c, rain_gas_pers.c) left out whole, \"\n"
  "                              \"with C4 (MGS2_GL_RAIN_SPRITES=0)\\n\");   /* task #161 */"),
 ("                t->tab_bad, t->tab_ranges, x->thin_lcalls, x->thin_lstrips, x->thin_ldrop, x->thin_lseg, t->line_none); } }",
  "                t->tab_bad, t->tab_ranges, x->thin_lcalls, x->thin_lstrips, x->thin_ldrop, x->thin_lseg, t->line_none);\n"
  "      if (!t->keep[3])   /* task #161 */\n"
  "        fprintf(f, \"picture: the rain's sprites left out: actors %ld; blocks %ld (not right %ld, over %ld); ranges %ld; calls left out whole %ld\\n\",\n"
  "                t->found[3], t->spr_blocks, t->spr_bad, t->spr_over, t->spr_ranges, t->spr_none); } }"),
])

edit('boot/xlive_chk.c', '770a21c3', [
 ("        if (getenv(\"MGS2_GL_STREAK_CALLS\")) fx_thin_calls(&ft, atoi(getenv(\"MGS2_GL_STREAK_CALLS\")));   /* task #158 */",
  "        if (getenv(\"MGS2_GL_STREAK_CALLS\")) fx_thin_calls(&ft, atoi(getenv(\"MGS2_GL_STREAK_CALLS\")));   /* task #158 */\n"
  "        if (getenv(\"MGS2_GL_RAIN_SPRITES\")) fx_thin_rainspr(&ft, atoi(getenv(\"MGS2_GL_RAIN_SPRITES\")));   /* task #161 */"),
 ("          if (ft.keep_calls > 1) fprintf(stderr, \"xlive mode: the rain's streak calls drawn 1 in %d, whole calls by block, with C4 \"\n"
  "                                         \"(MGS2_GL_STREAK_CALLS)\\n\", ft.keep_calls); } }",
  "          if (ft.keep_calls > 1) fprintf(stderr, \"xlive mode: the rain's streak calls drawn 1 in %d, whole calls by block, with C4 \"\n"
  "                                         \"(MGS2_GL_STREAK_CALLS)\\n\", ft.keep_calls);\n"
  "          if (!ft.keep[3]) fprintf(stderr, \"xlive mode: the rain's sprites left out whole, with C4 (MGS2_GL_RAIN_SPRITES=0)\\n\"); } }   /* task #161 */"),
 ("static void thin_chk_l(const XlCall *c)\n{",
  "/* task #161 check (MGS2_THIN_CHK=1, x86, C4 on): each sprite and quad call's owner by the census's walk, and whether the rain's sprite\n"
  "   filter left it out: body_sph.c's, charaspread.c's, line_sph.c's and rain_gas_pers.c's left out, no other's */\n"
  "static long ts_own[4][2];   /* [one of the rain's four / another actor / no block / no owner or no source][left out] */\n"
  "static void thin_chk_s(const XlCall *c)\n{\n"
  "  const int out = glf.ft->nd && fx_drop_has(glf.ft, c->src);\n"
  "  char nm[40]; const int w = fx_who_name(glf.ram, c->src, nm, sizeof nm);\n"
  "  const int rs = w == 0 && (!strcmp(nm, \"body_sph.c\") || !strcmp(nm, \"charaspread.c\") || !strcmp(nm, \"line_sph.c\") || !strcmp(nm, \"rain_gas_pers.c\"));\n"
  "  ts_own[w == 0 ? !rs : w == -2 ? 2 : 3][out]++;\n"
  "  static int shown;   /* (the first calls left out that the census gives to another actor, or drawn that it gives to the four) */\n"
  "  if (w == 0 && rs != out && shown < 12) { shown++; fprintf(stderr, \"thin check sprites: vb %ld entry %06x src %08x owner %s left out %d\\n\", cur_vb, c->entry, c->src, nm, out); }\n"
  "}\n"
  "static void thin_chk_l(const XlCall *c)\n{"),
 ("    if (tc && gb_now && glf.ft && glf.h.c4 && c->entry == VC_LINE && !c->cont) thin_chk_l(c); }   /* task #154 */",
  "    if (tc && gb_now && glf.ft && glf.h.c4 && c->entry == VC_LINE && !c->cont) thin_chk_l(c);   /* task #154 */\n"
  "    if (tc && gb_now && glf.ft && glf.h.c4 && (c->entry == VC_SPR || c->entry == VC_QUAD) && !c->cont) thin_chk_s(c); }   /* task #161 */"),
 ("                tl_own[0][1], tl_own[0][0], tl_own[1][1], tl_own[1][0], tl_own[2][1], tl_own[2][0], tl_own[3][1], tl_own[3][0]); }",
  "                tl_own[0][1], tl_own[0][0], tl_own[1][1], tl_own[1][0], tl_own[2][1], tl_own[2][0], tl_own[3][1], tl_own[3][0]);\n"
  "      if (!t->keep[3])   /* task #161 */\n"
  "        fprintf(stderr, \"xlive gb: the rain's sprites left out: actors %ld; blocks %ld (not right %ld, over %ld); ranges %ld; calls left out whole %ld\\n\",\n"
  "                t->found[3], t->spr_blocks, t->spr_bad, t->spr_over, t->spr_ranges, t->spr_none);\n"
  "      if (ts_own[0][0] + ts_own[0][1] + ts_own[1][0] + ts_own[1][1] + ts_own[2][0] + ts_own[2][1] + ts_own[3][0] + ts_own[3][1])\n"
  "        fprintf(stderr, \"xlive gb: thin check sprites: sprite and quad calls by the census's owner, left out / drawn: the rain's four %ld / %ld, \"\n"
  "                \"another actor %ld / %ld, no block %ld / %ld, no owner %ld / %ld\\n\", ts_own[0][1], ts_own[0][0], ts_own[1][1], ts_own[1][0],\n"
  "                ts_own[2][1], ts_own[2][0], ts_own[3][1], ts_own[3][0]); }"),
])
