/* texcache.c: see texcache.h. */
#include <stdlib.h>
#include <string.h>
#include "texcache.h"
#include "gslayout.h"
#include "me_rt.h"   /* task #149: -DMGS2_ME_SAFE (ME-safe calls) */
#ifdef __psp__
#include <malloc.h>     /* task #159: newlib's mallinfo, malloc_usable_size and malloc_trim */
#include <unistd.h>     /* sbrk */
#endif

typedef struct {
  uint64_t id;                 /* 0: a free entry */
  int pal;                     /* 0: a texture (t, data), 1: a palette (npal entries in data) */
  TcTex t; uint8_t *data; uint32_t size; int npal;
  long frame;                  /* the last frame that used it */
  uint64_t ma_pid; int ma;     /* a texture: the palette identity its last minalpha was worked out with, and that minalpha */
  int prev, next, hnext;       /* LRU list (head: the most recent), hash chain; free list in next */
} Ent;
struct TkCache {
  GuMap *map; uint32_t budget; long frame, guard;   /* gl23: guard (tk_guard) */
  Ent *e; int cap, freel;
  int *bucket; uint32_t nbucket;   /* a power of two */
  int head, tail;
  TkStats st;
  uint8_t *pool; uint32_t punits, pfree;   /* tk_pool: 64-byte units; the first free run's unit (TK_NIL: none) */
  int blk;                                 /* tk_blk */
  int flags;                               /* tk_flags */
  int t16;                                 /* tk_t16 */
  int um;                                  /* sb9 tk_umemo */
  struct UgE *ug; struct UtE *ut; uint32_t uclk;   /* sb9 0a: the G and T tables (made at the first look-up with the memo on); a clock
                                                       for their uses (the least recently used way goes) */
  uint32_t kgen; TkKeep kl; uint64_t kpid;   /* gl35 (texcache.h tk_keep_ok): the generation, the last look-up's record, um_hit's palette identity */
  int uf; struct UfE *ufe; uint8_t ufm[16];        /* sb10 T1 tk_ufront: the memo's front (made with G and T), each set's most recent way */
};
#define TK_NIL 0xffffffffu
/* gl41 heap fix (a): the entry table is made 128 KB at once (gl40: 256 entries, doubled as it filled: its last growth, 64 -> 128 KB at vb
   10,850, made newlib take 128 KB fresh at its top for a moment, with ~12-25 KB to spare; attic/ge_native/step1_results.md section 6).
   The entries are handed out in the same order either way (a growth only appends fresh entries to an empty free list). */
#define TK_CAP0 ((int)(131072u / sizeof(Ent)))

static uint64_t mix(uint64_t h, uint64_t v) { h = (h ^ v) * 0x9e3779b97f4a7c15ull; return h ^ h >> 31; }

TkCache *tk_new(GuMap *map, uint32_t budget)
{
  TkCache *c = calloc(1, sizeof *c); if (!c) return 0;
  c->map = map; c->budget = budget; c->freel = c->head = c->tail = -1; c->nbucket = 4096; c->blk = 1; c->flags = 1; c->t16 = 1; c->um = 1;
  c->guard = TK_NO_GUARD;   /* gl23 */
  c->uf = 1;   /* sb10 T1: the memo's front on by default (OPT2_TX_FRONT) */
#ifdef UM_SIM
  c->uf = 0;   /* (the table-size simulation counts the memo's own path at every look-up) */
#endif
  if (!(c->bucket = malloc(c->nbucket * sizeof *c->bucket))) { free(c); return 0; }
  for (uint32_t i = 0; i < c->nbucket; i++) c->bucket[i] = -1;
  return c;
}
void tk_free(TkCache *c)
{
  if (!c) return;
  if (!c->pool) for (int i = 0; i < c->cap; i++) free(c->e[i].data);
  free(c->e); free(c->bucket); free(c->ug); free(c->ut); free(c->ufe); free(c);
}
const TkStats *tk_stats(const TkCache *c) { return &c->st; }
#ifdef __psp__
const void *tk_table(const TkCache *c) { return c->e; }
#endif
void tk_blk(TkCache *c, int mode) { c->blk = mode; c->kgen++; }
void tk_flags(TkCache *c, int on) { c->flags = on; c->kgen++; }
void tk_t16(TkCache *c, int on) { c->t16 = on; c->kgen++; }
int tk_t16_on(const TkCache *c) { return c->t16; }

/* ---- tk_pool: a free run's first 8 bytes hold the next run's unit and its own length in units ---- */
typedef struct { uint32_t next, len; } Run;
static Run *run_at(TkCache *c, uint32_t u) { return (Run *)(c->pool + (size_t)u * 64); }
int tk_pool(TkCache *c, void *mem, uint32_t size)
{
  c->kgen++;   /* gl35 */
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
  c->kgen++;   /* gl35 */
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
    if (c->e[i].frame < c->frame - 1 && c->e[i].frame < c->guard) { drop(c, i); p = p_alloc(c, size); }   /* gl23: and not guarded */
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
    if (c->e[i].frame < c->frame - 1 && c->e[i].frame < c->guard) drop(c, i);   /* gl23: and not guarded */
    i = prev;
  }
}
/* task #159: newlib's realloc (dlmalloc 2.6) grows a block that ends at the top chunk in place only when the top already holds the growth;
   otherwise it takes the whole new size from a top grown by sbrk and joins it to the block, so the break rises by the new size, not the
   growth, and the rest stays free at the top, under the 128 KB at which newlib gives memory back by itself (me_step2_notes.md).  So on
   the PSP, after a growth of a table that ended at the top chunk (its block ends malloc_usable_size - 4 bytes on, newlib's next chunk;
   the top, mallinfo's keepcost, runs to the break) while the top was short, the free top is given back at once (malloc_trim): the break
   then stands where a growth in place would have left it (me14p8's resize hook, psp/xl_psp.c in me14). */
static Ent *grow_table(TkCache *c, int n)
{
  size_t want = (size_t)n * sizeof(Ent);
#if defined(__psp__) && !defined(MGS2_ME_SAFE)   /* task #180: ME-safe, the table is in me_rt's heap, whose host resize hook gives newlib's free top back (psp/xl_psp.c me_resize) */
  int top = 0;
  if (c->e) {
    struct mallinfo mi = mallinfo(); size_t have = malloc_usable_size(c->e);
    top = want > have && (uint8_t *)c->e + have - 4 + mi.keepcost == (uint8_t *)sbrk(0) && mi.keepcost < want - have + 64;
  }
  Ent *q = realloc(c->e, want);
  if (top && q) c->st.tab_trims += malloc_trim(0);
#else
  Ent *q = realloc(c->e, want);
#endif
  if (q) { c->st.tab_grows++; c->st.tab_cap = n; } else c->st.tab_fail++;
  return q;
}
static int insert(TkCache *c, uint64_t id, int pal, uint8_t *data, uint32_t size)
{
  c->kgen++;   /* gl35 */
  make_room(c, size);
  if (c->freel < 0) {
    int n = c->cap ? 2 * c->cap : TK_CAP0; Ent *q = grow_table(c, n); if (!q) return -1;
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
long tk_now(const TkCache *c) { return c->frame; }   /* gl23 */
void tk_guard(TkCache *c, long g) { c->guard = g; }

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
/* the palette of texture entry i, *pidp the CLUT identity (gu_clut_id), which becomes the palette's: its entry, found (the entry hint first,
   taken only when it holds that key: keys are unique in the cache) or made, after which the texture's lowest alpha is worked out again if
   it was for another palette; -1 when it cannot be made (counted unsup) */
static inline void pal_ma(TkCache *c, int i, int j, int n, uint64_t pid)   /* (gl35: apart, tk_keep_again does the same) */
{
  Ent *e = &c->e[i];
  if (e->ma_pid != pid || !pid) {   /* the lowest PS2 alpha of the entries the texture uses, worked out again for another palette */
    const uint8_t *pa = c->e[j].data + n * 4; int m = 255;
    for (int k = 0; k < n; k++) if ((e->t.used[k >> 3] >> (k & 7) & 1) && pa[k] < m) m = pa[k];
    e->ma = m; e->ma_pid = pid;
  }
}
static int tk_pal(TkCache *c, int i, uint64_t *pidp, int n, int csa, int cpsm, uint64_t texa, int hint, int *conv)
{
  c->st.pal_uses++;
  uint64_t pid = mix(mix(*pidp, (uint64_t)(uint32_t)n << 8 | (uint64_t)(uint32_t)csa), (cpsm == GS_CT16 || cpsm == GS_CT16S) ? texa : cpsm == GS_CT24 ? (texa & 0xff) : 0) | 1;
  uint64_t key = pid ^ 0x5a5a5a5a5a5a5a5aull;
  *pidp = pid;
  int j = hint >= 0 && hint < c->cap && c->e[hint].id == key ? hint : find(c, key);
  if (j < 0) {   /* the GE's n entries, then their n PS2 alphas */
    uint32_t *p = tk_alloc(c, (uint32_t)n * 5); long unk = 0;
    if (!p) { c->st.unsup++; return -1; }
    for (int k = 0; k < n; k++) p[k] = pal_entry(c->map, cpsm, csa * 16 + k, texa, &unk, (uint8_t *)(p + n) + k);
    if ((j = insert(c, key, 1, (uint8_t *)p, (uint32_t)n * 5)) < 0) { tk_release(c, p, (uint32_t)n * 5); c->st.unsup++; return -1; }
    c->e[j].npal = n; c->st.pal_misses++; c->st.unknown += unk; *conv = 1;
  } else c->st.pal_hits++;
  touch(c, j);
  pal_ma(c, i, j, n, pid);
  return j;
}
int tk_get(TkCache *c, const uint64_t *gr, int ctxt, TkTex *o) { return tk_get_cl(c, gr, ctxt, gr[0x08 + ctxt], o); }
int tk_get_cl(TkCache *c, const uint64_t *gr, int ctxt, uint64_t cl, TkTex *o)
{
  c->kgen++; c->kl.ok = 0;   /* gl35 */
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
  touch(c, i); o->ent = i;
  if (!idx) {   /* direct colour: no palette */
    o->kind = TK_TEX; o->t = c->e[i].t; o->data = c->e[i].data; o->id = id;
    return TK_TEX;
  }
  /* the palette: the CLUT buffer's entries csa * 16 .. (as gsref reads them), and TEXA for CT16 entries */
  int j = tk_pal(c, i, &pid, n, csa, cpsm, texa, -1, &o->pal_conv);
  if (j < 0) return o->kind = TK_UNSUP;
  Ent *e = &c->e[i];
  o->kind = TK_TEX; o->t = e->t; o->data = e->data; o->pal = (const uint32_t *)c->e[j].data; o->npal = n; o->id = id; o->pal_id = pid; o->minalpha = e->ma;
  o->pal_ent = j;
  return TK_TEX;
}

/* ---- sb9 step 0a (stagec_notes.md 0.3): the memo by uploads ---- */
#define UG_W 4          /* G: geometry entries by TEX0 & 0x3ffffffff and CLAMP, in sets of UG_W (the least recently used goes), 1 << UG_SB sets */
#define UG_SB 7
#define UT_W 4          /* T: textures by those, the pages' signature and TEXA for a 16-bit texture, in sets of UT_W, 1 << UT_SB sets */
#define UT_SB 8
#define UG_N (UG_W << UG_SB)
#define UT_N (UT_W << UT_SB)
#define UG_PG 16        /* pages a region may read */
typedef struct UgE {
  uint64_t t0, cl;              /* the key (t0 bit 63: in use) */
  uint32_t size, use;           /* tc_texture_size; when last used (0: never) */
  uint8_t ok, npg, lanes, pad;  /* ok 1: an indexed texture, 2: a 16-bit one, 0: not served (not converted, or over UG_PG pages) */
  int16_t u0, v0, w, h;         /* tc_region (the check mode compares them) */
  uint16_t pg[UG_PG]; uint32_t mk[UG_PG];   /* gu_area_pages */
} UgE;
typedef struct UtE { uint64_t sig, k0, cl, texa, id, pid; uint32_t use; int ent, pent; } UtE;   /* the key (sig, G's key, TEXA); when last
                                                                       used (0: never); the texture's entry and identity; the last palette
                                                                       identity and entry (a hint) */
void tk_umemo(TkCache *c, int mode) { c->um = mode; c->kgen++; }
/* ---- sb10, Stage C step 1 T1 (stagec_notes.md 1.3): the memo's front.  Keyed by G's key (TEX0 & 0x3ffffffff, CLAMP) and the whole TEXA,
   it names the G and T entries the memo's own path found or filled for that key, with the map's generation then (GuStats xfers +
   xfer_local + drawn + dt_fail: every change to what a block holds moves it, CLUT loads do not).  The same generation: no block changed,
   so gu_pages_sig would give the T entry's signature, ug_get would find that G entry and T's search that T entry; the look-up goes
   straight to the T entry's check, with ug_get's and the search's counters and use stamps as they would make them.  Filled only after
   the memo's own path (gu_pages_sig settles the map first, so no transfer is pending while an entry stands).  2 ways a set, 16 sets ---- */
#define UF_SB 4
typedef struct UfE { uint64_t k0, cl, texa, gen; struct UgE *g; struct UtE *t; } UfE;
void tk_ufront(TkCache *c, int mode) { c->uf = mode; c->kgen++; }
static uint64_t uf_gen(const TkCache *c) { const GuStats *u = gu_stats(c->map); return (uint64_t)(u->xfers + u->xfer_local + u->drawn + u->dt_fail); }
static uint32_t uf_set(uint32_t h, uint64_t texa) { return (h ^ ((uint32_t)texa * 0x9e3779b1u ^ (uint32_t)(texa >> 32))) >> (32 - UF_SB); }
static void uf_put(TkCache *c, uint32_t s, uint64_t k0, uint64_t cl, uint64_t texa, uint64_t gen, struct UgE *g, struct UtE *t)
{
  UfE *f = &c->ufe[s * 2]; int w;
  if (f[0].k0 == k0 && f[0].cl == cl && f[0].texa == texa) w = 0;
  else if (f[1].k0 == k0 && f[1].cl == cl && f[1].texa == texa) w = 1;
  else w = !c->ufm[s];   /* the other way than the most recent */
  f[w].k0 = k0; f[w].cl = cl; f[w].texa = texa; f[w].gen = gen; f[w].g = g; f[w].t = t; c->ufm[s] = (uint8_t)w; c->st.uf_fill++;
}
#ifdef UM_SIM
#include <stdio.h>
/* x86 only (task #128, table sizes): the misses other table shapes would have on the same look-ups (sets x ways; in a set the least
   recently used way goes).  G keyed by TEX0 & 0x3ffffffff and CLAMP; T by those, the signature and TEXA (not by G's serial, so a G
   entry lost costs only its pages again); I (0b's identity table) by the identity and the two parameter words.  Run with both check
   modes, so every look-up that reaches T or I is followed by a fill as gl12's path makes it.  Pages per G entry made and per look-up. */
typedef struct { int sets, ways; uint64_t *tag; long look, miss; } Sim;
static Sim sim_g[] = { {256,1},{512,1},{1024,1},{128,2},{256,2},{512,2},{64,4},{128,4},{256,4},{512,4},{4096,8} };
static Sim sim_t[] = { {512,1},{1024,1},{2048,1},{256,2},{512,2},{1024,2},{128,4},{256,4},{512,4},{1024,4},{8192,8} };
static Sim sim_i[] = { {64,1},{256,1},{512,1},{1024,1},{64,2},{128,2},{256,2},{512,2},{64,4},{128,4},{256,4},{4096,8} };
static long sim_npg_g[UG_PG + 1], sim_npg_l[UG_PG + 1], sim_vfail, sim_mem_r, sim_mem_n, sim_mem_l, sim_own;
static int sim_find(Sim *s, uint64_t h, int put)   /* 1: there (now the most recent).  put 0: a look-up (counted); 1: put there
                                                         (not counted); 2: a look-up that puts it there when missing (counted) */
{
  if (!s->tag && !(s->tag = calloc((size_t)s->sets * (size_t)s->ways, 8))) return 0;
  if (!h) h = 1;
  uint64_t *t = s->tag + (size_t)((h >> 20) % (uint64_t)s->sets) * (size_t)s->ways; int w;
  for (w = 0; w < s->ways && t[w] != h; w++) ;
  int hit = w < s->ways;
  if (put != 1) { s->look++; s->miss += !hit; }
  if (!hit && !put) return 0;
  if (!hit) w = s->ways - 1;
  for (; w > 0; w--) t[w] = t[w - 1];
  t[0] = h; return hit;
}
static void sim_line(const char *nm, Sim *s, int n)
{
  fprintf(stderr, "um sim %s:", nm);
  for (int i = 0; i < n; i++) fprintf(stderr, " %dx%d %ld (%.2f%%)", s[i].sets, s[i].ways, s[i].miss, s[i].look ? 100.0 * s[i].miss / s[i].look : 0.0);
  fprintf(stderr, "; look-ups %ld\n", s[0].look);
}
static void sim_print(void)
{
  sim_line("G", sim_g, (int)(sizeof sim_g / sizeof *sim_g)); sim_line("T", sim_t, (int)(sizeof sim_t / sizeof *sim_t));
  sim_line("I", sim_i, (int)(sizeof sim_i / sizeof *sim_i));
  fprintf(stderr, "um sim: pages per G entry made / per look-up reaching T:");
  for (int k = 0; k <= UG_PG; k++) if (sim_npg_g[k] || sim_npg_l[k]) fprintf(stderr, " %d: %ld / %ld", k, sim_npg_g[k], sim_npg_l[k]);
  fprintf(stderr, "; identity table: slot matched, load check failed %ld\n", sim_vfail);
  fprintf(stderr, "um sim: the map's pools at most (x86 sizes): records %ld KB, nodes %ld KB, loads %ld KB; local copies' bytes at most %ld KB\n",
          sim_mem_r >> 10, sim_mem_n >> 10, sim_mem_l >> 10, sim_own >> 10);
}
static void sim_once(void) { static int once; if (!once) { once = 1; atexit(sim_print); } }
static void sim_gl(uint64_t k0, uint64_t cl) { sim_once(); uint64_t h = mix(mix(0x51u, k0), cl); for (size_t i = 0; i < sizeof sim_g / sizeof *sim_g; i++) sim_find(&sim_g[i], h, 2); }
static void sim_tl(uint64_t h, int put) { for (size_t i = 0; i < sizeof sim_t / sizeof *sim_t; i++) sim_find(&sim_t[i], h, put); }
void um_sim_ld(uint64_t sig, uint32_t rk, uint32_t rt, int put)
{ sim_once(); uint64_t h = mix(mix(0x77u, sig), (uint64_t)rk << 32 | rt); for (size_t i = 0; i < sizeof sim_i / sizeof *sim_i; i++) sim_find(&sim_i[i], h, put); }
void um_sim_vfail(void) { sim_vfail++; }
void um_sim_mem(int capr, size_t szr, int capn, size_t szn, int capl, size_t szl, long own)
{ long r = (long)capr * (long)szr, n = (long)capn * (long)szn, l = (long)capl * (long)szl;
  if (r > sim_mem_r) sim_mem_r = r;
  if (n > sim_mem_n) sim_mem_n = n;
  if (l > sim_mem_l) sim_mem_l = l;
  if (own > sim_own) sim_own = own; }
#endif
/* G's key k0 = TEX0 & 0x3ffffffff | bit 63 and CLAMP, hashed to 32 bits (G's set: the top UG_SB bits; T's set mixes in the signature) */
static uint32_t ug_hash(uint64_t k0, uint64_t cl)
{
  uint32_t h = (uint32_t)k0 * 0x9e3779b1u ^ (uint32_t)(k0 >> 32);
  h = (h ^ (uint32_t)cl) * 0x85ebca77u; h = (h ^ (uint32_t)(cl >> 32)) * 0xc2b2ae3du; return h ^ h >> 16;
}
static UgE *ug_get(TkCache *c, uint64_t k0, uint64_t t0, uint64_t cl, uint32_t hk)
{
  UgE *g = &c->ug[(hk >> (32 - UG_SB)) * UG_W], *v = g, *ge = g + UG_W;
  for (; g < ge; g++) { if (g->t0 == k0 && g->cl == cl) { g->use = c->uclk; return g; } if (g->use < v->use) v = g; }
  g = v; memset(g, 0, sizeof *g); g->t0 = k0; g->cl = cl; g->use = c->uclk; c->st.um_gnew++;   /* the least recently used way (empty: 0) */
  int psm = (int)(t0 >> 20 & 63), bits, u0, v0, w, h, n; uint32_t lanes;
  int idx = psm == GS_T8 || psm == GS_T4 || psm == GS_T8H || psm == GS_T4HL || psm == GS_T4HH, d16 = psm == GS_CT16 || psm == GS_CT16S;
  if ((!idx && !d16) || tc_region(t0, cl, &bits, &u0, &v0, &w, &h)) return g;
  if ((n = gu_area_pages(psm, (uint32_t)(t0 & 0x3fff), (uint32_t)(t0 >> 14 & 63), u0, v0, u0 + w - 1, v0 + h - 1, g->pg, g->mk, UG_PG, &lanes)) < 0) return g;
  g->npg = (uint8_t)n; g->lanes = (uint8_t)lanes; g->size = (uint32_t)tc_texture_size(t0, cl);
  g->u0 = (int16_t)u0; g->v0 = (int16_t)v0; g->w = (int16_t)w; g->h = (int16_t)h; g->ok = idx ? 1 : 2;
  return g;
}
/* tk_get_cl's hit, as it makes it, for the entry T names */
static int um_hit(TkCache *c, uint64_t t0, uint64_t texa, const UgE *g, UtE *t, TkTex *o)
{
  int psm = (int)(t0 >> 20 & 63), cpsm = (int)(t0 >> 51 & 15), csa = (int)(t0 >> 56 & 31), i = t->ent;
  c->st.uses++; c->st.um_hits++;
  if (g->ok == 2) {   /* direct colour: no palette */
    c->st.hits++; touch(c, i);
    o->kind = TK_TEX; o->data = c->e[i].data; o->pal = 0; o->npal = 0; o->id = t->id; o->pal_id = 0; o->minalpha = 0; o->conv = o->pal_conv = 0;
    o->ent = i; o->pal_ent = -1; o->tp = &c->e[i].t; o->size = g->size;
    return TK_TEX;
  }
  int n = (psm == GS_T8 || psm == GS_T8H) ? 256 : 16, pf = 0, pconv = 0;
  uint64_t pid = gu_clut_id(c->map, cpsm, csa * 16, n, &pf);
  if (pf & GU_ID_LOST) { memset(o, 0, sizeof *o); c->st.lost++; return o->kind = TK_LOST; }
  c->kpid = pid;   /* gl35 */
  c->st.hits++; touch(c, i);
  int j = tk_pal(c, i, &pid, n, csa, cpsm, texa, t->pent, &pconv);
  if (j < 0) { memset(o, 0, sizeof *o); return o->kind = TK_UNSUP; }
  t->pid = pid; t->pent = j;
  const Ent *e = &c->e[i];
  o->kind = TK_TEX; o->data = e->data; o->pal = (const uint32_t *)c->e[j].data; o->npal = n; o->id = t->id; o->pal_id = pid; o->minalpha = e->ma;
  o->conv = 0; o->pal_conv = pconv; o->ent = i; o->pal_ent = j; o->tp = &e->t; o->size = g->size;
  return TK_TEX;
}
#ifdef UM_SIM
static long sim_gn;
#endif
int tk_get_um(TkCache *c, const uint64_t *gr, int ctxt, uint64_t cl, TkTex *o)
{
  uint64_t t0 = gr[0x06 + ctxt], texa = gr[0x3b], sig = 0, tx = 0; int kind, why;
  if (c->um && !c->ug && (!(c->ug = calloc(UG_N, sizeof *c->ug)) || !(c->ut = calloc(UT_N, sizeof *c->ut)) || !(c->ufe = calloc(2u << UF_SB, sizeof *c->ufe)))) {
    free(c->ug); free(c->ut); c->ug = 0; c->ut = 0; c->um = 0; }
  if (!c->um) {   /* as gl12 */
    if ((kind = tk_get_cl(c, gr, ctxt, cl, o)) == TK_TEX) { o->tp = &o->t; o->size = (uint32_t)tc_texture_size(t0, cl); }
    return kind;
  }
  c->st.um_asks++; c->uclk++; c->kl.ok = 0;
  uint64_t k0 = (t0 & 0x3ffffffffull) | 1ull << 63; uint32_t h = ug_hash(k0, cl);
  uint64_t fgen = 0; uint32_t fs = 0; int fhit = 0; UgE *g; UtE *t = 0;
  if (c->uf) {   /* sb10 T1: the front */
    fgen = uf_gen(c); fs = uf_set(h, texa);
    const UfE *f = &c->ufe[fs * 2]; int w = f[0].k0 == k0 && f[0].cl == cl && f[0].texa == texa ? 0 : f[1].k0 == k0 && f[1].cl == cl && f[1].texa == texa ? 1 : -1;
    if (w >= 0 && f[w].gen == fgen && f[w].g->t0 == k0 && f[w].g->cl == cl && f[w].t->k0 == k0 && f[w].t->cl == cl
        && f[w].t->texa == (f[w].g->ok == 2 ? texa : 0)) {   /* (T's key has TEXA only for a 16-bit texture) */
      g = f[w].g; t = f[w].t; c->ufm[fs] = (uint8_t)w;
      if (c->uf == 2) {   /* the check mode: the memo's own path as well (its stamps are the ones set below), the same entries */
        uint64_t s2 = 0, x2 = 0; UgE *g2 = ug_get(c, k0, t0, cl, h); int y = g2 == g && g->ok;
        if (y && g->ok == 2) { uint32_t ta0 = (uint32_t)(texa & 0xff), ta1 = (uint32_t)(texa >> 32 & 0xff);
          y = c->t16 ? (ta0 == 0 || ta0 >= 0x80) && (ta1 == 0 || ta1 >= 0x80) : ta0 == 0 && ta1 >= 0x80; x2 = texa; }
        if (y) y = !gu_pages_sig(c->map, g->lanes, g->npg, g->pg, g->mk, &s2);
        if (y) { uint32_t x = (h ^ (uint32_t)s2) * 0x9e3779b1u ^ (uint32_t)(s2 >> 32); UtE *t2 = &c->ut[(x >> (32 - UT_SB)) * UT_W], *te = t2 + UT_W;
                 for (; t2 < te; t2++) if (t2->sig == s2 && t2->k0 == k0 && t2->cl == cl && t2->texa == x2) break;
                 y = t2 == t; }
        c->st.uf_chk++; c->st.uf_bad += !y;
      }
      g->use = c->uclk; sig = t->sig; tx = t->texa; c->st.uf_hits++; fhit = 1;
      goto found;
    }
  }
  c->kgen++;   /* gl35: not the front's answer */
  g = ug_get(c, k0, t0, cl, h);
#ifdef UM_SIM
  uint64_t sim_h = 0; sim_gl((t0 & 0x3ffffffffull) | 1ull << 63, cl);
  if (g->ok && g->npg <= UG_PG && c->st.um_gnew != sim_gn) { sim_gn = c->st.um_gnew; sim_npg_g[g->npg]++; }
#endif
  if (!g->ok) { c->st.um_geo++; goto slow; }
  if (g->ok == 2) {   /* a 16-bit texture converts only with a TEXA tk_get_cl takes, and is kept under it */
    uint32_t ta0 = (uint32_t)(texa & 0xff), ta1 = (uint32_t)(texa >> 32 & 0xff);
    if (!(c->t16 ? (ta0 == 0 || ta0 >= 0x80) && (ta1 == 0 || ta1 >= 0x80) : ta0 == 0 && ta1 >= 0x80)) { c->st.um_texa++; goto slow; }
    tx = texa;
  }
  if ((why = gu_pages_sig(c->map, g->lanes, g->npg, g->pg, g->mk, &sig))) { c->st.um_sig[why]++; goto slow; }
#ifdef UM_SIM
  sim_h = mix(mix(mix(0x33u, (t0 & 0x3ffffffffull) | 1ull << 63), cl), mix(sig, tx)); sim_tl(sim_h, 0); sim_npg_l[g->npg]++;
#endif
  { /* T's set; the way holding this key (an empty way never matches: k0 has bit 63), else the least recently used one is filled */
    uint32_t x = (h ^ (uint32_t)sig) * 0x9e3779b1u ^ (uint32_t)(sig >> 32); UtE *v, *te;
    v = t = &c->ut[(x >> (32 - UT_SB)) * UT_W]; te = t + UT_W;
    for (; t < te; t++) { if (t->sig == sig && t->k0 == k0 && t->cl == cl && t->texa == tx) break; if (t->use < v->use) v = t; }
    if (t == te) { t = v; c->st.um_slot++; goto fill; }
  }
found:
  t->use = c->uclk;
  if ((unsigned)t->ent >= (unsigned)c->cap || c->e[t->ent].id != t->id) { c->st.um_gone++; goto fill; }
  if (c->uf && !fhit) uf_put(c, fs, k0, cl, texa, fgen, g, t);   /* sb10 T1 */
  if (c->um == 1) {
    kind = um_hit(c, t0, texa, g, t, o);
    if (fhit && c->uf == 1 && kind == TK_TEX && g->ok == 1 && !o->pal_conv) {   /* gl35: the pure path, as tk_keep_again does it again */
      TkKeep *K = &c->kl; K->g = g; K->t = t; K->pid_in = c->kpid; K->pid = o->pal_id; K->ufgen = fgen; K->kgen = c->kgen;
      K->fs = (uint16_t)fs; K->i = (int16_t)o->ent; K->j = (int16_t)o->pal_ent; K->w = c->ufm[fs]; K->ok = 1; }
    return kind;
  }
  { /* the check mode: tk_get_cl's answer against the memo's */
    int ent = t->ent, pent = t->pent; uint64_t id = t->id, pid = t->pid;
    kind = tk_get_cl(c, gr, ctxt, cl, o); c->st.um_chk++;
    int same = kind == TK_TEX && o->ent == ent && o->id == id && o->data == c->e[ent].data && !memcmp(&o->t, &c->e[ent].t, sizeof o->t)
      && g->size == (uint32_t)tc_texture_size(t0, cl) && o->t.u0 == g->u0 && o->t.v0 == g->v0 && o->t.w == g->w && o->t.h == g->h;
    if (same && g->ok == 1 && pid == o->pal_id && pent >= 0 && pent < c->cap && c->e[pent].id == (pid ^ 0x5a5a5a5a5a5a5a5aull)) same = pent == o->pal_ent;
    if (!same) c->st.um_bad++;
    goto filled;
  }
fill:
  kind = tk_get_cl(c, gr, ctxt, cl, o);
filled:
  if (kind == TK_TEX) {
    t->sig = sig; t->k0 = k0; t->cl = cl; t->texa = tx; t->use = c->uclk; t->id = o->id; t->ent = o->ent; t->pid = o->pal_id;
    t->pent = g->ok == 1 ? o->pal_ent : -1;
    c->st.um_fill++; o->tp = &o->t; o->size = g->size;
    if (c->uf) uf_put(c, fs, k0, cl, texa, fgen, g, t);   /* sb10 T1 */
#ifdef UM_SIM
    sim_tl(sim_h, 1);
#endif
  }
  return kind;
slow:
  if ((kind = tk_get_cl(c, gr, ctxt, cl, o)) == TK_TEX) { o->tp = &o->t; o->size = g->ok ? g->size : (uint32_t)tc_texture_size(t0, cl); }
  return kind;
}

/* gl35, the texture kept by material (texcache.h; attic/gl35/keep_design.txt K3) */
const TkKeep *tk_keep_last(const TkCache *c) { return &c->kl; }
int tk_keep_ok(TkCache *c, const TkKeep *K, uint64_t t0)
{
  if (!K->ok || K->kgen != c->kgen || c->um != 1 || c->uf != 1 || K->ufgen != uf_gen(c)) return 0;
  int psm = (int)(t0 >> 20 & 63), cpsm = (int)(t0 >> 51 & 15), csa = (int)(t0 >> 56 & 31), pf = 0;
  uint64_t pid = gu_clut_id(c->map, cpsm, csa * 16, (psm == GS_T8 || psm == GS_T8H) ? 256 : 16, &pf);   /* (um_hit's own call: the same note in the load) */
  return !(pf & GU_ID_LOST) && pid == K->pid_in;
}
void tk_keep_again(TkCache *c, const TkKeep *K, uint64_t t0, TkTex *o)
{
  UgE *g = K->g; UtE *t = K->t; int psm = (int)(t0 >> 20 & 63), n = (psm == GS_T8 || psm == GS_T8H) ? 256 : 16, i = K->i, j = K->j;
  c->st.um_asks++; c->uclk++; c->kl = *K;
  c->ufm[K->fs] = K->w; g->use = c->uclk; c->st.uf_hits++;   /* the front's answer */
  t->use = c->uclk;
  c->st.uses++; c->st.um_hits++;   /* um_hit */
  c->kpid = K->pid_in;
  c->st.hits++; touch(c, i);
  c->st.pal_uses++; c->st.pal_hits++; touch(c, j); pal_ma(c, i, j, n, K->pid);   /* tk_pal: the palette is there */
  t->pid = K->pid; t->pent = j;
  const Ent *e = &c->e[i];
  o->kind = TK_TEX; o->data = e->data; o->pal = (const uint32_t *)c->e[j].data; o->npal = n; o->id = t->id; o->pal_id = K->pid; o->minalpha = e->ma;
  o->conv = 0; o->pal_conv = 0; o->ent = i; o->pal_ent = j; o->tp = &e->t; o->size = g->size;
}
