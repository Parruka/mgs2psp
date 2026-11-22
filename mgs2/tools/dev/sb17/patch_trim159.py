# patch_trim159.py (task #159): in /home/claude/sb17, after patch_rainspr.py.  The masters of sb17 (s17h, s17t, s17n) put the break's
# highest point 88 KB above gl16's, at vb 10,849: the texture cache's entry table doubles there (512 to 1,024 entries, 64 KB to 128 KB)
# while it ends at the top of newlib's heap, and newlib's realloc then takes the whole new size from a top grown by sbrk, not the growth
# (me_step2_notes.md, "newlib's realloc"; me14p8 fixed it in its resize hook).  The same fix here, on the PSP only: after a table growth
# whose old block ended at the top chunk while the top was short, newlib's free top is given back at once (malloc_trim(0)).  x86: no
# change.  The picture and every hash stay as they are (only where the heap's break stands moves).  Asserts each file's md5 before the edit.
import hashlib
T = '/home/claude/sb17/'
def edit(rel, md5, pairs):
    p = T + rel; s = open(p).read()
    assert hashlib.md5(s.encode()).hexdigest().startswith(md5), (rel, hashlib.md5(s.encode()).hexdigest()[:8])
    for old, new in pairs:
        assert s.count(old) == 1, (rel, old[:60], s.count(old)); s = s.replace(old, new)
    open(p, 'w').write(s); print(rel, hashlib.md5(s.encode()).hexdigest()[:8])

edit('ge/texcache.h', 'e158edb0', [
 ("  long uf_hits, uf_fill, uf_chk, uf_bad;   /* sb10 T1 tk_ufront: look-ups the front answered; entries put; mode 2 answers compared, differing */",
  "  long uf_hits, uf_fill, uf_chk, uf_bad;   /* sb10 T1 tk_ufront: look-ups the front answered; entries put; mode 2 answers compared, differing */\n"
  "  long tab_grows, tab_cap, tab_trims;   /* task #159: the entry table's growths, its entries now; on the PSP, growths after which newlib's\n"
  "                                  free top went back (malloc_trim) */"),
])

edit('ge/texcache.c', '37b02643', [
 ('#include "gslayout.h"\n',
  '#include "gslayout.h"\n'
  '#ifdef __psp__\n'
  '#include <malloc.h>     /* task #159: newlib\'s mallinfo, malloc_usable_size and malloc_trim */\n'
  '#include <unistd.h>     /* sbrk */\n'
  '#endif\n'),
 ("static int insert(TkCache *c, uint64_t id, int pal, uint8_t *data, uint32_t size)\n"
  "{\n"
  "  make_room(c, size);\n"
  "  if (c->freel < 0) {\n"
  "    int n = c->cap ? 2 * c->cap : 256; Ent *q = realloc(c->e, (size_t)n * sizeof *q); if (!q) return -1;\n",
  "/* task #159: newlib's realloc (dlmalloc 2.6) grows a block that ends at the top chunk in place only when the top already holds the growth;\n"
  "   otherwise it takes the whole new size from a top grown by sbrk and joins it to the block, so the break rises by the new size, not the\n"
  "   growth, and the rest stays free at the top, under the 128 KB at which newlib gives memory back by itself (me_step2_notes.md).  So on\n"
  "   the PSP, after a growth of a table that ended at the top chunk (its block ends malloc_usable_size - 4 bytes on, newlib's next chunk;\n"
  "   the top, mallinfo's keepcost, runs to the break) while the top was short, the free top is given back at once (malloc_trim): the break\n"
  "   then stands where a growth in place would have left it (me14p8's resize hook, psp/xl_psp.c in me14). */\n"
  "static Ent *grow_table(TkCache *c, int n)\n"
  "{\n"
  "  size_t want = (size_t)n * sizeof(Ent);\n"
  "#ifdef __psp__\n"
  "  int top = 0;\n"
  "  if (c->e) {\n"
  "    struct mallinfo mi = mallinfo(); size_t have = malloc_usable_size(c->e);\n"
  "    top = want > have && (uint8_t *)c->e + have - 4 + mi.keepcost == (uint8_t *)sbrk(0) && mi.keepcost < want - have + 64;\n"
  "  }\n"
  "  Ent *q = realloc(c->e, want);\n"
  "  if (top && q) c->st.tab_trims += malloc_trim(0);\n"
  "#else\n"
  "  Ent *q = realloc(c->e, want);\n"
  "#endif\n"
  "  if (q) { c->st.tab_grows++; c->st.tab_cap = n; }\n"
  "  return q;\n"
  "}\n"
  "static int insert(TkCache *c, uint64_t id, int pal, uint8_t *data, uint32_t size)\n"
  "{\n"
  "  make_room(c, size);\n"
  "  if (c->freel < 0) {\n"
  "    int n = c->cap ? 2 * c->cap : 256; Ent *q = grow_table(c, n); if (!q) return -1;\n"),
])

edit('psp/xl_psp.c', 'a76c8bd2', [
 ('            "from one upload\\n", k->blk, k->blk_back, gu_stats(map)->blocks, gu_stats(map)->blocks_one);\n',
  '            "from one upload\\n", k->blk, k->blk_back, gu_stats(map)->blocks, gu_stats(map)->blocks_one);\n'
  '    fprintf(f, "picture: the texture cache\'s table: grown %ld times, %ld entries now; newlib\'s free top given back after %ld of the "\n'
  '            "growths (task #159)\\n", k->tab_grows, k->tab_cap, k->tab_trims);\n'),
])
