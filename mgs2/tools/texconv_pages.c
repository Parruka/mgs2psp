/* texconv_pages <EE RAM dump>: convert every material of every texture page (SLES_503.83 page list, re/dg_objects.md §4-5)
   with ge/texconv.c, as the PSP renderer would at stage load. Prints counts, sizes, the time taken and how many
   materials have texels below the alpha-test reference 0x40. Numbers only (the dump is game data).
   Build: gcc -O2 -o texconv_pages texconv_pages.c ../ge/texconv.c */
#include "../ge/texconv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint8_t *ram;
static const uint8_t *ram_ptr(uint32_t a) { return ram + (a & 0x1ffffff); }
static uint32_t r32(uint32_t a) { uint32_t v; memcpy(&v, ram_ptr(a), 4); return v; }
static uint64_t r64(uint32_t a) { uint64_t v; memcpy(&v, ram_ptr(a), 8); return v; }
enum { PAGE_LIST = 0x1e6820, PAGE_COUNT = 0x18e8f0 };

typedef struct { uint32_t page; uint64_t t0, cl; } Key;   /* texture identity: page, TEX0 without CBP / CSA / CLD, CLAMP */
static Key keys[8192]; static int nkeys;

int main(int argc, char **argv)
{
  if (argc < 2) { fprintf(stderr, "usage: texconv_pages <ram dump>\n"); return 1; }
  FILE *f = fopen(argv[1], "rb"); ram = malloc(32 << 20);
  if (!f || fread(ram, 1, 32 << 20, f) != 32u << 20) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
  static uint8_t buf[512 * 512]; uint32_t pal[256]; uint8_t pa[256], have[32];
  long mats = 0, unsup = 0, dup = 0, bytes = 0, texels = 0, pals = 0, missing = 0, palmiss = 0, alpha_lt40 = 0, alpha_all_lt40 = 0, alpha_gt80 = 0, alpha_max = 0, fmt[2] = { 0, 0 };
  clock_t c0 = clock();
  uint32_t np = r32(PAGE_COUNT);
  for (uint32_t i = 0; i < np; i++) {
    uint32_t p = r32(PAGE_LIST + 4 * i), hdr = r32(p + 0x334), nm = r32(hdr + 0x10), ma = r32(p + 0x338);
    TcPage pg; if (tc_page(&pg, ram_ptr(p), ram_ptr) < 0) { printf("page %u: packet not parsed\n", i); continue; }
    for (uint32_t k = 0; k < nm; k++, mats++) {
      uint32_t m = ma + k * 0xa0; uint64_t t0 = 0, cl = 0; int have_t0 = 0;
      for (int q = 1; q < 8; q++) { uint64_t reg = r64(m + 0x28 + 16 * q), v = r64(m + 0x20 + 16 * q); if (reg == 0x06) { t0 = v; have_t0 = 1; } else if (reg == 0x08) cl = v; }
      if (!have_t0 || !tc_texture_size(t0, cl)) { unsup++; continue; }
      uint64_t key0 = t0 & ~(0x3fffull << 37 | 31ull << 56 | 7ull << 61);
      int j; for (j = 0; j < nkeys && !(keys[j].page == p && keys[j].t0 == key0 && keys[j].cl == cl); j++) {}
      TcTex t;
      tc_texture(&pg, t0, cl, &t, buf);   /* converted again for duplicates only to get its used-index mask */
      if (j < nkeys) dup++;
      else { if (nkeys < 8192) keys[nkeys++] = (Key){ p, key0, cl }; bytes += (long)tc_texture_size(t0, cl); texels += (long)t.w * t.h; missing += t.missing; fmt[t.psm == 8]++; }
      tc_palette(&pg, t0 >> 37 & 0x3fff, pal, pa, have); pals++;
      int csa = t0 >> 56 & 31, ma_ = tc_min_alpha(&t, pa, csa);
      for (int e = 0; e < 256; e++) { int x = (e + (t.psm == 4 ? csa * 16 : 0)) & 255; if ((t.used[e >> 3] >> (e & 7) & 1) && !(have[x >> 3] >> (x & 7) & 1)) palmiss++; }
      if (ma_ < 0x40) alpha_lt40++;
      int mx = 0, n = t.psm == 8 ? 256 : 16; for (int e = 0; e < n; e++) if (t.used[e >> 3] >> (e & 7) & 1) { int a = pa[(e + (t.psm == 4 ? csa * 16 : 0)) & 255]; if (a > mx) mx = a; }
      if (mx < 0x40) alpha_all_lt40++;
      if (mx > 0x80) alpha_gt80++;
      if (mx > alpha_max) alpha_max = mx;
    }
  }
  double ms = (clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
  printf("pages %u, materials %ld (%ld not T8/T4 region or repeat), distinct textures %ld (T8 %ld, T4 %ld; %ld duplicates)\n", np, mats, unsup, (long)nkeys, fmt[1], fmt[0], dup);
  printf("texels %ld, GE texture bytes %ld (%.2f MB), texels no upload wrote %ld, used palette entries no upload wrote %ld\n", texels, bytes, bytes / 1048576.0, missing, palmiss);
  printf("palettes converted %ld (one per material), materials with a used texel below alpha 0x40: %ld (all below: %ld)\n", pals, alpha_lt40, alpha_all_lt40);
  printf("materials with a used palette alpha above 0x80 (a blend factor above 1.0, which the GE's 8-bit alpha cannot hold): %ld; highest %#lx\n", alpha_gt80, alpha_max);
  printf("time %.1f ms on this PC (conversion of every material, duplicates included)\n", ms);
  return 0;
}
