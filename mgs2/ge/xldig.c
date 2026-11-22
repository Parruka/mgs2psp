/* xldig.c: see xldig.h. */
#include <string.h>
#include "xldig.h"
#include "gsfront.h"

static FILE *f;
static long vb = -1, ncall, nprim;
static uint64_t hprim;
static int used;   /* xdg_used */

/* FNV-1a over 64-bit words, with a shift after each multiply so that a difference in a high bit reaches the low ones */
static uint64_t h64(uint64_t h, const void *p, size_t n)
{
  const uint8_t *b = p; size_t i = 0;
  for (; i + 8 <= n; i += 8) { uint64_t w; memcpy(&w, b + i, 8); h ^= w; h *= 0x100000001b3ull; h ^= h >> 31; }
  for (; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; h ^= h >> 31; }
  return h;
}

void xdg_open(FILE *o) { f = o; vb = -1; ncall = nprim = 0; hprim = 0xcbf29ce484222325ull; }
static void line(void) { if (f && vb >= 0) fprintf(f, "v %ld calls %ld prims %ld %016llx\n", vb, ncall, nprim, (unsigned long long)hprim); }
void xdg_vblank(long v) { line(); vb = v; ncall = nprim = 0; hprim = 0xcbf29ce484222325ull; }
void xdg_used(int on) { used = on; }
/* d as a primitive drawn with it reads it, the rest 0 (GS: the alpha test's fields only with ATE, DATM only with DATE, blending only with
   ABE or AA1 and FIX only where C takes it, the fog colour only with FGE, the texture registers only with TME, the CLUT only for
   palette formats, mip levels only up to MXL, a clamp region only for the REGION modes) */
static void d_used(GfDraw *o, const GfDraw *d)
{
  *o = *d;
  if (!o->ate) o->atst = o->aref = o->afail = 0;
  if (!o->date) o->datm = 0;
  if (!o->abe && !o->aa1) o->ba = o->bb = o->bc = o->bd = o->fix = o->pabe = 0; else if (o->bc != 2) o->fix = 0;
  if (!o->fge) o->fcr = o->fcg = o->fcb = 0;
  if (!o->tme) {
    o->fst = 0; o->tpsm = o->tw = o->th = o->tcc = o->tfx = o->cpsm = o->csa = 0;
    for (int i = 0; i < 7; i++) o->tbp[i] = o->tbw[i] = 0;
    o->wms = o->wmt = o->minu = o->maxu = o->minv = o->maxv = 0; o->lcm = o->mxl = o->mmag = o->mmin = o->l = 0; o->k = 0;
    return;
  }
  int pal = o->tpsm == 0x13 || o->tpsm == 0x14 || o->tpsm == 0x1b || o->tpsm == 0x24 || o->tpsm == 0x2c;   /* T8, T4, T8H, T4HL, T4HH */
  if (!pal) o->cpsm = o->csa = 0;
  for (int i = 1 + (o->mxl < 6 ? o->mxl : 6); i < 7; i++) o->tbp[i] = o->tbw[i] = 0;
  if (o->wms < 2) o->minu = o->maxu = 0;
  if (o->wmt < 2) o->minv = o->maxv = 0;
}
/* a vertex as d reads it: S, T and Q only when textured with STQ, UV only with FST, fog only with FGE */
static void v_used(GfVtx *o, const GfVtx *v, const GfDraw *d)
{
  *o = *v;
  if (!d->tme || d->fst) o->s = o->t = o->q = 0;
  if (!d->tme || !d->fst) o->u = o->v = 0;
  if (!d->fge) o->f = 0;
}
void xdg_call(const XlCall *c)
{
  if (!f) return;
  if (used) {   /* the call's GS state as its primitives would read it, when its A+D packet set it, instead of the whole memory and registers */
    uint64_t hd = 0;
    if (c->ad) { GfDraw d, u; memset(&d, 0, sizeof d); gf_setup(c->gr, &d); memset(&u, 0, sizeof u); d_used(&u, &d); hd = h64(0xcbf29ce484222325ull, &u, sizeof u); }
    fprintf(f, "c %ld %ld e%06x p%x t%03x i%03x n%d a%d %x %x ds%016llx\n", vb, ncall++, (unsigned)c->entry, (unsigned)c->pc,
            (unsigned)c->top, (unsigned)c->itop, c->cont, c->ad, (unsigned)c->ad_qa, (unsigned)c->tag_qa, (unsigned long long)hd);
    return;
  }
  uint64_t hv = h64(0xcbf29ce484222325ull, c->vm, 16384), hg = h64(0xcbf29ce484222325ull, c->gr, 256 * sizeof *c->gr);
  fprintf(f, "c %ld %ld e%06x p%x t%03x i%03x n%d a%d %x %x vm%016llx gr%016llx\n", vb, ncall++, (unsigned)c->entry, (unsigned)c->pc,
          (unsigned)c->top, (unsigned)c->itop, c->cont, c->ad, (unsigned)c->ad_qa, (unsigned)c->tag_qa, (unsigned long long)hv, (unsigned long long)hg);
}
void xdg_prim(const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n)
{
  if (!f) return;
  int k[3] = { g->path, g->unk_used, n };
  nprim++; hprim = h64(hprim, k, sizeof k);
  if (used) { GfDraw u; GfVtx w; memset(&u, 0, sizeof u); d_used(&u, d); hprim = h64(hprim, &u, sizeof u);
              for (int i = 0; i < n; i++) { memset(&w, 0, sizeof w); v_used(&w, v[i], d); hprim = h64(hprim, &w, sizeof w); } return; }
  hprim = h64(hprim, d, sizeof *d);
  for (int i = 0; i < n; i++) hprim = h64(hprim, v[i], sizeof *v[i]);
}
void xdg_close(void) { line(); if (f) fflush(f); vb = -1; }
