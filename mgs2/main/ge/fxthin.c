/* fxthin.c (task #114): see fxthin.h.  Portable C; reads game memory only. */
#include <string.h>
#include "fxthin.h"
#ifndef __psp__
#include <stdio.h>
#include <stdlib.h>
#endif

#define FX_RAM (32u << 20)
#define RD(a) (*(const uint32_t *)(ram + (a)))
static const struct { const char *s; int k; uint32_t obj, n, stride; } NAME[] = {   /* the file name, its kind, where the actor keeps its
                                         DG object (task #161: the first of n pointers, stride bytes apart) */
  { "d_splash_camera.c", 0, 0x40, 1, 0 }, { "ft_splsh.c", 1, 0x860, 1, 0 }, { "rain.c", 2, 0, 0, 0 }, { "rain_slow.c", 2, 0, 0, 0 },
  { "body_sph.c", 3, 0x78, 1, 0 }, { "charaspread.c", 3, 0xa8, 1, 0 }, { "rain_gas_pers.c", 3, 0x184, 1, 0 },
  { "line_sph.c", 3, 0xa0, 35, 0x50 },   /* (task #161) */
  { "d_splash_fall.c", 4, 0x4c, 1, 0 }, { "d_splash_floor.c", 5, 0x50, 1, 0 }, { "ripple.c", 6, 0x40, 3, 4 },
  { "d_splash_parts_slow.c", 7, 0x44, 1, 0 }, { "d_thunder_parts.c", 8, 0x4b0, 3, 4 } };   /* gl38 (FxThin.all only) */
#define NAME_N (int)(sizeof NAME / sizeof NAME[0])
#define LV_HEAD 0x1b9d94u   /* the first level's head actor; level k's at + 0x64 k, its tail at head + 0x30 */
#define LV_N 11
#define WALK_MAX 4096       /* actors a scan walks at most (the deck has about 290) */
#define TAB_N 64            /* task #154: a streak actor's table: up to TAB_N entries of 0x20 bytes from +0xb0 to the end of the actor's
                               block (rain.c's block 0x5b0 bytes: 40 entries; rain_slow.c's 0x8b0: 64), the block's address at +0x10 */
#define TAB_0 0xb0u

static int ok_addr(uint32_t a, uint32_t n) { return a >= 0x100000u && a <= FX_RAM - n && !(a & 3); }
static int clamp_share(int n) { return n < 1 ? 1 : n > 8 ? 8 : n; }
void fx_thin_init(FxThin *t, int keep_sheets, int keep_splash, int keep_streaks)
{
  memset(t, 0, sizeof *t);
  t->keep[0] = clamp_share(keep_sheets); t->keep[1] = clamp_share(keep_splash); t->keep[2] = keep_streaks == 0 ? 0 : clamp_share(keep_streaks);   /* task #158: 0 none */
  t->keep_calls = 1; t->keep[3] = 1;
  for (int k = 4; k < FX_KINDS; k++) t->keep[k] = 1;   /* gl38: never thinned */
}
void fx_thin_calls(FxThin *t, int keep_calls) { t->keep_calls = clamp_share(keep_calls); }   /* task #158 */
void fx_thin_rainspr(FxThin *t, int keep) { t->keep[3] = keep ? 1 : 0; }   /* task #161 */
int fx_thin_on(const FxThin *t) { return t->keep[0] > 1 || t->keep[1] > 1 || t->keep[2] != 1 || t->keep_calls > 1 || !t->keep[3]; }
static int name_kind(const uint8_t *ram, uint32_t np)   /* the NAME entry, -1: none of ours */
{
  if (!ok_addr(np, 32)) return -1;
  for (int k = 0; k < NAME_N; k++) if (!memcmp(ram + np, NAME[k].s, strlen(NAME[k].s) + 1)) return k;
  return -1;
}
static unsigned fx_slot(const FxThin *t, uint32_t act, uint32_t np)   /* the pair's entry, or the empty one where it goes */
{
  unsigned h = ((act ^ np * 0x9e3779b1u) * 0x9e3779b1u) >> 23 & (FX_CACHE - 1);
  while (t->c[h].ak && ((t->c[h].ak & ~3u) != act || t->c[h].np != np)) h = (h + 1) & (FX_CACHE - 1);
  return h;
}
/* the block of the allocator that starts at p: [p, the next block's header), when the header 0x10 bytes before p looks right (previous,
   next, 0, size: the next header's previous is this header); 0 otherwise */
static uint32_t blk_end(const uint8_t *ram, uint32_t p)
{
  const uint32_t nx = ok_addr(p, 0x10) ? RD(p - 0x0c) : 0;
  return ok_addr(nx, 0x10) && nx > p && nx - p <= 0x100000u && RD(nx) == p - 0x10 ? nx : 0;
}
#ifndef __psp__
static void fx_rdump(const uint8_t *ram, uint32_t a, uint32_t ae)   /* x86 (MGS2_FX_RDUMP=1): the first 4 streak actors (by address and
                                                                          name) with their tables and blocks, once */
{
  static int on = -1, nd; static uint32_t seen[4];
  if (on < 0) on = getenv("MGS2_FX_RDUMP") && atoi(getenv("MGS2_FX_RDUMP"));
  if (!on || nd == 4) return;
  const uint32_t np = RD(a + 0x28);
  for (int i = 0; i < nd; i++) if (seen[i] == (a ^ np)) return;
  seen[nd++] = a ^ np;
  printf("fxrdump: actor %08x name %.20s, header %08x %08x %08x %08x, its block %x bytes\n", a, ok_addr(np, 32) ? (const char *)ram + np : "?",
         RD(a - 0x10), RD(a - 0x0c), RD(a - 8), RD(a - 4), ae ? ae - a : 0);
  for (uint32_t i = 0; i < TAB_N && (!ae || TAB_0 + 0x20 * (i + 1) <= ae - a); i++) {
    const uint32_t e = a + TAB_0 + 0x20 * i, p = RD(e + 0x10);
    printf("fxrdump: %2u", i); for (uint32_t q = 0; q < 0x20; q += 4) printf(" %08x", RD(e + q));
    if (ok_addr(p, 0x10)) { const uint32_t nx = RD(p - 0x0c); printf(" h %08x %08x %08x %08x", RD(p - 0x10), RD(p - 0x0c), RD(p - 8), RD(p - 4));
      if (ok_addr(nx, 0x10)) printf(" nx %08x %08x %08x %08x", RD(nx), RD(nx + 4), RD(nx + 8), RD(nx + 12)); }
    printf("\n"); }
}
#endif
/* task #154: a streak actor's blocks, in address order, neighbours joined, as ranges for the streaks */
static void fx_streaks(FxThin *t, const uint8_t *ram, uint32_t a)
{
  const uint32_t ae = blk_end(ram, a);
#ifndef __psp__
  fx_rdump(ram, a, ae);
#endif
  if (!ae || ae - a < TAB_0 + 0x20) { t->bad++; return; }
  const uint32_t ne = (ae - a - TAB_0) / 0x20 < TAB_N ? (ae - a - TAB_0) / 0x20 : TAB_N;   /* the entries the block holds */
  XcThin b[TAB_N]; int nb = 0;
  for (uint32_t i = 0; i < ne; i++) {
    const uint32_t p = RD(a + TAB_0 + 0x10 + 0x20 * i);
    if (!p) { t->tab_empty++; continue; }
    const uint32_t e = blk_end(ram, p);
    if (!e) { t->tab_bad++; continue; }
    int j = nb; while (j > 0 && b[j - 1].lo > p) { b[j] = b[j - 1]; j--; }   /* (in address order: one compare each when the table is) */
    b[j].lo = p; b[j].hi = e; nb++;
  }
  t->found[2]++; t->tab_blocks += nb;
  for (int i = 0; i < nb; ) {   /* a block that starts at the last one's end + its header (16 bytes no effect's data is in) joins it */
    const uint32_t lo = b[i].lo; uint32_t hi = b[i].hi;
    for (i++; i < nb && b[i].lo <= hi + 0x10; i++) if (b[i].hi > hi) hi = b[i].hi;
    if (t->n == FX_RANGES) { t->over++; continue; }
    t->r[t->n].lo = lo; t->r[t->n].hi = hi; t->r[t->n].keep = t->keep[2]; t->r[t->n].line = 1; t->r[t->n].kcall = t->keep_calls; t->n++; t->tab_ranges++;
  }
}
/* task #161: a rain sprite actor's blocks into t->d (a table's entries only inside the actor's own block) */
static void fx_rainspr(FxThin *t, const uint8_t *ram, uint32_t a, int q)
{
  uint32_t lim = 0x1000;   /* (the walk checked the actor's first 0x1000 bytes) */
  if (NAME[q].n > 1) { const uint32_t ae = blk_end(ram, a); if (!ae) { t->bad++; return; } if (ae - a < lim) lim = ae - a; }
  t->found[3]++;
  for (uint32_t i = 0; i < NAME[q].n; i++) {
    const uint32_t o = NAME[q].obj + NAME[q].stride * i;
    if (o + 4 > lim) break;
    const uint32_t p = RD(a + o);
    if (!p) continue;
    const uint32_t e = blk_end(ram, p);
    if (!e) { t->spr_bad++; continue; }
    if (t->nd == FX_DROPS) { t->spr_over++; continue; }
    t->d[t->nd].lo = p; t->d[t->nd].hi = e; t->nd++; t->spr_blocks++;
  }
}
/* task #161: the blocks in address order (a shell sort: no library call), neighbours joined as fx_streaks joins them */
static void fx_drops_join(FxThin *t)
{
  static const int gap[] = { 109, 41, 19, 5, 1 };
  for (int g = 0; g < 5; g++) for (int i = gap[g]; i < t->nd; i++) {
    const uint32_t lo = t->d[i].lo, hi = t->d[i].hi; int j = i;
    while (j >= gap[g] && t->d[j - gap[g]].lo > lo) { t->d[j] = t->d[j - gap[g]]; j -= gap[g]; }
    t->d[j].lo = lo; t->d[j].hi = hi; }
  int m = 0;
  for (int i = 0; i < t->nd; i++) {
    if (m && t->d[i].lo <= t->d[m - 1].hi + 0x10) { if (t->d[i].hi > t->d[m - 1].hi) t->d[m - 1].hi = t->d[i].hi; }
    else t->d[m++] = t->d[i]; }
  t->nd = m; t->spr_ranges += m;
}
int fx_drop_has(const FxThin *t, uint32_t src)   /* task #161 */
{
  int lo = 0, hi = t->nd - 1;
  while (lo <= hi) { const int m = (lo + hi) >> 1;
    if (src < t->d[m].lo) hi = m - 1; else if (src >= t->d[m].hi) lo = m + 1; else return 1; }
  return 0;
}
/* gl38: a rain owner's blocks into t->b (FxThin.all) */
static void fx_bd_blocks(FxThin *t, const uint8_t *ram, uint32_t a, int q)
{
  for (uint32_t i = 0; i < NAME[q].n; i++) {
    const uint32_t obj = RD(a + NAME[q].obj + NAME[q].stride * i);
    if (!obj) continue;
    const uint32_t nx = blk_end(ram, obj);
    if (!nx) { t->b_bad++; continue; }
    if (t->nb == FX_RANGES) { t->b_over++; continue; }
    t->b[t->nb].lo = obj; t->b[t->nb].hi = nx; t->b[t->nb].kind = (uint32_t)NAME[q].k; t->nb++; }
}
int fx_thin_scan(FxThin *t, const uint8_t *ram, int lines)
{
  t->n = 0; t->nd = 0; t->nb = 0; t->scans++;
  if (!fx_thin_on(t)) return 0;
  if (memcmp(t->sig, ram + 0x1f1c00u, sizeof t->sig)) {   /* another stage image: its actors' functions and names are new */
    memcpy(t->sig, ram + 0x1f1c00u, sizeof t->sig); memset(t->c, 0, sizeof t->c); t->nc = 0; t->flushes++; }
  int steps = 0;
  for (int lv = 0; lv < LV_N; lv++) {
    uint32_t head = LV_HEAD + 0x64u * (uint32_t)lv, tail = head + 0x30, a = RD(head);
    while (a != tail) {
      if (!ok_addr(a, 0x1000) || ++steps > WALK_MAX) { t->bad++; t->actors += steps; fx_drops_join(t); return t->n; }
      uint32_t act = RD(a + 4) & ~3u, np = RD(a + 0x28);
      unsigned h = fx_slot(t, act, np);
      if (!t->c[h].ak) {   /* a pair not seen yet: its name read once */
        if (t->nc >= FX_CACHE / 4 * 3) { memset(t->c, 0, sizeof t->c); t->nc = 0; t->flushes++; h = fx_slot(t, act, np); }
        const int q = name_kind(ram, np);
        t->c[h].ak = act | (q < 0 ? 1u : 2u); t->ck[h] = (uint8_t)(q < 0 ? 0 : q); t->c[h].np = np; t->nc++; t->named++; }
      if ((t->c[h].ak & 3) == 2) {
        const int q = t->ck[h], k = NAME[q].k;
        if (t->all && k != 2 && k != 3) fx_bd_blocks(t, ram, a, q);   /* gl38 */
        if (t->keep[k] != 1 || (k == 2 && t->keep_calls > 1)) {   /* (task #158: the streaks' 0, none, and their calls 1 in N take
                                                                       their ranges too) */
          if (k == 3) { if (lines) fx_rainspr(t, ram, a, q); }   /* task #161 (lines: C4 on, the sprites are drawn) */
          else if (k == 2) { if (lines) fx_streaks(t, ram, a); }
          else { const uint32_t obj = RD(a + NAME[q].obj);
            if (obj) {   /* the object's block: [obj, the next block), the next block's previous one being this block */
              const uint32_t nx = blk_end(ram, obj);
              if (!nx) t->bad++;
              else if (t->n == FX_RANGES) t->over++;
              else { t->r[t->n].lo = obj; t->r[t->n].hi = nx; t->r[t->n].keep = t->keep[k]; t->r[t->n].line = 0; t->r[t->n].kcall = 1; t->n++;
                     t->found[k]++; }
            } }
        }
      }
      a = RD(a);
    }
  }
  t->actors += steps;
  fx_drops_join(t);   /* task #161 */
  return t->n;
}

#ifndef __psp__
/* x86 census (sb15, task #152; MGS2_FX_WHO=1): which actor's block each effect call's records come from (XlCall.src), by the actor's
   file name and where in the actor the block's pointer sits; printed at the end */
#define WHO_K 5
static const uint32_t WHO_E[WHO_K] = { 0x19bb50u, 0x19be50u, 0x19af68u, 0x19a9f0u, 0x19ac78u };
static const char *const WHO_N[WHO_K] = { "sprites +3740", "quads +3a38", "streaks +2b60", "fans +25f0", "strips +2870" };
#define WHO_ROWS 512
static struct { char name[40]; uint32_t off, size; long n[WHO_K], sh[WHO_K]; } who[WHO_ROWS];   /* sh: calls from a block more than
                                                                                   one actor points at (MGS2_FX_WHO=3) */
static int nwho, who_on = -1;
static long who_calls[WHO_K], who_nosrc[WHO_K], who_noblk[WHO_K], who_noown[WHO_K], who_over, who_bounded, who_unbounded, who_shared[WHO_K];
static struct { uint32_t h, nx, a, off; int nref; } wb[256]; static int nwb, wbi;   /* recent blocks: header, next header, owner actor,
                                                                                   offset; actors pointing at it (MGS2_FX_WHO=3) */
static void who_print(void)
{
  printf("fxwho: calls by kind:"); for (int k = 0; k < WHO_K; k++) printf(" %s %ld (no src %ld, no block %ld, no owner %ld);", WHO_N[k],
    who_calls[k], who_nosrc[k], who_noblk[k], who_noown[k]); printf(" rows over %ld; actors scanned by their block %ld, by 0x400 bytes %ld\n",
    who_over, who_bounded, who_unbounded);
  for (int i = 0; i < nwho; i++) { printf("fxwho: %-28s +%03x size %6u:", who[i].name, who[i].off, who[i].size);
    for (int k = 0; k < WHO_K; k++) { if (who[i].n[k]) printf(" %s %ld", WHO_N[k], who[i].n[k]); }
    printf("\n"); }
  if (who_on >= 3) {   /* task #154: how sure the owners are */
    printf("fxwhosh: calls from a block more than one actor points at, by kind:");
    for (int k = 0; k < WHO_K; k++) { printf(" %s %ld;", WHO_N[k], who_shared[k]); }
    printf("\n");
    for (int i = 0; i < nwho; i++) { int any = 0; for (int k = 0; k < WHO_K; k++) any |= who[i].sh[k] != 0;
      if (!any) continue;
      printf("fxwhosh: %-28s +%03x:", who[i].name, who[i].off);
      for (int k = 0; k < WHO_K; k++) if (who[i].sh[k]) printf(" %s %ld of %ld", WHO_N[k], who[i].sh[k], who[i].n[k]);
      printf("\n"); } }
}
static uint32_t who_span(const uint8_t *ram, uint32_t a)   /* the actor's own block, [a, the next block); 0x400 bytes when its header does
                                                              not look right.  (The first census scanned 0x1000 bytes from every actor,
                                                              into the blocks after a small one: an owner could be the actor before it.) */
{
  const uint32_t nx = RD(a - 0x0c);
  if (ok_addr(nx, 0x10) && nx > a && nx - a <= 0x100000u && RD(nx) == a - 0x10) { who_bounded++; return nx - a; }
  who_unbounded++; return 0x400;
}
static int who_owner(const uint8_t *ram, uint32_t obj, uint32_t *a_out, uint32_t *off_out)
{
  int steps = 0;
  for (int lv = 0; lv < LV_N; lv++) {
    uint32_t head = LV_HEAD + 0x64u * (uint32_t)lv, tail = head + 0x30, a = RD(head);
    while (a != tail) {
      if (!ok_addr(a, 0x1000) || ++steps > WALK_MAX) return 0;
      const uint32_t span = who_span(ram, a);
      if (ok_addr(a, span)) for (uint32_t o = 0; o < span; o += 4) if (RD(a + o) == obj) { *a_out = a; *off_out = o; return 1; }
      a = RD(a);
    }
  }
  return 0;
}
/* MGS2_FX_WHO=3 (task #154): the actors that point at obj from their own block; the first 64 blocks more than one points at printed with
   them (file name + where) */
static int who_nref(const uint8_t *ram, uint32_t obj, uint32_t size)
{
  static int shown; int steps = 0, nr = 0; char line[1024]; int ln = 0; line[0] = 0;
  for (int lv = 0; lv < LV_N; lv++) {
    uint32_t head = LV_HEAD + 0x64u * (uint32_t)lv, tail = head + 0x30, a = RD(head);
    while (a != tail) {
      if (!ok_addr(a, 0x1000) || ++steps > WALK_MAX) return nr;
      const uint32_t span = who_span(ram, a);
      if (ok_addr(a, span)) for (uint32_t o = 0; o < span; o += 4) if (RD(a + o) == obj) {
        nr++;
        if (ln < 900) { const uint32_t np = RD(a + 0x28); char nm[24] = "?";
          if (ok_addr(np, 24)) { memcpy(nm, ram + np, 23); nm[23] = 0; for (int i = 0; nm[i]; i++) if (nm[i] < 32 || nm[i] > 126) { nm[i] = 0; break; } }
          ln += snprintf(line + ln, sizeof line - (size_t)ln, " %s+%03x", nm, o); }
        break; }
      a = RD(a);
    }
  }
  if (nr > 1 && shown < 64) { shown++; printf("fxwhosh: block %08x size %x, %d actors:%s\n", obj, size, nr, line); }
  return nr;
}
/* the block holding src and its owner (wb[*j]): 0, or -1 no source, -2 no block, -3 no actor points at it */
static int who_find(const uint8_t *ram, uint32_t src, int *jo)
{
  if (!ok_addr(src, 16)) return -1;
  int j = -1;
  for (int i = 0; i < nwb; i++) if (src >= wb[i].h + 0x10 && src < wb[i].nx && RD(wb[i].h + 4) == wb[i].nx && RD(wb[i].nx) == wb[i].h) { j = i; break; }
  if (j < 0) {   /* the block holding src: a header [previous, next, 0, size] whose next header points back at it */
    uint32_t h = (src & ~3u) - 0x10, lo = src > 0x180000u ? src - 0x80000u : 0x100000u, nx = 0;
    for (; h >= lo; h -= 4) { if (RD(h + 8)) continue; nx = RD(h + 4);
      if (nx > src && nx - h <= 0x100000u && ok_addr(nx, 0x10) && RD(nx) == h) break; }
    if (h < lo) return -2;
    j = wbi; wbi = (wbi + 1) & 255; if (nwb < 256) nwb++;
    wb[j].h = h; wb[j].nx = nx; wb[j].a = 0; wb[j].off = 0; wb[j].nref = 0;
  }
  *jo = j;
  const uint32_t obj = wb[j].h + 0x10;
  if (wb[j].a == 1) return -3;   /* a block no actor points at (checked once while it stays in wb) */
  if (!wb[j].a || !ok_addr(wb[j].a, 0x1000) || RD(wb[j].a + wb[j].off) != obj) {
    uint32_t a, off; if (!who_owner(ram, obj, &a, &off)) { wb[j].a = 1; return -3; }
    wb[j].a = a; wb[j].off = off; wb[j].nref = who_on >= 3 ? who_nref(ram, obj, wb[j].nx - obj) : 0; }
  return 0;
}
static void who_nm(const uint8_t *ram, uint32_t a, char *nm, int cap)   /* the actor's file name (printable, cap > 1) */
{
  const uint32_t np = RD(a + 0x28); nm[0] = '?'; nm[1] = 0;
  if (ok_addr(np, 40)) { memcpy(nm, ram + np, (size_t)cap - 1); nm[cap - 1] = 0; for (int i = 0; nm[i]; i++) if (nm[i] < 32 || nm[i] > 126) { nm[i] = 0; break; } }
}
int fx_who_name(const uint8_t *ram, uint32_t src, char *name, int cap)
{
  int j; const int r = who_find(ram, src, &j);
  if (!r) who_nm(ram, wb[j].a, name, cap);
  return r;
}
void fx_who_note(const uint8_t *ram, uint32_t entry, uint32_t src)
{
  if (who_on < 0) { who_on = getenv("MGS2_FX_WHO") ? atoi(getenv("MGS2_FX_WHO")) : 0; if (who_on < 0) who_on = 0; if (who_on) atexit(who_print); }
  if (!who_on) return;
  int k = 0; while (k < WHO_K && WHO_E[k] != entry) k++;
  if (k == WHO_K) return;
  who_calls[k]++;
  int j; const int r = who_find(ram, src, &j);
  if (r == -1) { who_nosrc[k]++; return; }
  if (r == -2) { who_noblk[k]++; return; }
  if (r == -3) { who_noown[k]++; return; }
  const uint32_t obj = wb[j].h + 0x10;
  char nm[40]; who_nm(ram, wb[j].a, nm, sizeof nm);
  uint32_t size = wb[j].nx - obj;
  int ro = 0; while (ro < nwho && (strcmp(who[ro].name, nm) || who[ro].off != wb[j].off)) ro++;
  if (ro == nwho) { if (nwho == WHO_ROWS) { who_over++; return; } snprintf(who[ro].name, sizeof who[ro].name, "%s", nm); who[ro].off = wb[j].off; who[ro].size = size; nwho++; }
  who[ro].n[k]++;
  if (wb[j].nref > 1) { who[ro].sh[k]++; who_shared[k]++; }
}
#endif
