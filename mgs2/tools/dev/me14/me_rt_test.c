/* me_rt_test.c (task #149): x86 checks of ge/me_rt.c.  me_floorf / me_ceilf against the C library on every one of the 2^32 floats
   (NaNs: both NaN); me_memcpy / me_memset / me_memcmp against the C library at every alignment and length up to 300 bytes; the heap
   under a random load (allocations, frees, reallocs, aligned blocks; contents checked, me_heap_check every 1,000 steps, everything
   freed at the end must give one free block again); the heap over pools: a small first region, regions added by me_heap_add and by
   a hook (me_heap_grow) that gives a region when nothing fits and refuses after a number of them, and its second hook that takes a
   region back once it is empty, under the same load (contents, me_heap_check every 1,000 steps; everything freed at the end must
   leave each kept pool one free block, which me_heap_check proves, and must give every region the hook gave back).
   Prints FAIL lines and a last line "me_rt_test: ok" or "me_rt_test: N failures". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "me_rt.h"

static long fails;
static uint64_t rs = 0x9e3779b97f4a7c15ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 16); }

static void t_round(void)
{
  long bad_f = 0, bad_c = 0;
  for (uint64_t u = 0; u <= 0xffffffffull; u++) {
    uint32_t b = (uint32_t)u; float x; memcpy(&x, &b, 4);
    float a = me_floorf(x), r = floorf(x), c = me_ceilf(x), s = ceilf(x);
    uint32_t ab, rb, cb, sb; memcpy(&ab, &a, 4); memcpy(&rb, &r, 4); memcpy(&cb, &c, 4); memcpy(&sb, &s, 4);
    if (isnan(x)) { if (!isnan(a)) bad_f++; if (!isnan(c)) bad_c++; continue; }
    if (ab != rb) { if (bad_f++ < 5) printf("FAIL floorf %08x: %08x, libm %08x\n", b, ab, rb); }
    if (cb != sb) { if (bad_c++ < 5) printf("FAIL ceilf %08x: %08x, libm %08x\n", b, cb, sb); }
  }
  printf("floorf: %ld differ, ceilf: %ld differ (of 2^32)\n", bad_f, bad_c);
  fails += bad_f + bad_c;
}
static void t_mem(void)
{
  static uint8_t a[512], b[512], c[512], d[512]; long bad = 0;
  for (int i = 0; i < 512; i++) { a[i] = (uint8_t)rnd(); c[i] = (uint8_t)rnd(); }
  for (int so = 0; so < 8; so++) for (int dof = 0; dof < 8; dof++) for (int n = 0; n <= 300; n++) {
    memcpy(b, c, 512); memcpy(d, c, 512);
    me_memcpy(b + dof, a + so, n); memcpy(d + dof, a + so, n);
    if (memcmp(b, d, 512)) { if (bad++ < 5) printf("FAIL memcpy so %d do %d n %d\n", so, dof, n); }
    int v = (int)(rnd() & 0x1ff) - 128;
    me_memset(b + dof, v, n); memset(d + dof, v, n);
    if (memcmp(b, d, 512)) { if (bad++ < 5) printf("FAIL memset do %d n %d v %d\n", dof, n, v); }
    memcpy(b, a, 512); if (n) b[so + (int)(rnd() % (unsigned)n)] ^= (uint8_t)(1 + rnd() % 255);
    for (int k = 0; k < 2; k++) {
      int x = me_memcmp(a + so, b + so, n), y = memcmp(a + so, b + so, n);
      if ((x > 0) != (y > 0) || (x < 0) != (y < 0)) { if (bad++ < 5) printf("FAIL memcmp so %d n %d: %d, libc %d\n", so, n, x, y); }
      memcpy(b, a, 512);
    }
    int x = me_memcmp(a + so, c + dof, n), y = memcmp(a + so, c + dof, n);
    if ((x > 0) != (y > 0) || (x < 0) != (y < 0)) { if (bad++ < 5) printf("FAIL memcmp2 so %d do %d n %d\n", so, dof, n); }
  }
  printf("memcpy, memset, memcmp: %ld differ\n", bad);
  fails += bad;
}
#define NS 4096
static void t_heap(size_t region, long steps, int maxlog)
{
  static struct { uint8_t *p; size_t n; uint8_t v; } s[NS];
  uint8_t *reg = malloc(region); long bad = 0, refused = 0;
  memset(s, 0, sizeof s);
  if (me_heap_init(reg + 3, region - 3)) { printf("FAIL heap init\n"); fails++; free(reg); return; }
  for (long t = 0; t < steps; t++) {
    int i = (int)(rnd() % NS), op = (int)(rnd() % 10);
    size_t n = (size_t)(rnd() % (1u << (1 + rnd() % (unsigned)maxlog)));
    if (s[i].p) {   /* check, then free or realloc */
      for (size_t k = 0; k < s[i].n; k++) if (s[i].p[k] != (uint8_t)(s[i].v + k)) { if (bad++ < 5) printf("FAIL heap contents step %ld\n", t); break; }
      if (op < 6) { me_free(s[i].p); s[i].p = 0; }
      else {
        uint8_t *q = me_realloc(s[i].p, n);
        if (!q && n) { refused++; continue; }
        size_t keep = n < s[i].n ? n : s[i].n;
        for (size_t k = 0; k < keep; k++) if (q[k] != (uint8_t)(s[i].v + k)) { if (bad++ < 5) printf("FAIL realloc contents step %ld\n", t); break; }
        s[i].p = q; s[i].n = n; s[i].v = (uint8_t)rnd(); for (size_t k = 0; k < n; k++) q[k] = (uint8_t)(s[i].v + k);
        if (!n) s[i].p = 0;
      }
    } else {
      uint8_t *q;
      if (op == 0) { size_t al = (size_t)1 << (4 + rnd() % 4); q = me_aligned_alloc(al, n); if (q && ((uintptr_t)q & (al - 1))) { if (bad++ < 5) printf("FAIL aligned\n"); } }
      else if (op == 1) { q = me_calloc(1, n); if (q) for (size_t k = 0; k < n; k++) if (q[k]) { if (bad++ < 5) printf("FAIL calloc\n"); break; } }
      else q = me_malloc(n);
      if (!q) { refused++; continue; }
      if ((uintptr_t)q & (sizeof(void *) == 8 ? 15 : 7)) { if (bad++ < 5) printf("FAIL alignment %p\n", (void *)q); }
      s[i].p = q; s[i].n = n; s[i].v = (uint8_t)rnd(); for (size_t k = 0; k < n; k++) q[k] = (uint8_t)(s[i].v + k);
    }
    if (t % 1000 == 999) { int c = me_heap_check(); if (c) { if (bad++ < 5) printf("FAIL heap check step %ld: %d\n", t, c); } }
  }
  for (int i = 0; i < NS; i++) if (s[i].p) { me_free(s[i].p); s[i].p = 0; }
  const MeHeapStats *h = me_heap_stats();
  int c = me_heap_check(); size_t used0 = h->used;
  void *all = me_malloc(h->size - 2 * sizeof(size_t));   /* everything back as one block (the whole region) */
  printf("heap %zu KB, %ld steps, sizes to 2^%d: peak %zu KB, extent %zu KB, blocks peak %ld, refused %ld, allocs %ld frees %ld reallocs %ld "
         "(moved %ld); after freeing all: used %zu, check %d, one block %s\n", region >> 10, steps, maxlog, h->peak >> 10, h->extent >> 10,
         h->blocks_peak, refused, h->allocs, h->frees, h->reallocs, h->moved, used0, c, all ? "yes" : "NO");
  if (c || used0 || !all) bad++;
  fails += bad; free(reg);
}
/* the hooks: regions from the C library's malloc, at least 16 KB, refused after max_grow given and not taken back; each given back
   is checked to be one the hook gave and still out */
#define NGR 4096
static void *gr[NGR]; static size_t gro[NGR], grn[NGR]; static int ngr, nout, max_grow, given, bad_back; static long rs_in, rs_mv, rs_no, rs_bad;
static size_t grow_min = 16u << 10;   /* the least region more gives */
static size_t more_pad;               /* pools v4: more gives 0-3 times this many bytes more than asked (as psp/xl_psp.c rounds up), so
                                         that a table often has a free block after it in its region */
static void *more(size_t want, size_t *got)
{
  int k = 0; while (k < ngr && gr[k]) k++;   /* (a slot less emptied, or a new one) */
  if (nout == max_grow || k == NGR) return 0;
  size_t n = (want < grow_min ? grow_min : want) + (more_pad ? (size_t)(rnd() % 4) * more_pad : 0);
  void *r = malloc(n + 5); if (!r) return 0;
  gro[k] = 5; grn[k] = n; gr[k] = r; if (k == ngr) ngr++;
  nout++; given++; *got = n; return (uint8_t *)r + 5;   /* (an odd start: the pool aligns itself) */
}
static void less(void *region)
{
  for (int k = 0; k < ngr; k++) if (gr[k] && (uint8_t *)gr[k] + gro[k] == (uint8_t *)region) { free(gr[k]); gr[k] = 0; nout--; return; }
  bad_back++;
}
/* pools v3's hook: a region the hook gave made want bytes long: by realloc (its start kept 5 bytes in), or moved by hand to a new start
   0-15 bytes into a new block with its bytes copied (another alignment), or refused (1 in 8) */
static void *resize(void *region, size_t want)
{
  int k = 0; while (k < ngr && !(gr[k] && (uint8_t *)gr[k] + gro[k] == (uint8_t *)region)) k++;
  if (k == ngr) { rs_bad++; return 0; }
  unsigned c = (unsigned)(rnd() % 8);
  if (!c) { rs_no++; return 0; }
  if (c < 5) { uint8_t *q = realloc(gr[k], want + gro[k]); if (!q) return 0; gr[k] = q; grn[k] = want; rs_in++; return q + gro[k]; }
  size_t d = (size_t)(rnd() % 16); uint8_t *q = malloc(want + d); if (!q) return 0;
  memcpy(q + d, region, grn[k] < want ? grn[k] : want); free(gr[k]); gr[k] = q; gro[k] = d; grn[k] = want; rs_mv++; return q + d;
}
static void t_pools(size_t first, int adds, int grows, long steps, int maxlog, int rsz)
{
  static struct { uint8_t *p; size_t n; uint8_t v; } s[NS];
  uint8_t *reg = malloc(first), *add[4]; long bad = 0, refused = 0;
  memset(s, 0, sizeof s); ngr = nout = given = bad_back = 0; max_grow = grows; rs_in = rs_mv = rs_no = rs_bad = 0;
  if (me_heap_init(reg + 1, first - 1)) { printf("FAIL pools init\n"); fails++; free(reg); return; }
  for (int k = 0; k < adds; k++) { add[k] = malloc(32u << 10); if (me_heap_add(add[k] + 2, (32u << 10) - 2)) { printf("FAIL heap add\n"); bad++; } }
  me_heap_grow(more, less); me_heap_resize(rsz ? resize : 0);
  size_t added0 = me_heap_stats()->added;   /* me_heap_add's regions: kept */
  for (long t = 0; t < steps; t++) {
    int i = (int)(rnd() % NS), op = (int)(rnd() % 10);
    size_t n = (size_t)(rnd() % (1u << (1 + rnd() % (unsigned)maxlog)));
    if (s[i].p) {
      for (size_t k = 0; k < s[i].n; k++) if (s[i].p[k] != (uint8_t)(s[i].v + k)) { if (bad++ < 5) printf("FAIL pools contents step %ld\n", t); break; }
      if (op < 6) { me_free(s[i].p); s[i].p = 0; }
      else {
        uint8_t *q = me_realloc(s[i].p, n);
        if (!q && n) { refused++; continue; }
        size_t keep = n < s[i].n ? n : s[i].n;
        for (size_t k = 0; k < keep; k++) if (q[k] != (uint8_t)(s[i].v + k)) { if (bad++ < 5) printf("FAIL pools realloc contents step %ld\n", t); break; }
        s[i].p = q; s[i].n = n; s[i].v = (uint8_t)rnd(); for (size_t k = 0; k < n; k++) q[k] = (uint8_t)(s[i].v + k);
        if (!n) s[i].p = 0;
      }
    } else {
      uint8_t *q;
      if (op == 0) { size_t al = (size_t)1 << (4 + rnd() % 4); q = me_aligned_alloc(al, n); if (q && ((uintptr_t)q & (al - 1))) { if (bad++ < 5) printf("FAIL pools aligned\n"); } }
      else if (op == 1) { q = me_calloc(1, n); if (q) for (size_t k = 0; k < n; k++) if (q[k]) { if (bad++ < 5) printf("FAIL pools calloc\n"); break; } }
      else q = me_malloc(n);
      if (!q) { refused++; continue; }
      s[i].p = q; s[i].n = n; s[i].v = (uint8_t)rnd(); for (size_t k = 0; k < n; k++) q[k] = (uint8_t)(s[i].v + k);
    }
    if (t % 1000 == 999) { int c = me_heap_check(); if (c) { if (bad++ < 5) printf("FAIL pools check step %ld: %d\n", t, c); } }
  }
  for (int i = 0; i < NS; i++) if (s[i].p) { me_free(s[i].p); s[i].p = 0; }
  const MeHeapStats *h = me_heap_stats();
  int c = me_heap_check();
  printf("pools: first %zu KB, %d added, hook up to %d out%s: %ld steps, sizes to 2^%d: grows %ld, given back %ld, hook refused %ld, "
         "resized %ld (%ld with a free block after it; in place %ld, moved %ld; refused %ld, unknown regions %ld), the most "
         "added %zu KB, peak %zu KB, refused %ld; after freeing all: pools %d (%zu KB), still out %d, used %zu, blocks %ld, check %d, wrong "
         "regions given back %d\n", first >> 10, adds, grows, rsz ? ", resize on" : "", steps, maxlog, h->grows, h->returned, h->grow_fails,
         h->resized, h->resized_tail, rs_in, rs_mv, rs_no, rs_bad, h->added_peak >> 10,
         h->peak >> 10, refused, h->pools, h->size >> 10, nout, h->used, h->blocks, c, bad_back);
  if (c || h->used || h->blocks || h->pools != 1 + adds || nout || bad_back || h->grows != given || h->returned != given || h->added != added0 || rs_bad || h->resized != rs_in + rs_mv) bad++;
  me_heap_grow(0, 0); me_heap_resize(0);
  fails += bad; free(reg); for (int k = 0; k < adds; k++) free(add[k]); for (int k = 0; k < ngr; k++) free(gr[k]);
}
/* pools v3: 8 tables that double from 1-4 KB to 1 MB (as the picture code's do) and 24 small blocks, in a heap whose first region is
   8 KB and whose hook gives regions of the request's own size, so that a table soon fills a region alone; the resize hook in place,
   moved to another alignment, or refused, at random */
static void t_resize(long steps)
{
  static struct { uint8_t *p; size_t n; uint8_t v; } s[32];
  uint8_t *reg = malloc(8u << 10); long bad = 0, refused = 0;
  memset(s, 0, sizeof s); ngr = nout = given = bad_back = 0; max_grow = NGR; rs_in = rs_mv = rs_no = rs_bad = 0; grow_min = 0; more_pad = 40;
  if (me_heap_init(reg + 3, (8u << 10) - 3)) { printf("FAIL resize init\n"); fails++; free(reg); return; }
  me_heap_grow(more, less); me_heap_resize(resize);
  for (long t = 0; t < steps; t++) {
    int i = (int)(rnd() % 32), op = (int)(rnd() % 16);
    if (s[i].p) for (size_t k = 0; k < s[i].n; k++) if (s[i].p[k] != (uint8_t)(s[i].v + k)) { if (bad++ < 5) printf("FAIL resize contents step %ld\n", t); break; }
    size_t n;
    if (i < 8) {
      if (!s[i].p) n = (size_t)1024 << (rnd() % 3);
      else if (op < 13) n = s[i].n * 2 + (size_t)(rnd() % 64);
      else n = s[i].n / 2 + 1;
      if (n > ((size_t)1 << 20)) { me_free(s[i].p); s[i].p = 0; continue; }
    } else {
      if (s[i].p && op < 8) { me_free(s[i].p); s[i].p = 0; continue; }
      n = 16 + (size_t)(rnd() % 2048);
    }
    uint8_t *q = s[i].p ? me_realloc(s[i].p, n) : me_malloc(n);
    if (!q) { refused++; continue; }
    size_t keep = s[i].p ? (n < s[i].n ? n : s[i].n) : 0;
    for (size_t k = 0; k < keep; k++) if (q[k] != (uint8_t)(s[i].v + k)) { if (bad++ < 5) printf("FAIL resize realloc contents step %ld\n", t); break; }
    s[i].p = q; s[i].n = n; s[i].v = (uint8_t)rnd(); for (size_t k = 0; k < n; k++) q[k] = (uint8_t)(s[i].v + k);
    if (t % 1000 == 999) { int c = me_heap_check(); if (c) { if (bad++ < 5) printf("FAIL resize check step %ld: %d\n", t, c); } }
  }
  for (int i = 0; i < 32; i++) if (s[i].p) { me_free(s[i].p); s[i].p = 0; }
  const MeHeapStats *h = me_heap_stats(); int c = me_heap_check();
  printf("resize: %ld steps: grows %ld, given back %ld, resized %ld (%ld with a free block after it; in place %ld, moved %ld; refused %ld, unknown regions %ld), reallocs %ld "
         "(moved %ld), the most added %zu KB, peak %zu KB, refused %ld; after freeing all: pools %d, still out %d, used %zu, blocks %ld, check "
         "%d, wrong regions given back %d\n", steps, h->grows, h->returned, h->resized, h->resized_tail, rs_in, rs_mv, rs_no, rs_bad, h->reallocs, h->moved,
         h->added_peak >> 10, h->peak >> 10, refused, h->pools, nout, h->used, h->blocks, c, bad_back);
  if (c || h->used || h->blocks || h->pools != 1 || nout || bad_back || h->grows != given || h->returned != given || h->added || rs_bad ||
      h->resized != rs_in + rs_mv || !rs_in || !rs_mv || !rs_no || !h->resized_tail || h->resized_tail == h->resized) bad++;
  me_heap_grow(0, 0); me_heap_resize(0); grow_min = 16u << 10; more_pad = 0;
  fails += bad; free(reg); for (int k = 0; k < ngr; k++) if (gr[k]) free(gr[k]);
}
int main(int argc, char **argv)
{
  if (argc < 2 || strchr(argv[1], 'm')) t_mem();
  if (argc < 2 || strchr(argv[1], 'h')) { t_heap(1u << 20, 2000000, 12); t_heap(8u << 20, 2000000, 17); t_heap(64u << 10, 500000, 9); }
  if (argc < 2 || strchr(argv[1], 'p')) { t_pools(64u << 10, 2, 12, 2000000, 17, 0); t_pools(16u << 10, 0, 6, 1000000, 14, 0);
                                          t_pools(256u << 10, 1, 0, 500000, 15, 0); t_pools(8u << 10, 0, 29, 2000000, 16, 0);
                                          t_pools(64u << 10, 2, 12, 2000000, 17, 1); t_pools(8u << 10, 0, 29, 2000000, 16, 1); }
  if (argc < 2 || strchr(argv[1], 's')) { t_resize(1000000); t_resize(300000); }
  if (argc < 2 || strchr(argv[1], 'r')) t_round();
  if (me_faults.overlap) printf("(overlapping copies counted: %ld)\n", me_faults.overlap);
  printf("me_rt_test: %s\n", fails ? "FAILED" : "ok");
  if (fails) printf("me_rt_test: %ld failures\n", fails);
  return fails != 0;
}
