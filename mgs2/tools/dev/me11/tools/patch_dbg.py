#!/usr/bin/env python3
# patch_dbg.py <copy of xlive_chk.c>: me11 b debug (task #142): with MGS2_XLIVE_SNAPDBG=1 the overlay keeps the granules it does not lay
# (flag 0x100 in the record's kind) and, as it lays the others, compares those with the RAM the walk reads: each run that differs is
# printed (vb, channel, kind, address, bytes, changed bytes, whether its granules are upload sources now) and counted by 64 KB region.
import sys, hashlib
fn = sys.argv[1]
src = open(fn).read()
assert hashlib.md5(src.encode()).hexdigest().startswith('5c06fe51'), 'unexpected source'
def rep(old, new):
    global src
    assert src.count(old) == 1, ('anchor count', src.count(old), old[:60])
    src = src.replace(old, new)
rep('static int ov_pk, ov_nrg; static uint8_t ov_rg[512];',
    'static int ov_pk, ov_nrg; static uint8_t ov_rg[512];\n'
    'static int ov_dbg; static long ov_dprn, ov_dch[512], ov_dvb[512], ov_dst[512];   /* me11 b debug */')
# df_copy: runs of either acceptance; the others flagged (debug only)
rep('''      for (uint32_t g = 0; g <= n; g += 16) { int acc = g < n && (ov_up || !CS_UP(a + g) || (ov_pk && k < 2) || ov_rg[((a + g) >> 16) & 511]);
        if (acc && !in) { g0 = g; in = 1; }
        else if (!acc && in) { if (nr == cap) { cap *= 2; if (!(o = realloc(o, (size_t)cap * sizeof *o))) exit(1); }
          o[nr].a = a + g0; o[nr].n = (g < n ? g : n) - g0; o[nr].k = k; nr++; in = 0; } } }''',
    '''      if (ov_dbg) { int cur = -1;
        for (uint32_t g = 0; g <= n; g += 16) { int acc = g < n ? (ov_up || !CS_UP(a + g) || (ov_pk && k < 2) || ov_rg[((a + g) >> 16) & 511]) : -1;
          if (acc != cur) { if (cur >= 0) { if (nr == cap) { cap *= 2; if (!(o = realloc(o, (size_t)cap * sizeof *o))) exit(1); }
              o[nr].a = a + g0; o[nr].n = (g < n ? g : n) - g0; o[nr].k = k | (cur ? 0 : 0x100u); nr++; }
            g0 = g; cur = acc; } }
        continue; }
      for (uint32_t g = 0; g <= n; g += 16) { int acc = g < n && (ov_up || !CS_UP(a + g) || (ov_pk && k < 2) || ov_rg[((a + g) >> 16) & 511]);
        if (acc && !in) { g0 = g; in = 1; }
        else if (!acc && in) { if (nr == cap) { cap *= 2; if (!(o = realloc(o, (size_t)cap * sizeof *o))) exit(1); }
          o[nr].a = a + g0; o[nr].n = (g < n ? g : n) - g0; o[nr].k = k; nr++; in = 0; } } }''')
rep('''  for (long i = 0; i < df_nr; i++) { tot += 12 + df_r[i].n; b += df_r[i].n; ds_w.pb[DS_PLACE(df_r[i].a)] += df_r[i].n; }''',
    '''  for (long i = 0; i < df_nr; i++) { tot += 12 + df_r[i].n; if (df_r[i].k & 0x100u) continue; b += df_r[i].n; ds_w.pb[DS_PLACE(df_r[i].a)] += df_r[i].n; }''')
# ov_lay: flagged runs compared, not laid
rep('''  while (o < end) { uint32_t a, n; memcpy(&a, o, 4); memcpy(&n, o + 4, 4); memcpy(ov_save + sp, live + a, n); sp += n; memcpy(live + a, o + 12, n); o += 12 + n; }
}''',
    '''  while (o < end) { uint32_t a, n, k; memcpy(&a, o, 4); memcpy(&n, o + 4, 4); memcpy(&k, o + 8, 4);
    if (k & 0x100u) { if (memcmp(live + a, o + 12, n)) { long c = 0; int up = 1; uint32_t r = (a >> 16) & 511;
        for (uint32_t i = 0; i < n; i++) c += live[a + i] != o[12 + i];
        for (uint32_t g = 0; g < n; g += 16) up &= CS_UP(a + g);
        ov_dch[r] += c; if (ov_dvb[r] != e->vb) { ov_dvb[r] = e->vb; ov_dst[r]++; }
        if (ov_dprn < 300) { ov_dprn++; fprintf(stderr, "me11 dbg: vb %ld ch %d kind %u at %07x bytes %u: %ld changed, all upload sources now %d\\n",
                                                   e->vb, e->ch, k & 0xff, a, n, c, up); } }
      o += 12 + n; continue; }
    memcpy(ov_save + sp, live + a, n); sp += n; memcpy(live + a, o + 12, n); o += 12 + n; }
}''')
# ov_back: flagged runs were not laid
rep('''  while (o < end) { uint32_t n; memcpy(&n, o + 4, 4);
    if (nr == rcap)''',
    '''  while (o < end) { uint32_t n, k; memcpy(&n, o + 4, 4); memcpy(&k, o + 8, 4); if (k & 0x100u) { o += 12 + n; continue; }
    if (nr == rcap)''')
rep('''          ov_pk = getenv("MGS2_XLIVE_SNAPP") && atoi(getenv("MGS2_XLIVE_SNAPP"));\n''',
    '''          ov_pk = getenv("MGS2_XLIVE_SNAPP") && atoi(getenv("MGS2_XLIVE_SNAPP"));\n'''
    '''          ov_dbg = getenv("MGS2_XLIVE_SNAPDBG") && atoi(getenv("MGS2_XLIVE_SNAPDBG"));\n'''
    '''          if (ov_dbg) { fprintf(stderr, "xlive mode: me11 b debug: the granules not laid are compared with RAM as the walk reads it (MGS2_XLIVE_SNAPDBG)\\n");\n'''
    '''            atexit(ov_dsum); }\n''')
rep('''static void ov_lay(DfE *e)''',
    '''static void ov_dsum(void)
{
  for (int r = 0; r < 512; r++) if (ov_dch[r]) fprintf(stderr, "me11 dbg region %03x0000: %ld bytes changed in %ld steps\\n", r, ov_dch[r], ov_dst[r]);
  fprintf(stderr, "me11 dbg: %ld runs printed\\n", ov_dprn);
}
static void ov_lay(DfE *e)''')
open(fn, 'w').write(src)
print('patched', hashlib.md5(src.encode()).hexdigest())
