#!/usr/bin/env python3
# patch_ws.py <xlive_chk.c>: me12 (task #142) step 3: the guard cost and the arena fit.  Every store the game code reports is counted by
# its store site ("me12 ws: site <offset> stores <n>" at the end, with a totals line), and the copy-size histogram gets a fourth form,
# the design's copy (every kind but the upload sources outside the packets: the packets' own upload granules are copied), with a fit line
# at arena sizes of 1 to 4 MB (steps under each size, steps over it, their mean bytes over).  Each anchor must occur once.
import sys, hashlib
fn = sys.argv[1]
src = open(fn).read()
assert hashlib.md5(src.encode()).hexdigest().startswith('eb92e9fb'), 'unexpected source'
def rep(old, new):
    global src
    assert src.count(old) == 1, ('anchor count', src.count(old), old[:60])
    src = src.replace(old, new)
rep('extern char __executable_start;\n',
    'extern char __executable_start;\n'
    '/* me12 step 3: every reported store by its store site (the guard cost of a list of sites or functions) */\n'
    'typedef struct { uintptr_t w; long n; } WsE;\n'
    '#define WS_N (1 << 18)\n'
    'static WsE *ws_t; static long ws_tn, ws_drop;\n'
    'static void ws_count(uintptr_t w)\n'
    '{\n'
    '  uint32_t h = (uint32_t)(((uint64_t)w * 0x9E3779B97F4A7C15ull) >> 46) & (WS_N - 1);\n'
    '  while (ws_t[h].n && ws_t[h].w != w) h = (h + 1) & (WS_N - 1);\n'
    '  if (!ws_t[h].n) { if (ws_tn > WS_N * 3 / 4) { ws_drop++; return; } ws_t[h].w = w; ws_tn++; }\n'
    '  ws_t[h].n++;\n'
    '}\n')
rep('  wt_all++;\n', '  wt_all++; if (ws_t) ws_count(w);\n')
rep('(unsigned)wt_t[i].a0);\n',
    '(unsigned)wt_t[i].a0);\n'
    '  { long sn = 0; for (i = 0; ws_t && i < WS_N; i++) if (ws_t[i].n) { sn++; fprintf(stderr, "me12 ws: site %lx stores %ld\\n", (unsigned long)ws_t[i].w, ws_t[i].n); }\n'
    '    fprintf(stderr, "me12 ws totals: store sites %ld (dropped %ld), last vblank %ld\\n", sn, ws_drop, cur_vb); }\n')
rep('!(wt_t = calloc(WT_N, sizeof *wt_t))) exit(1);', '!(wt_t = calloc(WT_N, sizeof *wt_t)) || !(ws_t = calloc(WS_N, sizeof *ws_t))) exit(1);')
rep('static long cs_hist[3][513];', 'static long cs_hist[4][513];')
rep('    if (f) cs_hist[2][f >> 14 > 512 ? 512 : f >> 14]++; }',
    '    if (f) cs_hist[2][f >> 14 > 512 ? 512 : f >> 14]++;\n'
    '    { long dz = a + cs_w.stepb[0][1] + cs_w.stepb[1][1]; if (dz) cs_hist[3][dz >> 14 > 512 ? 512 : dz >> 14]++; } }')
rep('static const char *wn[3] = { "all kinds, without upload sources", "tags and the data after them, without upload sources", "all kinds with the upload sources" };',
    'static const char *wn[4] = { "all kinds, without upload sources", "tags and the data after them, without upload sources", "all kinds with the upload sources",\n'
    '                               "the design\'s copy: all kinds but the upload sources outside the packets" };')
rep('for (int w = 0; w < 3; w++) { long n = 0, c = 0;', 'for (int w = 0; w < 4; w++) { long n = 0, c = 0;')
rep('        fprintf(stderr, " %s %s%d KB;", pn[j], i >= 512 ? ">= " : "<= ", i >= 512 ? 8192 : (i + 1) * 16); } }\n    fputc(\'\\n\', stderr); }\n}\n',
    '        fprintf(stderr, " %s %s%d KB;", pn[j], i >= 512 ? ">= " : "<= ", i >= 512 ? 8192 : (i + 1) * 16); } }\n    fputc(\'\\n\', stderr); }\n'
    '  { static const int lk[8] = { 1024, 1536, 2048, 2560, 2736, 3072, 3584, 4096 }; long n = 0; for (int i = 0; i <= 512; i++) n += cs_hist[3][i];\n'
    '    fprintf(stderr, "me12 copy fit (the design\'s copy, %ld steps):", n);\n'
    '    for (int j = 0; j < 8 && n; j++) { long in = 0, out = 0; double ov = 0;\n'
    '      for (int i = 0; i <= 512; i++) { if (i < lk[j] / 16) in += cs_hist[3][i]; else { out += cs_hist[3][i]; ov += cs_hist[3][i] * (i * 16 + 8.0 - lk[j]); } }\n'
    '      fprintf(stderr, " under %d KB %ld (%.2f%%), over %ld (mean %.0f KB over);", lk[j], in, 100.0 * in / n, out, out ? ov / out : 0.0); }\n'
    '    fputc(\'\\n\', stderr); }\n'
    '}\n')
open(fn, 'w').write(src)
print('patched', hashlib.md5(src.encode()).hexdigest())
