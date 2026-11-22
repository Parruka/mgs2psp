/* gsfront.c: the GS front end (gsfront.h).  Register and vertex rules as tools/gsref.c's gs_reg, gif_qw (PACKED) and vkick; transfers,
   IMAGE data and CLUT loads go to the texture sources (ge/gsup.c) when the front end has a map. */
#include <string.h>
#include "gsfront.h"
#include "gsup.h"

void gf_setup(const uint64_t *gr, GfDraw *d)
{
  uint64_t pr = gr[0x00], at = (gr[0x1a] & 1) ? pr : gr[0x1b];
  d->prim = pr & 7; d->iip = at >> 3 & 1; d->tme = at >> 4 & 1; d->fge = at >> 5 & 1; d->abe = at >> 6 & 1; d->aa1 = at >> 7 & 1; d->fst = at >> 8 & 1; d->ctxt = at >> 9 & 1;
  int c = d->ctxt;
  uint64_t xo = gr[0x18 + c], sc = gr[0x40 + c], fr = gr[0x4c + c], zb = gr[0x4e + c], te = gr[0x47 + c], al = gr[0x42 + c], t0 = gr[0x06 + c], cl = gr[0x08 + c], t1 = gr[0x14 + c];
  d->ofx = xo & 0xffff; d->ofy = xo >> 32 & 0xffff;
  d->scx0 = sc & 0x7ff; d->scx1 = sc >> 16 & 0x7ff; d->scy0 = sc >> 32 & 0x7ff; d->scy1 = sc >> 48 & 0x7ff;
  d->fbp = (fr & 0x1ff) * 32; d->fbw = fr >> 16 & 63; d->fpsm = fr >> 24 & 63; d->fbmsk = fr >> 32;
  d->zbp = (zb & 0x1ff) * 32; d->zpsm = (zb >> 24 & 15) | 0x30; d->zmsk = zb >> 32 & 1;
  d->ate = te & 1; d->atst = te >> 1 & 7; d->aref = te >> 4 & 0xff; d->afail = te >> 12 & 3; d->date = te >> 14 & 1; d->datm = te >> 15 & 1; d->zte = te >> 16 & 1; d->ztst = te >> 17 & 3;
  d->ba = al & 3; d->bb = al >> 2 & 3; d->bc = al >> 4 & 3; d->bd = al >> 6 & 3; d->fix = al >> 32 & 0xff;
  d->pabe = gr[0x49] & 1; d->fba = gr[0x4a + c] & 1; d->colclamp = gr[0x46] & 1;
  d->fcr = gr[0x3d] & 0xff; d->fcg = gr[0x3d] >> 8 & 0xff; d->fcb = gr[0x3d] >> 16 & 0xff;
  d->tpsm = t0 >> 20 & 63; d->tw = t0 >> 26 & 15; d->th = t0 >> 30 & 15; d->tcc = t0 >> 34 & 1; d->tfx = t0 >> 35 & 3; d->cpsm = t0 >> 51 & 15; d->csa = t0 >> 56 & 31;
  d->tbp[0] = t0 & 0x3fff; d->tbw[0] = t0 >> 14 & 63;
  uint64_t m1 = gr[0x34 + c], m2 = gr[0x36 + c];
  for (int i = 0; i < 3; i++) { d->tbp[1 + i] = m1 >> (20 * i) & 0x3fff; d->tbw[1 + i] = m1 >> (20 * i + 14) & 63; d->tbp[4 + i] = m2 >> (20 * i) & 0x3fff; d->tbw[4 + i] = m2 >> (20 * i + 14) & 63; }
  d->wms = cl & 3; d->wmt = cl >> 2 & 3; d->minu = cl >> 4 & 0x3ff; d->maxu = cl >> 14 & 0x3ff; d->minv = cl >> 24 & 0x3ff; d->maxv = cl >> 34 & 0x3ff;
  d->lcm = t1 & 1; d->mxl = t1 >> 2 & 7; d->mmag = t1 >> 5 & 1; d->mmin = t1 >> 6 & 7; d->l = t1 >> 19 & 3;
  int k = t1 >> 32 & 0xfff; if (k & 0x800) k -= 0x1000; d->k = k / 16.0f;
}

void gf_reset(GfGs *g, GfPrimFn fn, void *ctx)
{
  memset(g, 0, sizeof *g); g->q = 1.0f; g->dirty = 1; g->fn = fn; g->ctx = ctx;
}
void gf_unknown(GfGs *g, int what) { g->unk |= what & (GF_U_PRIM | GF_U_QUEUE | GF_U_PQ); g->curu |= what & ~(GF_U_PRIM | GF_U_QUEUE | GF_U_PQ); }

/* the vertex fields a primitive takes from its vertices: colour (all of them when Gouraud, else the last one's; gsref's export reads
   them all), S, T and Q or UV when textured, fog when fogged */
static int used(const GfDraw *d)
{
  int u = GF_U_RGBAQ; if (d->tme) u |= d->fst ? GF_U_UV : GF_U_ST | GF_U_Q; if (d->fge) u |= GF_U_FOG;
  return u;
}
/* the area a draw in context c may write, for the texture sources: its frame buffer and, with the depth test on and depth written,
   its depth buffer (at the frame buffer's width, as on the GS), within the scissor (gsref's write rules) */
static void drew_ctx(GfGs *g, int c)
{
  uint64_t sc = g->gr[0x40 + c], fr = g->gr[0x4c + c], zb = g->gr[0x4e + c], te = g->gr[0x47 + c];
  int x0 = sc & 0x7ff, x1 = sc >> 16 & 0x7ff, y0 = sc >> 32 & 0x7ff, y1 = sc >> 48 & 0x7ff; uint32_t fbw = fr >> 16 & 63;
  gu_drawn(g->up, (int)(fr >> 24 & 63), (uint32_t)(fr & 0x1ff) * 32, fbw, ~(uint32_t)(fr >> 32), x0, y0, x1, y1);
  if ((te >> 16 & 1) && !(zb >> 32 & 1)) gu_drawn(g->up, (int)(zb >> 24 & 15) | 0x30, (uint32_t)(zb & 0x1ff) * 32, fbw, ~0u, x0, y0, x1, y1);
}
void gf_drew(GfGs *g, int ctxts) { if (g->up) for (int c = 0; c < 2; c++) if (ctxts >> c & 1) drew_ctx(g, c); }
/* a, b, c with their unknown fields ua, ub, uc */
static void emit(GfGs *g, int n, const GfVtx *a, const GfVtx *b, const GfVtx *c, int uv)
{
  if (n == 3 && (int64_t)(b->x - a->x) * (c->y - a->y) == (int64_t)(b->y - a->y) * (c->x - a->x)) { g->zero_area++; return; }
  if (g->dirty) { gf_setup(g->gr, &g->d); g->dirty = 0; }
  g->unk_used = (g->unk & (GF_U_PRIM | GF_U_QUEUE)) | (uv & used(&g->d));
  g->prims[g->d.prim]++; if (g->unk_used) g->uncertain++;
  const GfVtx *v[3] = { a, b, c };
  if (g->fn) g->fn(g->ctx, g, &g->d, v, n);
  if (g->up) drew_ctx(g, g->d.ctxt);   /* after the callback: a primitive reads its texels before it writes */
}
/* a vertex kick: draw = 0 for XYZ3 / XYZF3 and the ADC bit (the vertex enters the queue without drawing) */
static void kick(GfGs *g, int draw)
{
  int prim = g->gr[0x00] & 7, u = g->curu, *qu = g->vqu; GfVtx v = g->cur, *q = g->vq;
  switch (prim) {
    case 0: if (draw) emit(g, 1, &v, 0, 0, u); break;
    case 1: qu[g->vqn] = u; q[g->vqn++] = v; if (g->vqn == 2) { if (draw) emit(g, 2, &q[0], &q[1], 0, qu[0] | qu[1]); g->vqn = 0; } break;
    case 2: if (g->vqn) { if (draw) emit(g, 2, &q[0], &v, 0, qu[0] | u); } q[0] = v; qu[0] = u; g->vqn = 1; break;
    case 3: qu[g->vqn] = u; q[g->vqn++] = v; if (g->vqn == 3) { if (draw) emit(g, 3, &q[0], &q[1], &q[2], qu[0] | qu[1] | qu[2]); g->vqn = 0; } break;
    case 4: if (g->vqn == 2) { if (draw) emit(g, 3, &q[0], &q[1], &v, qu[0] | qu[1] | u); q[0] = q[1]; qu[0] = qu[1]; q[1] = v; qu[1] = u; }
            else { qu[g->vqn] = u; q[g->vqn++] = v; } break;
    case 5: if (g->vqn == 2) { if (draw) emit(g, 3, &q[0], &q[1], &v, qu[0] | qu[1] | u); q[1] = v; qu[1] = u; } else { qu[g->vqn] = u; q[g->vqn++] = v; } break;
    case 6: qu[g->vqn] = u; q[g->vqn++] = v; if (g->vqn == 2) { if (draw) emit(g, 2, &q[0], &q[1], 0, qu[0] | qu[1]); g->vqn = 0; } break;
    default: g->prim7++;
  }
}
static float u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
void gf_reg(GfGs *g, int r, uint64_t v)
{
  GfVtx *c = &g->cur;
  switch (r) {
    case 0x00: g->gr[0] = v; g->vqn = 0; g->dirty = 1; g->unk &= ~(GF_U_PRIM | GF_U_QUEUE); return;
    case 0x01: c->r = v & 255; c->g = v >> 8 & 255; c->b = v >> 16 & 255; c->a = v >> 24 & 255; c->q = u2f(v >> 32); g->curu &= ~(GF_U_RGBAQ | GF_U_Q); return;
    case 0x02: c->s = u2f(v); c->t = u2f(v >> 32); g->curu &= ~GF_U_ST; return;
    case 0x03: c->u = v & 0x3fff; c->v = v >> 16 & 0x3fff; g->curu &= ~GF_U_UV; return;
    case 0x04: case 0x0c: c->x = v & 0xffff; c->y = v >> 16 & 0xffff; c->z = v >> 32 & 0xffffff; c->f = v >> 56; g->curu &= ~GF_U_FOG; kick(g, r == 0x04); return;
    case 0x05: case 0x0d: c->x = v & 0xffff; c->y = v >> 16 & 0xffff; c->z = v >> 32; kick(g, r == 0x05); return;
    case 0x0a: c->f = v >> 56; g->curu &= ~GF_U_FOG; return;
    case 0x16: case 0x17: { const uint64_t M = 0xFFFFFFE003F00000ull; g->gr[r - 0x10] = (g->gr[r - 0x10] & ~M) | (v & M); break; }   /* TEX2: TEX0's CLUT fields */
    case 0x54: return;   /* HWREG: transfer data, not kept (gsref) */
  }
  g->gr[r & 0xff] = v; g->dirty = 1;
  if (g->up) {
    if (r == 0x06 || r == 0x07 || r == 0x16 || r == 0x17) gu_clut_load(g->up, g->gr, r & 1);
    else if (r == 0x53) gu_trxdir(g->up, g->gr);
  }
}
void gf_image(GfGs *g, uint32_t ee, uint32_t n) { if (g->up) gu_image(g->up, ee, n); }
void gf_image_run(GfGs *g, uint32_t ee, uint32_t n) { if (g->up) gu_image_run(g->up, ee, n); }
void gf_packed(GfGs *g, int desc, const uint32_t *q)
{
  GfVtx *c = &g->cur;
  switch (desc) {
    case 0x0: gf_reg(g, 0x00, q[0] & 0x7ff); return;
    case 0x1: c->r = q[0] & 255; c->g = q[1] & 255; c->b = q[2] & 255; c->a = q[3] & 255; c->q = g->q;
              g->curu = (g->curu & ~(GF_U_RGBAQ | GF_U_Q)) | (g->unk & GF_U_PQ ? GF_U_Q : 0); return;
    case 0x2: c->s = u2f(q[0]); c->t = u2f(q[1]); g->q = u2f(q[2]); g->curu &= ~GF_U_ST; g->unk &= ~GF_U_PQ; return;
    case 0x3: c->u = q[0] & 0x3fff; c->v = q[1] & 0x3fff; g->curu &= ~GF_U_UV; return;
    case 0x4: c->x = q[0] & 0xffff; c->y = q[1] & 0xffff; c->z = q[2] >> 4 & 0xffffff; c->f = q[3] >> 4 & 0xff; g->curu &= ~GF_U_FOG; kick(g, !(q[3] >> 15 & 1)); return;
    case 0x5: c->x = q[0] & 0xffff; c->y = q[1] & 0xffff; c->z = q[2]; kick(g, !(q[3] >> 15 & 1)); return;
    case 0xa: c->f = q[3] >> 4 & 0xff; g->curu &= ~GF_U_FOG; return;
    case 0xc: c->x = q[0] & 0xffff; c->y = q[1] & 0xffff; c->z = q[2] >> 4 & 0xffffff; c->f = q[3] >> 4 & 0xff; g->curu &= ~GF_U_FOG; kick(g, 0); return;
    case 0xd: c->x = q[0] & 0xffff; c->y = q[1] & 0xffff; c->z = q[2]; kick(g, 0); return;
    case 0xe: gf_reg(g, (int)(q[2] & 0xff), q[0] | (uint64_t)q[1] << 32); return;
    case 0xf: return;
  }
  gf_reg(g, desc, q[0] | (uint64_t)q[1] << 32);
}
