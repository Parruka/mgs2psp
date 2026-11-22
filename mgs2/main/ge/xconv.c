/* xconv.c: see xconv.h.  Each part follows gsref's export (tools/gsref.c ge_prim, dc_export, fp_end, fp_export), which the PS2
   pictures and the MGS2GE replays were checked with; gsref GSREF_XLCONV=1 compares the two batch by batch. */
#include "xconv.h"
#include "vu1conv.h"
#ifndef __psp__
#include "c1model.h"   /* sb11 C1 census and GE model (x86 only) */
#include "c2model.h"   /* sb12 C2 census and GE model (x86 only) */
#include "c3model.h"   /* sb13 C3 census of the rooms' meshes (x86 only) */
#endif
#include <stddef.h>
#include <string.h>
#include <math.h>   /* c1opt2: fabsf (xc_c1_zin) */
#include "me_rt.h"   /* task #149: -DMGS2_ME_SAFE (ME-safe calls) */

static XcStats St;
const XcStats *xc_stats(void) { return &St; }

/* the GS state of the call's first primitive (xconv.h XcGs), as gsref's setup() reads it from the registers (PRMODECONT picks PRIM or
   PRMODE) */
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
   GE offset 2048 - 240, 2048 - 136), depth x zk, fog (fogcol: FOGCOL, GS register 0x3d's low 24 bits), and the texture scale (uvk: 8 for
   df0's 16-bit UVs, 1 for float UVs) */
static const float XC_ID[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };   /* sb15 4x: the GE's world for vertices in clip space (x, y, w in z) */
static void xc_xform(const VcXform *x, const XcGs *d, const XcTex *t, int uvk, const XcHost *h, uint32_t fogcol, GerXform *gx)
{
  memset(gx, 0, sizeof *gx);
  memcpy(gx->world, x->world, sizeof gx->world);
  gx->proj[0] = 1; gx->proj[5] = 1; gx->proj[10] = x->pa; gx->proj[11] = 1; gx->proj[14] = -x->pb;
  /* float only (the PSP has no double unit, and the x86 check build rounds as the PSP's FPU): 480 / 512 = 0.9375, 272 / 512 = 0.53125 */
  float ofx = d->ofx / 16.0f, ofy = d->ofy / 16.0f, zk =
#ifdef MGS2_ME_SAFE
    h->zkf;
#else
    (float)h->zk;
#endif
  gx->vs[0] = x->vs[0] * 0.9375f; gx->vo[0] = (2048 - 240) + (x->vo[0] - ofx) * 0.9375f;
  gx->vs[1] = x->vs[1] * 0.53125f; gx->vo[1] = (2048 - 136) + (x->vo[1] - ofy) * 0.53125f;
  gx->vs[2] = -x->vs[2] * zk; gx->vo[2] = x->vo[2] * zk;
  gx->fog1 = x->fog_b / x->fog_a; gx->fog2 = x->fog_a / 255.0f; gx->fogcol = fogcol;
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
/* sb14 (3v, 3z): Datt 0 while a two-pass call's UVs, colours and flags wait (vc_df0_att from Drec's streams); Dbox the box of Dv's
   positions when Dbox_ok (vc_df0_pos's, or xc_df0_off's) */
static int Datt = 1, Dbox_ok; static int16_t Dbox[6]; static const XlRec *Drec;
static VcVert *Dvp = Dv;   /* sb14 3w: where the call's vertices are: Dv, or the frame's room after the GerXform (xc_df0's place for them) */
int xc_c3_chk;   /* xconv.h */
/* the df0 call xc_call read: 1 when XcHost.cut leaves it out */
static int xc_df0_off(const XcGs *d)
{
  if (Dbox_ok) {   /* sb14: vc_df0_pos's box, the same as the loop's below */
    float flo[3], fhi[3];
    for (int j = 0; j < 3; j++) { flo[j] = Dbox[j] * (1.0f / 32768); fhi[j] = Dbox[3 + j] * (1.0f / 32768); }
    return xc_off(Dx.world, flo, fhi, &Dx, d);
  }
  int lo[3] = { 32767, 32767, 32767 }, hi[3] = { -32768, -32768, -32768 };
  const VcVert *v = Dvp;
  for (int i = 0; i < Dn; i++) {
    if (v[i].x < lo[0]) lo[0] = v[i].x;
    if (v[i].x > hi[0]) hi[0] = v[i].x;
    if (v[i].y < lo[1]) lo[1] = v[i].y;
    if (v[i].y > hi[1]) hi[1] = v[i].y;
    if (v[i].z < lo[2]) lo[2] = v[i].z;
    if (v[i].z > hi[2]) hi[2] = v[i].z;
  }
  float flo[3], fhi[3];   /* the GE reads a 16-bit position as s / 32768 (exact in a float) */
  for (int j = 0; j < 3; j++) { flo[j] = lo[j] * (1.0f / 32768); fhi[j] = hi[j] * (1.0f / 32768); }
  if (Dn > 0) { for (int j = 0; j < 3; j++) { Dbox[j] = (int16_t)lo[j]; Dbox[3 + j] = (int16_t)hi[j]; } Dbox_ok = 1; }   /* sb14: for 3z */
  return xc_off(Dx.world, flo, fhi, &Dx, d);
}
/* sb14 3z (XcHost.c3z): 1 when every w of the df0 call (xc_zr_df0's w = a . p + t, p a vertex's position as the stream holds it) lies
   inside the range the depth range keeps for the call's projection, by the box b of its positions, as xc_c2_zin does for C2's unskinned
   form: a . p + t over the box lies in [d - r, d + r], d at the box's centre q (exact: halved sums of int16), r = |a| . the half extents
   e; widened by 1e-5 of the magnitudes big (the float rounding of each w and of this bound is below 1e-6 of them) and 1e-30.  Then every
   w is in front (the count n) and none can move the range, so xc_zr_df0 would only count the call and its n vertices.  A NaN or an
   infinity makes a test fail: no */
static int xc_df0_zin(const XcZr *z, const VcXform *x, const int16_t *b)
{
  if (!z->on || x->pa != z->pa || x->pb != z->pb || x->vs[2] != z->vs2 || x->vo[2] != z->vo2) return 0;
  const float a0 = x->world[2] / 32768, a1 = x->world[5] / 32768, a2 = x->world[8] / 32768, t = x->world[11];
  const float q0 = 0.5f * (float)((int)b[0] + b[3]), q1 = 0.5f * (float)((int)b[1] + b[4]), q2 = 0.5f * (float)((int)b[2] + b[5]);
  const float e0 = 0.5f * (float)((int)b[3] - b[0]), e1 = 0.5f * (float)((int)b[4] - b[1]), e2 = 0.5f * (float)((int)b[5] - b[2]);
  const float m0 = fabsf(q0) + e0, m1 = fabsf(q1) + e1, m2 = fabsf(q2) + e2;   /* the largest |p| on each axis */
  const float d0 = t + a0 * q0 + a1 * q1 + a2 * q2, r0 = fabsf(a0) * e0 + fabsf(a1) * e1 + fabsf(a2) * e2;
  const float big = fabsf(t) + fabsf(a0) * m0 + fabsf(a1) * m1 + fabsf(a2) * m2;
  if (!(big - big == 0) || !(d0 - d0 == 0) || !(r0 - r0 == 0)) return 0;
  const float E = 1e-5f * big + 1e-30f, lo = d0 - r0 - E, hi = d0 + r0 + E;
  return lo > 0 && lo >= z->wmin && hi <= z->wmax;
}
/* xc_zr_df0 for Dx and Dv's first n vertices (the call xc_call read), or by the box's bound when 3z decides (the range unchanged: it
   only counts the call and its vertices, all in front).  With xc_c3_chk: the bound against every vertex's w */
static void xc_zr_df0d(const XcHost *h, int n)
{
  XcZr *z = h->zr;
  if (h->c3z && Dbox_ok && n > 0 && xc_df0_zin(z, &Dx, Dbox)) {
#ifndef __psp__
    if (xc_c3_chk) {
      XcZr c = *z; xc_zr_df0(&c, &Dx, Dvp, n); St.c3_chk++;
      if (c.calls != z->calls + 1 || c.verts != z->verts + n || c.wmin != z->wmin || c.wmax != z->wmax || c.on != z->on || c.lo != z->lo || c.hi != z->hi) St.c3_chk_bad++;
    }
#endif
    z->calls++; z->verts += n; St.c3z_in++; St.c3z_in_v += n;
    return;
  }
  xc_zr_df0(z, &Dx, Dvp, n);
}
/* a call the cut leaves out gives the frame's depth range (XcZr) what xc_df0 would have given it: a depth-testing, depth-writing call
   with a triangle to draw, its vertices' w.  The frame's depth map and the next frame's class 3 window for the effects are then the same
   with the cut on or off (renderer_notes 11.15) */
static void xc_df0_zr(const XcHost *h, const XcGs *d)
{
  if (!h->zr || !d->zte || d->zmsk || (d->ate && d->atst == 0 && d->afail == 1)) return;   /* xc_state's GERF_ZWRITE */
  int i = 2;
  if (Datt) while (i < Dn && (Dfl[i] & 0x8000)) i++;
  else {   /* sb14 3v: the flags from the stream B, as vc_df0_att would write them */
    const XlRec *r = Drec;
    while (i < Dn && (vc_df0_flag(r->p, r->kind, r->row, i) & 0x8000)) i++;
  }
  if (i >= Dn) return;   /* every triangle has ADC: xc_df0 draws nothing (no strip) */
  if (d->ztst >= 2) xc_zr_df0d(h, Dn); else h->zr->always++;
}

/* P04+df0: the call's strips (the data's ADC bits) with hardware transform; the GE clips and the scissor cuts.  Its input: xc_call's
   Dn, Dx, Dv, Dfl */
static int xc_df0(const XlCall *c, const XcHost *h, const XcGs *d, int ti, const XcTex *t, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size)
{
  static uint16_t runs[2048], idx[4096];
  VcVert *v = Dvp; uint16_t *fl = Dfl; VcXform x = Dx; int n = Dn;
  (void)c;
  if (n < 0) return n;
  int nst = vc_strips(fl, n, runs, 1024), ni = 0;
  if (!nst) return 0;
  for (int k = 0; k < nst && !h->noidx; k++) {   /* the strips joined into one by repeated vertices */
    if (k) { idx[ni] = idx[ni - 1]; ni++; idx[ni++] = runs[2 * k]; }
    for (int j = 0; j < runs[2 * k + 1]; j++) idx[ni++] = (uint16_t)(runs[2 * k] + j);
  }
  GerXform gx; xc_xform(&x, d, t, 8, h, (uint32_t)(c->gr[0x3d] & 0xffffff), &gx);
  gx.nv = n; gx.nst = nst; gx.nidx = ni;
  uint32_t o = 0, need = sizeof gx + n * sizeof *v + nst * 4 + ni * 2;
  if (need > cap) return -5;
  memcpy(blob + o, &gx, sizeof gx); o += sizeof gx;
  if ((uint8_t *)v != blob + o) memcpy(blob + o, v, n * sizeof *v);   /* sb14 3w: there already */
  o += n * sizeof *v;
  memcpy(blob + o, runs, nst * 4); o += nst * 4;
  memcpy(blob + o, idx, ni * 2); o += ni * 2;
  xc_state(d, ti, c->entry, b); b->kind = GERK_XFORM; b->twopass = (uint8_t)xc_twopass(d, t, x.alpha); b->count = nst;
  if (h->zr && (b->flags & GERF_ZWRITE)) { if (b->ztst >= 2) xc_zr_df0d(h, n); else h->zr->always++; }   /* GERF_ZWRITE has ZTEST; sb14:
                                                                                     x, v, n are Dx, Dv, Dn */
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
long xc_rec3_calls, xc_rec3_vm, xc_rec3_n, xc_rec3_diff;   /* sb6 (xconv.h) */
typedef int16_t __attribute__((may_alias)) xc_h16;
/* sb6 lane 3: vertex j's colour and UV quadwords (T + 0x101 + 3 j, 0x102 + 3 j) as the VU memory holds them: from the prim's attributes
   (XlRec p[1]: V4-16 signed, plus ROW for kind 3, as ge/xlate.c ud_dec decodes them), else from the VU memory */
static inline void fx_attr(const XlCall *c, const XlRec *r, int j, uint32_t *col, uint32_t *uv)
{
  if (r) { const xc_h16 *a = (const xc_h16 *)r->p[1] + 8 * j; int k3 = r->kind[1] == 3;
    for (int q = 0; q < 4; q++) { col[q] = (uint32_t)(int32_t)a[q] + (k3 ? r->row[q] : 0); uv[q] = (uint32_t)(int32_t)a[4 + q] + (k3 ? r->row[q] : 0); } }
  else { memcpy(col, vq(c->vm, c->top + 0x101 + 3 * j), 16); memcpy(uv, vq(c->vm, c->top + 0x102 + 3 * j), 16); }
}
/* sb15 fx: one scratch for the effect converters (xc_fpos, xc_spr, xc_quad, xc_line).  Each converts one call into the frame's data and
   returns; nothing points into the scratch afterwards and no two of them run at once, so they share it (step 4's own arrays were 262 KB) */
static union {
  struct { VcVertF v[1024], pv[8192]; uint16_t fl[1024], pfan[1024][2]; uint8_t cls[1024], cf[1024], drop[1024]; float cc[1024][4]; } f;
  struct { VcVertF v[2 * 256]; uint8_t drop[256]; } s;
  struct { VcVertF v[4 * 256], pv[2048]; VcCv cv[4 * 256]; uint8_t st[256], cls[4 * 256]; uint16_t pfan[512][2]; } q;
  struct { VcVertF v[1024], sv[1024], pv[2048]; uint16_t fl[1024], run[512][2]; float cc[1024][4]; uint8_t cls[1024]; } l;
} Fx;
static int xc_fpos(const XlCall *c, const XcHost *h, const XcGs *d, int ti, const XcTex *t, int k2870, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size)
{
  VcVertF *const v = Fx.f.v, *const pv = Fx.f.pv; uint16_t *const fl = Fx.f.fl, (*const pfan)[2] = Fx.f.pfan; uint8_t *const cls = Fx.f.cls, *const cf = Fx.f.cf;
  float (*const cc)[4] = Fx.f.cc;
  if (h->trace) h->trace->n = 0;
  /* a call whose vertices all lie at one point draws nothing (the deck's unused particles; some of them hold colour words above 255) */
  int n0 = (int)(vq(c->vm, c->top + 0x14)[0] & 0xffff), same = n0 <= 1024;
  /* sb6 lane 3: the vertices from the prim's streams when they hold every quadword xc_fpos reads, else from the VU memory, decoded */
  const XlRec *rr = c->rec && c->rec->lane == 3 ? c->rec : 0;
#ifndef __psp__
  int chk = 0;   /* (x86, xc_rec_chk: this call is checked; chk 2: a difference seen) */
#endif
  if (rr && !vc_fpos_rec_ok(rr->p, rr->num, rr->kind, n0)) { rr = 0; xc_rec3_vm++; xl_vm_sync(); }
  else if (rr) {
    xc_rec3_calls++;
#ifndef __psp__   /* (x86 only) */
    if (xc_rec_chk) {   /* every quadword of the vertices xc_fpos reads (position, colour, UV of the header's count) against the VU memory, decoded */
      xl_vm_sync(); chk = 1; xc_rec3_n++;
      for (int i = 0; i < n0 && chk == 1; i++) { uint32_t q[2][4];
        fx_attr(c, rr, i, q[0], q[1]);
        if (memcmp((const uint32_t *)rr->p[0] + 4 * i, vq(c->vm, c->top + 0x100 + 3 * i), 16) || memcmp(q[0], vq(c->vm, c->top + 0x101 + 3 * i), 16) ||
            memcmp(q[1], vq(c->vm, c->top + 0x102 + 3 * i), 16)) chk = 2; }
    }
#endif
  }
  if (rr) { const uint32_t *P = (const uint32_t *)rr->p[0]; for (int i = 1; i < n0 && same; i++) same = !memcmp(P + 4 * i, P, 12); }
  else for (int i = 1; i < n0 && same; i++) same = !memcmp(vq(c->vm, c->top + 0x100 + 3 * i), vq(c->vm, c->top + 0x100), 12);
#ifndef __psp__
  if (chk && same) xc_rec3_diff += chk == 2;
#endif
  if (same) return 0;
  VcXform x; int n; const int tq = h->c4x ? d->tme : 1;   /* sb15 4x: an untextured call's UV q not checked */
  if (rr) {
    n = vc_fpos_rec(c->vm, c->top, rr->p, rr->num, rr->kind, rr->row, &x, v, fl, 1024, tq);
    if (n == -8) { rr = 0; xc_rec3_calls--; xc_rec3_vm++; xl_vm_sync(); n = vc_fpos(c->vm, c->top, &x, v, fl, 1024, tq); }   /* (not expected:
                                                        vc_fpos_rec_ok above) */
#ifndef __psp__
    else if (chk) {   /* vc_fpos on the VU memory, decoded: its every output the same */
      static VcVertF ev[1024]; static uint16_t ef[1024]; VcXform ex;
      int ne = vc_fpos(c->vm, c->top, &ex, ev, ef, 1024, tq);
      if (ne != n || memcmp(&ex, &x, sizeof x) || (n > 0 && (memcmp(ev, v, (size_t)n * sizeof *v) || memcmp(ef, fl, 2 * (size_t)n)))) chk = 2;
      xc_rec3_diff += chk == 2;
    }
#endif
  } else n = vc_fpos(c->vm, c->top, &x, v, fl, 1024, tq);
  if (n < 0) return n;
  /* task #114: a thinned effect (XcHost.thin): strips 0, keep, 2 keep ... drawn; drop[i]: vertex i is in a strip left out */
  uint8_t *const drop = Fx.f.drop; int keep = 1;
  for (int k = 0; k < h->nthin; k++) if (!h->thin[k].line && c->src - h->thin[k].lo < h->thin[k].hi - h->thin[k].lo) { keep = h->thin[k].keep; break; }
  if (keep > 1) { int s = 0;
    for (int i = 0; i < n; i++) { if (i && (fl[i] & 0x8000) && i + 1 < n && (fl[i + 1] & 0x8000) && !(fl[i - 1] & 0x8000)) s++;
      drop[i] = (uint8_t)(s % keep != 0); }
    St.thin_calls++; St.thin_strips += s + 1; St.thin_drop += s - s / keep; }
  /* the clip positions and flags as the micro program computes them; the flag words it stores back */
  int amin = 255, fm = h->clipfast;   /* sb3: the clamps left out where no input needs them (XcHost.clipfast): the matrix once a call */
  for (int k = 0; fm && k < 16; k++) fm = vc_clip_in(x.m[k]);
  XcTrace *tr = h->trace;
  for (int i = 0; i < n; i++) { float p[3] = { v[i].x, v[i].y, v[i].z };
    int a = rr ? (int)(((uint32_t)(int32_t)((const xc_h16 *)rr->p[1])[8 * i + 3] + (rr->kind[1] == 3 ? rr->row[3] : 0)) & 0xff) :
                 (int)(vq(c->vm, c->top + 0x101 + 3 * i)[3] & 0xff);   /* (sb6: the colour's w) */
    if (a < amin) amin = a;
    if (keep > 1 && drop[i] && !tr) { cf[i] = 0; continue; }   /* (its triangles are not drawn; a trace gets every position) */
    St.clip[0]++;
    if (fm && vc_clip_in(p[0]) && vc_clip_in(p[1]) && vc_clip_in(p[2])) vc_vu_clip_nc(x.m, p, cc[i]);
    else { vc_vu_clip(x.m, p, cc[i]); St.clip[1] += h->clipfast; }
    cf[i] = (uint8_t)vc_cflags(cc[i]); }
  if (tr) { tr->n = n;
    for (int i = 0; i < n; i++) { int f = cf[i] | (i >= 1 ? cf[i - 1] : 0) | (i >= 2 ? cf[i - 2] : 0);   /* FCSET 0 before the loop */
      memcpy(tr->c[i], cc[i], 16); tr->post[i] = (uint16_t)(fl[i] | (f && !(fl[i] & 0x8000) ? 0xc000 : 0)); } }
  if (h->c4x) memcpy(x.world, XC_ID, sizeof x.world);   /* sb15 4x: the vertices in clip space (below), the GE's world the identity */
  GerXform gx; xc_xform(&x, d, t, 1, h, (uint32_t)(c->gr[0x3d] & 0xffffff), &gx);
  const float g[4] = { gx.vs[0], gx.vo[0], gx.vs[1], gx.vo[1] };
  int npf = 0, npv = 0, drawn = 0;
  memset(cls, 0, n);
  for (int i = 2; i < n; i++) {
    if (fl[i] & 0x8000) { St.tris[0]++; continue; }
    if (keep > 1 && drop[i]) { St.thin_tris++; continue; }   /* task #114: its strip is left out (class 0) */
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
    for (int k = 0; k < 3; k++) { int j = i - 2 + k; uint32_t col[4], uv[4]; fx_attr(c, rr, j, col, uv);
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
      o2->u = P->uv[0]; o2->v = P->uv[1];
      if (h->c4x) { o2->x = P->c[0]; o2->y = P->c[1]; o2->z = P->c[3]; } else { o2->x = P->p[0]; o2->y = P->p[1]; o2->z = P->p[2]; }   /* (sb15 4x) */
      o2->c = (uint32_t)cv[0] | (uint32_t)cv[1] << 8 | (uint32_t)cv[2] << 16 | (uint32_t)(cv[3] >= 0x80 ? 0xff : cv[3] * 2) << 24;
    }
  }
  if (tr) memcpy(tr->cls, cls, n);
  if (!drawn) return 0;
  if (h->c4x)   /* sb15 4x: each vertex's clip position as the micro program computes it (x, y, with w in z); a thinned strip's vertices,
                   whose triangles are not drawn, keep what they hold */
    for (int i = 0; i < n; i++) if (!(keep > 1 && drop[i] && !tr)) { v[i].x = cc[i][0]; v[i].y = cc[i][1]; v[i].z = cc[i][3]; }
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

/* ---- sb15, Stage C step 4 (stagec_notes.md step 4; XcHost.c4): P04+3740's sprites, +3a38's quads, +2b60's streaks as GERK_XFORMF
   batches (ge/replay_fmt.h), the primitive in GerBatch.prim: the converters (ge/vu1conv.h vc_spr, vc_quad, vc_line, vc_clip_seg) as gsref's
   spchk, qdchk and lnchk check them against the PS2's micro code.  The texture is looked up only for a call that draws something ---- */
static void c4_tex(const XlCall *c, const XcHost *h, const XcGs *d, XcTex *t)
{
  memset(t, 0, sizeof *t); t->index = -1;
  if (d->tme && h->tex && !ME_CALLP(h->tex)(h->ctx, c->gr, d->ctxt, t)) t->index = -1;
}
static inline uint32_t c4_col(const float *col, float Q)   /* a clipped vertex's colour as the PS2 draws it: ftoi0(col / w + 0.0005), alpha doubled */
{
  int cv[4];
  for (int q = 0; q < 4; q++) { cv[q] = (int)(col[q] * Q + 0.0005f); cv[q] = cv[q] < 0 ? 0 : cv[q] > 255 ? 255 : cv[q]; }
  return (uint32_t)cv[0] | (uint32_t)cv[1] << 8 | (uint32_t)cv[2] << 16 | (uint32_t)(cv[3] >= 0x80 ? 0xff : cv[3] * 2) << 24;
}
/* the lowest colour alpha (PS2 scale) of n records of st quadwords from T + 0x101 (xc_twopass's a) */
static int c4_amin(const XlCall *c, int n, int st)
{
  int amin = 255;
  for (int i = 0; i < n; i++) { int a = (int)(vq(c->vm, c->top + 0x101 + (uint32_t)(st * i))[3] & 0xff); if (a < amin) amin = a; }
  return amin;
}
/* sprites: the drawn sprites' corner pairs (clip x, y, with w in z: the GE's world the identity), one GU_SPRITES; no classes */
static int xc_spr(const XlCall *c, const XcHost *h, const XcGs *d, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size)
{
  VcVertF *const v = Fx.s.v; uint8_t *const drop = Fx.s.drop; VcXform x;
  int n = vc_spr(c->vm, c->top, &x, v, drop, 256), m = 0;
  if (n < 0) return n;
  for (int i = 0; i < n; i++) m += !drop[i];
  St.c4_n[0] += n; St.c4_drop[0] += n - m; St.c4_drawn[0] += m;
  for (int i = 0, j = 0; i < n; i++) if (!drop[i]) { const VcVertF *a = &v[2 * j++]; St.c4_clip[0] += a->x == -a->z || a->y == -a->z || a[1].x == a->z || a[1].y == a->z; }
  if (!m) return 0;
  XcTex t; c4_tex(c, h, d, &t);
  GerXform gx; xc_xform(&x, d, &t, 1, h, (uint32_t)(c->gr[0x3d] & 0xffffff), &gx);
  const uint32_t nv = 2u * (uint32_t)m, need = sizeof gx + nv * sizeof *v;
  if (need > cap) return -5;
  gx.nv = nv;
  uint32_t o = 0;
  memcpy(blob + o, &gx, sizeof gx); o += sizeof gx;
  memcpy(blob + o, v, nv * sizeof *v); o += nv * sizeof *v;
  xc_state(d, t.index, c->entry, b); b->kind = GERK_XFORMF; b->prim = 6; b->twopass = (uint8_t)xc_twopass(d, &t, c4_amin(c, n, 6)); b->count = nv;
  *size = o; return 1;
}
/* quads: the quads drawn whole or clipped (the dropped ones left out: they draw nothing), 4 corners each in the strip's classes (corners
   0, 1: 0; 2, 3: 1 whole, 4 clipped), the clipped triangles' polygons from vc_clip_tri as xc_fpos's class 4; fx_draw draws them as it
   draws the fans */
static int xc_quad(const XlCall *c, const XcHost *h, const XcGs *d, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size)
{
  VcVertF *const v = Fx.q.v, *const pv = Fx.q.pv; VcCv *const cv = Fx.q.cv; uint8_t *const st = Fx.q.st, *const cls = Fx.q.cls;
  uint16_t (*const pfan)[2] = Fx.q.pfan; VcXform x;
  int n = vc_quad(c->vm, c->top, &x, v, cv, st, 256);
  if (n < 0) return n;
  int npf = 0, npv = 0, drawn = 0, q = 0;   /* q: the quads kept */
  St.c4_n[1] += n;
  for (int i = 0; i < n; i++) {
    if (st[i] == 1) { St.c4_drop[1]++; continue; }
    if (q != i) memcpy(&v[4 * q], &v[4 * i], 4 * sizeof *v);
    uint8_t *k4 = cls + 4 * q; k4[0] = k4[1] = 0;
    if (st[i] == 0) { k4[2] = k4[3] = 1; drawn = 1; St.c4_drawn[1]++; q++; continue; }
    k4[2] = k4[3] = 4; St.c4_clip[1]++;
    for (int t = 0; t < 2; t++) {
      VcCv in[3] = { cv[4 * i + t], cv[4 * i + t + 1], cv[4 * i + t + 2] }, out[16];
      int m = vc_clip_tri(in, out);
      if (m < 3) { St.c4_poly_empty++; continue; }
      if (npf == 512 || npv + m > 2048) { St.c4_full++; continue; }
      pfan[npf][0] = (uint16_t)(4 * q + 2 + t); pfan[npf][1] = (uint16_t)m; npf++; drawn = 1; St.c4_poly++; St.c4_poly_v += m;
      for (int k = 0; k < m; k++) {
        const VcCv *P = &out[k]; VcVertF *o2 = &pv[npv++];
        o2->u = P->uv[0]; o2->v = P->uv[1]; o2->c = c4_col(P->col, 1.0f / P->c[3]); o2->x = P->c[0]; o2->y = P->c[1]; o2->z = P->c[3];   /* (4x) */
      }
    }
    q++;
  }
  if (!drawn) return 0;
  XcTex t; c4_tex(c, h, d, &t);
  GerXform gx; xc_xform(&x, d, &t, 1, h, (uint32_t)(c->gr[0x3d] & 0xffffff), &gx);
  const uint32_t nv = 4u * (uint32_t)q, nc = (nv + 3) & ~3u, need = sizeof gx + (nv + (uint32_t)npv) * sizeof *v + nc + (uint32_t)npf * 4;
  if (need > cap) return -5;
  gx.nv = nv; gx.nfan = (uint32_t)npf; gx.nfv = (uint32_t)npv;
  uint32_t o = 0;
  memcpy(blob + o, &gx, sizeof gx); o += sizeof gx;
  memcpy(blob + o, v, nv * sizeof *v); o += nv * sizeof *v;
  memcpy(blob + o, pv, (uint32_t)npv * sizeof *pv); o += (uint32_t)npv * sizeof *pv;
  memcpy(blob + o, cls, nv); memset(blob + o + nv, 0, nc - nv); o += nc;
  memcpy(blob + o, pfan, (uint32_t)npf * 4); o += (uint32_t)npf * 4;
  xc_state(d, t.index, c->entry, b); b->kind = GERK_XFORMF; b->prim = 4; b->twopass = (uint8_t)xc_twopass(d, &t, c4_amin(c, n, 12)); b->count = nv;
  *size = o; return 1;
}
/* streaks: the strip's drawn segments in the PS2's order as vertex pairs (sb15 4x: their clip positions as the micro program computes
   them, x, y with w in z, the GE's world the identity, as the sprites'), a run of k > 1 joined segments as its k + 1 vertices (listed
   after the vertices: uint16 first vertex, count); then the clipper's lines, their ends in pairs (the clipper's clip positions; colour
   and UV as the PS2 draws them).  GerXform nv: the strip's
   vertices kept, nst: the runs, nfv: the clipper's ends; nfan 0, no classes.  fx_draw: GU_LINES over the pairs before, between and after
   the runs (the clipper's lines with the last), GU_LINE_STRIP for each run, so the GE draws the PS2's segments in its order.  The
   vertices of segments the strip does not draw are left out (sb15: the frame's data).  Task #154: a call the host thins (a range with
   line 1) draws strips 0, keep, 2 keep ... (a strip starts at vertex 0 and at each ADC vertex); the others' segments class 0, as if
   the strip did not draw them */
#ifndef __psp__
uint16_t xc_lchk_sv[1024][2], xc_lchk_pv[1024]; int xc_lchk_nsv, xc_lchk_npv;   /* x86 check (task #154, xlive_chk.c thin_chk_l): the
                                       strip's pieces as written (first vertex, vertices), then each clipper's line's segment */
#endif
static int xc_line(const XlCall *c, const XcHost *h, const XcGs *d, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size)
{
  VcVertF *const v = Fx.l.v, *const sv = Fx.l.sv, *const pv = Fx.l.pv; uint16_t *const fl = Fx.l.fl, (*const run)[2] = Fx.l.run;
  float (*const cc)[4] = Fx.l.cc; uint8_t *const cls = Fx.l.cls; VcXform x;
#ifdef XC_TME_OLD   /* x86 proof build only (-DXC_TME_OLD): refuse untextured lines with a stale UV q, as before the tme argument */
  int n = vc_line(c->vm, c->top, &x, v, fl, cc, cls, 1024, 1), npv = 0, nsv = 0, nst = 0;
#else
  int n = vc_line(c->vm, c->top, &x, v, fl, cc, cls, 1024, d->tme), npv = 0, nsv = 0, nst = 0;
#endif
  if (n < 0) return n;
  { int keep = 1;   /* task #154 */
    for (int k = 0; k < h->nthin; k++) if (h->thin[k].line && c->src - h->thin[k].lo < h->thin[k].hi - h->thin[k].lo) { keep = h->thin[k].keep; break; }
    if (keep > 1) { int s = 0; long nd = 0;
      for (int i = 1; i < n; i++) { if (!cls[i]) s++; else if (s % keep) { cls[i] = 0; nd++; } }
      St.thin_lcalls++; St.thin_lstrips += s + 1; St.thin_ldrop += s - s / keep; St.thin_lseg += nd; } }
#ifndef __psp__
  xc_lchk_nsv = xc_lchk_npv = 0;
#endif
  for (int i = 1; i < n; i++) {   /* the strip: a run of class 1 segments i..e is its vertices i-1..e (runs share no vertex) */
    if (cls[i] != 1) continue;
    int e = i; while (e + 1 < n && cls[e + 1] == 1) e++;
    const int k = e - i + 2;
    if (k > 2) { run[nst][0] = (uint16_t)nsv; run[nst][1] = (uint16_t)k; nst++; }
#ifndef __psp__
    xc_lchk_sv[xc_lchk_nsv][0] = (uint16_t)(i - 1); xc_lchk_sv[xc_lchk_nsv][1] = (uint16_t)k; xc_lchk_nsv++;
#endif
    memcpy(&sv[nsv], &v[i - 1], (size_t)k * sizeof *v);
    for (int q = 0; q < k; q++) { const float *C = cc[i - 1 + q]; sv[nsv + q].x = C[0]; sv[nsv + q].y = C[1]; sv[nsv + q].z = C[3]; }   /* (4x) */
    nsv += k;
    i = e;
  }
  for (int i = 1; i < n; i++) {
    if (!cls[i]) continue;
    St.c4_n[2]++;
    if (cls[i] == 1) { St.c4_drawn[2]++; continue; }
    if (cls[i] == 2) { St.c4_drop[2]++; continue; }
    St.c4_clip[2]++;
    VcCv in[2], o2[2];
    for (int k = 0; k < 2; k++) {
      const int j = i - 1 + k; const uint32_t *C = vq(c->vm, c->top + 0x101 + 3u * (uint32_t)j);
      memcpy(in[k].c, cc[j], 16);
      for (int q = 0; q < 4; q++) in[k].col[q] = (float)(int32_t)C[q];
      in[k].uv[0] = v[j].u; in[k].uv[1] = v[j].v; in[k].uv[2] = 1.0f; in[k].p[0] = v[j].x; in[k].p[1] = v[j].y; in[k].p[2] = v[j].z;
    }
    if (!vc_clip_seg(in, o2)) { St.c4_line_drop++; continue; }
    if (npv + 2 > 2048) { St.c4_full++; continue; }
    for (int k = 0; k < 2; k++) {
      const VcCv *P = &o2[k]; VcVertF *q = &pv[npv++];
      q->u = P->uv[0]; q->v = P->uv[1]; q->c = c4_col(P->col, 1.0f / P->c[3]); q->x = P->c[0]; q->y = P->c[1]; q->z = P->c[3];   /* (4x) */
    }
#ifndef __psp__
    xc_lchk_pv[xc_lchk_npv++] = (uint16_t)i;
#endif
    St.c4_lines++;
  }
  if (!nsv && !npv) return 0;
  XcTex t; c4_tex(c, h, d, &t);
  memcpy(x.world, XC_ID, sizeof x.world);   /* (4x: clip positions) */
  GerXform gx; xc_xform(&x, d, &t, 1, h, (uint32_t)(c->gr[0x3d] & 0xffffff), &gx);
  const uint32_t need = sizeof gx + (uint32_t)(nsv + npv) * sizeof *v + (uint32_t)nst * 4;
  if (need > cap) return -5;
  gx.nv = (uint32_t)nsv; gx.nst = (uint32_t)nst; gx.nfv = (uint32_t)npv;
  uint32_t o = 0;
  memcpy(blob + o, &gx, sizeof gx); o += sizeof gx;
  memcpy(blob + o, sv, (uint32_t)nsv * sizeof *sv); o += (uint32_t)nsv * sizeof *sv;
  memcpy(blob + o, pv, (uint32_t)npv * sizeof *pv); o += (uint32_t)npv * sizeof *pv;
  memcpy(blob + o, run, (uint32_t)nst * 4); o += (uint32_t)nst * 4;
  xc_state(d, t.index, c->entry, b); b->kind = GERK_XFORMF; b->prim = 2; b->twopass = (uint8_t)xc_twopass(d, &t, c4_amin(c, n, 3)); b->count = (uint32_t)(nsv + npv);
  *size = o; return 1;
}
static int xc_c4(const XlCall *c, const XcHost *h, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size)
{
  const int j = c->entry == VC_SPR ? 0 : c->entry == VC_QUAD ? 1 : 2;
  St.c4_calls[j]++;
  XcGs d; xc_gs(c->gr, &d);
  int r = j == 0 ? xc_spr(c, h, &d, b, blob, cap, size) : j == 1 ? xc_quad(c, h, &d, b, blob, cap, size) : xc_line(c, h, &d, b, blob, cap, size);
  if (r < 0) St.c4_bad[j]++; else if (!r) St.c4_empty[j]++; else St.c4_batches[j]++;
#ifdef XC_C4_DBG   /* x86 diagnosis only (mk.sh <dir> -DXC_C4_DBG): what a refused call holds */
  if (r < 0 && r != -5) {
    static int nd; const int st = j == 0 ? 6 : j == 1 ? 12 : 3; const int n = (int)(vq(c->vm, c->top + 0x14)[0] & 0xffff);
    if (nd < 60) { nd++; int shown = 0;
      fprintf(stderr, "C4DBG kind %d r %d n %d top %03x gr3d %08x\n", j, r, n, (unsigned)c->top, (unsigned)(c->gr[0x3d] & 0xffffffff));
      for (int i = 0; i < n && i < 1024 && shown < 6; i++) {
        const uint32_t *C = vq(c->vm, c->top + 0x101 + (uint32_t)(st * i)), *U = vq(c->vm, c->top + 0x102 + (uint32_t)(st * i));
        const int bc = C[0] > 255 || C[1] > 255 || C[2] > 255 || C[3] > 255, bq = U[2] != 0x1000;
        if (bc || bq) { shown++; fprintf(stderr, "C4DBG   rec %d col %08x %08x %08x %08x uv %08x %08x %08x %08x\n", i, (unsigned)C[0], (unsigned)C[1], (unsigned)C[2], (unsigned)C[3], (unsigned)U[0], (unsigned)U[1], (unsigned)U[2], (unsigned)U[3]); }
      }
    }
  }
#endif
  return r;
}

/* ---- P06 and P07 (ge/vu1conv.h vc_p06, vc_p07; replay_fmt.h GERK_LIT).  The blob: the call's GerXform, a GerLit per pass the
   program draws, the vertices of passes 1 and 2, pass 3's, the strips.  Passes 2 and 3 come from the translator after the call (xc_pass), each with its own
   texture: their GerLit's texture scale is written then. ---- */
int xc_rec_chk; long xc_rec_n, xc_rec_diff, xc_rec_calls, xc_rec6_calls, xc_rec6_vm, xc_rec6_n, xc_rec6_diff;   /* sb1, sb2 (xconv.h) */
long xc_rec7_calls, xc_rec7_vm, xc_rec7_n, xc_rec7_diff;   /* sb5 (xconv.h) */
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
static const XcHeld *Hr;   /* sb4: the held call xc_held_run converts (its c->vm its record's image or Sv), else 0 */
static uint8_t Sv[16384] __attribute__((aligned(64)));   /* the scratch VU memory: a held call's rebuilt memory, or its image when pass 3 is drawn */
static void sv_rebuild(const XcHeld *r);
static void sv7_rebuild(const XcHeld *r);   /* sb10 T2: a held P07 call's vertices in Sv (vc_p07_rec -8) */
/* ---- sb11 C1 (stagec_notes.md 1.7-1.9; xconv.h XcHost.c1): a lane 4 P07 call drawn by the GE's own skinning, lighting and texture
   matrix.  Its blob: [GerXform][a GerLit a pass][pass 1's vertices][pass 2's own (uv2 2)][GerC1 with its nw bones][strips], each part
   from a 16-byte boundary, the pads zeroed; each GerLit's vtx is the GerC1 (ge/replay_fmt.h).  The vertices are built at each call:
   checking a kept copy against the streams would read the same bytes as building it ---- */
#define C1_GXL(np) ((uint32_t)(sizeof(GerXform) + (uint32_t)(np) * sizeof(GerLit)))
#define VT_C1(nw) (0x342u | ((uint32_t)(nw) - 1u) << 14)   /* GU_WEIGHTS(nw) | GU_WEIGHT_8BIT | GU_VERTEX_16BIT | GU_NORMAL_16BIT | GU_TEXTURE_16BIT */
static uint32_t xc_c1_bound(uint32_t n, uint32_t hy)   /* (a vertex at most 8 weights + 16 bytes; passes 1 and 2's; the strips at most n) */
{
  const uint32_t np = 1 + ((hy & 0x10) != 0) + ((hy & 0x700) != 0);
  return ((C1_GXL(np) + 15u) & ~15u) + 2 * ((n * 24u + 15u) & ~15u) + ((GERC1_SIZE(8) + 15u) & ~15u) + n * 4u;
}
uint32_t xc_c1_hold_bound(const XlCall *c, uint32_t bound)
{
  const uint32_t *hd = vq(c->vm, c->top), b1 = xc_c1_bound(hd[0] & 0xffff, hd[3] & 0xffff);
  return b1 > bound ? b1 : bound;
}
/* the depth range (XcZr) of a C1 call: w, the GE's view z, of each vertex skinned as the GE does it, sum_k a_k (row . bone_k (p / 32768))
   + t over its slots k (a_k its weight / 128, row and t the view matrix's w row), in float as xc_zr_lit's (it differs from the exact
   form's w by rounding only, far less than the depth map's margin, gelist.h gx_depth_map).  vc_p07_c1b computes it as it writes the
   vertices (c1opt; xc_lit_c1 gives it each bone's w row, xc_c1_zf), this merges the call's range [lo, hi] of its cnt vertices in front.
   (A bound by each bone over the box of the call's positions was 18% of the map's width too wide in most face frames on x86: the
   faces' depth precision, so it is not used) */
static void xc_c1_zf(const VcXform *x, const GerC1 *g, float zf[8][4])
{
  const float a = x->world[2], b = x->world[5], c = x->world[8];
  for (int k = 0; k < (int)g->nw; k++) {   /* bone k's w row, for p as the stream holds it and a weight as a whole number */
    const float *m = g->bone[k];
    for (int col = 0; col < 3; col++) zf[k][col] = (m[3 * col] * a + m[3 * col + 1] * b + m[3 * col + 2] * c) * (1.0f / (32768.0f * 128.0f));
    zf[k][3] = (m[9] * a + m[10] * b + m[11] * c) * (1.0f / 128.0f);
  }
}
/* c1opt2: 1 when every w of the call lies inside the range the depth range keeps so far for its projection, by a bound over the box of
   its positions and the least and largest sum of a vertex's weights (vc_p07_c1s): w - t = sum_k a_k (zf_k . (p, 1)) with a_k >= 0 summing
   to the vertex's S, each zf_k . (p, 1) inside [the least, the largest of that affine function over the box], so w - t inside S times
   [the least of those over the slots, the largest]; widened by 1e-5 of the magnitudes (float rounding of w and of this bound is below
   2e-6 of them).  Its exact w then cannot move the range and all lie in front (cnt n), so vc_p07_c1b need not compute them: the result
   is the same.  x86 codec route: 77% of the C1 calls (70% of their vertices) were inside by this bound, 93% by their exact w; the bound
   never missed an exact w.  c1opt3: the box as its centre q and half extents e (exact: halved sums of int16), so a slot's least and
   largest over it are d - r and d + r with d = zf . (q, 1) and r = |zf| . e (the same bound as c1opt2's min and max of the corners'
   products, without a branch per axis: 570 instructions a call on the PSP model before, 188 now, codec window).  A NaN or an infinity in any slot's
   terms makes chk not finite: no (and a NaN zt fails the tests) */
static int xc_c1_zin(const XcZr *z, const VcXform *x, const VcP7C1 *k, const float zf[8][4], float zt)
{
  if (!z->on || x->pa != z->pa || x->pb != z->pb || x->vs[2] != z->vs2 || x->vo[2] != z->vo2) return 0;
  const float q0 = 0.5f * (float)((int)k->blo[0] + k->bhi[0]), q1 = 0.5f * (float)((int)k->blo[1] + k->bhi[1]),
              q2 = 0.5f * (float)((int)k->blo[2] + k->bhi[2]);
  const float e0 = 0.5f * (float)((int)k->bhi[0] - k->blo[0]), e1 = 0.5f * (float)((int)k->bhi[1] - k->blo[1]),
              e2 = 0.5f * (float)((int)k->bhi[2] - k->blo[2]);
  const float m0 = fabsf(q0) + e0, m1 = fabsf(q1) + e1, m2 = fabsf(q2) + e2;   /* the largest |p| on each axis */
  float mn = 3.0e38f, mx = -3.0e38f, big = 0, chk = 0;
  for (int j = 0; j < k->nw; j++) {
    const float *f = zf[j], a0 = fabsf(f[0]), a1 = fabsf(f[1]), a2 = fabsf(f[2]);
    const float d = f[3] + f[0] * q0 + f[1] * q1 + f[2] * q2, r = a0 * e0 + a1 * e1 + a2 * e2;
    const float a = fabsf(f[3]) + a0 * m0 + a1 * m1 + a2 * m2, dl = d - r, dh = d + r;
    mn = dl < mn ? dl : mn; mx = dh > mx ? dh : mx; big = a > big ? a : big; chk += (dh - dl) + a;
  }
  if (!(chk - chk == 0)) return 0;
  const float s0 = (float)k->smin, s1 = (float)k->smax, E = 1e-5f * (fabsf(zt) + s1 * big) + 1e-30f;
  const float blo = zt + (mn >= 0 ? s0 * mn : s1 * mn) - E, bhi = zt + (mx >= 0 ? s1 * mx : s0 * mx) + E;
  return blo > 0 && blo >= z->wmin && bhi <= z->wmax;
}
static void xc_zr_c1(XcZr *z, const VcXform *x, float lo, float hi, long cnt)
{
  if (!cnt) return;
  z->calls++; z->verts += cnt;
  if (z->on && x->pa == z->pa && x->pb == z->pb && x->vs[2] == z->vs2 && x->vo[2] == z->vo2) {
    if (lo < z->wmin) z->wmin = lo;
    if (hi > z->wmax) z->wmax = hi;
    return;
  }
  xc_zr_end(z);
  z->pa = x->pa; z->pb = x->pb; z->vs2 = x->vs[2]; z->vo2 = x->vo[2]; z->wmin = lo; z->wmax = hi; z->on = 1;
}
/* -8: today's path takes the call (vc_p07_c1 -8 or -9, vc_p07_c1s or vc_p07_c1b -8, a bone p7_loop would not take), counted in St.c1_fall;
   -5 no room;
   0 nothing drawn; 1 the batch (pass 1's), as xc_lit */
static int xc_lit_c1(const XlCall *c, const XcHost *h, const XcGs *d, int ti, const XcTex *t, GerBatch *b, uint8_t *blob, uint32_t cap,
                     uint32_t *size, uint32_t fogcol, uint32_t hy, uint16_t *fl, uint16_t *runs)
{
  const XlRec *rr = c->rec; VcLit L; VcP7C1 k;
  const int n = vc_p07_c1(c->vm, c->top, rr->p, rr->kind, rr->num, &L, &k);
  if (n < 0) { St.c1_fall[n == -9]++; return -8; }
  const int np = 1 + ((hy & 0x10) != 0) + ((hy & 0x700) != 0);
  if (cap < xc_c1_bound((uint32_t)n, hy)) return -5;
  const uint32_t o1 = C1_GXL(np), ov = (o1 + 15u) & ~15u;
  if (vc_p07_c1s(rr->p, &L, &k) < 0) { St.c1_fall[2]++; return -8; }
  const uint32_t a1 = ((uint32_t)(n * k.stride) + 15u) & ~15u, og = ov + (L.uv2 == 2 ? 2 * a1 : a1), gsz = GERC1_SIZE(k.nw);
  GerC1 *g = (GerC1 *)(blob + og);
  for (int j = 0; j < k.nw; j++) if (!vc_p07_bone(c->vm, 32u + 4u * k.slot[j], g->bone[j])) { St.c1_fall[3]++; return -8; }
  g->nw = (uint32_t)k.nw;
  xc_state(d, ti, c->entry, b);   /* c1opt2: here (it sets only b), as its flags say whether the depth range takes the call */
  const int zneed = h->zr && (b->flags & GERF_ZWRITE) && b->ztst >= 2;
  float zf[8][4], zw[2] = { 0, 0 }; long zn = 0; int zin = 0;   /* the depth range as the vertices are written (xc_zr_c1) */
  if (zneed) { xc_c1_zf(&L.x, g, zf); zin = xc_c1_zin(h->zr, &L.x, &k, zf, L.x.world[11]); }
  if (vc_p07_c1b(rr->p, &L, &k, blob + ov, fl, zneed && !zin ? (const float (*)[4])zf : 0, L.x.world[11], zw, &zn) < 0) { St.c1_fall[2]++; return -8; }
  g->voff[0] = g->voff[2] = (int32_t)ov - (int32_t)og; g->voff[1] = L.uv2 == 2 ? (int32_t)(ov + k.v2) - (int32_t)og : g->voff[0];
  memcpy(g->lcol, k.lcol, sizeof g->lcol); g->amb = k.amb;
  memcpy(g->ldir, k.ldir, sizeof g->ldir); memcpy(g->tgen, k.tgen, sizeof g->tgen);
  const int nst = vc_strips(fl, n, runs, 128);
  if (!nst) return 0;
  const uint32_t orun = (og + gsz + 15u) & ~15u, end = orun + (uint32_t)nst * 4u;
  memset(blob + og + gsz, 0, orun - og - gsz); memcpy(blob + orun, runs, (size_t)nst * 4);
  GerXform gx; xc_xform(&L.x, d, t, 8, h, fogcol, &gx);   /* (xc_lit's: UV1 16-bit) */
  if (L.fog >= 0) { gx.fog1 = (float)L.fog / 255.0f * 1099511627776.0f; gx.fog2 = 1.0f / 1099511627776.0f; }
  gx.nv = (uint32_t)n; gx.nst = (uint32_t)nst; gx.nidx = 0;
  memcpy(blob, &gx, sizeof gx);
  memset(blob + o1, 0, ov - o1);
  memset(&Lp, 0, sizeof Lp);
  int kk = 0;
  for (int pass = 1; pass <= 3; pass++) {
    if ((pass == 2 && !(hy & 0x10)) || (pass == 3 && !(hy & 0x700))) continue;
    const uint32_t at = (uint32_t)(sizeof gx + kk++ * sizeof(GerLit));
    GerLit p; memset(&p, 0, sizeof p);
    p.xf = -(int32_t)at; p.vtx = (int32_t)og - (int32_t)at; p.run = (int32_t)orun - (int32_t)at;
    p.vtype = VT_C1(k.nw); p.stride = (uint32_t)k.stride; p.nv = (uint32_t)n; p.nst = (uint32_t)nst; p.pass = (uint32_t)pass | GERL_C1;
    p.su = gx.su; p.sv = gx.sv; p.tu = gx.tu; p.tv = gx.tv;   /* pass 1's; xc_pass writes the others' */
    memcpy(blob + at, &p, sizeof p);
    if (pass == 1 || (pass == 2 && L.uv2 > 0) || (pass == 3 && L.p3 > 0)) Lp.rec[pass] = at;
    Lp.uvk[pass] = pass == 3 ? 1 : 8; Lp.du[pass] = pass == 2 ? L.du : 0; Lp.dv[pass] = pass == 2 ? L.dv : 0;   /* (pass 3: float UVs' scale,
                                                    which the list folds into the texture matrix) */
  }
  Lp.on = 1; Lp.entry = c->entry; Lp.top = c->top; Lp.amin = L.amin;
  b->kind = GERK_LIT; b->twopass = (uint8_t)xc_twopass(d, t, L.amin); b->count = (uint32_t)nst;   /* (xc_state above) */
  b->first = (uint32_t)sizeof gx;
  if (h->zr && (b->flags & GERF_ZWRITE)) {
    if (b->ztst < 2) h->zr->always++;
    else if (zin) { xc_zr_c1(h->zr, &L.x, h->zr->wmin, h->zr->wmax, n); St.c1_zin++; St.c1_zin_v += n; }   /* (the range unchanged) */
    else xc_zr_c1(h->zr, &L.x, zw[0], zw[1], zn);
  }
  St.face_v += n; St.face_nopass[2] += L.uv2 < 0; St.face_own2 += L.uv2 == 2; St.face_nopass[3] += L.p3 < 0;
  St.c1_calls++; St.c1_v += n; St.c1_nw[k.nw]++; St.c1_bytes += end;
  *size = end; return 1;
}
/* ---- sb12 C2 (stagec_notes.md step 2; xconv.h XcHost.c2): a lane 2 P06 call drawn by the GE's own skinning (the skinned entries: two
   bones), lighting and (pass 3) texture matrix, by mapping (b).  Its blob: [GerXform][a GerLit a pass][pass 1's vertices][pass 2's own
   (uv2 2)][GerC2][strips], each part from a 16-byte boundary, the pads zeroed; each GerLit's vtx is the GerC2 (ge/replay_fmt.h).  Pass 3
   draws pass 1's vertices (its texture coordinates come from the texture matrix) ---- */
#define VT_C2(skin) ((skin) ? 0x455eu : 0x15eu)   /* GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_NORMAL_16BIT | GU_VERTEX_16BIT, skinned with
                                                    GU_WEIGHT_16BIT | GU_WEIGHTS(2) */
static uint32_t xc_c2_bound(uint32_t n, uint32_t hy)   /* (a vertex at most 24 bytes; passes 1 and 2's; the strips at most n) */
{
  const uint32_t np = 1 + ((hy & 0x10) != 0) + ((hy & 0x700) != 0);
  return ((C1_GXL(np) + 15u) & ~15u) + 2 * ((n * 24u + 15u) & ~15u) + (((uint32_t)sizeof(GerC2) + 15u) & ~15u) + n * 4u;
}
uint32_t xc_c2_hold_bound(const XlCall *c, uint32_t bound)
{
  const uint32_t *hd = vq(c->vm, c->top + 0x14), b2 = xc_c2_bound(hd[0] & 0xffff, hd[1] & 0xffff);
  return b2 > bound ? b2 : bound;
}
/* 1 when every w of a skinned call lies inside the range the depth range keeps so far for its projection, by a bound over the box of its
   positions (vc_p06_c2box), as xc_c1_zin: w = a . q + t with q = p w + (B3 p + tB)(1 - w) (p6_loop's blend, the weight w in 0..1), so it
   lies between f0(p) = a . p + t and f1(p) = c . p + c3 + t (c_j = a . row j of B, c3 = a . tB), each inside [d - r, d + r] over the box
   (d at its centre, r = |coefficients| . half extents); widened by 1e-5 of the magnitudes, big (the float rounding of today's w, about 10
   steps of 2^-23 of them, and of this bound).  Its exact w then cannot move the range and all lie in front (count n), so vc_p06_c2v need
   not compute them: the result is the same.  A NaN or an infinity makes big not finite: no */
static int xc_c2_zin(const XcZr *z, const VcXform *x, const VcP6C2 *k, const float *zr)
{
  if (!z->on || x->pa != z->pa || x->pb != z->pb || x->vs[2] != z->vs2 || x->vo[2] != z->vo2) return 0;
  const float q0 = 0.5f * (float)((int)k->blo[0] + k->bhi[0]), q1 = 0.5f * (float)((int)k->blo[1] + k->bhi[1]),
              q2 = 0.5f * (float)((int)k->blo[2] + k->bhi[2]);
  const float e0 = 0.5f * (float)((int)k->bhi[0] - k->blo[0]), e1 = 0.5f * (float)((int)k->bhi[1] - k->blo[1]),
              e2 = 0.5f * (float)((int)k->bhi[2] - k->blo[2]);
  const float m0 = fabsf(q0) + e0, m1 = fabsf(q1) + e1, m2 = fabsf(q2) + e2;   /* the largest |p| on each axis */
  const float a0 = zr[0], a1 = zr[1], a2 = zr[2], t = zr[3];
  float c[4], mb[3];
  for (int j = 0; j < 4; j++) c[j] = a0 * k->b[j][0] + a1 * k->b[j][1] + a2 * k->b[j][2];
  for (int r = 0; r < 3; r++) mb[r] = fabsf(k->b[0][r]) * m0 + fabsf(k->b[1][r]) * m1 + fabsf(k->b[2][r]) * m2 + fabsf(k->b[3][r]);   /* the
                                                    largest |B3 p + tB| on each axis */
  const float d0 = t + a0 * q0 + a1 * q1 + a2 * q2, r0 = fabsf(a0) * e0 + fabsf(a1) * e1 + fabsf(a2) * e2;
  const float d1 = t + c[3] + c[0] * q0 + c[1] * q1 + c[2] * q2, r1 = fabsf(c[0]) * e0 + fabsf(c[1]) * e1 + fabsf(c[2]) * e2;
  const float big = fabsf(t) + fabsf(a0) * (m0 + mb[0]) + fabsf(a1) * (m1 + mb[1]) + fabsf(a2) * (m2 + mb[2]);
  if (!(big - big == 0) || !(d0 - d0 == 0) || !(d1 - d1 == 0) || !(r0 - r0 == 0) || !(r1 - r1 == 0)) return 0;
  const float E = 1e-5f * big + 1e-30f;
  const float lo = (d0 - r0 < d1 - r1 ? d0 - r0 : d1 - r1) - E, hi = (d0 + r0 > d1 + r1 ? d0 + r0 : d1 + r1) + E;
  return lo > 0 && lo >= z->wmin && hi <= z->wmax;
}
/* -8: today's path takes the call (vc_p06_c2 -8 or -9, vc_p06_c2v -8, or less room than its bound: counted in St.c2_fall); 0 nothing
   drawn; 1 the batch (pass 1's), as xc_lit */
static int xc_lit_c2(const XlCall *c, const XcHost *h, const XcGs *d, int ti, const XcTex *t, GerBatch *b, uint8_t *blob, uint32_t cap,
                     uint32_t *size, uint32_t fogcol, uint32_t hy, uint16_t *fl, uint16_t *runs)
{
  const XlRec *rr = c->rec; VcLit L; VcP6C2 k;
  const int n = vc_p06_c2(c->vm, c->top, c->entry, rr->p, rr->kind, rr->num, rr->row, &L, &k);
  if (n < 0) { St.c2_fall[n == -9]++; return -8; }
  const int np = 1 + ((hy & 0x10) != 0) + ((hy & 0x700) != 0);
  if (cap < xc_c2_bound((uint32_t)n, hy)) { St.c2_fall[3]++; return -8; }
  const uint32_t o1 = C1_GXL(np), ov = (o1 + 15u) & ~15u, st = k.skin ? 24u : 20u, a1 = ((uint32_t)n * st + 15u) & ~15u;
  xc_state(d, ti, c->entry, b);   /* (here, as xc_lit_c1: its flags say whether the depth range takes the call) */
  const int zneed = h->zr && (b->flags & GERF_ZWRITE) && b->ztst >= 2;
  float zr[4] = { 0, 0, 0, 0 }, zw[2] = { 0, 0 }; long zn = 0; int zin = 0;
  if (zneed) {   /* xc_zr_lit's w row: positions as the GE reads them, 16-bit ones / 32768 (unskinned), floats (skinned: the blend) */
    const float ps = k.skin ? 1.0f : 32768.0f;
    zr[0] = L.x.world[2] / ps; zr[1] = L.x.world[5] / ps; zr[2] = L.x.world[8] / ps; zr[3] = L.x.world[11];
    if (k.skin) { vc_p06_c2box(rr->p, n, &k); zin = xc_c2_zin(h->zr, &L.x, &k, zr); }
  }
  if (vc_p06_c2v(rr->p, &L, &k, blob + ov, fl, zneed && !zin ? zr : 0, zw, &zn) < 0) { St.c2_fall[2]++; return -8; }
  St.lit_kind[L.kind]++; St.lit_v += n; St.lit_cull += L.cull;   /* (xc_lit's counts: every C2 call is in the fast form, no ramp) */
  if (L.uv2 == 2) St.lit_own2++;
  if (L.p3 < 0) St.lit_nopass[3]++;
  const uint32_t og = ov + (L.uv2 == 2 ? 2 * a1 : a1), gsz = GERC2_SIZE(k.skin);
  GerC2 *g = (GerC2 *)(blob + og);
  g->voff[0] = g->voff[2] = (int32_t)ov - (int32_t)og; g->voff[1] = L.uv2 == 2 ? (int32_t)(ov + a1) - (int32_t)og : g->voff[0];
  g->skin = (uint32_t)k.skin; memcpy(g->lcol, k.lcol, sizeof g->lcol); g->emis = k.emis;
  memcpy(g->ldir, k.ldir, sizeof g->ldir); memcpy(g->tgen, k.tgen, sizeof g->tgen);
  if (k.skin) memcpy(g->bone1, k.bone1, sizeof g->bone1);
  const int nst = vc_strips(fl, n, runs, 128);
  if (!nst) return 0;
  const uint32_t orun = (og + gsz + 15u) & ~15u, end = orun + (uint32_t)nst * 4u;
  memset(blob + og + gsz, 0, orun - og - gsz); memcpy(blob + orun, runs, (size_t)nst * 4);
  GerXform gx; xc_xform(&L.x, d, t, 8, h, fogcol, &gx);   /* (xc_lit's: UV1 16-bit) */
  gx.fog1 = (float)L.fog / 255.0f * 1099511627776.0f; gx.fog2 = 1.0f / 1099511627776.0f;   /* (xc_lit's: the constant F) */
  gx.nv = (uint32_t)n; gx.nst = (uint32_t)nst; gx.nidx = 0;
  memcpy(blob, &gx, sizeof gx);
  memset(blob + o1, 0, ov - o1);
  memset(&Lp, 0, sizeof Lp);
  int kk = 0;
  for (int pass = 1; pass <= 3; pass++) {
    if ((pass == 2 && !(hy & 0x10)) || (pass == 3 && !(hy & 0x700))) continue;
    const uint32_t at = (uint32_t)(sizeof gx + kk++ * sizeof(GerLit));
    GerLit p; memset(&p, 0, sizeof p);
    p.xf = -(int32_t)at; p.vtx = (int32_t)og - (int32_t)at; p.run = (int32_t)orun - (int32_t)at;
    p.vtype = VT_C2(k.skin); p.stride = st; p.nv = (uint32_t)n; p.nst = (uint32_t)nst; p.pass = (uint32_t)pass | GERL_C2;
    p.su = gx.su; p.sv = gx.sv; p.tu = gx.tu; p.tv = gx.tv;   /* pass 1's; xc_pass writes the others' */
    memcpy(blob + at, &p, sizeof p);
    if (pass == 1 || (pass == 2 && L.uv2 > 0) || (pass == 3 && L.p3 > 0)) Lp.rec[pass] = at;
    Lp.uvk[pass] = pass == 3 ? 1 : 8; Lp.du[pass] = pass == 2 ? L.du : 0; Lp.dv[pass] = pass == 2 ? L.dv : 0;   /* (pass 3: float UVs'
                                                    scale, today's, which the list folds into the texture matrix) */
  }
  Lp.on = 1; Lp.entry = c->entry; Lp.top = c->top; Lp.amin = L.amin;
  b->kind = GERK_LIT; b->twopass = (uint8_t)xc_twopass(d, t, L.amin); b->count = (uint32_t)nst;   /* (xc_state above) */
  b->first = (uint32_t)sizeof gx;
  if (h->zr && (b->flags & GERF_ZWRITE)) {
    if (b->ztst < 2) h->zr->always++;
    else if (zin) { xc_zr_c1(h->zr, &L.x, h->zr->wmin, h->zr->wmax, n); St.c2_zin++; St.c2_zin_v += n; }   /* (the range unchanged) */
    else xc_zr_c1(h->zr, &L.x, zw[0], zw[1], zn);
  }
  St.c2_calls++; St.c2_v += n; St.c2_skin += k.skin; St.c2_bytes += end;
  *size = end; return 1;
}
static int xc_lit(const XlCall *c, const XcHost *h, const XcGs *d, int ti, const XcTex *t, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size,
                  uint32_t fogcol)
{
  static uint8_t v3[64 * 24]; static uint16_t fl[64], runs[128];
  const int p7 = c->entry == VC_P07;
  const uint8_t *vm = c->vm; uint32_t top = c->top;   /* sb4: a held call's rebuilt memory after vc_p06_rec's -8 */
  const uint32_t *hd = vq(c->vm, c->top + (p7 ? 0 : 0x14));
  uint32_t hy = (p7 ? hd[3] : hd[1]) & 0xffff, n0 = hd[0] & 0xffff;   /* the count and the pass flags: P06 T+0x14 x, y; P07 T+0 x, w */
  int np = 1 + ((hy & 0x10) != 0) + ((hy & 0x700) != 0);   /* a record for each pass the program draws */
  uint32_t o1 = (uint32_t)(sizeof(GerXform) + np * sizeof(GerLit));
  if (n0 <= 64 && cap < o1 + n0 * (24 + 24 + 24 + 4)) return -5;
  if (p7 && h->c1 && c->rec && c->rec->lane == 4 && !(Hr && xc_held_force)) {   /* sb11 C1: the GE's skinning and lighting, where it can */
    const int r = xc_lit_c1(c, h, d, ti, t, b, blob, cap, size, fogcol, hy, fl, runs);
    if (r != -8) return r;
  }
  if (!p7 && h->c2 && c->rec && c->rec->lane == 2 && !(Hr && xc_held_force)) {   /* sb12 C2: the GE's skinning and lighting, where it can */
    const int r = xc_lit_c2(c, h, d, ti, t, b, blob, cap, size, fogcol, hy, fl, runs);
    if (r != -8) return r;
  }
  VcLit L; int n; const XlRec *rr = 0;   /* sb2: a lane's streams, when the call's vertices come from them */
  if (p7 && c->rec && c->rec->lane == 4) {   /* sb5 lane 4: the vertices from the slot's streams, else the VU memory decoded */
    rr = c->rec;
    n = Hr && xc_held_force ? -8 : vc_p07_rec(c->vm, c->top, rr->p, rr->kind, rr->num, &L, blob + o1, v3, fl, 0, 64);
    if (n == -8) {   /* sb10 T2: a held call's vertices are rebuilt in the scratch memory around its placed record (Sv at T 64) */
      rr = 0; xc_rec7_vm++;
      if (Hr) sv7_rebuild(Hr); else xl_vm_sync();
      n = vc_p07(c->vm, c->top, &L, blob + o1, v3, fl, 0, 64); }
    else {
      xc_rec7_calls++;
#ifndef __psp__   /* (x86 only: no room taken on the PSP for the check's buffers) */
      if (xc_rec_chk) {   /* the same call on the VU memory, decoded for it: every output the same */
        static uint8_t e1[64 * 24], e2r[64 * 24], e2v[64 * 24], e3[64 * 24]; static uint16_t ef[64]; VcLit LE;
        if (Hr) sv7_rebuild(Hr); else xl_vm_sync();   /* (sb10 T2: a held call's VU memory is Sv, its vertices rebuilt) */
        int ne = vc_p07(c->vm, c->top, &LE, e1, e3, ef, 0, 64); xc_rec7_n++;
        int d = ne != n;
        if (!d && n > 0) {
          d = memcmp(&LE, &L, sizeof L) || memcmp(e1, blob + o1, (size_t)vc_p06_size1(&L) * (size_t)n) || memcmp(ef, fl, 2 * (size_t)n) ||
              (L.p3 > 0 && memcmp(e3, v3, (size_t)vc_p06_size3(&L) * (size_t)n));
          if (!d && L.uv2 == 2) {
            int tr = vc_p07_uv2_rec(rr->p[3], &L, blob + o1, e2r), tv = vc_p06_uv2(c->vm, c->top, &L, blob + o1, e2v);
            d = tr != tv || memcmp(e2r, e2v, (size_t)(16 + 4 * ((tr & 1) + (tr >> 1))) * (size_t)n);
          }
        }
        xc_rec7_diff += d;
      }
#endif
    }
  } else if (p7) n = vc_p07(c->vm, c->top, &L, blob + o1, v3, fl, 0, 64);
  else if (c->rec && c->rec->lane == 2) {   /* sb2 lane 2: the vertices from the game's data, else the VU memory decoded */
    rr = c->rec;
    n = Hr && xc_held_force ? -8 : vc_p06_rec(c->vm, c->top, c->entry, rr->p, rr->kind, rr->num, rr->row, &L, blob + o1, v3, fl, 0, 64);
    if (n == -8) {   /* sb4: a held call's VU memory is rebuilt in the scratch memory at its own T (the real one has moved on) */
      rr = 0; xc_rec6_vm++;
      if (Hr) { sv_rebuild(Hr); vm = Sv; top = Hr->top; } else xl_vm_sync();
      n = vc_p06(vm, top, c->entry, &L, blob + o1, v3, fl, 0, 64); }
    else {
      xc_rec6_calls++;
      if (xc_rec_chk) {   /* the same call on the VU memory, decoded for it: every output the same */
        static uint8_t e1[64 * 24], e2r[64 * 24], e2v[64 * 24], e3[64 * 24]; static uint16_t ef[64]; VcLit LE;
        const uint8_t *em = vm; uint32_t et = top;
        if (Hr) { sv_rebuild(Hr); em = Sv; et = Hr->top; } else xl_vm_sync();
        int ne = vc_p06(em, et, c->entry, &LE, e1, e3, ef, 0, 64); xc_rec6_n++;
        int d = ne != n;
        if (!d && n > 0) {
          d = memcmp(&LE, &L, sizeof L) || memcmp(e1, blob + o1, (size_t)vc_p06_size1(&L) * (size_t)n) || memcmp(ef, fl, 2 * (size_t)n) ||
              (L.p3 > 0 && memcmp(e3, v3, (size_t)vc_p06_size3(&L) * (size_t)n));
          if (!d && L.uv2 == 2) {
            int tr = vc_p06_uv2_rec(rr->p[3], &L, blob + o1, e2r), tv = vc_p06_uv2(em, et, &L, blob + o1, e2v);
            d = tr != tv || memcmp(e2r, e2v, (size_t)(16 + 4 * ((tr & 1) + (tr >> 1))) * (size_t)n);
          }
        }
        xc_rec6_diff += d;
      }
    }
  } else n = vc_p06(c->vm, c->top, c->entry, &L, blob + o1, v3, fl, 0, 64);
  if (n < 0) { St.face_unseen += n == -7; return n; }
#ifndef __psp__
  if (p7 && c1m_on) c1m_call(c->vm, c->top, rr ? rr->p : 0, n, &L, blob + o1, L.p3 > 0 ? v3 : 0);   /* sb11: reads only */
  if (!p7 && c2m_on) c2m_call(vm, top, c->entry, rr ? rr->p : 0, n, &L, blob + o1, L.p3 > 0 ? v3 : 0);   /* sb12: reads only */
#endif
  if (p7) {
    St.face_v += n; St.face_exact += !L.fast; St.face_nopass[2] += L.uv2 < 0; St.face_own2 += L.uv2 == 2; St.face_nopass[3] += L.p3 < 0;
#ifdef XC_P7CHK
    if (L.fast) {   /* the call again in the exact form, compared byte for byte (as tools/dev/p07chk.c) */
      static uint8_t e1[64 * 24], e3[64 * 24]; static uint16_t ef[64]; VcLit LE;
      if (rr) { if (Hr) sv7_rebuild(Hr); else xl_vm_sync(); }   /* (sb5: a lane 4 call's vertices are still waiting in the VU memory
                                                                     model; sb10 T2: a held call's are rebuilt in Sv) */
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
  if (L.uv2 == 2) {   /* 16, 20, 20, 24 bytes */
    t2 = rr ? (p7 ? vc_p07_uv2_rec(rr->p[3], &L, blob + o1, blob + ov2) : vc_p06_uv2_rec(rr->p[3], &L, blob + o1, blob + ov2)) :
         vc_p06_uv2(vm, top, &L, blob + o1, blob + ov2);
    s2 = 16 + 4 * ((t2 & 1) + (t2 >> 1)); }
  uint32_t ov3 = ov2 + n * s2, orun = ov3 + (L.p3 > 0 ? n * s3 : 0), end = orun + nst * 4;
  if (L.p3 > 0) memcpy(blob + ov3, v3, n * s3);
  memcpy(blob + orun, runs, nst * 4);
  GerXform gx; xc_xform(&L.x, d, t, L.uv16 ? 8 : 1, h, fogcol, &gx);
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
#ifndef __psp__
  if (!p7 && c2m_on) c2m_pass(1, d->tme, d->tfx, d->tcc, d->abe, (uint32_t)(d->ba | d->bb << 2 | d->bc << 4 | d->bd << 6 | d->fix << 8), d->fge, ti >= 0);   /* sb12 */
#endif
  if (h->zr && (b->flags & GERF_ZWRITE)) { if (b->ztst >= 2) xc_zr_lit(h->zr, &L.x, blob + o1, t1, s1, n); else h->zr->always++; }
  *size = end; return 1;
}
/* a pass's GS state and texture, with the GS registers after its A+D packet (sb4: xc_pass's look-up, which a held call does at the pass) */
static void xc_pass_look(const XlCall *c, const XcHost *h, XcGs *d, XcTex *t)
{
  xc_gs(c->gr, d);
  memset(t, 0, sizeof *t); t->index = -1;
  if (d->tme && h->tex && !ME_CALLP(h->tex)(h->ctx, c->gr, d->ctxt, t)) t->index = -1;
}
/* the pass's batch from them (Lp: its call's xc_lit), its texture scale written into the call's blob */
static void xc_pass_do(const XcGs *d, const XcTex *t, int pass, uint32_t entry, GerBatch *b, uint8_t *blob)
{
  GerLit p; memcpy(&p, blob + Lp.rec[pass], sizeof p);
  float ts[4];
  xc_lit_ts(d, t, Lp.uvk[pass], Lp.du[pass], Lp.dv[pass], ts);
  p.su = ts[0]; p.sv = ts[1]; p.tu = ts[2]; p.tv = ts[3];
  memcpy(blob + Lp.rec[pass], &p, sizeof p);
  xc_state(d, t->index, entry, b); b->kind = GERK_LIT; b->twopass = (uint8_t)xc_twopass(d, t, Lp.amin); b->count = p.nst;
  b->first = Lp.rec[pass];
  if (Lp.entry == VC_P07) St.face_pass[pass]++; else St.lit_pass[pass]++;
#ifndef __psp__
  if (Lp.entry != VC_P07 && c2m_on) c2m_pass(pass, d->tme, d->tfx, d->tcc, d->abe, (uint32_t)(d->ba | d->bb << 2 | d->bc << 4 | d->bd << 6 | d->fix << 8), d->fge, t->index >= 0);   /* sb12 */
#endif
}
int xc_pass(const XlCall *c, int pass, const XcHost *h, GerBatch *b, uint8_t *blob)
{
  if (!Lp.on || pass < 2 || pass > 3 || c->entry != Lp.entry || c->top != Lp.top) return 0;
  if (!Lp.rec[pass]) return 0;
  XcGs d; XcTex t; xc_pass_look(c, h, &d, &t);
  xc_pass_do(&d, &t, pass, c->entry, b, blob);
  return 1;
}


int xc_call(const XlCall *c, const XcHost *h, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size)
{
  Lp.on = 0;
  int k = c->entry == XL_DF0 ? 0 : c->entry == XL_FX ? 1 : c->entry == XL_FX2 ? 2 : is_p06(c->entry) ? 3 : c->entry == VC_P07 ? 4 : -1;
  if (k < 0 && h->c4 && !c->cont && (c->entry == VC_SPR || c->entry == VC_QUAD || c->entry == VC_LINE)) return xc_c4(c, h, b, blob, cap, size);   /* sb15 */
  if (k < 0 || c->cont) return 0;
  St.calls[k]++;
#ifndef __psp__
  if (c3m_on) c3m_kind(k);   /* sb13 */
#endif
  XcGs d; xc_gs(c->gr, &d);
  if (!k) {   /* df0: the input first, so that a call off the scissor never asks for its texture */
    Datt = 1; Dbox_ok = 0; Dvp = Dv;   /* sb14 */
    if (c->rec) {   /* sb1 lane: the streams from the game's data */
      const XlRec *r = c->rec; xc_rec_calls++;
      if (h->c3v) {   /* sb14 3v: the positions and their box now, the rest once the cut has kept the call */
        if (h->c3w && cap >= sizeof(GerXform) + sizeof Dv) { Dvp = (VcVert *)(blob + sizeof(GerXform)); St.c3w_calls++; }   /* sb14 3w */
        Dn = vc_df0_pos(c->vm, c->top, r->p, r->kind, r->row, &Dx, Dvp, Dbox, 1024); Dbox_ok = Dn > 0; Datt = 0; Drec = r; St.c3v_calls++;
#ifndef __psp__   /* (x86 only: no check arrays or code in the PSP's hot path) */
        if (xc_c3_chk) {   /* the two passes against vc_df0_rec: every output the same; the flags read from the stream the same as converted */
          static VcVert Ev[1024]; static uint16_t Efl[1024]; VcXform Ex; int16_t eb[6];
          int En = vc_df0_rec(c->vm, c->top, r->p, r->kind, r->row, &Ex, Ev, Efl, 1024), bad = En != Dn || memcmp(&Ex, &Dx, sizeof Ex);
          if (Dn > 0) {
            vc_df0_att(r->p, r->kind, r->row, &Dx, Dvp, Dfl, Dn);
            bad |= memcmp(Ev, Dvp, (size_t)Dn * sizeof *Ev) || memcmp(Efl, Dfl, (size_t)Dn * 2);
            for (int i = 0; i < Dn; i++) bad |= vc_df0_flag(r->p, r->kind, r->row, i) != Dfl[i];
            eb[0] = eb[1] = eb[2] = 32767; eb[3] = eb[4] = eb[5] = -32768;
            for (int i = 0; i < Dn; i++) {
              eb[0] = Ev[i].x < eb[0] ? Ev[i].x : eb[0]; eb[1] = Ev[i].y < eb[1] ? Ev[i].y : eb[1]; eb[2] = Ev[i].z < eb[2] ? Ev[i].z : eb[2];
              eb[3] = Ev[i].x > eb[3] ? Ev[i].x : eb[3]; eb[4] = Ev[i].y > eb[4] ? Ev[i].y : eb[4]; eb[5] = Ev[i].z > eb[5] ? Ev[i].z : eb[5];
            }
            bad |= memcmp(eb, Dbox, sizeof eb) != 0;
          }
          St.c3_chk++; St.c3_chk_bad += bad;
        }
#endif
      } else
      Dn = vc_df0_rec(c->vm, c->top, r->p, r->kind, r->row, &Dx, Dv, Dfl, 1024);
      if (xc_rec_chk) {
        if (!Datt && Dn > 0) vc_df0_att(r->p, r->kind, r->row, &Dx, Dvp, Dfl, Dn);   /* sb14: the whole input for the check */
        Datt = 1;
        static VcVert Ev[1024]; static uint16_t Efl[1024]; VcXform Ex;
        xl_vm_sync(); int En = vc_df0(c->vm, c->top, &Ex, Ev, Efl, 1024); xc_rec_n++;
        if (En != Dn || memcmp(&Ex, &Dx, sizeof Ex) || (En > 0 && (memcmp(Ev, Dvp, (size_t)En * sizeof *Ev) || memcmp(Efl, Dfl, (size_t)En * 2)))) xc_rec_diff++;
      }
    } else
    Dn = vc_df0(c->vm, c->top, &Dx, Dv, Dfl, 1024);
    int off = Dn > 0 && h->cut && xc_df0_off(&d);
#ifndef __psp__
    if (c3m_on) {   /* sb13: reads only, before the depth range takes the call (sb14: with the whole input) */
      if (!Datt && Dn > 0) vc_df0_att(Drec->p, Drec->kind, Drec->row, &Dx, Dvp, Dfl, Dn);
      Datt = 1; c3m_call(c, &d, h, Dn, Dvp, Dfl, &Dx, off); }
#endif
    if (off) { if (!Datt) { St.c3v_cut++; St.c3v_cut_v += Dn; }
      xc_df0_zr(h, &d); St.cut[0]++; St.cut_v[0] += Dn; St.empty[0]++; return 0; }
    if (!Datt) { if (Dn > 0) vc_df0_att(Drec->p, Drec->kind, Drec->row, &Dx, Dvp, Dfl, Dn); Datt = 1; }   /* sb14 3v: the second pass */
  }
  XcTex t; memset(&t, 0, sizeof t); t.index = -1;
  if (d.tme && h->tex && !ME_CALLP(h->tex)(h->ctx, c->gr, d.ctxt, &t)) t.index = -1;
  int r = k >= 3 ? xc_lit(c, h, &d, t.index, &t, b, blob, cap, size, (uint32_t)(c->gr[0x3d] & 0xffffff)) :
          k ? xc_fpos(c, h, &d, t.index, &t, k == 2, b, blob, cap, size) : xc_df0(c, h, &d, t.index, &t, b, blob, cap, size);
  if (r < 0) St.bad[k]++; else if (!r) St.empty[k]++; else St.batches[k]++;
  return r;
}

/* ---- sb4, step c (xconv.h; renderer_notes 11.29): lane 2's P06 calls held to the end of their transfer ---- */
int xc_held_force, xc_held_poison;
static int hq_skin(uint32_t e) { return e == VC_P06S || e == VC_P06ST; }
static int hq_tint(uint32_t e) { return e == VC_P06T || e == VC_P06ST; }
static int hq_p3(uint32_t hy) { return (hy & 0x600) && !(hy & 0x100); }   /* pass 3 drawn (xc_lit's p3 > 0) */
/* the quadwords a held call keeps (what p06_main, p06_any, vc_xform and xc_lit read besides the vertices) and XcHeld.img's places for them:
   absolute fog (2), alpha and clamp (3), the viewport (4, 5) in img[0 .. 3]; T-relative the matrix (0..3), the lights (4..6), the colours
   (8..0xb), the skinned entries' skin matrix (0xc..0xf), pass 3's matrix (0x10..0x12, when drawn), the header (0x14) in img[4 + k]; pass 3's
   scale and offset (T + 0xfe, 0xff) in img[25], img[26].  hi_io dir 0: from the VU memory vm (a call at T top) into the image; 1: back */
static inline __attribute__((always_inline)) void hi_run(uint32_t (*img)[4], uint8_t *vm, uint32_t qa, int i, int n, int dir)
{
  for (int j = 0; j < n; j++) {
    uint32_t *q = (uint32_t *)(vm + (((qa + (uint32_t)j) & 0x3ffu) << 4)), *m = img[i + j];
    if (dir) { q[0] = m[0]; q[1] = m[1]; q[2] = m[2]; q[3] = m[3]; } else { m[0] = q[0]; m[1] = q[1]; m[2] = q[2]; m[3] = q[3]; }
  }
}
static inline __attribute__((always_inline)) void hi_io(uint32_t (*img)[4], uint8_t *vm, uint32_t top, uint32_t entry, uint32_t hy, int dir)
{
  hi_run(img, vm, 2, 0, 4, dir);
  hi_run(img, vm, top, 4, 7, dir);
  hi_run(img, vm, top + 8, 12, hq_skin(entry) ? 8 : 4, dir);
  if (hq_p3(hy)) { hi_run(img, vm, top + 0x10, 20, 3, dir); hi_run(img, vm, top + 0xfe, 25, 2, dir); }
  hi_run(img, vm, top + 0x14, 24, 1, dir);
}
#define HI_T 6u   /* a held call converts at T 6: its VU memory is the image, img[a - 2] for address a (hi_vm), or Sv when pass 3 is drawn */
#define HI_T7 64u   /* sb10 T2: a held P07 call converts in Sv at T 64: its slots at abs 0 .. 63, T + 0 .. 3 at 64 .. 67, T + 0xfe, 0xff at
                       318, 319, the vertices (sv7_rebuild) from 320 on */
static const uint8_t *hi_vm(const XcHeld *r) { return (const uint8_t *)r + offsetof(XcHeld, img) - 32; }
static int hold7_test(const XlCall *c, uint32_t *bound, int *npd);
int xc_hold_test(const XlCall *c, uint32_t *bound, int *npd)
{
  const XlRec *r = c->rec;
  if (r && r->lane == 4) return !c->cont && c->entry == VC_P07 && !vc_p06_exact && hold7_test(c, bound, npd);   /* sb10 T2 */
  if (!r || r->lane != 2 || c->cont || !is_p06(c->entry) || vc_p06_exact) return 0;
  const uint32_t *h = vq(c->vm, c->top + 0x14);   /* (call() decoded T + 0 .. 0x14 for lane 2) */
  uint32_t n = h[0] & 0xffff, hy = h[1] & 0xffff;
  if (n < 1 || n > 64 || r->row[2] != 0x1000u) return 0;   /* ROW z: every UV z (else vc_p06_rec -8, then vc_p06 -6: bad input) */
#define HS(s, kd) (r->p[s] && r->kind[s] == (kd) && r->num[s] >= n)   /* vc_p06_rec's P6S */
  if (!HS(0, 2) || !HS(1, 2) || !HS(2, 4) || ((hy & 0x10) && !HS(3, 4)) || (hq_tint(c->entry) && !HS(5, 2))) return 0;
#undef HS
  if ((((h[2] & 0xffff) - 0x8000u) & 0x8000) != 0) return 0;   /* ADC on every vertex (XYZF2 w from the header's z): no strip */
  const xc_h16 *B = (const xc_h16 *)r->p[1]; uint32_t i = 2;
  while (i < n && (B[4 * i + 3] & 0x8000)) i++;   /* a vertex from the third on without ADC (stream B's w): a strip (vc_strips) */
  if (i >= n) return 0;
  *npd = ((hy & 0x10) != 0) + hq_p3(hy);
  *bound = (uint32_t)(sizeof(GerXform) + (size_t)(1 + ((hy & 0x10) != 0) + ((hy & 0x700) != 0)) * sizeof(GerLit)) + n * (24 + 24 + 24 + 4);
  return 1;
}
static void hold7(const XlCall *c, XcHeld *r);
void xc_hold(const XlCall *c, const XcHost *h, XcHeld *r)
{
  Lp.on = 0;   /* xc_call's work before xc_lit, in its order */
  if (c->entry == VC_P07) {   /* sb10 T2 */
    St.calls[4]++;
    xc_gs(c->gr, &r->d);
    memset(&r->t, 0, sizeof r->t); r->t.index = -1;
    if (r->d.tme && h->tex && !ME_CALLP(h->tex)(h->ctx, c->gr, r->d.ctxt, &r->t)) r->t.index = -1;
    r->entry = c->entry; r->top = c->top; r->hy = vq(c->vm, c->top)[3] & 0xffff; r->fogcol = (uint32_t)(c->gr[0x3d] & 0xffffff);
    r->rec = *c->rec; r->np = 0; r->slot = r->pslot[0] = r->pslot[1] = -1;
    hold7(c, r);
    return;
  }
  St.calls[3]++;
  xc_gs(c->gr, &r->d);
  memset(&r->t, 0, sizeof r->t); r->t.index = -1;
  if (r->d.tme && h->tex && !ME_CALLP(h->tex)(h->ctx, c->gr, r->d.ctxt, &r->t)) r->t.index = -1;
  r->entry = c->entry; r->top = c->top; r->hy = vq(c->vm, c->top + 0x14)[1] & 0xffff; r->fogcol = (uint32_t)(c->gr[0x3d] & 0xffffff);
  r->rec = *c->rec; r->np = 0; r->slot = r->pslot[0] = r->pslot[1] = -1;
  if (xc_held_poison) memset(r->img, 0xa5, sizeof r->img);
  hi_io(r->img, (uint8_t *)c->vm, r->top, r->entry, r->hy, 0);
}
int xc_hold_pass(const XlCall *c, int pass, const XcHost *h, XcHeld *r)
{
  /* xc_pass's Lp.rec[pass]: P06 as hq_p3 says; sb10 T2, P07: passes 2 and 3 both drawn when the header's flags have 0x210 (hold7_test
     takes no other P07 call that draws a pass 2 or 3: no UV2 z other than 1.0, no flag 0x100 or 0x400) */
  if (!(r->entry == VC_P07 ? (pass == 2 || pass == 3) && (r->hy & 0x210) == 0x210 :
        pass == 2 ? (r->hy & 0x10) != 0 : pass == 3 ? hq_p3(r->hy) : 0) || r->np >= 2) return 0;
  xc_pass_look(c, h, &r->pd[r->np], &r->pt[r->np]); r->pk[r->np] = pass; r->np++;
  return 1;
}
static uint8_t LpKeep[sizeof Lp];
static const uint8_t *C7Pl[XC_7S];   /* sb10 T2: the record data each P07 slot of Sv holds (0: not known) */
static void c7_log_reset(void);
void xc_held_begin(void) { memcpy(LpKeep, &Lp, sizeof Lp); memset(C7Pl, 0, sizeof C7Pl); }
void xc_held_end(void) { memcpy(&Lp, LpKeep, sizeof Lp); c7_log_reset(); }   /* (sb10 T2: the held calls have run, so their copies go) */
/* the VU memory P06 reads, as the real one held it at the call (at its own T): the streams' vectors at T + 0x100 + s + 6 i as the
   translator decodes them (A, B, F V4-16: four halfwords sign-extended; C, D V2-16 masked: x, y, ROW z, y), then the record's quadwords
   over them (the real memory's last writes: the header's UNPACK comes after the streams) */
static void sv_rebuild(const XcHeld *r)
{
  if (xc_held_poison) memset(Sv, 0xa5, sizeof Sv);
  const XlRec *R = &r->rec; uint32_t n = r->img[24][0] & 0xffff, a = r->top + 0x100, w[4];   /* img[24]: T + 0x14, the header */
  for (uint32_t i = 0; i < n; i++, a += 6) {
    for (int s = 0; s < 6; s++) {
      if (s == 4 || (s == 3 && !(r->hy & 0x10)) || (s == 5 && !hq_tint(r->entry))) continue;   /* E is not read; D for pass 2, F for the factor */
      if (s == 2 || s == 3) { const xc_h16 *q = (const xc_h16 *)R->p[s] + 2 * i;
        w[0] = (uint32_t)(int32_t)q[0]; w[1] = (uint32_t)(int32_t)q[1]; w[2] = R->row[2]; w[3] = (uint32_t)(int32_t)q[1]; }
      else { const xc_h16 *q = (const xc_h16 *)R->p[s] + 4 * i; for (int j = 0; j < 4; j++) w[j] = (uint32_t)(int32_t)q[j]; }
      memcpy(Sv + (((a + (uint32_t)s) & 0x3ff) << 4), w, 16);
    }
  }
  hi_io((uint32_t (*)[4])r->img, Sv, r->top, r->entry, r->hy, 1);
}
static void place7(const XcHeld *r);
int xc_held_run(const XcHeld *r, const XcHost *h, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size)
{
  XlCall c; memset(&c, 0, sizeof c);
  c.entry = r->entry; c.top = HI_T; c.rec = &r->rec; c.ad_qa = c.tag_qa = ~0u;   /* c.gr 0: xc_lit reads no GS register */
  if (r->entry == VC_P07) {   /* sb10 T2: the record placed in Sv at T 64 */
    place7(r); c.vm = Sv; c.top = HI_T7;
    Hr = r; Lp.on = 0;
    int k = xc_lit(&c, h, &r->d, r->t.index, &r->t, b, blob, cap, size, r->fogcol);
    Hr = 0;
    if (k < 0) St.bad[4]++; else if (!k) St.empty[4]++; else St.batches[4]++;
    return k;
  }
  memset(C7Pl, 0, sizeof C7Pl);   /* (sb10 T2: a P06 call may write Sv) */
  if (!hq_p3(r->hy)) c.vm = hi_vm(r);   /* the image is the VU memory at T 6 */
  else {   /* pass 3 reads T + 0xfe, 0xff as well: the image in the scratch memory at T 6 */
    if (xc_held_poison) memset(Sv, 0xa5, sizeof Sv);
    memcpy(Sv + 2 * 16, r->img, 25 * 16); memcpy(Sv + ((HI_T + 0xfe) << 4), r->img[25], 2 * 16);
    c.vm = Sv;
  }
  Hr = r; Lp.on = 0;
  int k = xc_lit(&c, h, &r->d, r->t.index, &r->t, b, blob, cap, size, r->fogcol);
  Hr = 0;
  if (k < 0) St.bad[3]++; else if (!k) St.empty[3]++; else St.batches[3]++;
  return k;
}
void xc_held_pass(const XcHeld *r, int k, GerBatch *b, uint8_t *blob) { xc_pass_do(&r->pd[k], &r->pt[k], r->pk[k], r->entry, b, blob); }

/* ---- sb10, Stage C step 1 T2 (stagec_notes.md, step 1): lane 4's P07 calls held to the end of their transfer, as step c holds lane 2's
   P06 calls.  A held call's record keeps where the 64 bytes of each slot of VU addresses 0 .. 63 that it reads were as the call saw them:
   the game's data a straight copy last put in the slot (ge/xlate.h xl_vsrc: valid to the transfer's end), else a copy in the log below
   (one copy for each slot and generation); and T + 0 .. 3, T + 0xfe, 0xff.  At the run the record is placed in Sv (a slot only when Sv
   does not hold that data already) with T at 64, and xc_lit converts it there: the same quadwords, so the same results.  The streams are
   the game's data, as for lane 2; vc_p07_rec's -8 rebuilds the vertices in Sv as the VU memory held them (sv7_rebuild).  hold7_test takes
   only a call that makes a batch and whose every read lies in its record: the header's count 1 .. 64, 1 .. 4 bones, no flag 0x804 or
   0x500 and passes 2 and 3 both or neither (0x210), T + 1 x zero (p07_any's -7 paths), the vertices at T + 0x100 .. on without a wrap
   (vc_p07_rec's test at the call's T), the streams vc_p07_rec reads, every UV1 z 1.0 (p07_any's -6) and with passes 2 and 3 every UV2 z
   (pass 2 drawn), every bone offset below 29 (each matrix in abs 32 .. 63, which p07_any then reads too), and a vertex from the third on
   without ADC (a strip) ---- */
int xc_c7_chk; long xc_c7_n, xc_c7_diff, xc_c7_held, xc_c7_snap, xc_c7_put, xc_c7_rb;   /* (xconv.h) */
static const uint8_t S7S[XC_7S] = { 0, 1, 2, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };   /* the slots, as XcHeld.p7.sp */
#define C7_LOG 128   /* the log: 128 slot copies, 8 KB (xc_hold7_room) */
static uint8_t C7Log[C7_LOG][64] __attribute__((aligned(64)));
static int c7_n; static uint32_t c7_sok, c7_sgen[16]; static const uint8_t *c7_sptr[16];   /* copies made; slot s's last copy (bit s of
                                                                                         c7_sok), of generation c7_sgen[s], at c7_sptr[s] */
static void c7_log_reset(void) { c7_n = 0; c7_sok = 0; }
int xc_hold7_room(void) { return C7_LOG - c7_n >= XC_7S; }
void xc_hold7_reset(void) { c7_log_reset(); }
typedef uint32_t __attribute__((may_alias)) xc_u32;
static int hold7_test(const XlCall *c, uint32_t *bound, int *npd)
{
  const XlRec *r = c->rec;
  const uint32_t *h = vq(c->vm, c->top);   /* (call() decoded T + 0 .. 3 for lane 4) */
  const uint32_t n = h[0] & 0xffff, nb = h[2] & 0xffff, fl = h[3] & 0xffff, p23 = (fl & 0x210) == 0x210;
  if (n < 1 || n > 64 || nb < 1 || nb > 4 || (fl & 0xd04) || ((fl & 0x210) && !p23) || (vq(c->vm, c->top + 1)[0] & 0xffff) ||
      (c->top & 0x3ffu) + 0x100u + 7u * n > 1024u) return 0;
#define HS(s, kd, m) (r->p[s] && r->kind[s] == (kd) && r->num[s] >= (m))   /* vc_p07_rec's P7S */
  if (!HS(0, 2, n) || !HS(1, 2, n) || !HS(2, 2, n) || (p23 && !HS(3, 2, n)) || !HS(5, 5, 2 * n) || r->p[4]) return 0;
#undef HS
  const xc_h16 *A = (const xc_h16 *)r->p[0], *C = (const xc_h16 *)r->p[2], *D = (const xc_h16 *)r->p[3];
  const xc_u32 *F = (const xc_u32 *)r->p[5];
  const uint32_t bm = nb == 4 ? 0x80808080u : ((1u << (8 * nb)) - 1) & 0x80808080u;   /* bit 7 of the bones' bytes */
  uint32_t bad = 0, strip = 0;
  for (uint32_t i = 0; i < n; i++) {
    const uint32_t w = F[2 * i + 1];   /* the bone offsets: byte b below 29 for b < nb (bit 7 of (byte & 0x7f) + 99 and of the byte clear) */
    bad |= ((uint32_t)(uint16_t)C[4 * i + 2] ^ 0x1000u) | ((((w & 0x7f7f7f7fu) + 0x63636363u) | w) & bm);
    if (p23) bad |= (uint32_t)(uint16_t)D[4 * i + 2] ^ 0x1000u;
    if (i >= 2) strip |= ~(uint32_t)(uint16_t)A[4 * i + 3] & 0x8000u;   /* (stream A's w: the vertex's flags, bit 15 ADC) */
  }
  if (bad || !strip) return 0;
  *npd = p23 ? 2 : 0;
  *bound = (uint32_t)(sizeof(GerXform) + (size_t)(1 + 2 * p23) * sizeof(GerLit)) + n * (24 + 24 + 24 + 4);
  return 1;
}
#ifndef __psp__
static uint32_t c7_qa(int i, uint32_t top)   /* the x86 check's quadwords: abs 2 .. 11, 16 .. 19, 24 .. 63, T + 0 .. 3, T + 0xfe, 0xff */
{
  return i < 10 ? 2u + (uint32_t)i : i < 14 ? 6u + (uint32_t)i : i < 54 ? 10u + (uint32_t)i : i < 58 ? top + (uint32_t)(i - 54) : top + 0xfeu + (uint32_t)(i - 58);
}
#endif
static void hold7(const XlCall *c, XcHeld *r)
{
  xc_c7_held++;
  for (int k = 0; k < XC_7S; k++) {
    const uint32_t s = S7S[k]; const uint8_t *p;
    if (xl_vsrc_gen[s] == xl_vgen[s] && xl_vsrc[s]) p = (const uint8_t *)xl_vsrc[s];   /* the game's data, which the slot holds */
    else if ((c7_sok >> s & 1) && c7_sgen[s] == xl_vgen[s]) p = c7_sptr[s];   /* the copy an earlier held call made of this generation */
    else { uint8_t *q = C7Log[c7_n++]; memcpy(q, c->vm + (s << 6), 64); c7_sgen[s] = xl_vgen[s]; c7_sptr[s] = q; c7_sok |= 1u << s; p = q; xc_c7_snap++; }
    r->p7.sp[k] = p;
  }
  for (uint32_t j = 0; j < 4; j++) memcpy(r->p7.t7[j], vq(c->vm, c->top + j), 16);
  for (uint32_t j = 0; j < 2; j++) memcpy(r->p7.t7[4 + j], vq(c->vm, c->top + 0xfe + j), 16);
#ifndef __psp__
  if (xc_c7_chk) for (int i = 0; i < 60; i++) memcpy(r->chk7[i], vq(c->vm, c7_qa(i, c->top)), 16);
#endif
}
static void place7(const XcHeld *r)
{
  if (xc_held_poison) { memset(Sv, 0xa5, sizeof Sv); memset(C7Pl, 0, sizeof C7Pl); }
  for (int k = 0; k < XC_7S; k++) {
    const uint8_t *p = r->p7.sp[k];
    if (p != C7Pl[k]) { memcpy(Sv + ((uint32_t)S7S[k] << 6), p, 64); C7Pl[k] = p; xc_c7_put++; }
  }
  memcpy(Sv + (HI_T7 << 4), r->p7.t7[0], 4 * 16); memcpy(Sv + ((HI_T7 + 0xfe) << 4), r->p7.t7[4], 2 * 16);
#ifndef __psp__
  if (xc_c7_chk) {   /* the placed memory against the call's own, quadword for quadword */
    int d = 0;
    for (int i = 0; i < 60; i++) d |= memcmp(r->chk7[i], vq(Sv, c7_qa(i, HI_T7)), 16) != 0;
    xc_c7_n++; xc_c7_diff += d;
  }
#endif
}
/* the vertices as the VU memory held them at the call (vc_p07 reads T + 0x100 + 7 i + j): streams A .. D (V4-16, each halfword
   sign-extended; D with passes 2 and 3) at j 0 .. 3, F's two vectors (V4-8 unsigned, each byte a word: the weights, the bone offsets)
   at j 5, 6; E is not sent (hold7_test) and not read */
static void sv7_rebuild(const XcHeld *r)
{
  const XlRec *R = &r->rec; const uint32_t n = r->p7.t7[0][0] & 0xffff; uint32_t a = HI_T7 + 0x100, w[4];
  if (xc_held_poison) memset(Sv + (a << 4), 0xa5, 7 * 64 * 16);
  xc_c7_rb++;
  for (uint32_t i = 0; i < n; i++, a += 7) {
    for (uint32_t s = 0; s < 4; s++) {
      if (s == 3 && !(r->hy & 0x10)) continue;
      const xc_h16 *q = (const xc_h16 *)R->p[s] + 4 * i;
      for (int j = 0; j < 4; j++) w[j] = (uint32_t)(int32_t)q[j];
      memcpy(Sv + ((a + s) << 4), w, 16);
    }
    const uint8_t *f = (const uint8_t *)R->p[5] + 8 * i;
    for (int j = 0; j < 4; j++) w[j] = f[j];
    memcpy(Sv + ((a + 5) << 4), w, 16);
    for (int j = 0; j < 4; j++) w[j] = f[4 + j];
    memcpy(Sv + ((a + 6) << 4), w, 16);
  }
}
