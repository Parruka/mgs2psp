/* gx_dump.c <a.ger> [frames]: ge/gelist.c on a GE replay file on x86 (renderer_notes 11.13): each frame's list as the PSP test player
   builds it (psp/ge_replay GELIST: depth as stored, or a raw depth file's own map, the list ending with RET, frame buffers 0x000000 / 0x088000, depth 0x110000, the
   draw buffer alternating from the first), its counts and digest (the player prints the same digest), and a walk of the list as the GE
   reads it: BASE, VADDR and IADDR, the vertex type, and every PRIM's vertices and indices, with the vertex and index addresses moving
   on after each PRIM as the GE moves them; each PRIM must read inside the array its address names.  Exit 0 when every PRIM does.
   Build: gcc -O2 -I/mnt/project-files/mgs2/ge gx_dump.c /mnt/project-files/mgs2/ge/gelist.c -lm.  Game data: personal use only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gelist.h"

static uint8_t *g_file, *g_aux; static long g_fsize; static uint32_t g_capaux;
/* a fake GE address space: the file at 0x08000000, the list's aux at 0x09800000 (another BASE) */
static uint32_t addr(void *ctx, const void *p)
{
  (void)ctx; const uint8_t *q = p;
  if (q >= g_file && q <= g_file + g_fsize) return 0x08000000u + (uint32_t)(q - g_file);
  if (q >= g_aux && q <= g_aux + g_capaux) return 0x09800000u + (uint32_t)(q - g_aux);
  fprintf(stderr, "gx_dump: an address outside the file and aux\n"); exit(2);
}
static const uint8_t *host(uint32_t a, uint32_t n, const char **what)
{
  if (a >= 0x08000000u && a + n <= 0x08000000u + (uint32_t)g_fsize) { *what = "file"; return g_file + (a - 0x08000000u); }
  if (a >= 0x09800000u && a + n <= 0x09800000u + g_capaux) { *what = "aux"; return g_aux + (a - 0x09800000u); }
  *what = "nowhere"; return 0;
}
static int vsize(uint32_t vt)   /* the GE vertex size: texture, colour, position (no weights, normals or morphs here) */
{
  int t = vt & 3, c = vt >> 2 & 7, p = vt >> 7 & 3, sz = 0, al = 1;
  static const int ts[4] = { 0, 1, 2, 4 }, ps[4] = { 0, 1, 2, 4 };
  if (t) { sz = 2 * ts[t]; al = ts[t]; }
  if (c) { int cs = c == 7 ? 4 : 2; sz = (sz + cs - 1) / cs * cs + cs; if (cs > al) al = cs; }
  if (p) { int s = ps[p]; sz = (sz + s - 1) / s * s + 3 * s; if (s > al) al = s; }
  return (sz + al - 1) / al * al;
}

int main(int argc, char **argv)
{
  if (argc < 2) { fprintf(stderr, "usage: gx_dump <a.ger> [frames]\n"); return 2; }
  int frames = argc > 2 ? atoi(argv[2]) : 1;
  FILE *f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 2; }
  fseek(f, 0, SEEK_END); g_fsize = ftell(f); fseek(f, 0, SEEK_SET);
  g_file = malloc((size_t)g_fsize + 64); if (!g_file || fread(g_file, 1, (size_t)g_fsize, f) != (size_t)g_fsize) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
  fclose(f);
  const GerHeader *h = (const GerHeader *)g_file;
  if (h->magic != GER_MAGIC) { fprintf(stderr, "%s: not a GER2 file\n", argv[1]); return 2; }
  const GerTex *t = (const GerTex *)(g_file + h->off_tex); const uint8_t *d = g_file + h->off_data;
  const uint8_t **td = calloc(h->ntex + 1, sizeof *td); const uint32_t **tp = calloc(h->ntex + 1, sizeof *tp);
  for (uint32_t k = 0; k < h->ntex; k++) { td[k] = d + t[k].data; tp[k] = (const uint32_t *)(d + t[k].pal); }
  uint32_t cw = (1 << 20) / 4; g_capaux = 512 << 10;
  uint32_t *w = malloc(cw * 4); g_aux = malloc(g_capaux);
  GxList L; gx_init(&L, w, cw, g_aux, g_capaux); L.ret = 1; L.dig_on = 1;
  GxHost H = { addr, 0, { 0x000000, 0x088000 }, 0x110000 };
  GxFrame F = { (const GerBatch *)(g_file + h->off_batch), h->nbatch, (GerVert *)(g_file + h->off_vert), h->nvert, d, t, h->ntex, td, tp, 1, 0 };
  float *vz = 0;   /* raw depth (zscale 1, the live renderer's): the frame's own map, each frame from the stored z (as the player) */
  if (h->zscale == 1.0f) {
    gx_depth_map(h->zlo, h->zhi, 1.0f, &F.zs, &F.zo); vz = malloc((h->nvert ? h->nvert : 1) * sizeof *vz);
    for (uint32_t k = 0; k < h->nvert; k++) vz[k] = F.v[k].z;
    printf("raw depth: Z24 %.0f..%.0f mapped, GE depth = Z24 x %g + %g\n", (double)h->zlo, (double)h->zhi, (double)F.zs, (double)F.zo);
  }
  int bad = 0;
  for (int fr = 0; fr < frames; fr++) {
    if (vz) for (uint32_t k = 0; k < h->nvert; k++) F.v[k].z = vz[k];
    int r = gx_frame(&L, &H, &F, fr & 1);
    const GxCount *c = &L.c;
    printf("%s vb %u frame %d (draw buffer %d): %ld words, %ld aux bytes, %ld batches, %ld PRIMs (%ld df0 strips, %ld effect runs, %ld effect "
           "triangles cut by the GE, %ld sprite cuts), %ld second passes, %ld texture sets, %ld vertex addresses, blends not mapped %ld, "
           "textures missing %ld, dropped %ld (%d); digest %016llx\n", argv[1], h->vb, fr, fr & 1, c->words, c->aux, c->batches, c->prims,
           c->xf_strips, c->fx_runs, c->fx_tris, c->cuts, c->passes2, c->tex_set, c->vaddr, c->blend_other, c->tex_none, c->dropped, r,
           (unsigned long long)L.dig);
    /* the walk */
    uint32_t base = 0, va = 0, ia = 0, vt = 0; long nprim = 0, nvert = 0, nidx = 0, cmd[256] = { 0 }, adv = 0;
    for (uint32_t i = 0; i < L.n; i++) {
      uint32_t x = L.w[i], op = x >> 24, a = x & 0xffffff; cmd[op]++;
      switch (op) {
        case 0x10: base = a; break;
        case 0x01: va = (base & 0x0f0000) << 8 | a; break;
        case 0x02: ia = (base & 0x0f0000) << 8 | a; break;
        case 0x12: vt = a; break;
        case 0x04: {
          uint32_t n = a & 0xffff, prim = a >> 16 & 7; int vs = vsize(vt), idx = vt >> 11 & 3; const char *wv, *wi; nprim++;
          if (prim > 6 || !vs) { printf("  word %u: PRIM %u with vertex type %06x\n", i, prim, vt); bad++; break; }
          if (idx) {
            if (idx != 2) { printf("  word %u: index type %d\n", i, idx); bad++; break; }
            const uint16_t *ix = (const uint16_t *)host(ia, 2 * n, &wi); uint32_t mx = 0;
            if (!ix) { printf("  word %u: PRIM reads %u indices at %08x: %s\n", i, n, ia, wi); bad++; break; }
            for (uint32_t k = 0; k < n; k++) if (ix[k] > mx) mx = ix[k];
            if (!host(va, (mx + 1) * (uint32_t)vs, &wv)) { printf("  word %u: indexed PRIM reads vertex %u at %08x: %s\n", i, mx, va, wv); bad++; }
            ia += 2 * n; nidx += n;
          } else {
            if (!host(va, n * (uint32_t)vs, &wv)) { printf("  word %u: PRIM reads %u vertices of %d bytes at %08x: %s\n", i, n, vs, va, wv); bad++; }
            if (i && (L.w[i - 1] >> 24) == 0x04) adv++;   /* a PRIM right after a PRIM: the vertex address the GE moved on */
            va += n * (uint32_t)vs; nvert += n;
          }
          break;
        }
      }
    }
    printf("  walk: %ld PRIMs (%ld after a PRIM, on the moved address), %ld vertices read in order, %ld indices; %ld BASE, %ld VADDR, %ld "
           "IADDR, %ld VTYPE, %ld state words else; last word %08x\n", nprim, adv, nvert, nidx, cmd[0x10], cmd[0x01], cmd[0x02], cmd[0x12],
           (long)L.n - nprim - cmd[0x10] - cmd[0x01] - cmd[0x02] - cmd[0x12], L.n ? L.w[L.n - 1] : 0);
  }
  if (bad) printf("%d PRIMs read outside their arrays\n", bad);
  return bad ? 1 : 0;
}
