#!/usr/bin/env python3
# patch_tally.py <copy of xlive_chk.c with patch_dbg>: me11 b census by granule (task #142): with MGS2_XLIVE_SNAPDBG=1, after each
# transfer's late walk (so its own uploads are marked), every 16-byte granule it read is tallied by its own 64 KB region (not the range's
# start, as me10's census did): upload-source bytes read, and bytes changed between the kick and the walk (the copy against RAM as the
# walk found it: the saved live bytes for the granules laid, RAM for the others), upload and other kinds apart, laid or not; steps after
# vb 60 only (the boot's apart).  "me11 rg" lines at exit.
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
    'static long tg_up[512], tg_upl[512], tg_cu[512], tg_cun[512], tg_co[512], tg_st[512], tg_last[512], tg_boot[512], tg_frames, tg_fvb = -1;')
rep('''static void ov_back(DfE *e)''',
    '''static void ov_tally(DfE *e)   /* after the walk, before ov_back: per granule, by its own region */
{
  const uint8_t *o = e->lg, *end = e->lg + e->ln; size_t sp = 0; const uint8_t *live = (const uint8_t *)(uintptr_t)df_live; int boot = e->vb <= 60;
  if (!boot && e->vb != tg_fvb) { tg_fvb = e->vb; tg_frames++; }
  while (o < end) { uint32_t a, n, k; memcpy(&a, o, 4); memcpy(&n, o + 4, 4); memcpy(&k, o + 8, 4); int laid = !(k & 0x100u);
    const uint8_t *cp = o + 12, *late = laid ? ov_save + sp : live + a;
    for (uint32_t g = 0; g < n; g += 16) { uint32_t m = n - g < 16 ? n - g : 16, r = ((a + g) >> 16) & 511; int u = CS_UP(a + g); long c = 0;
      if (memcmp(cp + g, late + g, m)) for (uint32_t i = 0; i < m; i++) c += cp[g + i] != late[g + i];
      if (boot) { tg_boot[r] += c; continue; }
      if (u) { tg_up[r] += m; if (laid) tg_upl[r] += m; tg_cu[r] += c; if (!laid) tg_cun[r] += c; } else tg_co[r] += c;
      if (c && u && tg_last[r] != e->vb) { tg_last[r] = e->vb; tg_st[r]++; } }
    if (laid) sp += n;
    o += 12 + n; }
}
static void ov_back(DfE *e)''')
rep('''      clock_gettime(CLOCK_MONOTONIC, &b); t_walk += df_ms(&a, &b); ov_back(e); return; }   /* me11 */''',
    '''      clock_gettime(CLOCK_MONOTONIC, &b); t_walk += df_ms(&a, &b); if (ov_dbg) ov_tally(e); ov_back(e); return; }   /* me11 */''')
rep('''static void ov_dsum(void)
{''',
    '''static void ov_dsum(void)
{
  fprintf(stderr, "me11 rg: by granule, %ld shown frames after vb 60 (KB a frame: upload sources read, of them laid; bytes changed in flight: upload sources, of them not laid, steps; other kinds; the boot's)\\n", tg_frames);
  for (int r = 0; r < 512; r++) if (tg_up[r] || tg_co[r] || tg_boot[r])
    fprintf(stderr, "me11 rg region %03x0000: uploads %.1f KB a frame (laid %.1f); changed uploads %ld bytes (not laid %ld) in %ld steps; other kinds %ld; boot %ld\\n",
            r, tg_frames ? tg_up[r] / 1024.0 / tg_frames : 0, tg_frames ? tg_upl[r] / 1024.0 / tg_frames : 0, tg_cu[r], tg_cun[r], tg_st[r], tg_co[r], tg_boot[r]);''')
open(fn, 'w').write(src)
print('patched', hashlib.md5(src.encode()).hexdigest())
