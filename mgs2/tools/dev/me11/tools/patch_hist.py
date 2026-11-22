#!/usr/bin/env python3
# patch_hist.py <copy of xlive_chk.c with patch_dbg>: me11 b (task #142): the per-step copy sizes after the boot (steps from vb 61 on with
# transfers; the bytes the overlay keeps, so with MGS2_XLIVE_SNAPK/U/P/R the design's copy) in 16 KB bins, printed at exit
# ("me11 hist: <KB from> <steps>"), with the largest step and its vblank.
import sys, hashlib
fn = sys.argv[1]
src = open(fn).read()
assert hashlib.md5(src.encode()).hexdigest().startswith('388f619d'), 'unexpected source'
def rep(old, new):
    global src
    assert src.count(old) == 1, ('anchor count', src.count(old), old[:60])
    src = src.replace(old, new)
rep('static int ov_dbg; static long ov_dprn, ov_dch[512], ov_dvb[512], ov_dst[512];   /* me11 b debug */',
    'static int ov_dbg; static long ov_dprn, ov_dch[512], ov_dvb[512], ov_dst[512];   /* me11 b debug */\n'
    'static long hs_bin[1024], hs_max, hs_maxvb, hs_n;   /* me11 b: per-step copy sizes after the boot, 16 KB bins */')
rep('''  df_ntr = 0; if (ds_step > ds_w.maxb) ds_w.maxb = ds_step;
  ds_step = 0;''',
    '''  df_ntr = 0; if (ds_step > ds_w.maxb) ds_w.maxb = ds_step;
  if (ds_step && vb > 60) { long bn = ds_step >> 14; hs_bin[bn > 1023 ? 1023 : bn]++; hs_n++; if (ds_step > hs_max) { hs_max = ds_step; hs_maxvb = vb; } }
  ds_step = 0;''')
rep('''static void ov_dsum(void)
{''',
    '''static void hs_print(void)
{
  fprintf(stderr, "me11 hist: %ld steps with transfers after vb 60, the largest %.1f KB at vb %ld\\n", hs_n, hs_max / 1024.0, hs_maxvb);
  for (int i = 0; i < 1024; i++) if (hs_bin[i]) fprintf(stderr, "me11 hist: %d %ld\\n", i * 16, hs_bin[i]);
}
static void ov_dsum(void)
{''')
rep('''          ov_dbg = getenv("MGS2_XLIVE_SNAPDBG") && atoi(getenv("MGS2_XLIVE_SNAPDBG"));\n''',
    '''          ov_dbg = getenv("MGS2_XLIVE_SNAPDBG") && atoi(getenv("MGS2_XLIVE_SNAPDBG")); atexit(hs_print);\n''')
open(fn, 'w').write(src)
print('patched', hashlib.md5(src.encode()).hexdigest())
