#!/usr/bin/env python3
# patch_rg.py <xlive_chk.c>: me11 (task #142) the design's copy set: with MGS2_XLIVE_SNAPK and MGS2_XLIVE_SNAPU=0, upload granules are
# copied too when they lie in the packets (kinds 0 and 1: tags and the data after them; MGS2_XLIVE_SNAPP=1) or in one of the listed
# 64 KB regions of EE RAM (MGS2_XLIVE_SNAPR=<hex>,<hex>,...: the regions whose texture sources the game rewrites).  Each anchor once.
import sys, hashlib
fn = sys.argv[1]
src = open(fn).read()
assert hashlib.md5(src.encode()).hexdigest().startswith('5287423e'), 'unexpected source'
def rep(old, new):
    global src
    assert src.count(old) == 1, ('anchor count', src.count(old), old[:60])
    src = src.replace(old, new)
rep('static int ov_on, ov_kmask, ov_up; static uint8_t *ov_save; static size_t ov_cap;',
    'static int ov_on, ov_kmask, ov_up; static uint8_t *ov_save; static size_t ov_cap;\n'
    'static int ov_pk, ov_nrg; static uint8_t ov_rg[512];   /* me11 b: the packets\' upload granules, and those in the listed 64 KB regions */')
rep('int acc = g < n && (ov_up || !CS_UP(a + g));',
    'int acc = g < n && (ov_up || !CS_UP(a + g) || (ov_pk && k < 2) || ov_rg[((a + g) >> 16) & 511]);')
rep('ov_up = getenv("MGS2_XLIVE_SNAPU") && atoi(getenv("MGS2_XLIVE_SNAPU")); df_snap = 2;\n',
    'ov_up = getenv("MGS2_XLIVE_SNAPU") && atoi(getenv("MGS2_XLIVE_SNAPU")); df_snap = 2;\n'
    '          ov_pk = getenv("MGS2_XLIVE_SNAPP") && atoi(getenv("MGS2_XLIVE_SNAPP"));\n'
    '          if (getenv("MGS2_XLIVE_SNAPR")) { const char *s = getenv("MGS2_XLIVE_SNAPR"); while (*s) { char *e; unsigned long r = strtoul(s, &e, 16);\n'
    '              if (e == s) break; if (r < 512 && !ov_rg[r]) { ov_rg[r] = 1; ov_nrg++; } s = *e ? e + 1 : e; } }\n'
    '          if (ov_pk || ov_nrg) fprintf(stderr, "xlive mode: me11 b: upload granules copied too:%s%s %d regions of 64 KB (MGS2_XLIVE_SNAPP, MGS2_XLIVE_SNAPR)\\n",\n'
    '                                       ov_pk ? " the packets\' own," : "", ov_nrg ? " those in" : "", ov_nrg);\n')
open(fn, 'w').write(src)
print('patched', hashlib.md5(src.encode()).hexdigest())
