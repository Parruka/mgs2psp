/* texcache.c: see texcache.h. */
#include <stdlib.h>
#include <string.h>
#include "texcache.h"
#include "gslayout.h"

typedef struct {
  uint64_t id;                 /* 0: a free entry */
  int pal;                     /* 0: a texture (t, data), 1: a palette (npal entries in data) */
  TcTex t; uint8_t *data; uint32_t size; int npal;
  long frame;                  /* the last frame that used it */
  uint64_t ma_pid; int ma;     /* a texture: the palette identity its last minalpha was worked out with, and that minalpha */
  int prev, next, hnext;       /* LRU list (head: the most recent), hash chain; free list in next */
} Ent;
struct TkCache {
  GuMap *map; uint32_t budget; long frame;
  Ent *e; int cap, freel;
  int *bucket; uint32_t nbucket;   /* a power of two */
  int head, tail;
  TkStats st;
  uint8_t *pool; uint32_t punits, pfree;   /* tk_pool: 64-byte units; the first free run's unit (TK_NIL: none) */
  int blk;                                 /* tk_blk */
  int flags;                               /* tk_flags */
  int t16;                                 /* tk_t16 */
};
#define TK_NIL 0xffffffffu

static uint64_t mix(uint64_t h, uint64_t v) { h = (h ^ v) * 0x9e3779b97f4a7c15ull; return h ^ h >> 31; }

TkCache *tk_new(GuMap *map, uint32_t budget)
{
  TkCache *c = calloc(1, sizeof *c); if (!c) return 0;
  c->map = map; c->budget = budget; c->freel = c->head = c->tail = -1; c->nbucket = 4096; c->blk = 1; c->flags = 1; c->t16 = 1;
  if (!(c->bucket = malloc(c->nbucket * sizeof *c->bucket))) { free(c); return 0; }
  for (uint32_t i = 0; i < c->nbucket; i++) c->bucket[i] = -1;
  return c;
}
void tk_free(TkCache *c)
{
  if (!c) return;
  if (!c->pool) for (int i = 0; i < c->cap; i++) free(c->e[i].data);
  free(c->e); free(c->bucket); free(c);
}
const TkStats *tk_stats(const TkCache *c) { return &c->st; }
void tk_blk(TkCache *c, int mode) { c->blk = mode; }
void tk_flags(TkCache *c, int on) { c->flags = on; }
void tk_t16(TkCache *c, int on) { c->t16 = on; }
int tk_t16_on(const TkCache *c) { return c->t16; }

/* ---- tk_pool: a free run's first 8 bytes hold the next run's unit and its own length in units ---- */
typedef struct { uint32_t next, len; } Run;
static Run *run_at(TkCache *c, uint32_t u) { return (Run *)(c->pool + (size_t)u * 64); }
int tk_pool(TkCache *c, void *mem, uint32_t size)
{
  uintptr_t a = ((uintptr_t)mem + 63) & ~(uintptr_t)63; uint32_t skip = (uint32_t)(a - (uintptr_t)mem);
  if (!mem || size < skip + 64 || c->cap) return -1;
  c->pool = (uint8_t *)a; c->punits = (size - skip) / 64; c->pfree = 0;
  Run *r = run_at(c, 0); r->next = TK_NIL; r->len = c->punits;
  return 0;
}
static void *p_alloc(TkCache *c, uint32_t size)   /* first fit */
{
  uint32_t n = size ? (size + 63) / 64 : 1, *link = &c->pfree;
  for (uint32_t u = c->pfree; u != TK_NIL; link = &run_at(c, u)->next, u = *link) {
    Run *r = run_at(c, u);
    if (r->len < n) continue;
    if (r->len == n) *link = r->next;
    else { Run *s = run_at(c, u + n); s->next = r->next; s->len = r->len - n; *link = u + n; }
    return c->pool + (size_t)u * 64;
  }
  return 0;
}
static void p_free(TkCache *c, void *p, uint32_t size)   /* back into the list in address order, joined with its neighbours */
{
  uint32_t u = (uint32_t)(((uint8_t *)p - c->pool) / 64), n = size ? (size + 63) / 64 : 1, prev = TK_NIL, v = c->pfree;
  while (v != TK_NIL && v < u) { prev = v; v = run_at(c, v)->next; }
  Run *b = run_at(c, u); b->len = n; b->next = v;
  if (v != TK_NIL && u + n == v) { Run *nx = run_at(c, v); b->len += nx->len; b->next = nx->next; }
  if (prev == TK_NIL) c->pfree = u;
  else { Run *pr = run_at(c, prev); if (prev + pr->len == u) { pr->len += b->len; pr->next = b->next; } else pr->next = u; }
}
static void tk_release(TkCache *c, void *p, uint32_t size) { if (!p) return; if (c->pool) p_free(c, p, size); else free(p); }

static void lru_unlink(TkCache *c, int i)
{
  Ent *e = &c->e[i];
  if (e->prev >= 0) c->e[e->prev].next = e->next; else c->head = e->next;
  if (e->next >= 0) c->e[e->next].prev = e->prev; else c->tail = e->prev;
}
static void lru_front(TkCache *c, int i)
{
  Ent *e = &c->e[i]; e->prev = -1; e->next = c->head;
  if (c->head >= 0) c->e[c->head].prev = i; else c->tail = i;
  c->head = i;
}
static int find(TkCache *c, uint64_t id)
{
  for (int i = c->bucket[id & (c->nbucket - 1)]; i >= 0; i = c->e[i].hnext) if (c->e[i].id == id) return i;
  return -1;
}
static void drop(TkCache *c, int i)
{
  Ent *e = &c->e[i]; int *p = &c->bucket[e->id & (c->nbucket - 1)];
  while (*p != i) p = &c->e[*p].hnext;
  *p = e->hnext;
  lru_unlink(c, i);
  c->st.live_bytes -= e->size; c->st.live--; c->st.evicted++;
  tk_release(c, e->data, e->size); memset(e, 0, sizeof *e); e->next = c->freel; c->freel = i;
}
/* size bytes for a conversion: with a pool, when nothing fits, the least recently used entries that no frame in flight uses go until it does */
static void *tk_alloc(TkCache *c, uint32_t size)
{
  if (!c->pool) return malloc(size);
  void *p = p_alloc(c, size);
  for (int i = c->tail; !p && i >= 0; ) {
    int prev = c->e[i].prev;
    if (c->e[i].frame < c->frame - 1) { drop(c, i); p = p_alloc(c, size); }
    i = prev;
  }
  if (!p) c->st.nomem++;
  return p;
}
/* room for size more bytes: the least recently used entries that no frame in flight uses go first */
static void make_room(TkCache *c, uint32_t size)
{
  if (!c->budget) return;
  for (int i = c->tail; i >= 0 && c->st.live_bytes + size > c->budget; ) {
    int prev = c->e[i].prev;
    if (c->e[i].frame < c->frame - 1) drop(c, i);
    i = prev;
  }
}
static int insert(TkCache *c, uint64_t id, int pal, uint8_t *data, uint32_t size)
{
  make_room(c, size);
  if (c->freel < 0) {
    int n = c->cap ? 2 * c->cap : 256; Ent *q = realloc(c->e, (size_t)n * sizeof *q); if (!q) return -1;
    memset(q + c->cap, 0, (size_t)(n - c->cap) * sizeof *q);
    for (int i = n - 1; i >= c->cap; i--) { q[i].next = c->freel; c->freel = i; }
    c->e = q; c->cap = n;
  }
  int i = c->freel; Ent *e = &c->e[i]; c->freel = e->next;
  memset(e, 0, sizeof *e); e->id = id; e->pal = pal; e->data = data; e->size = size; e->frame = -2;
  e->hnext = c->bucket[id & (c->nbucket - 1)]; c->bucket[id & (c->nbucket - 1)] = i;
  lru_front(c, i);
  c->st.live_bytes += size; c->st.live++; c->st.conv_bytes += size;
  if (c->st.live_bytes > c->st.peak_bytes) c->st.peak_bytes = c->st.live_bytes;
  return i;
}
static void touch(TkCache *c, int i)
{
  Ent *e = &c->e[i];
  if (e->frame != c->frame) { e->frame = c->frame; c->st.frame_bytes += e->size; c->st.frame_n++;
    if (c->st.frame_bytes > c->st.peak_frame_bytes) c->st.peak_frame_bytes = c->st.frame_bytes; }
  if (c->head != i) { lru_unlink(c, i); lru_front(c, i); }
}
void tk_frame(TkCache *c) { c->frame++; c->st.frame_bytes = 0; c->st.frame_n = 0; }

typedef struct { GuMap *m; int psm; uint32_t tbp, tbw; long unk; int a16; } Fe;
static int fe(void *ctx, int x, int y)
{
  Fe *f = ctx; int64_t v = gu_texel(f->m, f->psm, f->tbp, f->tbw, x, y);
  if (v < 0) { f->unk++; return 0; }
  return (int)v;
}
/* a 16-bit texture's texels (n bytes) with the GE's alpha bit set from the alpha TEXA gives each (A bit 1: TA1; 0: 0 when AEM and
   RGB = 0, else TA0; tools/gsref.c): a16 bit 0: TA0 >= 0x80, bit 1: TA1 >= 0x80, bit 2: AEM.  TA0 0 and TA1 >= 0x80 (a16 2 or 6) is the
   A bit itself: nothing to do */
static void alpha16(uint8_t *d, size_t n, int a16)
{
  if ((a16 & 3) == 2) return;
  if (a16 == 3) { for (size_t i = 1; i < n; i += 2) d[i] |= 0x80; return; }                        /* every texel 1.0 (the codec's overlays) */
  if (a16 == 7) { for (size_t i = 1; i < n; i += 2) if (d[i] | d[i - 1]) d[i] |= 0x80; return; }   /* 1.0, but 0 where all 16 bits are 0 (its glow) */
  for (size_t i = 0; i + 1 < n; i += 2) {
    int v = d[i] | d[i + 1] << 8, c = v & 0x7fff, a = (v & 0x8000) ? a16 >> 1 & 1 : ((a16 & 4) && !c) ? 0 : a16 & 1;
    d[i + 1] = (uint8_t)(c >> 8 | a << 7);
  }
}
static void fb(void *ctx, uint32_t bn, uint32_t v[64], uint32_t kn[64])   /* the bits a texel of the format can be in */
{ Fe *f = ctx; gu_block(f->m, bn, f->psm == GS_T8H ? 0xff000000u : f->psm == GS_T4HL ? 0x0f000000u : f->psm == GS_T4HH ? 0xf0000000u : ~0u, v, kn); }
/* the texture into d (size bytes): a block at a time, or texel by texel when that is off or hands it back; 0, or -1 if unsupported */
static int convert(TkCache *c, uint64_t t0, uint64_t cl, Fe *fx, TcTex *t, uint8_t *d, size_t size)
{
  int r = c->blk ? tc_texture_blk(t0, cl, fb, fx, t, d) : 1;
  if (r > 0) { if (c->blk) c->st.blk_back++; r = tc_texture_cb(t0, cl, fe, fx, t, d); }
  else if (!r) {
    c->st.blk++;
    if (c->blk == 2) {   /* dev: the texel path as well, into a buffer of its own */
      uint8_t *d2 = malloc(size ? size : 1); TcTex t2; Fe f2 = *fx;
      if (d2 && (tc_texture_cb(t0, cl, fe, &f2, &t2, d2) || f2.unk != fx->unk || memcmp(d, d2, size) || memcmp(t, &t2, sizeof t2))) c->st.blk_diff++;
      free(d2);
    }
  }
  if (!r && fx->a16 >= 0) alpha16(d, size, fx->a16);
  return r;
}
/* CLUT entry k as the GE's 8888 (PS2 alpha 0x80 = 0xff) and its PS2 alpha in *pa; a CT16 entry's alpha by TEXA as the GS expands it */
static uint32_t pal_entry(const GuMap *m, int cpsm, int k, uint64_t texa, long *unk, uint8_t *pa)
{
  int64_t v = gu_clut(m, cpsm, k); uint32_t c, a;
  *pa = 0;
  if (v < 0) { (*unk)++; return 0; }
  if (cpsm == GS_CT32 || cpsm == GS_CT24) { c = (uint32_t)v & 0xffffff; a = cpsm == GS_CT24 ? (uint32_t)(texa & 0xff) : (uint32_t)v >> 24; }
  else {
    uint32_t s = (uint32_t)v; c = (s & 31) << 3 | (s >> 5 & 31) << 11 | (s >> 10 & 31) << 19;
    a = (s >> 15 & 1) ? (uint32_t)(texa >> 32 & 0xff) : ((texa >> 15 & 1) && !(s & 0x7fff)) ? 0 : (uint32_t)(texa & 0xff);
  }
  *pa = (uint8_t)a;
  return c | (a >= 0x80 ? 0xffu : a * 2) << 24;
}
int tk_get(TkCache *c, const uint64_t *gr, int ctxt, TkTex *o) { return tk_get_cl(c, gr, ctxt, gr[0x08 + ctxt], o); }
int tk_get_cl(TkCache *c, const uint64_t *gr, int ctxt, uint64_t cl, TkTex *o)
{
  uint64_t t0 = gr[0x06 + ctxt], texa = gr[0x3b];
  int psm = (int)(t0 >> 20 & 63), cpsm = (int)(t0 >> 51 & 15), csa = (int)(t0 >> 56 & 31), bits, u0, v0, w, h, f;
  uint32_t tbp = (uint32_t)(t0 & 0x3fff), tbw = (uint32_t)(t0 >> 14 & 63);
  memset(o, 0, sizeof *o);
  c->st.uses++;
  int idx = psm == GS_T8 || psm == GS_T4 || psm == GS_T8H || psm == GS_T4HL || psm == GS_T4HH;
  int d16 = psm == GS_CT16 || psm == GS_CT16S;
  /* a 16-bit texture becomes a GE 5551 texture, whose alpha bit gives 0 or 255: each alpha TEXA gives its texels (TA0, TA1, and 0 with
     AEM) must be 0 or at least 0x80, and fe16 sets the bit from it (task #83: the codec's images have TA0 = TA1 = 0x80) */
  uint32_t ta0 = (uint32_t)(texa & 0xff), ta1 = (uint32_t)(texa >> 32 & 0xff);
  int texa_ok = c->t16 ? (ta0 == 0 || ta0 >= 0x80) && (ta1 == 0 || ta1 >= 0x80) : ta0 == 0 && ta1 >= 0x80;
  if ((!idx && !(d16 && texa_ok)) || tc_region(t0, cl, &bits, &u0, &v0, &w, &h)) {   /* not converted yet: still a render target when a draw wrote it */
    int tw = 1 << (t0 >> 26 & 15), th = 1 << (t0 >> 30 & 15);
    if (c->flags) f = gu_area_flags(c->map, psm, tbp, tbw, 0, 0, tw - 1, th - 1);
    else gu_area_id(c->map, psm, tbp, tbw, 0, 0, tw - 1, th - 1, &f);
    o->kind = (f & GU_ID_DRAWN) ? TK_RT : TK_UNSUP;
    if (o->kind == TK_RT) c->st.rt++; else c->st.unsup++;
    return o->kind;
  }
  uint64_t aid = gu_area_id(c->map, psm, tbp, tbw, u0, v0, u0 + w - 1, v0 + h - 1, &f);
  if (f & GU_ID_DRAWN) { c->st.rt++; return o->kind = TK_RT; }
  if (f & GU_ID_LOST) { c->st.lost++; return o->kind = TK_LOST; }
  int n = (psm == GS_T8 || psm == GS_T8H) ? 256 : 16, pf = 0;
  uint64_t pid = idx ? gu_clut_id(c->map, cpsm, csa * 16, n, &pf) : 0;
  if (pf & GU_ID_LOST) { c->st.lost++; return o->kind = TK_LOST; }
  /* the texture: what its region holds and how it is read (TEX0's TBP0, TBW, PSM, TW, TH; CLAMP) */
  uint64_t id = mix(mix(mix(aid, t0 & 0x3ffffffffull), cl), d16 ? texa : 0) | 1;
  int i = find(c, id);
  if (i < 0) {
    size_t size = tc_texture_size(t0, cl); uint8_t *d = size ? tk_alloc(c, (uint32_t)size) : 0;
    Fe fx = { c->map, psm, tbp, tbw, 0, d16 && c->t16 ? (ta0 >= 0x80) | (ta1 >= 0x80) << 1 | (int)(texa >> 15 & 1) << 2 : -1 };
    if (!d || convert(c, t0, cl, &fx, &o->t, d, size)) { tk_release(c, d, (uint32_t)size); c->st.unsup++; return o->kind = TK_UNSUP; }
    if ((i = insert(c, id, 0, d, (uint32_t)size)) < 0) { tk_release(c, d, (uint32_t)size); c->st.unsup++; return o->kind = TK_UNSUP; }
    c->e[i].t = o->t; c->st.misses++; c->st.unknown += fx.unk; o->conv = 1;
  } else c->st.hits++;
  touch(c, i);
  if (!idx) {   /* direct colour: no palette */
    o->kind = TK_TEX; o->t = c->e[i].t; o->data = c->e[i].data; o->id = id;
    return TK_TEX;
  }
  /* the palette: the CLUT buffer's entries csa * 16 .. (as gsref reads them), and TEXA for CT16 entries */
  c->st.pal_uses++;
  pid = mix(mix(pid, (uint64_t)(uint32_t)n << 8 | (uint64_t)(uint32_t)csa), (cpsm == GS_CT16 || cpsm == GS_CT16S) ? texa : cpsm == GS_CT24 ? (texa & 0xff) : 0) | 1;
  int j = find(c, pid ^ 0x5a5a5a5a5a5a5a5aull);
  if (j < 0) {   /* the GE's n entries, then their n PS2 alphas */
    uint32_t *p = tk_alloc(c, (uint32_t)n * 5); long unk = 0;
    if (!p) { c->st.unsup++; return o->kind = TK_UNSUP; }
    for (int k = 0; k < n; k++) p[k] = pal_entry(c->map, cpsm, csa * 16 + k, texa, &unk, (uint8_t *)(p + n) + k);
    if ((j = insert(c, pid ^ 0x5a5a5a5a5a5a5a5aull, 1, (uint8_t *)p, (uint32_t)n * 5)) < 0) { tk_release(c, p, (uint32_t)n * 5); c->st.unsup++; return o->kind = TK_UNSUP; }
    c->e[j].npal = n; c->st.pal_misses++; c->st.unknown += unk; o->pal_conv = 1;
  } else c->st.pal_hits++;
  touch(c, j);
  Ent *e = &c->e[i];
  if (e->ma_pid != pid || !pid) {   /* the lowest PS2 alpha of the entries the texture uses, worked out again for another palette */
    const uint8_t *pa = c->e[j].data + n * 4; int m = 255;
    for (int k = 0; k < n; k++) if ((e->t.used[k >> 3] >> (k & 7) & 1) && pa[k] < m) m = pa[k];
    e->ma = m; e->ma_pid = pid;
  }
  o->kind = TK_TEX; o->t = e->t; o->data = e->data; o->pal = (const uint32_t *)c->e[j].data; o->npal = n; o->id = id; o->pal_id = pid; o->minalpha = e->ma;
  return TK_TEX;
}
