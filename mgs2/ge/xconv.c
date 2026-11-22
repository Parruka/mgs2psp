/* xconv.c: see xconv.h.  Each part follows gsref's export (tools/gsref.c ge_prim, dc_export, fp_end, fp_export), which the PS2
   pictures and the MGS2GE replays were checked with; gsref GSREF_XLCONV=1 compares the two batch by batch. */
#include "xconv.h"
#include "vu1conv.h"
#include <string.h>

static XcStats St;
const XcStats *xc_stats(void) { return &St; }

/* the GS state of the call's first primitive, as gsref's setup() reads it from the registers (PRMODECONT picks PRIM or PRMODE) */
typedef struct {
  int iip, tme, fge, abe, ctxt, ofx, ofy, scx0, scx1, scy0, scy1, zmsk, zte, ztst, ate, atst, aref, afail;
  int ba, bb, bc, bd, fix, tw, th, tcc, tfx;
} XcGs;
static void xc_gs(const uint64_t *gr, XcGs *d)
{
  uint64_t pr = gr[0x00], at = (gr[0x1a] & 1) ? pr : gr[0x1b];
  d->iip = at >> 3 & 1; d->tme = at >> 4 & 1; d->fge = at >> 5 & 1; d->abe = at >> 6 & 1; d->ctxt = at >> 9 & 1;
  int c = d->ctxt;
  uint64_t xo = gr[0x18 + c], sc = gr[0x40 + c], zb = gr[0x4e + c], te = gr[0x47 + c], al = gr[0x42 + c], t0 = gr[0x06 + c];
  d->ofx = xo & 0xffff; d->ofy = xo >> 32 & 0xffff;
  d->scx0 = sc & 0x7ff; d->scx1 = sc >> 16 & 0x7ff; d->scy0 = sc >> 32 & 0x7ff; d->scy1 = sc >> 48 & 0x7ff;
  d->zmsk = zb >> 32 & 1;
  d->ate = te & 1; d->atst = te >> 1 & 7; d->aref = te >> 4 & 0xff; d->afail = te >> 12 & 3; d->zte = te >> 16 & 1; d->ztst = te >> 17 & 3;
  d->ba = al & 3; d->bb = al >> 2 & 3; d->bc = al >> 4 & 3; d->bd = al >> 6 & 3; d->fix = al >> 32 & 0xff;
  d->tw = t0 >> 26 & 15; d->th = t0 >> 30 & 15; d->tcc = t0 >> 34 & 1; d->tfx = t0 >> 35 & 3;
}
/* the batch's state fields (gsref ge_prim): prim GU_TRIANGLE_STRIP as the transform batches carry it */
static void xc_state(const XcGs *d, int ti, uint32_t entry, GerBatch *b)
{
  memset(b, 0, sizeof *b);
  b->tex = ti >= 0 ? (uint16_t)ti : 0xffff; b->prim = 4;
  b->flags = (d->iip ? GERF_GOURAUD : 0) | (ti >= 0 ? GERF_TEX : 0) | (d->abe ? GERF_BLEND : 0) | (d->ate ? GERF_ATEST : 0) | (d->zte ? GERF_ZTEST : 0) |
             (d->zte && !d->zmsk ? GERF_ZWRITE : 0) | (d->fge ? GERF_FOG : 0) | (d->tcc ? GERF_TCC : 0);
  b->ztst = (uint8_t)d->ztst; b->atst = (uint8_t)d->atst; b->aref = (uint8_t)d->aref; b->afail = (uint8_t)d->afail;
  /* alpha tests that need no GE alpha test: ALWAYS passes; NEVER with AFAIL FB_ONLY is the PS2's way to draw without writing Z */
  if (d->ate && d->atst == 1) b->flags &= ~GERF_ATEST;
  if (d->ate && d->atst == 0 && d->afail == 1) b->flags &= ~(GERF_ATEST | GERF_ZWRITE);
  b->ba = (uint8_t)d->ba; b->bb = (uint8_t)d->bb; b->bc = (uint8_t)d->bc; b->bd = (uint8_t)d->bd; b->fix = (uint8_t)d->fix; b->tfx = (uint8_t)d->tfx;
  b->sx0 = (int16_t)(d->scx0 * 480 / 512); b->sy0 = (int16_t)(d->scy0 * 272 / 512); b->sx1 = (int16_t)((d->scx1 + 1) * 480 / 512 - 1); b->sy1 = (int16_t)((d->scy1 + 1) * 272 / 512 - 1);
  b->entry = entry;
}
/* two passes for AFAIL FB_ONLY with Z write when a vertex alpha (a, PS2 scale) times the texture's lowest alpha can be below AREF */
static int xc_twopass(const XcGs *d, const XcTex *t, int a)
{
  int ta = t->index >= 0 && t->kind == 1 && d->tcc ? t->minalpha : 0x80;
  return d->ate && d->afail == 1 && d->zte && !d->zmsk && d->atst >= 2 && (a * ta >> 7) < d->aref;
}
/* the GE transform of the call: world and projection from the VU matrix, the PS2 viewport in the replay's screen (512 -> 480 x 272,
   GE offset 2048 - 240, 2048 - 136), depth x zk, fog, and the texture scale (uvk: 8 for df0's 16-bit UVs, 1 for float UVs) */
static void xc_xform(const VcXform *x, const XcGs *d, const XcTex *t, int uvk, const XcHost *h, const uint64_t *gr, GerXform *gx)
{
  memset(gx, 0, sizeof *gx);
  memcpy(gx->world, x->world, sizeof gx->world);
  gx->proj[0] = 1; gx->proj[5] = 1; gx->proj[10] = x->pa; gx->proj[11] = 1; gx->proj[14] = -x->pb;
  /* float only (the PSP has no double unit, and the x86 check build rounds as the PSP's FPU): 480 / 512 = 0.9375, 272 / 512 = 0.53125 */
  float ofx = d->ofx / 16.0f, ofy = d->ofy / 16.0f, zk = (float)h->zk;
  gx->vs[0] = x->vs[0] * 0.9375f; gx->vo[0] = (2048 - 240) + (x->vo[0] - ofx) * 0.9375f;
  gx->vs[1] = x->vs[1] * 0.53125f; gx->vo[1] = (2048 - 136) + (x->vo[1] - ofy) * 0.53125f;
  gx->vs[2] = -x->vs[2] * zk; gx->vo[2] = x->vo[2] * zk;
  gx->fog1 = x->fog_b / x->fog_a; gx->fog2 = x->fog_a / 255.0f; gx->fogcol = (uint32_t)(gr[0x3d] & 0xffffff);
  gx->su = gx->sv = 1; gx->tu = gx->tv = 0;
  if (t->index >= 0 && t->kind == 1) {   /* PS2 u in PS2 texture widths (x 8 for df0's u16); GE u in GE texture widths from the region's origin */
    float TW = (float)(1 << d->tw), TH = (float)(1 << d->th), tw = (float)t->tw, th = (float)t->th;   /* powers of two: exact */
    gx->su = uvk * TW / tw; gx->tu = (float)-t->u0 / tw;
    gx->sv = uvk * TH / th; gx->tv = (float)-t->v0 / th;
  }
}
static const uint32_t *vq(const uint8_t *vm, uint32_t qa) { return (const uint32_t *)(vm + ((qa & 0x3ffu) << 4)); }

/* the depth range (xconv.h XcZr): tools/dev zrange.py's V2, which renderer_notes 11.13 measured */
void xc_zr_begin(XcZr *z) { memset(z, 0, sizeof *z); z->lo = 1; z->hi = 0; }
void xc_zr_end(XcZr *z)
{
  if (!z->on) return;
  z->on = 0;
  /* the clip planes z_clip = +w and -w: GE z = pa w - pb = minus z_clip, so w = pb / (pa - 1) and pb / (pa + 1) */
  float wa = z->pa != -1 ? z->pb / (z->pa + 1) : -1, wb = z->pa != 1 ? z->pb / (z->pa - 1) : -1, wn, wf;
  if (wa > 0 && wb > 0) { wn = wa < wb ? wa : wb; wf = wa < wb ? wb : wa; }
  else if (wa > 0 || wb > 0) { wn = wa > 0 ? wa : wb; wf = 3.0e38f; }
  else { wn = 1e-30f; wf = 3.0e38f; }
  float w0 = z->wmin < wn ? wn : z->wmin > wf ? wf : z->wmin, w1 = z->wmax < wn ? wn : z->wmax > wf ? wf : z->wmax;
  float z0 = z->vo2 - z->vs2 * (z->pa - z->pb / w0), z1 = z->vo2 - z->vs2 * (z->pa - z->pb / w1);   /* the PS2's Z = vs z_clip / w + vo */
  float lo = z0 < z1 ? z0 : z1, hi = z0 < z1 ? z1 : z0;
  if (z->lo > z->hi) { z->lo = lo; z->hi = hi; } else { if (lo < z->lo) z->lo = lo; if (hi > z->hi) z->hi = hi; }
}
/* a df0 call's vertices: w = the GE's view z = the PS2's clip w (the position reads s / 32768, the matrix carries the x 32768) */
static void xc_zr_df0(XcZr *z, const VcXform *x, const VcVert *v, int n)
{
  float a = x->world[2] / 32768, b = x->world[5] / 32768, c = x->world[8] / 32768, t = x->world[11], lo = 3.0e38f, hi = 0;
  long k = 0;
  for (int i = 0; i < n; i++) {
    float w = v[i].x * a + v[i].y * b + v[i].z * c + t;
    if (w > 0) { if (w < lo) lo = w; if (w > hi) hi = w; k++; }
  }
  if (!k) return;
  z->calls++; z->verts += k;
  if (z->on && x->pa == z->pa && x->pb == z->pb && x->vs[2] == z->vs2 && x->vo[2] == z->vo2) {
    if (lo < z->wmin) z->wmin = lo;
    if (hi > z->wmax) z->wmax = hi;
    return;
  }
  xc_zr_end(z);
  z->pa = x->pa; z->pb = x->pb; z->vs2 = x->vs[2]; z->vo2 = x->vo[2]; z->wmin = lo; z->wmax = hi; z->on = 1;
}

/* XcHost.cut: 1 when the 8 corners of the box [lo, hi] (object space, in the GE's units) all lie outside one side of the call's scissor,
   XC_CUT_PX GE pixels or more beyond it.  Each side is a half-space of the GE's view space (x, y, w): the GE's screen X >= X0 for a point
   in front (w > 0) is vs0 x + (vo0 - X0) w >= 0, an affine function of the object position, so it is below 0 on the whole box when it is
   at every corner.  Every vertex of the call lies in the box, so does every point of its triangles and of the near plane's clipped
   polygons, and the GE draws only points in front: none of them reaches the scissor.  A NaN never counts as outside.  w: the GE world
   rows (xc_xform's); the viewport and the scissor as xc_xform and xc_state make them. */
#define XC_CUT_PX 2.0f
static int xc_off(const float *w, const float lo[3], const float hi[3], const VcXform *x, const XcGs *d)
{
  float ofx = d->ofx / 16.0f, ofy = d->ofy / 16.0f;
  float vs0 = x->vs[0] * 0.9375f, vo0 = (2048 - 240) + (x->vo[0] - ofx) * 0.9375f, vs1 = x->vs[1] * 0.53125f, vo1 = (2048 - 136) + (x->vo[1] - ofy) * 0.53125f;
  float X0 = (2048 - 240) + (float)(d->scx0 * 480 / 512) - XC_CUT_PX, X1 = (2048 - 240) + (float)((d->scx1 + 1) * 480 / 512) + XC_CUT_PX;
  float Y0 = (2048 - 136) + (float)(d->scy0 * 272 / 512) - XC_CUT_PX, Y1 = (2048 - 136) + (float)((d->scy1 + 1) * 272 / 512) + XC_CUT_PX;
  int m = 15;
  for (int k = 0; k < 8 && m; k++) {
    float px = k & 1 ? hi[0] : lo[0], py = k & 2 ? hi[1] : lo[1], pz = k & 4 ? hi[2] : lo[2];
    float cx = px * w[0] + py * w[3] + pz * w[6] + w[9], cy = px * w[1] + py * w[4] + pz * w[7] + w[10], cw = px * w[2] + py * w[5] + pz * w[8] + w[11];
    float ax = vs0 * cx + vo0 * cw, ay = vs1 * cy + vo1 * cw;   /* the screen X and Y times w */
    m &= (ax - X0 * cw < 0) | (ax - X1 * cw > 0) << 1 | (ay - Y0 * cw < 0) << 2 | (ay - Y1 * cw > 0) << 3;
  }
  return m != 0;
}
static VcVert Dv[1024]; static uint16_t Dfl[1024]; static VcXform Dx; static int Dn;   /* a df0 call's input, read by xc_call */
/* the df0 call xc_call read: 1 when XcHost.cut leaves it out */
static int xc_df0_off(const XcGs *d)
{
  int lo[3] = { 32767, 32767, 32767 }, hi[3] = { -32768, -32768, -32768 };
  for (int i = 0; i < Dn; i++) {
    if (Dv[i].x < lo[0]) lo[0] = Dv[i].x;
    if (Dv[i].x > hi[0]) hi[0] = Dv[i].x;
    if (Dv[i].y < lo[1]) lo[1] = Dv[i].y;
    if (Dv[i].y > hi[1]) hi[1] = Dv[i].y;
    if (Dv[i].z < lo[2]) lo[2] = Dv[i].z;
    if (Dv[i].z > hi[2]) hi[2] = Dv[i].z;
  }
  float flo[3], fhi[3];   /* the GE reads a 16-bit position as s / 32768 (exact in a float) */
  for (int j = 0; j < 3; j++) { flo[j] = lo[j] * (1.0f / 32768); fhi[j] = hi[j] * (1.0f / 32768); }
  return xc_off(Dx.world, flo, fhi, &Dx, d);
}
/* a call the cut leaves out gives the frame's depth range (XcZr) what xc_df0 would have given it: a depth-testing, depth-writing call
   with a triangle to draw, its vertices' w.  The frame's depth map and the next frame's class 3 window for the effects are then the same
   with the cut on or off (renderer_notes 11.15) */
static void xc_df0_zr(const XcHost *h, const XcGs *d)
{
  if (!h->zr || !d->zte || d->zmsk || (d->ate && d->atst == 0 && d->afail == 1)) return;   /* xc_state's GERF_ZWRITE */
  int i = 2;
  while (i < Dn && (Dfl[i] & 0x8000)) i++;
  if (i >= Dn) return;   /* every triangle has ADC: xc_df0 draws nothing (no strip) */
  if (d->ztst >= 2) xc_zr_df0(h->zr, &Dx, Dv, Dn); else h->zr->always++;
}

/* P04+df0: the call's strips (the data's ADC bits) with hardware transform; the GE clips and the scissor cuts.  Its input: xc_call's
   Dn, Dx, Dv, Dfl */
static int xc_df0(const XlCall *c, const XcHost *h, const XcGs *d, int ti, const XcTex *t, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size)
{
  static uint16_t runs[2048], idx[4096];
  VcVert *v = Dv; uint16_t *fl = Dfl; VcXform x = Dx; int n = Dn;
  (void)c;
  if (n < 0) return n;
  int nst = vc_strips(fl, n, runs, 1024), ni = 0;
  if (!nst) return 0;
  for (int k = 0; k < nst && !h->noidx; k++) {   /* the strips joined into one by repeated vertices */
    if (k) { idx[ni] = idx[ni - 1]; ni++; idx[ni++] = runs[2 * k]; }
    for (int j = 0; j < runs[2 * k + 1]; j++) idx[ni++] = (uint16_t)(runs[2 * k] + j);
  }
  GerXform gx; xc_xform(&x, d, t, 8, h, c->gr, &gx);
  gx.nv = n; gx.nst = nst; gx.nidx = ni;
  uint32_t o = 0, need = sizeof gx + n * sizeof *v + nst * 4 + ni * 2;
  if (need > cap) return -5;
  memcpy(blob + o, &gx, sizeof gx); o += sizeof gx;
  memcpy(blob + o, v, n * sizeof *v); o += n * sizeof *v;
  memcpy(blob + o, runs, nst * 4); o += nst * 4;
  memcpy(blob + o, idx, ni * 2); o += ni * 2;
  xc_state(d, ti, c->entry, b); b->kind = GERK_XFORM; b->twopass = (uint8_t)xc_twopass(d, t, x.alpha); b->count = nst;
  if (h->zr && (b->flags & GERF_ZWRITE)) { if (b->ztst >= 2) xc_zr_df0(h->zr, &x, v, n); else h->zr->always++; }   /* GERF_ZWRITE has ZTEST */
  *size = o; return 1;
}

/* 2870's main-loop back-face test on triangle i (re/vu1_programs.md 3.7): e_i = c_i.xyz w_(i-1) - c_(i-1).xyz w_i (MULAw, MSUBw),
   z = e_(i-1).x e_i.y - e_i.x e_(i-1).y (OPMULA, OPMSUB); culled when (flags & 0x30) equals 0x20 if z's sign bit is set, else 0 */
static int xc_cull(const float (*c)[4], int i, uint16_t fl)
{
  float e[2][2];
  for (int k = 0; k < 2; k++) { const float *a = c[i - 1 + k], *p = c[i - 2 + k];
    for (int j = 0; j < 2; j++) e[k][j] = vc_vu(vc_vu(a[j] * p[3]) - p[j] * a[3]); }
  float z = vc_vu(vc_vu(e[0][0] * e[1][1]) - e[1][0] * e[0][1]); uint32_t u; memcpy(&u, &z, 4);
  return (fl & 0x30) == (u >> 31 ? 0x20u : 0u);
}
/* the GE's outcome for a triangle with these clip coordinates (gsref ge_outcome): 0 drawn as the PS2 draws it, 1 dropped (a vertex
   outside the GE's 0..4096 range), 2 clipped by the GE at the near plane, 3 drawn past the far plane, 4 nothing drawn.  g: x scale,
   x centre, y scale, y centre of the replay's viewport */
static int xc_outcome(const float *t[3], const float *g)
{
  const float B = 4095.0f + 15.5f / 16.0f;
  int neg = 0, pos = 0, nw = 0, near = 0, far = 0;
  for (int k = 0; k < 3; k++) {
    const float *c = t[k];
    float w = c[3], sx = c[0] * g[0] / w + g[1], sy = c[1] * g[2] / w + g[3], zg = -c[2];   /* GE z = minus the PS2's clip z */
    if (!(sx >= 0 && sx < B && sy >= 0 && sy < B)) return 1;
    if (w < 0) nw++;
    if (zg / w >= 1.000030517578125f) pos++;
    if (-zg / w >= 1.000030517578125f) neg++;
    if (zg < -w) near = 1;
    if (zg > w) far = 1;
  }
  if (nw == 3 || pos == 3 || neg == 3) return 4;
  if (far) return 3;
  if (near) {   /* the GE clips at its z = -w and drops a part with a new vertex outside the range */
    for (int k = 0; k < 3; k++) { const float *P = t[k], *Q = t[(k + 1) % 3];
      float dp = P[3] - P[2], dq = Q[3] - Q[2];
      if ((dp >= 0) != (dq >= 0)) { float u = dp / (dp - dq), c[4]; for (int j = 0; j < 4; j++) c[j] = P[j] + u * (Q[j] - P[j]);
        float sx = c[0] * g[0] / c[3] + g[1], sy = c[1] * g[2] / c[3] + g[3]; if (!(sx >= 0 && sx < B && sy >= 0 && sy < B)) return 1; } }
    return 2;
  }
  return 0;
}
/* P04+25f0 / +2870: the strip's triangles by class, and the PS2 clipper's polygons (ge/vu1conv.h vc_clip_tri) for the crossing ones */
static int xc_fpos(const XlCall *c, const XcHost *h, const XcGs *d, int ti, const XcTex *t, int k2870, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size)
{
  static VcVertF v[1024], pv[8192]; static uint16_t fl[1024], pfan[1024][2]; static uint8_t cls[1024], cf[1024]; static float cc[1024][4];
  if (h->trace) h->trace->n = 0;
  /* a call whose vertices all lie at one point draws nothing (the deck's unused particles; some of them hold colour words above 255) */
  int n0 = (int)(vq(c->vm, c->top + 0x14)[0] & 0xffff), same = n0 <= 1024;
  for (int i = 1; i < n0 && same; i++) same = !memcmp(vq(c->vm, c->top + 0x100 + 3 * i), vq(c->vm, c->top + 0x100), 12);
  if (same) return 0;
  VcXform x; int n = vc_fpos(c->vm, c->top, &x, v, fl, 1024);
  if (n < 0) return n;
  /* the clip positions and flags as the micro program computes them; the flag words it stores back */
  int amin = 255;
  for (int i = 0; i < n; i++) { float p[3] = { v[i].x, v[i].y, v[i].z }; vc_vu_clip(x.m, p, cc[i]); cf[i] = (uint8_t)vc_cflags(cc[i]);
    int a = (int)(vq(c->vm, c->top + 0x101 + 3 * i)[3] & 0xff); if (a < amin) amin = a; }
  XcTrace *tr = h->trace;
  if (tr) { tr->n = n;
    for (int i = 0; i < n; i++) { int f = cf[i] | (i >= 1 ? cf[i - 1] : 0) | (i >= 2 ? cf[i - 2] : 0);   /* FCSET 0 before the loop */
      memcpy(tr->c[i], cc[i], 16); tr->post[i] = (uint16_t)(fl[i] | (f && !(fl[i] & 0x8000) ? 0xc000 : 0)); } }
  GerXform gx; xc_xform(&x, d, t, 1, h, c->gr, &gx);
  const float g[4] = { gx.vs[0], gx.vo[0], gx.vs[1], gx.vo[1] };
  int npf = 0, npv = 0, drawn = 0;
  memset(cls, 0, n);
  for (int i = 2; i < n; i++) {
    if (fl[i] & 0x8000) { St.tris[0]++; continue; }
    if (!(cf[i - 2] | cf[i - 1] | cf[i])) {   /* inside: the strip draws it, unless 2870 culls it */
      if (k2870 && xc_cull(cc, i, fl[i])) { cls[i] = 2; St.tris[2]++; St.tris[5]++; continue; }
      cls[i] = 1; St.tris[1]++;
      if (memcmp(cc[i - 2], cc[i - 1], 16) || memcmp(cc[i - 1], cc[i], 16)) drawn = 1;   /* three equal clip positions: zero area */
      continue;
    }
    if (cf[i - 2] & cf[i - 1] & cf[i]) { cls[i] = 2; St.tris[2]++; continue; }   /* trivially rejected */
    const float *t3[3] = { cc[i - 2], cc[i - 1], cc[i] };
    int o = xc_outcome(t3, g), across = 0;
    if (h->zlo >= 0) for (int k = 0; k < 3; k++) { const float *q = t3[k]; float Z = q[3] > 0 ? x.vs[2] * q[2] / q[3] + x.vo[2] : 0;
      if (q[3] <= 0 || Z > h->zhi || Z < h->zlo) across = 1; }
    cls[i] = o == 0 && !across && (fl[i] & 0x30) == 0x30 ? 3 : 4; St.tris[cls[i]]++;
    VcCv in[3], out[16];   /* the clipper's input as the main loop stores it back: clip position, ITOF0 colour, ITOF12 UV (z = 1.0) */
    for (int k = 0; k < 3; k++) { int j = i - 2 + k; const uint32_t *col = vq(c->vm, c->top + 0x101 + 3 * j), *uv = vq(c->vm, c->top + 0x102 + 3 * j);
      memcpy(in[k].c, cc[j], 16);
      for (int q = 0; q < 4; q++) in[k].col[q] = (float)(int32_t)col[q];
      for (int q = 0; q < 3; q++) in[k].uv[q] = (float)(int32_t)uv[q] / 4096.0f;
      in[k].p[0] = v[j].x; in[k].p[1] = v[j].y; in[k].p[2] = v[j].z; }
    int m = vc_clip_tri(in, out);
    if (m < 3) { St.poly_empty++; continue; }
    /* the clipper's back-face test on the polygon's first three vertices (re/vu1_programs.md 2.3), as gsref's fp_end computes it */
    float e1[2], e2[2];
    for (int q = 0; q < 2; q++) { e1[q] = out[1].c[q] * out[0].c[3] - out[0].c[q] * out[1].c[3]; e2[q] = out[2].c[q] * out[1].c[3] - out[1].c[q] * out[2].c[3]; }
    int sgn = (e1[0] * e2[1] - e2[0] * e1[1]) < 0 ? 0x20 : 0;
    if ((int)(fl[i] & 0x30) == sgn) { St.poly_culled++; continue; }
    if (npf == 1024 || npv + m > 8192) { St.poly_full++; continue; }
    pfan[npf][0] = (uint16_t)i; pfan[npf][1] = (uint16_t)m; npf++; drawn = 1; St.polys++; St.poly_v += m;
    for (int k = 0; k < m; k++) {   /* world position, the colour the PS2 draws (ftoi0(col / w + 0.0005)), UV */
      const VcCv *P = &out[k]; VcVertF *o2 = &pv[npv++]; float Q = 1.0f / P->c[3]; int cv[4];
      for (int q = 0; q < 4; q++) { cv[q] = (int)(P->col[q] * Q + 0.0005f); cv[q] = cv[q] < 0 ? 0 : cv[q] > 255 ? 255 : cv[q]; }
      o2->u = P->uv[0]; o2->v = P->uv[1]; o2->x = P->p[0]; o2->y = P->p[1]; o2->z = P->p[2];
      o2->c = (uint32_t)cv[0] | (uint32_t)cv[1] << 8 | (uint32_t)cv[2] << 16 | (uint32_t)(cv[3] >= 0x80 ? 0xff : cv[3] * 2) << 24;
    }
  }
  if (tr) memcpy(tr->cls, cls, n);
  if (!drawn) return 0;
  gx.nv = n; gx.nfan = npf; gx.nfv = npv;
  uint32_t o = 0, nc = (n + 3) & ~3u, need = sizeof gx + (n + npv) * sizeof *v + nc + npf * 4;
  if (need > cap) return -5;
  memcpy(blob + o, &gx, sizeof gx); o += sizeof gx;
  memcpy(blob + o, v, n * sizeof *v); o += n * sizeof *v;
  memcpy(blob + o, pv, npv * sizeof *pv); o += npv * sizeof *pv;
  memcpy(blob + o, cls, n); memset(blob + o + n, 0, nc - n); o += nc;
  memcpy(blob + o, pfan, npf * 4); o += npf * 4;
  xc_state(d, ti, c->entry, b); b->kind = GERK_XFORMF; b->twopass = (uint8_t)xc_twopass(d, t, amin); b->count = n;
  *size = o; return 1;
}

/* ---- P06 and P07 (ge/vu1conv.h vc_p06, vc_p07; replay_fmt.h GERK_LIT).  The blob: the call's GerXform, a GerLit per pass the
   program draws, the vertices of passes 1 and 2, pass 3's, the strips.  Passes 2 and 3 come from the translator after the call (xc_pass), each with its own
   texture: their GerLit's texture scale is written then. ---- */
static const uint32_t LIT_VT[4] = { 0x11e, 0x11f, 0x19e, 0x19f };   /* VcVert, VcVertE, VcVertS, VcVertF (GU_TRANSFORM_3D) */
static struct { int on; uint32_t entry, top; int amin, uvk[4], du[4], dv[4]; uint32_t rec[4]; } Lp;   /* the call whose passes come next;
                                    rec[k]: pass k's GerLit in the blob (0: not drawn); uvk, du, dv: its texture scale's (xc_lit_ts) */
static int is_p06(uint32_t e) { return e == VC_P06 || e == VC_P06S || e == VC_P06T || e == VC_P06ST; }
/* a pass's texture scale and offset (xc_xform's for its GE texture; uvk: 8 for 16-bit UVs, 1 for float ones; du, dv: pass 2's UV2 - UV1
   in 4.12, exact in a float: integers times powers of two) */
static void xc_lit_ts(const XcGs *d, const XcTex *t, int uvk, int du, int dv, float *ts)
{
  ts[0] = ts[1] = 1; ts[2] = ts[3] = 0;
  if (t->index >= 0 && t->kind == 1) {
    float TW = (float)(1 << d->tw), TH = (float)(1 << d->th), tw = (float)t->tw, th = (float)t->th;
    ts[0] = uvk * TW / tw; ts[2] = ((float)du * TW / 4096.0f - (float)t->u0) / tw;
    ts[1] = uvk * TH / th; ts[3] = ((float)dv * TH / 4096.0f - (float)t->v0) / th;
  }
}
/* the depth range (XcZr) of a P06 call's vertices, as xc_zr_df0 (the positions as the GE reads them: 16-bit ones / 32768) */
static void xc_zr_lit(XcZr *z, const VcXform *x, const uint8_t *v, int type, int stride, int n)
{
  float ps = (type & 2) ? 1.0f : 32768.0f, a = x->world[2] / ps, b = x->world[5] / ps, c = x->world[8] / ps, t = x->world[11], lo = 3.0e38f, hi = 0;
  long k = 0;
  for (int i = 0; i < n; i++) {
    const uint8_t *o = v + stride * i; float w;
    if (type & 2) { float p[3]; memcpy(p, o + ((type & 1) ? 12 : 8), 12); w = p[0] * a + p[1] * b + p[2] * c + t; }
    else { int16_t p[3]; memcpy(p, o + ((type & 1) ? 12 : 8), 6); w = p[0] * a + p[1] * b + p[2] * c + t; }
    if (w > 0) { if (w < lo) lo = w; if (w > hi) hi = w; k++; }
  }
  if (!k) return;
  z->calls++; z->verts += k;
  if (z->on && x->pa == z->pa && x->pb == z->pb && x->vs[2] == z->vs2 && x->vo[2] == z->vo2) {
    if (lo < z->wmin) z->wmin = lo;
    if (hi > z->wmax) z->wmax = hi;
    return;
  }
  xc_zr_end(z);
  z->pa = x->pa; z->pb = x->pb; z->vs2 = x->vs[2]; z->vo2 = x->vo[2]; z->wmin = lo; z->wmax = hi; z->on = 1;
}
static int xc_lit(const XlCall *c, const XcHost *h, const XcGs *d, int ti, const XcTex *t, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size)
{
  static uint8_t v3[64 * 24]; static uint16_t fl[64], runs[128];
  const int p7 = c->entry == VC_P07;
  const uint32_t *hd = vq(c->vm, c->top + (p7 ? 0 : 0x14));
  uint32_t hy = (p7 ? hd[3] : hd[1]) & 0xffff, n0 = hd[0] & 0xffff;   /* the count and the pass flags: P06 T+0x14 x, y; P07 T+0 x, w */
  int np = 1 + ((hy & 0x10) != 0) + ((hy & 0x700) != 0);   /* a record for each pass the program draws */
  uint32_t o1 = (uint32_t)(sizeof(GerXform) + np * sizeof(GerLit));
  if (n0 <= 64 && cap < o1 + n0 * (24 + 24 + 24 + 4)) return -5;
  VcLit L; int n = p7 ? vc_p07(c->vm, c->top, &L, blob + o1, v3, fl, 0, 64) : vc_p06(c->vm, c->top, c->entry, &L, blob + o1, v3, fl, 0, 64);
  if (n < 0) { St.face_unseen += n == -7; return n; }
  if (p7) {
    St.face_v += n; St.face_exact += !L.fast; St.face_nopass[2] += L.uv2 < 0; St.face_own2 += L.uv2 == 2; St.face_nopass[3] += L.p3 < 0;
#ifdef XC_P7CHK
    if (L.fast) {   /* the call again in the exact form, compared byte for byte (as tools/dev/p07chk.c) */
      static uint8_t e1[64 * 24], e3[64 * 24]; static uint16_t ef[64]; VcLit LE;
      vc_p06_exact = 1; int ne = vc_p07(c->vm, c->top, &LE, e1, e3, ef, 0, 64); vc_p06_exact = 0;
      LE.fast = 1; St.face_chk++;
      St.face_chkdiff += ne != n || memcmp(&LE, &L, sizeof L) || memcmp(e1, blob + o1, (size_t)vc_p06_size1(&L) * (size_t)n) ||
                         (L.p3 > 0 && memcmp(e3, v3, 24 * (size_t)n)) || memcmp(ef, fl, 2 * (size_t)n);
    }
#endif
  } else {
    St.lit_kind[L.kind]++; St.lit_v += n; St.lit_exact += !L.fast; St.lit_cull += L.cull; St.lit_ramp += L.p3 == 2;
    if (L.uv2 < 0) St.lit_nopass[2]++;
    if (L.uv2 == 2) St.lit_own2++;
    if (L.p3 < 0) St.lit_nopass[3]++;
  }
  int nst = vc_strips(fl, n, runs, 128);
  if (!nst) return 0;
  int s1 = vc_p06_size1(&L), s3 = vc_p06_size3(&L), t1 = (L.uv16 ? 0 : 1) | (L.pos16 ? 0 : 2), t3 = 1 | (L.pos16 ? 0 : 2), t2 = t1, s2 = 0;
  uint32_t ov2 = o1 + n * s1;
  if (L.uv2 == 2) { t2 = vc_p06_uv2(c->vm, c->top, &L, blob + o1, blob + ov2); s2 = 16 + 4 * ((t2 & 1) + (t2 >> 1)); }   /* 16, 20, 20, 24 bytes */
  uint32_t ov3 = ov2 + n * s2, orun = ov3 + (L.p3 > 0 ? n * s3 : 0), end = orun + nst * 4;
  if (L.p3 > 0) memcpy(blob + ov3, v3, n * s3);
  memcpy(blob + orun, runs, nst * 4);
  GerXform gx; xc_xform(&L.x, d, t, L.uv16 ? 8 : 1, h, c->gr, &gx);
  if (L.fog >= 0) { gx.fog1 = (float)L.fog / 255.0f * 1099511627776.0f; gx.fog2 = 1.0f / 1099511627776.0f; }   /* the constant F: x 2^40,
                                                    2^-40 (P06; P07 with a = 0); else P07's per vertex F = a w + b, xc_xform's */
  gx.nv = n; gx.nst = nst; gx.nidx = 0;
  memcpy(blob, &gx, sizeof gx);
  memset(&Lp, 0, sizeof Lp);
  int k = 0;
  for (int pass = 1; pass <= 3; pass++) {
    if ((pass == 2 && !(hy & 0x10)) || (pass == 3 && !(hy & 0x700))) continue;
    uint32_t at = (uint32_t)(sizeof gx + k++ * sizeof(GerLit));
    GerLit p; memset(&p, 0, sizeof p);
    /* the pass's vertices: pass 1's (pass 2 with a texture offset), pass 2's own (uv2 2), pass 3's own */
    uint32_t ov = pass == 3 ? ov3 : pass == 2 && L.uv2 == 2 ? ov2 : o1;
    int tt = pass == 3 ? t3 : pass == 2 && L.uv2 == 2 ? t2 : t1, ss = pass == 3 ? s3 : pass == 2 && L.uv2 == 2 ? s2 : s1;
    p.xf = -(int32_t)at; p.vtx = (int32_t)(ov - at); p.run = (int32_t)(orun - at);
    p.vtype = LIT_VT[tt]; p.stride = (uint32_t)ss; p.nv = (uint32_t)n; p.nst = (uint32_t)nst; p.pass = (uint32_t)pass;
    p.su = gx.su; p.sv = gx.sv; p.tu = gx.tu; p.tv = gx.tv;   /* pass 1's; xc_pass writes the others' */
    memcpy(blob + at, &p, sizeof p);
    if (pass == 1 || (pass == 2 && L.uv2 > 0) || (pass == 3 && L.p3 > 0)) Lp.rec[pass] = at;
    Lp.uvk[pass] = (tt & 1) ? 1 : 8; Lp.du[pass] = pass == 2 ? L.du : 0; Lp.dv[pass] = pass == 2 ? L.dv : 0;
  }
  Lp.on = 1; Lp.entry = c->entry; Lp.top = c->top; Lp.amin = L.amin;
  xc_state(d, ti, c->entry, b); b->kind = GERK_LIT; b->twopass = (uint8_t)xc_twopass(d, t, L.amin); b->count = (uint32_t)nst;
  b->first = (uint32_t)sizeof gx;
  if (h->zr && (b->flags & GERF_ZWRITE)) { if (b->ztst >= 2) xc_zr_lit(h->zr, &L.x, blob + o1, t1, s1, n); else h->zr->always++; }
  *size = end; return 1;
}
int xc_pass(const XlCall *c, int pass, const XcHost *h, GerBatch *b, uint8_t *blob)
{
  if (!Lp.on || pass < 2 || pass > 3 || c->entry != Lp.entry || c->top != Lp.top) return 0;
  if (!Lp.rec[pass]) return 0;
  XcGs d; xc_gs(c->gr, &d);
  XcTex t; memset(&t, 0, sizeof t); t.index = -1;
  if (d.tme && h->tex && !h->tex(h->ctx, c->gr, d.ctxt, &t)) t.index = -1;
  GerLit p; memcpy(&p, blob + Lp.rec[pass], sizeof p);
  float ts[4];
  xc_lit_ts(&d, &t, Lp.uvk[pass], Lp.du[pass], Lp.dv[pass], ts);
  p.su = ts[0]; p.sv = ts[1]; p.tu = ts[2]; p.tv = ts[3];
  memcpy(blob + Lp.rec[pass], &p, sizeof p);
  xc_state(&d, t.index, c->entry, b); b->kind = GERK_LIT; b->twopass = (uint8_t)xc_twopass(&d, &t, Lp.amin); b->count = p.nst;
  b->first = Lp.rec[pass];
  if (Lp.entry == VC_P07) St.face_pass[pass]++; else St.lit_pass[pass]++;
  return 1;
}

int xc_call(const XlCall *c, const XcHost *h, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size)
{
  Lp.on = 0;
  int k = c->entry == XL_DF0 ? 0 : c->entry == XL_FX ? 1 : c->entry == XL_FX2 ? 2 : is_p06(c->entry) ? 3 : c->entry == VC_P07 ? 4 : -1;
  if (k < 0 || c->cont) return 0;
  St.calls[k]++;
  XcGs d; xc_gs(c->gr, &d);
  if (!k) {   /* df0: the input first, so that a call off the scissor never asks for its texture */
    Dn = vc_df0(c->vm, c->top, &Dx, Dv, Dfl, 1024);
    if (Dn > 0 && h->cut && xc_df0_off(&d)) { xc_df0_zr(h, &d); St.cut[0]++; St.cut_v[0] += Dn; St.empty[0]++; return 0; }
  }
  XcTex t; memset(&t, 0, sizeof t); t.index = -1;
  if (d.tme && h->tex && !h->tex(h->ctx, c->gr, d.ctxt, &t)) t.index = -1;
  int r = k >= 3 ? xc_lit(c, h, &d, t.index, &t, b, blob, cap, size) :
          k ? xc_fpos(c, h, &d, t.index, &t, k == 2, b, blob, cap, size) : xc_df0(c, h, &d, t.index, &t, b, blob, cap, size);
  if (r < 0) St.bad[k]++; else if (!r) St.empty[k]++; else St.batches[k]++;
  return r;
}
