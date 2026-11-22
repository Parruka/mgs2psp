# patch_me14_lc.py <gsup2.c> <out>: task #149 step 2, the memory finding: the GS map's local_copy reads every texel of the source before
# it makes the copy's buffer.  On these routes almost every local copy (a GS-to-GS transfer) reads an area the map does not know (a
# drawn one: 1,727 of the deck route's 1,728), and its buffer (x->need bytes, up to a frame's size) was made, zeroed and dropped each
# time; on the ME's heap that buffer set the region's size (the peak 1,018 KB above what the picture keeps).  The reads change nothing
# (gu_word, gu_texel, gu_index, the RAM readers: no counter, no state), so the second pass reads the same values as the first.
import sys
s = open(sys.argv[1], encoding='utf-8').read()
def rep(old, new, n=1):
    global s
    c = s.count(old)
    if c != n: sys.exit('%d matches (want %d) for: %s' % (c, n, old[:100]))
    s = s.replace(old, new)
rep('''static uint8_t *local_copy(GuMap *m, const Rec *x, uint64_t b, uint64_t p)
{''', '''/* one texel of a local copy's source as local_copy reads it (an index or 16-bit texel, or a 32-bit word): 0 when the map does not know
   all of its bits */
static inline int lc_src(const GuMap *m, int spsm, uint32_t sbp, uint32_t sbw, int texel, int sx, int sy, uint32_t *v)
{
  if (texel) { int64_t k = gu_texel(m, spsm, sbp, sbw, sx, sy); if (k < 0) return 0; *v = (uint32_t)k; return 1; }
  uint32_t kn, mk = spsm == GS_CT24 ? 0xffffffu : 0xffffffffu; *v = gu_word(m, gs_pa32(sbp, sbw, sx, sy, 0) >> 2, &kn); return (kn & mk) == mk;
}
static uint8_t *local_copy(GuMap *m, const Rec *x, uint64_t b, uint64_t p)
{''')
rep('''  uint8_t *o = calloc(1, x->need ? x->need : 1); if (!o) return 0;
  for (int j = 0; j < x->h; j++)
    for (int i = 0; i < x->w; i++) {
      uint32_t pi = (uint32_t)j * (uint32_t)x->w + (uint32_t)i, v, kn; int sx = (ssx + i) & 2047, sy = (ssy + j) & 2047;
      if (sidx || s16) { int64_t k = gu_texel(m, spsm, sbp, sbw, sx, sy); if (k < 0) { free(o); return 0; } v = (uint32_t)k; kn = 0; }
      else { v = gu_word(m, gs_pa32(sbp, sbw, sx, sy, 0) >> 2, &kn); if ((kn & (spsm == GS_CT24 ? 0xffffffu : 0xffffffffu)) != (spsm == GS_CT24 ? 0xffffffu : 0xffffffffu)) { free(o); return 0; } }
''', '''  /* task #149: every texel of the source first, the buffer only when the map knows them all (most local copies read a drawn area) */
  int tx = sidx || s16; uint32_t v;
  for (int j = 0; j < x->h; j++)
    for (int i = 0; i < x->w; i++) if (!lc_src(m, spsm, sbp, sbw, tx, (ssx + i) & 2047, (ssy + j) & 2047, &v)) return 0;
  uint8_t *o = calloc(1, x->need ? x->need : 1); if (!o) return 0;
  for (int j = 0; j < x->h; j++)
    for (int i = 0; i < x->w; i++) {
      uint32_t pi = (uint32_t)j * (uint32_t)x->w + (uint32_t)i;
      if (!lc_src(m, spsm, sbp, sbw, tx, (ssx + i) & 2047, (ssy + j) & 2047, &v)) { free(o); return 0; }   /* (cannot happen) */
''')
open(sys.argv[2], 'w', encoding='utf-8').write(s)
