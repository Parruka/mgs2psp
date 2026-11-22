# patch_rainspr2.py (task #161, x86 check only): the first sprite calls the filter leaves out whose block the census does not find: the
# allocator header the filter's range rests on (the census's search skips a header whose third word is not 0).  After patch_rainspr.py.
import hashlib
T = '/home/claude/sb17/'
def edit(rel, md5, pairs):
    p = T + rel; s = open(p).read()
    assert hashlib.md5(s.encode()).hexdigest().startswith(md5), (rel, hashlib.md5(s.encode()).hexdigest()[:8])
    for old, new in pairs:
        assert s.count(old) == 1, (rel, old[:60], s.count(old)); s = s.replace(old, new)
    open(p, 'w').write(s); print(rel, hashlib.md5(s.encode()).hexdigest()[:8])
edit('boot/xlive_chk.c', '17d62c69', [
 ("  if (w == 0 && rs != out && shown < 12) { shown++; fprintf(stderr, \"thin check sprites: vb %ld entry %06x src %08x owner %s left out %d\\n\", cur_vb, c->entry, c->src, nm, out); }\n}",
  "  if (w == 0 && rs != out && shown < 12) { shown++; fprintf(stderr, \"thin check sprites: vb %ld entry %06x src %08x owner %s left out %d\\n\", cur_vb, c->entry, c->src, nm, out); }\n"
  "  static int shown2;   /* (the first left out with no census block: the nearest header below src that a block's end check accepts) */\n"
  "  if (w == -2 && out && shown2 < 12) { shown2++; uint32_t h = (c->src & ~3u) - 0x10, lo = c->src > 0x180000u ? c->src - 0x80000u : 0x100000u;\n"
  "    for (; h >= lo; h -= 4) { const uint32_t nx = *(const uint32_t *)(glf.ram + h + 4);\n"
  "      if (nx > c->src && nx - h <= 0x100000u && nx < (32u << 20) && !(nx & 3) && *(const uint32_t *)(glf.ram + nx) == h) break; }\n"
  "    fprintf(stderr, \"thin check sprites: no census block, left out: vb %ld src %08x\", cur_vb, c->src);\n"
  "    if (h >= lo) fprintf(stderr, \" header %08x: %08x %08x %08x %08x\", h, *(const uint32_t *)(glf.ram + h), *(const uint32_t *)(glf.ram + h + 4),\n"
  "                         *(const uint32_t *)(glf.ram + h + 8), *(const uint32_t *)(glf.ram + h + 12));\n"
  "    fputc('\\n', stderr); }\n}"),
])
