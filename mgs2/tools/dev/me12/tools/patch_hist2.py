#!/usr/bin/env python3
# patch_hist2.py <xlive_chk.c>: me12 (task #142) histogram to 8 MB (513 buckets of 16 KB), a third line with the upload granules, and every
# writer printed (no 600 cap); applied after wruns.sh's three runs
import sys, hashlib
fn = sys.argv[1]; s = open(fn).read(); n0 = hashlib.md5(s.encode()).hexdigest()[:8]
def rep(a, b, cnt=1):
    global s
    assert s.count(a) == cnt, (a, s.count(a)); s = s.replace(a, b)
rep('for (i = 0; i < m && i < 600; i++) fprintf(stderr, "me12 wt: writer', 'for (i = 0; i < m; i++) fprintf(stderr, "me12 wt: writer')
rep('static long cs_hist[2][129];', 'static long cs_hist[3][513];')
rep('static const char *wn[2] = { "all kinds", "tags and the data after them" };', 'static const char *wn[3] = { "all kinds, without upload sources", "tags and the data after them, without upload sources", "all kinds with the upload sources" };')
rep('for (int w = 0; w < 2; w++) { long n = 0, c = 0; int pct[5] = { 50, 90, 99, 999, 1000 }; for (int i = 0; i <= 128; i++) n += cs_hist[w][i];',
    'for (int w = 0; w < 3; w++) { long n = 0, c = 0; int pct[5] = { 50, 90, 99, 999, 1000 }; for (int i = 0; i <= 512; i++) n += cs_hist[w][i];')
rep('fprintf(stderr, "me12 copy sizes (%s, without upload sources): %ld steps;", wn[w], n);', 'fprintf(stderr, "me12 copy sizes (%s): %ld steps;", wn[w], n);')
rep('while (i <= 128 && c + cs_hist[w][i] < need) c += cs_hist[w][i++];', 'while (i <= 512 && c + cs_hist[w][i] < need) c += cs_hist[w][i++];')
rep('fprintf(stderr, " %s %s%d KB;", pn[j], i >= 128 ? ">= " : "<= ", i >= 128 ? 2048 : (i + 1) * 16); } }',
    'fprintf(stderr, " %s %s%d KB;", pn[j], i >= 512 ? ">= " : "<= ", i >= 512 ? 8192 : (i + 1) * 16); } }')
rep('if (a) { cs_hist[0][a >> 14 > 128 ? 128 : a >> 14]++; cs_hist[1][t >> 14 > 128 ? 128 : t >> 14]++; } }',
    'long f = a + cs_w.stepb[0][1] + cs_w.stepb[1][1] + cs_w.stepb[2][1] + cs_w.stepb[3][1];\n    if (a) { cs_hist[0][a >> 14 > 512 ? 512 : a >> 14]++; cs_hist[1][t >> 14 > 512 ? 512 : t >> 14]++; }\n    if (f) cs_hist[2][f >> 14 > 512 ? 512 : f >> 14]++; }')
open(fn, 'w').write(s); print('patched', fn, n0, '->', hashlib.md5(s.encode()).hexdigest()[:8])
