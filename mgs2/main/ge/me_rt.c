/* me_rt.c (task #149, 2026-11-03): the picture code's own copies of what it took from the C library (ge/me_rt.h), built into the
   Media Engine's unit with the picture files.  Nothing here calls anything outside this file. */
#include "me_rt.h"

#if defined(__GNUC__) && !defined(__clang__)
#define ME_NOLD __attribute__((optimize("no-tree-loop-distribute-patterns")))   /* (no loop turned back into a memcpy or memset call) */
#else
#define ME_NOLD
#endif

MeFaults me_faults;

/* ---- memcpy, memset, memcmp ---- */
ME_NOLD void *me_memcpy(void *d, const void *s, size_t n)
{
  uint8_t *dp = (uint8_t *)d; const uint8_t *sp = (const uint8_t *)s;
#ifndef __psp__
  if (n && dp < sp + n && sp < dp + n && dp != sp) me_faults.overlap++;
#endif
  if (n >= 8 && !(((uintptr_t)dp ^ (uintptr_t)sp) & 3)) {   /* the same alignment: bytes up to a word, then words */
    while ((uintptr_t)dp & 3) { *dp++ = *sp++; n--; }
    uint32_t *dw = (uint32_t *)dp; const uint32_t *sw = (const uint32_t *)sp;
    for (; n >= 16; n -= 16, dw += 4, sw += 4) { uint32_t a = sw[0], b = sw[1], c = sw[2], e = sw[3]; dw[0] = a; dw[1] = b; dw[2] = c; dw[3] = e; }
    for (; n >= 4; n -= 4) *dw++ = *sw++;
    dp = (uint8_t *)dw; sp = (const uint8_t *)sw;
  }
  while (n) { *dp++ = *sp++; n--; }
  return d;
}
ME_NOLD void *me_memset(void *d, int c, size_t n)
{
  uint8_t *dp = (uint8_t *)d, v = (uint8_t)c;
  if (n >= 8) {
    while ((uintptr_t)dp & 3) { *dp++ = v; n--; }
    uint32_t w = v * 0x01010101u, *dw = (uint32_t *)dp;
    for (; n >= 16; n -= 16, dw += 4) { dw[0] = w; dw[1] = w; dw[2] = w; dw[3] = w; }
    for (; n >= 4; n -= 4) *dw++ = w;
    dp = (uint8_t *)dw;
  }
  while (n) { *dp++ = v; n--; }
  return d;
}
ME_NOLD int me_memcmp(const void *a, const void *b, size_t n)
{
  const uint8_t *p = (const uint8_t *)a, *q = (const uint8_t *)b;
  if (n >= 8 && !(((uintptr_t)p | (uintptr_t)q) & 3)) {   /* both word-aligned: words while equal */
    const uint32_t *pw = (const uint32_t *)p, *qw = (const uint32_t *)q;
    while (n >= 4 && *pw == *qw) { pw++; qw++; n -= 4; }
    p = (const uint8_t *)pw; q = (const uint8_t *)qw;
  }
  for (; n; n--, p++, q++) if (*p != *q) return (int)*p - (int)*q;
  return 0;
}

/* ---- floorf, ceilf: fdlibm's s_floorf.c / s_ceilf.c rules (newlib's), on the bits ---- */
typedef union { float f; uint32_t u; int32_t i; } MeF;
float me_floorf(float x)
{
  MeF v; v.f = x; int32_t i0 = v.i, j0 = ((i0 >> 23) & 0xff) - 0x7f;
  if (j0 < 23) {
    if (j0 < 0) {                                   /* |x| < 1: +0 for x >= 0 (and +denormals), -1 for x < 0, -0 kept */
      if (i0 >= 0) i0 = 0; else if (i0 & 0x7fffffff) i0 = (int32_t)0xbf800000u;
    } else {
      uint32_t m = 0x007fffffu >> j0;
      if (!((uint32_t)i0 & m)) return x;            /* integral */
      if (i0 < 0) i0 += 0x00800000 >> j0;
      i0 &= (int32_t)~m;
    }
  } else return j0 == 0x80 ? x + x : x;            /* inf or NaN; integral */
  v.i = i0; return v.f;
}
float me_ceilf(float x)
{
  MeF v; v.f = x; int32_t i0 = v.i, j0 = ((i0 >> 23) & 0xff) - 0x7f;
  if (j0 < 23) {
    if (j0 < 0) {                                   /* |x| < 1: -0 for x < 0 (and -denormals), 1 for x > 0, +0 kept */
      if (i0 < 0) i0 = (int32_t)0x80000000u; else if (i0) i0 = 0x3f800000;
    } else {
      uint32_t m = 0x007fffffu >> j0;
      if (!((uint32_t)i0 & m)) return x;
      if (i0 > 0) i0 += 0x00800000 >> j0;
      i0 &= (int32_t)~m;
    }
  } else return j0 == 0x80 ? x + x : x;
  v.i = i0; return v.f;
}

/* ---- the picture heap: TLSF.  A block: { psz, sz } then its bytes; psz is the block before it's size, kept while that one is free;
   sz the block's bytes (a multiple of MH_ALIGN, the header included) | MH_FREE | MH_PFREE (the block before is free).  A free block
   holds its list links after the header.  Two free blocks are never neighbours.  Each region (a pool) ends with a header of size 0 in
   use, so no block reaches from one pool into another. */
#define MH_ALIGN_LOG2 (sizeof(void *) == 8 ? 4 : 3)
#define MH_ALIGN ((size_t)1 << MH_ALIGN_LOG2)
#define MH_HDR (2 * sizeof(size_t))                 /* = MH_ALIGN on both (8 on the PSP, 16 on x86) */
#define MH_MIN (MH_HDR + 2 * sizeof(void *))
#define MH_SL_LOG2 4
#define MH_SL_N (1 << MH_SL_LOG2)
#define MH_FL_SHIFT (MH_SL_LOG2 + MH_ALIGN_LOG2)
#define MH_SMALL ((size_t)1 << MH_FL_SHIFT)
#define MH_FL_N 28
#define MH_FREE ((size_t)1)
#define MH_PFREE ((size_t)2)
#define MH_SZ(b) ((b)->sz & ~(size_t)3)
typedef struct MhBlk { size_t psz, sz; struct MhBlk *nf, *pf; } MhBlk;
#define MH_POOLS 32
static struct {
  uint8_t *base, *end;                            /* the first region's first block and end header (its extent is counted) */
  uint8_t *pb[MH_POOLS], *pe[MH_POOLS]; int np;   /* every pool's */
  void *pr[MH_POOLS];                             /* the region the host's hook gave for the pool, or 0 (the first, me_heap_add's) */
  void *(*more)(size_t, size_t *);                /* the host's hooks (main CPU only), or 0 */
  void (*less)(void *);
  void *(*resize)(void *, size_t);
  uint32_t flb; uint32_t slb[MH_FL_N]; MhBlk *fh[MH_FL_N][MH_SL_N]; MeHeapStats st;
} H;

static inline int mh_fls(size_t x)   /* the highest set bit's index (x > 0) */
{
#if defined(__psp__)
  return 31 - __builtin_clz((unsigned)x);
#else
  return (int)(sizeof(unsigned long) * 8 - 1) - __builtin_clzl((unsigned long)x);
#endif
}
static inline int mh_ffs(uint32_t x) { return 31 - __builtin_clz(x & (0u - x)); }   /* the lowest set bit's index (x > 0) */
static inline MhBlk *mh_next(MhBlk *b) { return (MhBlk *)((uint8_t *)b + MH_SZ(b)); }
static void mh_map(size_t s, int *fl, int *sl)
{
  if (s < MH_SMALL) { *fl = 0; *sl = (int)(s >> MH_ALIGN_LOG2); }
  else { int f = mh_fls(s); *sl = (int)(s >> (f - MH_SL_LOG2)) ^ MH_SL_N; *fl = f - (MH_FL_SHIFT - 1); }
}
static void mh_unlink(MhBlk *b)
{
  int fl, sl; mh_map(MH_SZ(b), &fl, &sl);
  if (b->nf) b->nf->pf = b->pf;
  if (b->pf) b->pf->nf = b->nf;
  else { H.fh[fl][sl] = b->nf; if (!b->nf) { H.slb[fl] &= ~(1u << sl); if (!H.slb[fl]) H.flb &= ~(1u << fl); } }
}
static void mh_link(MhBlk *b)   /* b: free, its size set */
{
  int fl, sl; mh_map(MH_SZ(b), &fl, &sl);
  b->pf = 0; b->nf = H.fh[fl][sl]; if (b->nf) b->nf->pf = b;
  H.fh[fl][sl] = b; H.slb[fl] |= 1u << sl; H.flb |= 1u << fl;
}
/* b+keep .. b+MH_SZ(b) becomes a free block (merged with the next one when that is free); b keeps keep bytes.  b is in use. */
static void mh_tail(MhBlk *b, size_t keep)
{
  size_t s = MH_SZ(b), r = s - keep;
  MhBlk *n = mh_next(b);
  if (n->sz & MH_FREE) { mh_unlink(n); r += MH_SZ(n); }
  else if (r < MH_MIN) return;
  b->sz = keep | (b->sz & MH_PFREE);
  MhBlk *t = (MhBlk *)((uint8_t *)b + keep); t->sz = r | MH_FREE;
  MhBlk *u = mh_next(t); u->psz = r; u->sz |= MH_PFREE;
  mh_link(t);
}
static size_t mh_need(size_t n)
{
  if (n > ((size_t)1 << 30)) return 0;   /* (no sum below wraps) */
  size_t s = (n + MH_HDR + MH_ALIGN - 1) & ~(MH_ALIGN - 1);
  return s < MH_MIN ? MH_MIN : s;
}
static void mh_used(MhBlk *b)   /* b now in use: the counts */
{
  H.st.used += MH_SZ(b); if (H.st.used > H.st.peak) H.st.peak = H.st.used;
  if ((uint8_t *)b >= H.base && (uint8_t *)b < H.end) { size_t e = (size_t)((uint8_t *)b + MH_SZ(b) - H.base); if (e > H.st.extent) H.st.extent = e; }
  if (++H.st.blocks > H.st.blocks_peak) H.st.blocks_peak = H.st.blocks;
}
/* the n bytes at base become a pool: one free block, then an end header of size 0 in use */
static int mh_pool(void *base, size_t n)
{
  uint8_t *a = (uint8_t *)(((uintptr_t)base + MH_ALIGN - 1) & ~(uintptr_t)(MH_ALIGN - 1));
  if (!base || H.np == MH_POOLS || n < 4096 || (size_t)(a - (uint8_t *)base) + 2 * MH_MIN + MH_HDR > n) return -1;
  size_t s = (n - (size_t)(a - (uint8_t *)base) - MH_HDR) & ~(MH_ALIGN - 1);
  MhBlk *b = (MhBlk *)a; b->psz = 0; b->sz = s | MH_FREE;
  MhBlk *e = (MhBlk *)(a + s); e->psz = s; e->sz = 0 | MH_PFREE;   /* the end: size 0, in use */
  H.pb[H.np] = a; H.pe[H.np] = a + s; H.pr[H.np] = 0; H.np++; H.st.pools = H.np; H.st.size += s;
  mh_link(b);
  return 0;
}
int me_heap_init(void *base, size_t n)
{
  me_memset(&H, 0, sizeof H);
  if (mh_pool(base, n)) return -1;
  H.base = H.pb[0]; H.end = H.pe[0];
  return 0;
}
int me_heap_add(void *base, size_t n)
{
  size_t s0 = H.st.size;
  if (!H.np || mh_pool(base, n)) return -1;
  H.st.added += H.st.size - s0;
  return 0;
}
static volatile uint32_t me_beat_own;            /* task #180 step 4c: me_rt.h ME_BEAT */
volatile uint32_t *me_beat_p = &me_beat_own;
uint32_t me_beat_n = 0;                         /* step 4d-4: the beats; every 64th moves *me_beat_p (me_rt.h ME_BEAT) */
void me_heap_grow(void *(*more)(size_t want, size_t *got), void (*less)(void *region)) { H.more = more; H.less = less; }
void me_heap_resize(void *(*resize)(void *region, size_t want)) { H.resize = resize; }
/* a free block of need bytes or more (a good fit), or 0 */
static MhBlk *mh_find(size_t need)
{
  size_t s = need; int fl, sl;
  if (s >= MH_SMALL) s += ((size_t)1 << (mh_fls(s) - MH_SL_LOG2)) - 1;   /* the first list whose every block fits */
  mh_map(s, &fl, &sl);
  uint32_t m = fl < MH_FL_N ? H.slb[fl] & (~0u << sl) : 0;
  if (!m && fl < MH_FL_N) { uint32_t f = fl + 1 < MH_FL_N ? H.flb & (~0u << (fl + 1)) : 0; if (f) { fl = mh_ffs(f); m = H.slb[fl]; } }
  if (m) return H.fh[fl][mh_ffs(m)];
  mh_map(need, &fl, &sl);   /* none in the lists whose every block fits: the first block that fits in the request's own list */
  if (fl >= MH_FL_N) return 0;
  MhBlk *b; for (b = H.fh[fl][sl]; b && MH_SZ(b) < need; b = b->nf) ;
  return b;
}
/* nothing fits: the host's hook, when set (the main CPU), gives a region that holds the block; it becomes a pool */
__attribute__((noinline)) static MhBlk *mh_grow(size_t need)
{
  if (!H.more || H.np == MH_POOLS) return 0;
  size_t want = need + MH_ALIGN + MH_HDR, got = 0, s0 = H.st.size;   /* (the region's alignment and its end header) */
  if (want < 4096) want = 4096;
  void *r = ME_CALLP(H.more)(want, &got);
  if (!r || got < want || mh_pool(r, got)) { H.st.grow_fails++; return 0; }
  H.pr[H.np - 1] = r; H.st.grows++; H.st.added += H.st.size - s0; if (H.st.added > H.st.added_peak) H.st.added_peak = H.st.added;
  return mh_find(need);
}
void *me_malloc(size_t n)
{
  size_t need = mh_need(n);
  H.st.allocs++;
  MhBlk *b = need && H.np ? mh_find(need) : 0;
  if (!b && need && H.np) b = mh_grow(need);
  if (!b) { if (!H.st.fails++) H.st.fail_first = n; return 0; }
  mh_unlink(b);
  b->sz &= ~MH_FREE; mh_next(b)->sz &= ~MH_PFREE;
  mh_tail(b, need);
  mh_used(b);
  return (uint8_t *)b + MH_HDR;
}
/* free block b fills a pool the host's hook gave: the region goes back to the host (less) */
__attribute__((noinline)) static int mh_give_back(MhBlk *b)
{
  int k = 1; while (k < H.np && H.pb[k] != (uint8_t *)b) k++;
  if (k == H.np || !H.pr[k]) return 0;
  void *r = H.pr[k]; size_t s = (size_t)(H.pe[k] - H.pb[k]);
  H.np--; H.pb[k] = H.pb[H.np]; H.pe[k] = H.pe[H.np]; H.pr[k] = H.pr[H.np];
  H.st.pools = H.np; H.st.size -= s; H.st.added -= s; H.st.returned++;
  ME_CALLP(H.less)(r);
  return 1;
}
void me_free(void *p)
{
  if (!p) return;
  MhBlk *b = (MhBlk *)((uint8_t *)p - MH_HDR); size_t s = MH_SZ(b);
  H.st.frees++; H.st.used -= s; H.st.blocks--;
  if (b->sz & MH_PFREE) { MhBlk *v = (MhBlk *)((uint8_t *)b - b->psz); mh_unlink(v); s += MH_SZ(v); b = v; }
  MhBlk *n = (MhBlk *)((uint8_t *)b + s);
  if (n->sz & MH_FREE) { mh_unlink(n); s += MH_SZ(n); n = (MhBlk *)((uint8_t *)b + s); }
  if (!MH_SZ(n) && H.less && (uint8_t *)b != H.base && mh_give_back(b)) return;   /* (n the end header: b may fill its pool) */
  b->sz = s | MH_FREE;   /* (the block before a free block is in use) */
  n->psz = s; n->sz |= MH_PFREE;
  mh_link(b);
}
void *me_calloc(size_t a, size_t b)
{
  if (b && a > (size_t)-1 / b) { H.st.allocs++; if (!H.st.fails++) H.st.fail_first = (size_t)-1; return 0; }
  void *p = me_malloc(a * b);
  if (p) me_memset(p, 0, a * b);
  return p;
}
/* realloc's block b (in use; need bytes wanted) cannot grow where it is: when it is the only block in use in a pool the host's hook
   gave (the pool's first block, then the end, or a free block and the end: the hook's region is a little larger than the block it was
   given for when the host rounds its size up, as psp/xl_psp.c's does to 64 bytes), the host resizes that region (H.resize), in place
   or moved with its bytes, and the pool and b take its new size and place; else 0 */
ME_NOLD __attribute__((noinline)) static MhBlk *mh_resize(MhBlk *b, size_t need)
{
  int k = 1; while (k < H.np && H.pb[k] != (uint8_t *)b) k++;
  if (k == H.np || !H.pr[k]) return 0;
  MhBlk *x = mh_next(b); int tail = (uint8_t *)x != H.pe[k];
  if (tail && (!(x->sz & MH_FREE) || (uint8_t *)mh_next(x) != H.pe[k])) return 0;
  uint8_t *r = (uint8_t *)H.pr[k]; size_t off = (size_t)((uint8_t *)b - r), s = MH_SZ(b), ps = (size_t)(H.pe[k] - H.pb[k]);
  size_t want = off + need + MH_HDR + MH_ALIGN;   /* the block, the end header, and room to align again if the region moves */
  if (tail) mh_unlink(x);   /* the free tail out of the lists before the host may move or free the region */
  uint8_t *r2 = (uint8_t *)ME_CALLP(H.resize)(r, want);
  if (!r2) { if (tail) mh_link(x); return 0; }   /* the region as it was: the caller moves the block */
  uint8_t *a = (uint8_t *)(((uintptr_t)r2 + MH_ALIGN - 1) & ~(uintptr_t)(MH_ALIGN - 1)), *o = r2 + off;
  if (a < o) for (size_t i = 0; i < s; i++) a[i] = o[i];   /* moved to another alignment: the block (its header first) to the start */
  else if (a > o) for (size_t i = s; i--; ) a[i] = o[i];
  size_t ns = (want - (size_t)(a - r2) - MH_HDR) & ~(MH_ALIGN - 1);   /* (need or more) */
  MhBlk *nb = (MhBlk *)a; nb->psz = 0; nb->sz = ns;   /* in use, the pool's first */
  MhBlk *e = (MhBlk *)(a + ns); e->psz = ns; e->sz = 0;   /* the end: size 0, in use */
  H.pb[k] = a; H.pe[k] = a + ns; H.pr[k] = r2;
  H.st.size += ns - ps; H.st.added += ns - ps; if (H.st.added > H.st.added_peak) H.st.added_peak = H.st.added; H.st.resized++;
  if (tail) H.st.resized_tail++;
  H.st.used -= s; H.st.blocks--; mh_tail(nb, need); mh_used(nb);
  return nb;
}
void *me_realloc(void *p, size_t n)
{
  if (!p) return me_malloc(n);
  if (!n) { me_free(p); return 0; }
  H.st.reallocs++;
  MhBlk *b = (MhBlk *)((uint8_t *)p - MH_HDR); size_t s = MH_SZ(b), need = mh_need(n);
  if (!need) { if (!H.st.fails++) H.st.fail_first = n; return 0; }
  if (need <= s) { H.st.used -= s; H.st.blocks--; mh_tail(b, need); mh_used(b); return p; }
  MhBlk *x = mh_next(b);
  if ((x->sz & MH_FREE) && s + MH_SZ(x) >= need) {   /* grows into the free block after it */
    H.st.used -= s; H.st.blocks--;
    mh_unlink(x); b->sz += MH_SZ(x); mh_next(b)->sz &= ~MH_PFREE;
    mh_tail(b, need); mh_used(b); return p;
  }
  if (H.resize) { MhBlk *nb = mh_resize(b, need); if (nb) return (uint8_t *)nb + MH_HDR; }
  void *q = me_malloc(n); if (!q) return 0;
  H.st.moved++; H.st.allocs--;   /* (counted as the realloc it is) */
  me_memcpy(q, p, s - MH_HDR); me_free(p); H.st.frees--;
  return q;
}
void *me_aligned_alloc(size_t al, size_t n)
{
  if (al <= MH_ALIGN) return me_malloc(n);
  if (al & (al - 1)) { H.st.allocs++; if (!H.st.fails++) H.st.fail_first = n; return 0; }
  uint8_t *p = (uint8_t *)me_malloc(n + al + MH_MIN); if (!p) return 0;
  MhBlk *b = (MhBlk *)(p - MH_HDR);
  uint8_t *a = (uint8_t *)(((uintptr_t)p + al - 1) & ~(uintptr_t)(al - 1));
  while (a != p && (size_t)(a - p) < MH_MIN) a += al;
  if (a != p) {   /* the bytes before a become a free block of their own */
    size_t g = (size_t)(a - p), s = MH_SZ(b);
    H.st.used -= s; H.st.blocks--;
    MhBlk *c = (MhBlk *)(a - MH_HDR);
    b->sz = g | MH_FREE | (b->sz & MH_PFREE);
    if (b->sz & MH_PFREE) { MhBlk *v = (MhBlk *)((uint8_t *)b - b->psz); mh_unlink(v); v->sz = (MH_SZ(v) + g) | MH_FREE; g = MH_SZ(v); b = v; }
    c->psz = g; c->sz = (s - (size_t)(a - p)) | MH_PFREE;
    mh_link(b);
    mh_used(c); b = c;
  }
  size_t need = mh_need(n);
  H.st.used -= MH_SZ(b); H.st.blocks--; mh_tail(b, need); mh_used(b);
  return a;
}
const MeHeapStats *me_heap_stats(void) { return &H.st; }
int me_heap_check(void)
{
  int bad = 0; size_t used = 0; long blocks = 0;
  for (int k = 0; k < H.np; k++) {
    size_t nfree = 0; int pfree = 0; uint8_t *end = H.pe[k];
    MhBlk *b = (MhBlk *)H.pb[k];
    while ((uint8_t *)b < end) {
      size_t s = MH_SZ(b);
      if (s < MH_MIN || (s & (MH_ALIGN - 1)) || (uint8_t *)b + s > end) return bad + 1;
      if (!!(b->sz & MH_PFREE) != pfree) bad++;
      if (pfree && b->psz != nfree) bad++;
      if (b->sz & MH_FREE) {
        if (pfree) bad++;   /* two free blocks side by side */
        int fl, sl; mh_map(s, &fl, &sl); MhBlk *x = H.fh[fl][sl]; while (x && x != b) x = x->nf; if (!x) bad++;
        pfree = 1; nfree = s;
      } else { pfree = 0; used += s; blocks++; }
      b = mh_next(b);
    }
    if ((uint8_t *)b != end || MH_SZ(b) != 0 || !!(b->sz & MH_PFREE) != pfree) bad++;
  }
  if (used != H.st.used || blocks != H.st.blocks) bad++;
  return bad;
}
