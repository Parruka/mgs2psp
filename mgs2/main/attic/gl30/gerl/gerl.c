/* gerl.c (gl30, 2026-11-20, build agent): gl30's G1 check off the twin.  Loads GER replay files (ge/replay_fmt.h, dumped by the x86 twin:
   MGS2_XLIVE_GER), runs gx_frame on each with the arm given (MGS2_GL_DZ's X: 1), a list block of GL_PSP_LIST bytes shared with aux (as
   the PSP and the twin), and writes per frame: a text line with the dz counts (and the list's words and aux bytes), and a binary record:
   the batch marks (batch, pass, words so far, alow: gelist.c sed-patched by mk.sh to call gxh_mark before each gx_batch and once at the
   end), the words, and aux from alow to its top.  The addresses are region-tagged (BASE bits): 1 the through vertices, 2 aux, 3 the
   data block.  Built twice by mk.sh, against gl29's gelist.c and gl30's (and gl30's with XS off); cmp.py compares batch by batch.
   Game data passes through it: personal use only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "gelist.h"
#include "gelive.h"

typedef struct { uint32_t i, pass, n, alow; } Mk;
static Mk *mk; static uint32_t mk_n, mk_cap;
void gxh_mark(uint32_t i, int pass, uint32_t n, uint32_t alow)
{
  if (mk_n == mk_cap) { mk_cap = mk_cap ? 2 * mk_cap : 4096; mk = realloc(mk, mk_cap * sizeof *mk); if (!mk) exit(3); }
  mk[mk_n].i = i; mk[mk_n].pass = (uint32_t)pass; mk[mk_n].n = n; mk[mk_n].alow = alow; mk_n++;
}
static GxFrame F; static uint8_t *lb; static uint32_t lbn = GL_PSP_LIST, nd;
static uint32_t addr(void *ctx, const void *p)
{
  const uint8_t *q = p; (void)ctx;
  if (F.v && q >= (const uint8_t *)F.v && q <= (const uint8_t *)(F.v + F.nv)) return 1u << 24 | (uint32_t)(q - (const uint8_t *)F.v);
  if (q >= lb && q <= lb + lbn) return 2u << 24 | (uint32_t)(q - lb);
  if (q >= F.d && q <= F.d + nd) return 3u << 24 | (uint32_t)(q - F.d);
  fprintf(stderr, "gerl: an address outside the frame\n"); exit(2);
}
int main(int argc, char **argv)
{
  if (argc < 4) { fprintf(stderr, "gerl ARM OUT.bin GER...\n"); return 1; }
  const int arm = atoi(argv[1]); FILE *o = fopen(argv[2], "wb");
  if (!o || posix_memalign((void **)&lb, 64, lbn)) return 1;
  for (int a = 3; a < argc; a++) {
    FILE *f = fopen(argv[a], "rb"); if (!f) { perror(argv[a]); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b; if (posix_memalign((void **)&b, 64, (size_t)sz) || fread(b, 1, (size_t)sz, f) != (size_t)sz) return 1;
    fclose(f);
    GerHeader h; memcpy(&h, b, sizeof h);
    if (h.magic != GER_MAGIC) { fprintf(stderr, "%s: not GER2\n", argv[a]); return 1; }
    const GerTex *t = (const GerTex *)(b + h.off_tex);
    const uint8_t **td = calloc(h.ntex + 1, sizeof *td); const uint32_t **tp = calloc(h.ntex + 1, sizeof *tp);
    for (uint32_t k = 0; k < h.ntex; k++) { td[k] = b + h.off_data + t[k].data; tp[k] = (const uint32_t *)(b + h.off_data + t[k].pal); }
    memset(&F, 0, sizeof F);
    F.b = (const GerBatch *)(b + h.off_batch); F.nb = h.nbatch; F.v = (GerVert *)(b + h.off_vert); F.nv = h.nvert;
    F.d = b + h.off_data; F.t = t; F.nt = h.ntex; F.tdata = td; F.tpal = tp; nd = h.ndata;
    gx_depth_map(h.zlo, h.zhi, 1.0f, &F.zs, &F.zo);
    GxList L; GxHost H = { addr, 0, { 0x000000, 0x088000 }, 0x110000 };
    memset(lb, 0xa5, lbn);   /* (the free bytes: anything; the lists must not depend on them) */
    gx_init(&L, (uint32_t *)lb, lbn / 4, lb, lbn); L.ret = 1; L.dig_on = 1; L.dz = arm; L.dzn = 0;
    mk_n = 0;
    int r = gx_frame(&L, &H, &F, h.vb / 2 & 1);
    gxh_mark(0xffffffffu, 0, L.n, L.alow);
    const GxDzCount *c = &L.dc;
#ifdef GX_NO_NSUB
    const int nsub = 0;   /* (gl29's gelist.h has no nsub) */
#else
    const int nsub = c->nsub;
#endif
    printf("vb %u rc %d words %u aux %u dig %016llx; 42:%d oth:%d pair:%d sub:%d fb:%d,%d,%d,%d,%d,%d aux:%u\n", h.vb, r, L.n, L.naux,
           (unsigned long long)L.dig, c->n42, c->noth, c->npair, nsub, c->fb[0], c->fb[1], c->fb[2], c->fb[3], c->fb[4], c->fb[5], c->aux);
    uint32_t hdr[5] = { h.vb, mk_n, L.n, L.alow, L.cap_aux };
    fwrite(hdr, 4, 5, o); fwrite(mk, sizeof *mk, mk_n, o); fwrite(L.w, 4, L.n, o); fwrite(L.aux + L.alow, 1, L.cap_aux - L.alow, o);
    free(td); free(tp); free(b);
  }
  fclose(o); return 0;
}
