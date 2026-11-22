/* xlview.h: a transfer's copy as the picture's late walk reads it (task #180, the ME picture's build order step 4b: me_step4_notes.md
   4b-1).  The copy is a list of extents: runs of guest bytes [a0, a1) kept at off in an arena, sorted by address and apart.  Guest RAM's
   addresses are keys 0 .. XV_SPR - 1 (the 32 MB mask), the scratchpad's XV_SPR + its offset.  A read finds its bytes in the copy where an
   extent holds them and in RAM (or the scratchpad) as they are now elsewhere: the x86 twin's copy laid over RAM for the transfer's walk
   (me_step1_notes.md 1, me11's ov_lay), without writing RAM.  Every extent's ends are multiples of 16.  Portable C, ME-safe; every
   function always inlined (no call between an ME_CALLP and its jalr, and nothing for the unit check's stack sizes to miss). */
#ifndef MGS2_XLVIEW_H
#define MGS2_XLVIEW_H
#include <stdint.h>

#define XV_SPR 0x40000000u         /* the scratchpad's first key (guest RAM: 32 MB; a gap between, so no extent joins the two) */
typedef struct { uint32_t a0, a1, off; } XvExt;
enum { XV_TAG, XV_SPAN, XV_LANE, XV_RAM, XV_NK };   /* what read: a tag, a run of quadwords, a lane's data, the GS map */
typedef struct XmView {
  const XvExt *x; int n;           /* the transfer's extents */
  const uint8_t *arena;            /* where their off counts from */
  const uint8_t *mem, *spr; uint32_t mem_size;   /* guest RAM and the scratchpad as they are now */
  int last;                        /* the extent the last look-up found (a hint: the walk reads in order) */
  long in[XV_NK], out[XV_NK], part[XV_NK];   /* reads from the copy, from RAM, partly from each */
  uint8_t *tmp[4]; uint32_t tmp_n[4]; int tmp_i;   /* the map's reads partly in the copy, put together (xm_ram) */
  uint8_t *fix[4]; uint32_t fix_n;   /* task #180 step 4d-2: four fixed buffers of fix_n bytes each for those reads (the ME's own memory, the
                                        eDRAM, in 4d-4), a longer read in tmp (the picture heap); fix_n 0: tmp only */
  long n_fix, n_tmp;                 /* the reads put together in the fixed buffers, in tmp */
} XmView;

/* the extent holding key k (>= 0), or -(j + 1) with j the first extent above k (n: none) */
static inline __attribute__((always_inline)) int xv_find(XmView *v, uint32_t k)
{
  const XvExt *x = v->x; int n = v->n, i = v->last, lo = 0, hi = n;
  if (i >= 0 && i < n && x[i].a0 <= k) {
    if (k < x[i].a1) return i;
    if (i + 1 == n || k < x[i + 1].a0) return -(i + 2);
    if (k < x[i + 1].a1) { v->last = i + 1; return i + 1; }
  }
  while (lo < hi) { int m = (lo + hi) >> 1; if (x[m].a1 <= k) lo = m + 1; else hi = m; }
  if (lo < n && x[lo].a0 <= k) { v->last = lo; return lo; }
  return -(lo + 1);
}
/* the bytes at key k: a pointer and, in *lim (given: the most wanted, <= what lies in one piece of RAM or the scratchpad), how many of
   them lie together there (all in the copy, or all outside it) */
static inline __attribute__((always_inline)) const uint8_t *xv_at(XmView *v, uint32_t k, uint32_t *lim, int what)
{
  int i = xv_find(v, k);
  if (i >= 0) { uint32_t r = v->x[i].a1 - k; if (r < *lim) *lim = r; v->in[what]++; return v->arena + v->x[i].off + (k - v->x[i].a0); }
  i = -i - 1;
  if (i < v->n) { uint32_t r = v->x[i].a0 - k; if (r < *lim) *lim = r; }
  v->out[what]++;
  return k >= XV_SPR ? v->spr + (k - XV_SPR) : v->mem + k;
}
/* guest address a's quadword as ge/xlive.c qp() aligns it, as a key */
static inline __attribute__((always_inline)) uint32_t xv_key(const XmView *v, uint32_t a) { return (a & 0x80000000u) ? XV_SPR + (a & 0x3ff0) : a & (v->mem_size - 1) & ~15u; }
/* a tag's quadword (the walk reads one; the lanes read on from it, within the margin the copy keeps after each tag) */
static inline __attribute__((always_inline)) const uint32_t *xv_q(XmView *v, uint32_t a) { uint32_t lim = 16; return (const uint32_t *)xv_at(v, xv_key(v, a), &lim, XV_TAG); }
/* n quadwords at key k in one piece (k + 16 n within RAM or the scratchpad): in the copy whole, or outside it whole; else 0 (counted) */
static inline __attribute__((always_inline)) const uint8_t *xv_whole(XmView *v, uint32_t k, uint32_t n, int what)
{
  uint32_t lim = 16 * n; const uint8_t *p = xv_at(v, k, &lim, what);
  if (lim < 16 * n) { v->part[what]++; return 0; }
  return p;
}
#endif
