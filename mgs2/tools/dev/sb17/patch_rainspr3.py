# patch_rainspr3.py (task #161, x86 check only): every sprite call the filter leaves out whose block the census does not find, not only the
# first 12: how many rest on a header whose third word is not 0 (a pool word: the census's search skips those headers), and the vblanks
# they fall in.  After patch_rainspr2.py.
import hashlib
T = '/home/claude/sb17/'
def edit(rel, md5, pairs):
    p = T + rel; s = open(p).read()
    assert hashlib.md5(s.encode()).hexdigest().startswith(md5), (rel, hashlib.md5(s.encode()).hexdigest()[:8])
    for old, new in pairs:
        assert s.count(old) == 1, (rel, old[:60], s.count(old)); s = s.replace(old, new)
    open(p, 'w').write(s); print(rel, hashlib.md5(s.encode()).hexdigest()[:8])
edit('boot/xlive_chk.c', '7028e7e7', [
 ("  static int shown2;   /* (the first left out with no census block: the nearest header below src that a block's end check accepts) */\n"
  "  if (w == -2 && out && shown2 < 12) { shown2++; uint32_t h = (c->src & ~3u) - 0x10, lo = c->src > 0x180000u ? c->src - 0x80000u : 0x100000u;\n"
  "    for (; h >= lo; h -= 4) { const uint32_t nx = *(const uint32_t *)(glf.ram + h + 4);\n"
  "      if (nx > c->src && nx - h <= 0x100000u && nx < (32u << 20) && !(nx & 3) && *(const uint32_t *)(glf.ram + nx) == h) break; }\n"
  "    fprintf(stderr, \"thin check sprites: no census block, left out: vb %ld src %08x\", cur_vb, c->src);\n",
  "  static int shown2;   /* (the first left out with no census block: the nearest header below src that a block's end check accepts) */\n"
  "  if (w == -2 && out) { uint32_t h = (c->src & ~3u) - 0x10, lo = c->src > 0x180000u ? c->src - 0x80000u : 0x100000u;\n"
  "    for (; h >= lo; h -= 4) { const uint32_t nx = *(const uint32_t *)(glf.ram + h + 4);\n"
  "      if (nx > c->src && nx - h <= 0x100000u && nx < (32u << 20) && !(nx & 3) && *(const uint32_t *)(glf.ram + nx) == h) break; }\n"
  "    ts_nb_n++; if (h >= lo && *(const uint32_t *)(glf.ram + h + 8)) ts_nb_pool++;   /* patch_rainspr3 */\n"
  "    if (ts_nb_vb0 < 0 || cur_vb < ts_nb_vb0) ts_nb_vb0 = cur_vb; if (cur_vb > ts_nb_vb1) ts_nb_vb1 = cur_vb;\n"
  "    if (shown2 >= 12) return; shown2++;\n"
  "    fprintf(stderr, \"thin check sprites: no census block, left out: vb %ld src %08x\", cur_vb, c->src);\n"),
 ("static long ts_own[4][2];   /* [one of the rain's four / another actor / no block / no owner or no source][left out] */\n",
  "static long ts_own[4][2];   /* [one of the rain's four / another actor / no block / no owner or no source][left out] */\n"
  "static long ts_nb_n, ts_nb_pool, ts_nb_vb0 = -1, ts_nb_vb1;   /* patch_rainspr3: left out with no census block: all; on a header whose\n"
  "                                                               third word is not 0; the first and last vblank */\n"),
 ("                ts_own[2][1], ts_own[2][0], ts_own[3][1], ts_own[3][0]); }\n",
  "                ts_own[2][1], ts_own[2][0], ts_own[3][1], ts_own[3][0]);\n"
  "      if (ts_nb_n) fprintf(stderr, \"xlive gb: thin check sprites: left out with no census block %ld: on a header whose third word is not 0 %ld; \"\n"
  "                           \"vb %ld..%ld\\n\", ts_nb_n, ts_nb_pool, ts_nb_vb0, ts_nb_vb1); }\n"),
])
