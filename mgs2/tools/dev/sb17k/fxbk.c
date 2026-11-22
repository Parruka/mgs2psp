/* fxbk.c (task #160): see fxbk.h.  The owner look-up is fxthin.c's x86 census (who_find, who_owner, who_span) for the PSP. */
#include <string.h>
#include "fxbk.h"

#define FX_RAM (32u << 20)
#define RD(a) (*(const uint32_t *)(ram + (a)))
#define LV_HEAD 0x1b9d94u   /* fxthin.c: the first level's head actor; level k's at + 0x64 k, its tail at head + 0x30 */
#define LV_N 11
#define WALK_MAX 4096
#define OWN_MAX 4000
static int ok_addr(uint32_t a, uint32_t n) { return a >= 0x100000u && a <= FX_RAM - n && !(a & 3); }

static uint32_t ent[32]; static long ent_n[32]; static int nent;
static uint32_t own_np[OWN_MAX]; static long own_n[OWN_MAX]; static int nown;
static long n_nosrc, n_noblk, n_noown, n_over, n_scan;
static struct { uint32_t h, nx, a, off; } wb[256]; static int nwb, wbi;   /* recent blocks: header, next header, owner actor (1: none),
                                                                            where in it the pointer sits */
static uint32_t bk_span(const uint8_t *ram, uint32_t a)   /* the actor's own block, [a, the next block); 0x400 bytes when its header does
                                                             not look right */
{
  const uint32_t nx = RD(a - 0x0c);
  if (ok_addr(nx, 0x10) && nx > a && nx - a <= 0x100000u && RD(nx) == a - 0x10) return nx - a;
  return 0x400;
}
static int bk_owner(const uint8_t *ram, uint32_t obj, uint32_t *a_out, uint32_t *off_out)
{
  int steps = 0; n_scan++;
  for (int lv = 0; lv < LV_N; lv++) {
    uint32_t head = LV_HEAD + 0x64u * (uint32_t)lv, tail = head + 0x30, a = RD(head);
    while (a != tail) {
      if (!ok_addr(a, 0x1000) || ++steps > WALK_MAX) return 0;
      const uint32_t span = bk_span(ram, a);
      if (ok_addr(a, span)) for (uint32_t o = 0; o < span; o += 4) if (RD(a + o) == obj) { *a_out = a; *off_out = o; return 1; }
      a = RD(a);
    }
  }
  return 0;
}
/* the owner actor of the block holding src: 0 (*a_out), or -1 no source, -2 no block, -3 no actor points at it */
static int bk_find(const uint8_t *ram, uint32_t src, uint32_t *a_out)
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
    wb[j].h = h; wb[j].nx = nx; wb[j].a = 0; wb[j].off = 0;
  }
  const uint32_t obj = wb[j].h + 0x10;
  if (wb[j].a == 1) return -3;   /* a block no actor points at (checked once while it stays in wb) */
  if (!wb[j].a || !ok_addr(wb[j].a, 0x1000) || RD(wb[j].a + wb[j].off) != obj) {
    uint32_t a, off; if (!bk_owner(ram, obj, &a, &off)) { wb[j].a = 1; return -3; }
    wb[j].a = a; wb[j].off = off; }
  *a_out = wb[j].a;
  return 0;
}
static int eidx(uint32_t e)
{
  for (int i = 0; i < nent; i++) if (ent[i] == e) return i;
  if (nent < 31) { ent[nent] = e; return nent++; }
  return 31;
}
static uint32_t oidx(uint32_t np)
{
  for (int i = 0; i < nown; i++) if (own_np[i] == np) { own_n[i]++; return (uint32_t)i + 1; }
  if (nown < OWN_MAX) { own_np[nown] = np; own_n[nown] = 1; return (uint32_t)++nown; }
  n_over++; return 0xffefu;
}
uint32_t fxbk_key(const uint8_t *ram, uint32_t e, uint32_t src, int fx)
{
  const int ei = eidx(e); uint32_t ow = 0;
  if (ei < 32) ent_n[ei]++;
  if (fx && ram) { uint32_t a = 0; const int r = bk_find(ram, src, &a);
    if (r == -1) { n_nosrc++; ow = 0xfff1u; } else if (r == -2) { n_noblk++; ow = 0xfff2u; } else if (r == -3) { n_noown++; ow = 0xfff3u; }
    else ow = oidx(RD(a + 0x28)); }
  return 0xfe000000u | ((uint32_t)ei << 16) | ow;
}
uint32_t fxbk_ekey(uint32_t e) { return 0xfe000000u | ((uint32_t)eidx(e) << 16); }
static void nm_of(const uint8_t *ram, uint32_t np, char *nm, int cap)   /* a file name (printable) */
{
  nm[0] = '?'; nm[1] = 0;
  if (ram && ok_addr(np & ~3u, 64)) { memcpy(nm, ram + np, (size_t)cap - 1); nm[cap - 1] = 0;
    for (int i = 0; nm[i]; i++) if (nm[i] < 32 || nm[i] > 126) { nm[i] = 0; break; } }
}
void fxbk_print(FILE *f, const uint8_t *ram)
{
  fprintf(f, "fxbk: entries");
  for (int i = 0; i < nent; i++) fprintf(f, " %d=%06x(%ld)", i, (unsigned)ent[i], ent_n[i]);
  fprintf(f, "; owners %d; no source %ld, no block %ld, no owner %ld, over %ld; owner scans %ld\n", nown, n_nosrc, n_noblk, n_noown, n_over, n_scan);
  for (int i = 0; i < nown; i++) { char nm[40]; nm_of(ram, own_np[i], nm, sizeof nm);
    fprintf(f, "fxbk: owner %d %08x %ld %s\n", i + 1, (unsigned)own_np[i], own_n[i], nm); }
  if (!ram) return;
  static uint32_t seen[1024]; int ns = 0, steps = 0;   /* the actors alive now: act function and file name, each pair once */
  for (int lv = 0; lv < LV_N; lv++) {
    uint32_t head = LV_HEAD + 0x64u * (uint32_t)lv, tail = head + 0x30, a = RD(head);
    while (a != tail) {
      if (!ok_addr(a, 0x1000) || ++steps > WALK_MAX) break;
      const uint32_t act = RD(a + 4) & ~3u, np = RD(a + 0x28); int k = 0;
      while (k < ns && seen[k] != act) k++;
      if (k == ns && ns < 1024) { seen[ns++] = act; char nm[40]; nm_of(ram, np, nm, sizeof nm);
        fprintf(f, "fxbk: act %08x level %d %s\n", (unsigned)act, lv, nm); }
      a = RD(a);
    }
  }
}
