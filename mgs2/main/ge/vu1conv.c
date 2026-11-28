/* vu1conv.c: see vu1conv.h. */
#include "vu1conv.h"
#include "hot39.h"   /* gl39 */
#include <math.h>
#include <string.h>
#include "me_rt.h"   /* task #149: -DMGS2_ME_SAFE (ME-safe calls) */
#include "natsplit.h"   /* gl34: NS_MARK */

static const uint32_t *vq(const uint8_t *vm, uint32_t qa) { return (const uint32_t *)(vm + ((qa & 0x3ffu) << 4)); }
/* a float as the VU reads it: no denormals, and an exponent of 255 is the largest finite value */
static float vfl(uint32_t u)
{
  uint32_t e = u & 0x7f800000u; float f;
  if (!e) u &= 0x80000000u; else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu;
  memcpy(&f, &u, 4); return f;
}
static float fabs_(float f) { return f < 0 ? -f : f; }

/* the call's matrix (columns at T+mq..mq+3) as GE world rows (x pscale: the GE reads s16 positions as s / 32768) and projection terms;
   the abs constants: fog, viewport, constant alpha */
static void vc_xform(const uint8_t *vm, uint32_t top, uint32_t mq, float pscale, VcXform *x)
{
  float *m = x->m;   /* every field is written (VcXform: _Static_assert below) */
  for (int c = 0; c < 4; c++) { const uint32_t *q = vq(vm, top + mq + c); m[4 * c] = vfl(q[0]); m[4 * c + 1] = vfl(q[1]); m[4 * c + 2] = vfl(q[2]); m[4 * c + 3] = vfl(q[3]); }
  /* GE rows: input axis i (x, y, z, translation) -> (clip x, clip y, clip w) */
  float *w = x->world;
  w[0] = m[0] * pscale; w[1] = m[1] * pscale; w[2] = m[3] * pscale; w[3] = m[4] * pscale; w[4] = m[5] * pscale; w[5] = m[7] * pscale;
  w[6] = m[8] * pscale; w[7] = m[9] * pscale; w[8] = m[11] * pscale; w[9] = m[12] * 1.0f; w[10] = m[13] * 1.0f; w[11] = m[15] * 1.0f;
  /* row 2 = -pa row 3 + pb e4 (a perspective projection after the model-view: pa, pb are the projection's z terms) */
  const float r20 = m[2], r21 = m[6], r22 = m[10], r23 = m[14], r30 = m[3], r31 = m[7], r32 = m[11], r33 = m[15];
  float d = 0, nn = 0, big = 0, res = 0, e;
  d += r20 * r30; nn += r30 * r30; d += r21 * r31; nn += r31 * r31; d += r22 * r32; nn += r32 * r32;
  const float pa = nn > 0 ? -d / nn : 0, pb = r23 + pa * r33;
  x->pa = pa; x->pb = pb;
  e = fabs_(r20 + pa * r30 - 0.0f); if (e > res) res = e; if (fabs_(r20) > big) big = fabs_(r20);
  e = fabs_(r21 + pa * r31 - 0.0f); if (e > res) res = e; if (fabs_(r21) > big) big = fabs_(r21);
  e = fabs_(r22 + pa * r32 - 0.0f); if (e > res) res = e; if (fabs_(r22) > big) big = fabs_(r22);
  e = fabs_(r23 + pa * r33 - pb); if (e > res) res = e; if (fabs_(r23) > big) big = fabs_(r23);
  x->resid = big > 0 ? res / big : 0;
  const uint32_t *fg = vq(vm, 0x02), *k3 = vq(vm, 0x03), *s = vq(vm, 0x04), *o = vq(vm, 0x05);
  x->fog_a = vfl(fg[0]); x->fog_b = vfl(fg[1]);
  x->vs[0] = vfl(s[0]); x->vs[1] = vfl(s[1]); x->vs[2] = vfl(s[2]); x->vo[0] = vfl(o[0]); x->vo[1] = vfl(o[1]); x->vo[2] = vfl(o[2]);
  x->n = 0; x->clip = 0;
  x->alpha = (int)vfl(k3[1]);
}
_Static_assert(sizeof(VcXform) == 42 * 4, "vc_xform writes every field of VcXform");
_Static_assert(sizeof(VcLit) == sizeof(VcXform) + 13 * 4, "vc_p06 writes every field of VcLit");
static uint32_t vc_a2(int a) { return a >= 0x80 ? 0xffu : (uint32_t)(a < 0 ? 0 : a * 2); }   /* PS2 alpha (0x80 = 1.0) -> GE (0xff) */

int vc_df0(const uint8_t *vm, uint32_t top, VcXform *x, VcVert *v, uint16_t *flags, int max)
{
  const uint32_t *h = vq(vm, top + 0x14);
  int n = (int)(h[0] & 0xffff);
  if (n > max) return -1;
  vc_xform(vm, top, 0, 32768.0f, x);
  x->n = n; x->clip = h[3] != 0;
  uint32_t a = vc_a2(x->alpha);
  int bad = 0, neg = 0;
  for (int i = 0; i < n; i++) {
    const uint32_t *p = vq(vm, top + 0x100 + 3 * i), *c = vq(vm, top + 0x101 + 3 * i), *t = vq(vm, top + 0x102 + 3 * i);
    for (int k = 0; k < 3; k++) if ((int32_t)p[k] != (int16_t)p[k]) bad = 1;
    for (int k = 0; k < 2; k++) { if ((int32_t)t[k] != (int16_t)t[k]) bad = 1; if ((int32_t)t[k] < 0) neg = 1; }
    v[i].x = (int16_t)p[0]; v[i].y = (int16_t)p[1]; v[i].z = (int16_t)p[2]; v[i].pad = 0;
    v[i].u = (uint16_t)t[0]; v[i].v = (uint16_t)t[1];
    v[i].c = (c[0] & 0xffu) | (c[1] & 0xffu) << 8 | (c[2] & 0xffu) << 16 | a << 24;
    flags[i] = (uint16_t)c[3];
  }
  return bad ? -2 : neg ? -3 : n;
}

typedef int16_t __attribute__((may_alias)) vc_h16;
int vc_df0_rec(const uint8_t *vm, uint32_t top, const void *const *p, const uint8_t *kind, const uint32_t *row, VcXform *x, VcVert *v, uint16_t *flags, int max)
{
  const uint32_t *h = vq(vm, top + 0x14);
  int n = (int)(h[0] & 0xffff);
  if (n > max) return -1;
  vc_xform(vm, top, 0, 32768.0f, x);
  x->n = n; x->clip = h[3] != 0;
  uint32_t a = vc_a2(x->alpha) << 24;
  const vc_h16 *pa = p[0], *pb = p[1], *pc = p[2];
  /* the quadwords as the translator decodes them (ge/xlate.c ud_dec): kind 2 the halfwords sign-extended, 3 plus ROW; C: x, y, ROW z, y */
  const uint32_t ra[4] = { kind[0] == 3 ? row[0] : 0, kind[0] == 3 ? row[1] : 0, kind[0] == 3 ? row[2] : 0, 0 };
  const uint32_t rb[4] = { kind[1] == 3 ? row[0] : 0, kind[1] == 3 ? row[1] : 0, kind[1] == 3 ? row[2] : 0, kind[1] == 3 ? row[3] : 0 };
  int bad = 0, neg = 0;
  for (int i = 0; i < n; i++) {
    uint32_t p0 = (uint32_t)(int32_t)pa[4 * i] + ra[0], p1 = (uint32_t)(int32_t)pa[4 * i + 1] + ra[1], p2 = (uint32_t)(int32_t)pa[4 * i + 2] + ra[2];
    int32_t t0 = pc[2 * i], t1 = pc[2 * i + 1];
    if ((int32_t)p0 != (int16_t)p0 || (int32_t)p1 != (int16_t)p1 || (int32_t)p2 != (int16_t)p2) bad = 1;
    if (t0 < 0 || t1 < 0) neg = 1;
    v[i].x = (int16_t)p0; v[i].y = (int16_t)p1; v[i].z = (int16_t)p2; v[i].pad = 0;
    v[i].u = (uint16_t)t0; v[i].v = (uint16_t)t1;
    v[i].c = (((uint32_t)(int32_t)pb[4 * i] + rb[0]) & 0xffu) | (((uint32_t)(int32_t)pb[4 * i + 1] + rb[1]) & 0xffu) << 8
             | (((uint32_t)(int32_t)pb[4 * i + 2] + rb[2]) & 0xffu) << 16 | a;
    flags[i] = (uint16_t)((uint32_t)(int32_t)pb[4 * i + 3] + rb[3]);
  }
  return bad ? -2 : neg ? -3 : n;
}

/* sb14 (vu1conv.h, 3v).  Streams without ROW (kind 2: every df0 stream of the three routes' census) by words: A's x | y << 16 is
   VcVert's x, y as vc_df0_rec stores them (a sign-extended halfword as int16 is the halfword), A's z | w << 16 with w cleared its z, pad,
   C's u | v << 16 its u, v; a UV is negative when bit 15 or 31 of C's word is set.  Kind 3 (plus ROW, which may leave 16 bits) as
   vc_df0_rec.  The box only when the count is returned (no position off 16 bits, no negative UV) */
typedef uint32_t __attribute__((may_alias)) vc_w32;
/* gl34 (vu1conv.h vc_df0_pos_k): the transform of the last call that came with a key */
static VcXform vc_km H39; static const void *vc_km_src H39; static uint32_t vc_km_gen H39;
long vc_km_n[4];
#ifndef __psp__
int vc_km_chk = -1;   /* (-1: the host has not set it) */
#endif
static inline __attribute__((always_inline)) int df0_pos(const uint8_t *vm, uint32_t top, const void *const *p, const uint8_t *kind, const uint32_t *row, VcXform *x, VcVert *v, int16_t *b, int max, const void *xsrc, uint32_t xgen)
{
  const uint32_t *h = vq(vm, top + 0x14);
  int n = (int)(h[0] & 0xffff);
  if (n > max) return -1;
  if (xsrc && xsrc == vc_km_src && xgen == vc_km_gen) {
    *x = vc_km; vc_km_n[0]++;
#ifndef __psp__
    if (vc_km_chk > 0) { VcXform e; vc_xform(vm, top, 0, 32768.0f, &e); vc_km_n[2]++; if (memcmp(&e, x, sizeof e)) vc_km_n[3]++; }
#endif
  } else { vc_xform(vm, top, 0, 32768.0f, x); if (xsrc) { vc_km = *x; vc_km_src = xsrc; vc_km_gen = xgen; vc_km_n[1]++; } }
  NS_MARK(NS_XF);
  x->n = n; x->clip = h[3] != 0;
  const vc_w32 *wc = (const vc_w32 *)p[2];
  int lx = 32767, ly = 32767, lz = 32767, hx = -32768, hy = -32768, hz = -32768, bad = 0;
  uint32_t ng = 0;
  if (kind[0] != 3) {
    const vc_w32 *wa = (const vc_w32 *)p[0]; vc_w32 *o = (vc_w32 *)v;
    for (int i = 0; i < n; i++, wa += 2, o += 4) {
      const uint32_t a0 = wa[0], a1 = wa[1]; ng |= wc[i];
      const int X = (int16_t)a0, Y = (int32_t)a0 >> 16, Z = (int16_t)a1;
      o[2] = a0; o[3] = a1 & 0xffffu;
      lx = X < lx ? X : lx; hx = X > hx ? X : hx; ly = Y < ly ? Y : ly; hy = Y > hy ? Y : hy; lz = Z < lz ? Z : lz; hz = Z > hz ? Z : hz;
    }
  } else {
    const vc_h16 *pa = p[0];
    for (int i = 0; i < n; i++) {
      uint32_t p0 = (uint32_t)(int32_t)pa[4 * i] + row[0], p1 = (uint32_t)(int32_t)pa[4 * i + 1] + row[1], p2 = (uint32_t)(int32_t)pa[4 * i + 2] + row[2];
      ng |= wc[i];
      if ((int32_t)p0 != (int16_t)p0 || (int32_t)p1 != (int16_t)p1 || (int32_t)p2 != (int16_t)p2) bad = 1;
      const int X = (int16_t)p0, Y = (int16_t)p1, Z = (int16_t)p2;
      v[i].x = (int16_t)X; v[i].y = (int16_t)Y; v[i].z = (int16_t)Z; v[i].pad = 0;
      lx = X < lx ? X : lx; hx = X > hx ? X : hx; ly = Y < ly ? Y : ly; hy = Y > hy ? Y : hy; lz = Z < lz ? Z : lz; hz = Z > hz ? Z : hz;
    }
  }
  if (bad) return -2;
  if (ng & 0x80008000u) return -3;
  b[0] = (int16_t)lx; b[1] = (int16_t)ly; b[2] = (int16_t)lz; b[3] = (int16_t)hx; b[4] = (int16_t)hy; b[5] = (int16_t)hz;
  return n;
}
int vc_df0_pos(const uint8_t *vm, uint32_t top, const void *const *p, const uint8_t *kind, const uint32_t *row, VcXform *x, VcVert *v, int16_t *b, int max)
{
  return df0_pos(vm, top, p, kind, row, x, v, b, max, 0, 0);
}
int vc_df0_pos_k(const uint8_t *vm, uint32_t top, const void *const *p, const uint8_t *kind, const uint32_t *row, VcXform *x, VcVert *v, int16_t *b, int max, const void *xsrc, uint32_t xgen)
{
  return df0_pos(vm, top, p, kind, row, x, v, b, max, xsrc, xgen);
}
/* gl36 (vu1conv.h): df0_pos in two parts for a group with a kept box (ge/xconv.c xc_call, xlate.h XR_BOX): the transform as df0_pos
   takes or computes it (the same memo, counts and split mark), and the positions' stores of its loop */
int vc_df0_xf_k(const uint8_t *vm, uint32_t top, VcXform *x, int max, const void *xsrc, uint32_t xgen)
{
  const uint32_t *h = vq(vm, top + 0x14);
  int n = (int)(h[0] & 0xffff);
  if (n > max) return -1;
  if (xsrc && xsrc == vc_km_src && xgen == vc_km_gen) {
    *x = vc_km; vc_km_n[0]++;
#ifndef __psp__
    if (vc_km_chk > 0) { VcXform e; vc_xform(vm, top, 0, 32768.0f, &e); vc_km_n[2]++; if (memcmp(&e, x, sizeof e)) vc_km_n[3]++; }
#endif
  } else { vc_xform(vm, top, 0, 32768.0f, x); if (xsrc) { vc_km = *x; vc_km_src = xsrc; vc_km_gen = xgen; vc_km_n[1]++; } }
  NS_MARK(NS_XF);
  x->n = n; x->clip = h[3] != 0;
  return n;
}
void vc_df0_pv(const void *const *p, const uint8_t *kind, const uint32_t *row, VcVert *v, int n)
{
  if (kind[0] != 3) {
    const vc_w32 *wa = (const vc_w32 *)p[0]; vc_w32 *o = (vc_w32 *)v;
    for (int i = 0; i < n; i++, wa += 2, o += 4) { const uint32_t a0 = wa[0], a1 = wa[1]; o[2] = a0; o[3] = a1 & 0xffffu; }
  } else {
    const vc_h16 *pa = p[0];
    for (int i = 0; i < n; i++) {
      uint32_t p0 = (uint32_t)(int32_t)pa[4 * i] + row[0], p1 = (uint32_t)(int32_t)pa[4 * i + 1] + row[1], p2 = (uint32_t)(int32_t)pa[4 * i + 2] + row[2];
      v[i].x = (int16_t)p0; v[i].y = (int16_t)p1; v[i].z = (int16_t)p2; v[i].pad = 0;
    }
  }
}
void vc_df0_att(const void *const *p, const uint8_t *kind, const uint32_t *row, const VcXform *x, VcVert *v, uint16_t *flags, int n)
{
  const uint32_t a = vc_a2(x->alpha) << 24;
  const vc_w32 *wc = (const vc_w32 *)p[2];
  if (kind[1] != 3) {
    const vc_w32 *wb = (const vc_w32 *)p[1]; vc_w32 *o = (vc_w32 *)v;
    for (int i = 0; i < n; i++, wb += 2, o += 4) {
      const uint32_t b0 = wb[0], b1 = wb[1];
      o[0] = wc[i];
      o[1] = (b0 & 0xffu) | (b0 >> 8 & 0xff00u) | (b1 & 0xffu) << 16 | a;
      flags[i] = (uint16_t)(b1 >> 16);
    }
  } else {
    const vc_h16 *pb = p[1];
    for (int i = 0; i < n; i++) {
      ((vc_w32 *)v)[4 * i] = wc[i];
      v[i].c = (((uint32_t)(int32_t)pb[4 * i] + row[0]) & 0xffu) | (((uint32_t)(int32_t)pb[4 * i + 1] + row[1]) & 0xffu) << 8
               | (((uint32_t)(int32_t)pb[4 * i + 2] + row[2]) & 0xffu) << 16 | a;
      flags[i] = (uint16_t)((uint32_t)(int32_t)pb[4 * i + 3] + row[3]);
    }
  }
}

int vc_fpos(const uint8_t *vm, uint32_t top, VcXform *x, VcVertF *v, uint16_t *flags, int max, int tme)
{
  const uint32_t *h = vq(vm, top + 0x14);
  int n = (int)(h[0] & 0xffff);
  if (n > max) return -1;
  vc_xform(vm, top, 0x10, 1.0f, x);
  x->n = n; x->clip = 1;   /* these entries always clip-test (header.w is ignored) */
  int bad = 0;
  for (int i = 0; i < n; i++) {
    const uint32_t *p = vq(vm, top + 0x100 + 3 * i), *c = vq(vm, top + 0x101 + 3 * i), *t = vq(vm, top + 0x102 + 3 * i);
    for (int k = 0; k < 4; k++) if (c[k] > 255) bad = 1;
    if (tme && t[2] != 0x1000) bad = 1;   /* (sb15 4x: untextured, the GS takes no ST or Q from it) */
    v[i].x = vfl(p[0]); v[i].y = vfl(p[1]); v[i].z = vfl(p[2]);
    v[i].u = (float)(int32_t)t[0] / 4096.0f; v[i].v = (float)(int32_t)t[1] / 4096.0f;
    v[i].c = (c[0] & 0xffu) | (c[1] & 0xffu) << 8 | (c[2] & 0xffu) << 16 | vc_a2((int)(c[3] & 0xffu)) << 24;
    flags[i] = (uint16_t)t[3];
  }
  return bad ? -4 : n;
}

/* sb6, Stage B lane 3 (vu1conv.h): vc_fpos's loop on the streams, K the attributes' kind (2: V4-16 signed, 3: plus ROW), each quadword
   as the translator decodes it (ge/xlate.c ud_dec), then vc_fpos's arithmetic in its order */
static inline __attribute__((always_inline)) int fpos_rec_loop(const uint32_t *P, const vc_h16 *A, const uint32_t *row, VcVertF *v, uint16_t *flags,
                                                               int n, const int K, const int tme)
{
  const uint32_t r0 = K == 3 ? row[0] : 0, r1 = K == 3 ? row[1] : 0, r2 = K == 3 ? row[2] : 0, r3 = K == 3 ? row[3] : 0;
  int bad = 0;
  for (int i = 0; i < n; i++) {
    const uint32_t *p = P + 4 * i; const vc_h16 *a = A + 8 * i;   /* vertex i: T + 0x100 + 3 i; T + 0x101 + 3 i (vector 2 i), 0x102 + 3 i */
    uint32_t c[4] = { (uint32_t)(int32_t)a[0] + r0, (uint32_t)(int32_t)a[1] + r1, (uint32_t)(int32_t)a[2] + r2, (uint32_t)(int32_t)a[3] + r3 };
    uint32_t t[4] = { (uint32_t)(int32_t)a[4] + r0, (uint32_t)(int32_t)a[5] + r1, (uint32_t)(int32_t)a[6] + r2, (uint32_t)(int32_t)a[7] + r3 };
    for (int k = 0; k < 4; k++) if (c[k] > 255) bad = 1;
    if (tme && t[2] != 0x1000) bad = 1;   /* (as vc_fpos) */
    v[i].x = vfl(p[0]); v[i].y = vfl(p[1]); v[i].z = vfl(p[2]);
    v[i].u = (float)(int32_t)t[0] / 4096.0f; v[i].v = (float)(int32_t)t[1] / 4096.0f;
    v[i].c = (c[0] & 0xffu) | (c[1] & 0xffu) << 8 | (c[2] & 0xffu) << 16 | vc_a2((int)(c[3] & 0xffu)) << 24;
    flags[i] = (uint16_t)t[3];
  }
  return bad;
}
int vc_fpos_rec(const uint8_t *vm, uint32_t top, const void *const *p, const uint16_t *num, const uint8_t *kind, const uint32_t *row, VcXform *x,
                VcVertF *v, uint16_t *flags, int max, int tme)
{
  const uint32_t *h = vq(vm, top + 0x14);
  int n = (int)(h[0] & 0xffff);
  if (n > max) return -1;
  if (!vc_fpos_rec_ok(p, num, kind, n)) return -8;
  vc_xform(vm, top, 0x10, 1.0f, x);
  x->n = n; x->clip = 1;   /* (as vc_fpos) */
  int bad = kind[1] == 2 ? fpos_rec_loop((const uint32_t *)p[0], (const vc_h16 *)p[1], row, v, flags, n, 2, tme) :
                           fpos_rec_loop((const uint32_t *)p[0], (const vc_h16 *)p[1], row, v, flags, n, 3, tme);
  return bad ? -4 : n;
}

float vc_vu(float f) { uint32_t u; memcpy(&u, &f, 4); return vfl(u); }
void vc_vu_clip(const float *m, const float *p, float *c)
{
  for (int k = 0; k < 4; k++) {
    float a = vc_vu(m[k] * p[0]);
    a = vc_vu(a + m[4 + k] * p[1]);
    a = vc_vu(a + m[8 + k] * p[2]);
    c[k] = vc_vu(a + m[12 + k]);   /* MADDw: column 3 times VF0.w = 1.0 */
  }
}

int vc_strips(const uint16_t *flags, int n, uint16_t *runs, int max)
{
  int ns = 0;
  for (int i = 2; i < n; ) {
    if (flags[i] & 0x8000) { i++; continue; }
    int j = i; while (j + 1 < n && !(flags[j + 1] & 0x8000)) j++;
    if (ns == max) return ns;
    runs[2 * ns] = (uint16_t)(i - 2); runs[2 * ns + 1] = (uint16_t)(j - i + 3); ns++;
    i = j + 1;
  }
  return ns;
}

int vc_cflags(const float *c)
{
  float w = fabs_(c[3]);
  return (c[0] > w) | (c[0] < -w) << 1 | (c[1] > w) << 2 | (c[1] < -w) << 3 | (c[2] > w) << 4 | (c[2] < -w) << 5;
}
/* one plane: flag bit (0..5), axis (0..2), s = +1 / -1 */
static int vc_plane(const VcCv *in, int n, VcCv *out, int bit, int axis, float s)
{
  int m = 0;
  for (int k = 0; k < n; k++) {
    const VcCv *a = &in[k], *b = &in[k + 1 < n ? k + 1 : 0];
    int ao = vc_cflags(a->c) >> bit & 1, bo = vc_cflags(b->c) >> bit & 1;
    if (ao && bo) continue;
    if (!ao) out[m++] = *a;
    if (ao != bo) {
      float da = a->c[3] - s * a->c[axis], db = b->c[3] - s * b->c[axis], q = 1.0f / (db - da);
      VcCv *r = &out[m++];
      for (int j = 0; j < 4; j++) r->c[j] = (a->c[j] * db - b->c[j] * da) * q;
      r->c[axis] = s * r->c[3];
      for (int j = 0; j < 4; j++) r->col[j] = (a->col[j] * db - b->col[j] * da) * q;
      for (int j = 0; j < 3; j++) r->uv[j] = (a->uv[j] * db - b->uv[j] * da) * q;
      for (int j = 0; j < 3; j++) r->p[j] = (a->p[j] * db - b->p[j] * da) * q;
    }
  }
  return m;
}
int vc_clip_tri(const VcCv in[3], VcCv out[16])
{
  VcCv a[16], b[16]; int n = 3;
  memcpy(a, in, 3 * sizeof *a);
  if ((vc_cflags(a[0].c) | vc_cflags(a[1].c) | vc_cflags(a[2].c)) & 0x10) { n = vc_plane(a, n, b, 4, 2, 1.0f); memcpy(a, b, n * sizeof *a); }
  int f = 0;
  for (int k = 0; k < n && k < 4; k++) { for (int j = 0; j < 4; j++) a[k].col[j] *= a[k].c[3]; f |= vc_cflags(a[k].c); }
  static const int BIT[5] = { 5, 0, 1, 2, 3 }, AX[5] = { 2, 0, 0, 1, 1 }; static const float SG[5] = { -1, 1, -1, 1, -1 };
  for (int p = 0; p < 5; p++) if (f >> BIT[p] & 1) { n = vc_plane(a, n, b, BIT[p], AX[p], SG[p]); memcpy(a, b, n * sizeof *a); }
  memcpy(out, a, n * sizeof *a);
  return n;
}

/* ---- P06 (vu1conv.h; re/vu1_programs.md 3.4, 3.8, 3.14) ---- */
/* the call's constants as the program reads them (each one through vfl): L[j][k] = T+4+j lane k (light k is column k: stored
   transposed), C[j][k] = T+8+j lane k (light colours 0..2, ambient), B[j][k] = T+0xc+j lane k (the skin matrix: columns x, y, z,
   translation), R[j][k] = row j of pass 3's matrix, lanes x and y (T+0x10+j, or the light ramp's), its scale s and offset o (T+0xfe,
   T+0xff x and y), the clamp and the alpha (abs 3 x and y); lit[k]: light k adds something (a light direction and a colour not zero) */
typedef struct { float L[3][3], C[4][3], B[4][3], R[3][2], s[2], o[2], cmax, alpha; int skin, tint, p3, lit[3]; } P6K;
static int32_t p6_ftoi(float a)   /* FTOI0: toward zero, saturating */
{
  return a >= 2147483648.0f ? 0x7fffffff : a <= -2147483648.0f ? (int32_t)0x80000000u : (int32_t)a;
}
/* One vertex as the program computes it (P06 0x07e-0x0e7 main loop, the skin blend 0x07e-0x08d, the factor 0x0e7, pass 3 0x1f3-0x25e;
   tools p06ref.c gave every kicked vertex of 1,563,883 captured ones, the four entries): every product and sum rounded on its own,
   in the program's order.  exact = 1 puts each result through the VU's flush and clamp (vc_vu), as gsref's interpreter does; exact = 0
   leaves that out, for a call whose constants are all below 2^20 in size (p6_small): the inputs are integers (positions, and 4.12
   normals, weights and factors) or those constants, so no result can reach the clamp, and a result too small for a float is made zero
   by the FPU's own mode (the PSP's FCR31 FS bit, the x86 build's MXCSR FTZ: MGS2_FPU_RZ, GSREF_RZ), so the bits are the same (on the PSP
   perhaps not the sign of such a zero, which no colour, position or texture coordinate keeps).  The fast form also leaves out the lights
   with no direction or no colour: their terms are zeros, which change no sum but its zero's sign.  p: the position (skinned: blended),
   uv: pass 3's u and v (k->p3 > 0); returns the RGBAQ colour as the program kicks it (r | g << 8 | b << 16 | a << 24) */
static inline uint32_t p6_vertex(const P6K *k, const uint32_t *P, const uint32_t *N, const uint32_t *S5, const int exact, float *p, float *uv)
{
#define O(x) (exact ? vc_vu(x) : (x))
  const float i12 = 1.0f / 4096;   /* ITOF12 / ITOF0 of an integer are exact, so no flush */
  float n[3];
  for (int j = 0; j < 3; j++) { p[j] = (float)(int32_t)P[j]; n[j] = (float)(int32_t)N[j] * i12; }
  if (k->skin) {   /* p = p w + (B (p, 1)) (1 - w), n = n w + (B3 n) (1 - w) */
    float w = (float)(int32_t)P[3] * i12, w1 = O(1.0f - w), bp[3], bn[3];
    for (int j = 0; j < 3; j++) {
      float a = O(k->B[0][j] * p[0]); a = O(a + k->B[1][j] * p[1]); a = O(a + k->B[2][j] * p[2]); bp[j] = O(a + k->B[3][j]);
      float b = O(k->B[0][j] * n[0]); b = O(b + k->B[1][j] * n[1]); bn[j] = O(b + k->B[2][j] * n[2]);
    }
    for (int j = 0; j < 3; j++) { p[j] = O(O(p[j] * w) + bp[j] * w1); n[j] = O(O(n[j] * w) + bn[j] * w1); }
  }
  /* d = max(L n, 0) (MULAx, MADDAy, MADDz, MAX); colour = C0 d.x + C1 d.y + C2 d.z + C3 (MULAx, MADDAy, MADDAz, MADDw), alpha from
     abs 3 y, MINI with abs 3 x; then the factor (+738, +ab0) */
  float d[3], col[4];
  for (int j = 0; j < 3; j++) {
    if (!exact && !k->lit[j]) continue;
    float a = O(k->L[0][j] * n[0]); a = O(a + k->L[1][j] * n[1]); d[j] = O(a + k->L[2][j] * n[2]);
    if (!(d[j] > 0.0f)) d[j] = d[j] < 0.0f ? 0.0f : d[j];
  }
  for (int c = 0; c < 3; c++) {
    float a = 0; int any = 0;
    for (int j = 0; j < 3; j++) {
      if (!exact && !k->lit[j]) continue;
      float t = k->C[j][c] * d[j];
      a = any ? O(a + t) : O(t); any = 1;
    }
    col[c] = any ? O(a + k->C[3][c]) : k->C[3][c];
  }
  col[3] = k->alpha;   /* MULw with 1.0 */
  for (int c = 0; c < 4; c++) col[c] = col[c] < k->cmax ? col[c] : k->cmax;
  if (k->tint) for (int c = 0; c < 4; c++) col[c] = O((float)(int32_t)S5[c] * i12 * col[c]);
  if (k->p3 > 0)   /* pass 3: (R n).xy s + o */
    for (int j = 0; j < 2; j++) {
      float a = O(k->R[0][j] * n[0]); a = O(a + k->R[1][j] * n[1]); a = O(a + k->R[2][j] * n[2]);
      uv[j] = O(O(a * k->s[j]) + k->o[j]);
    }
  return ((uint32_t)p6_ftoi(col[0]) & 0xff) | ((uint32_t)p6_ftoi(col[1]) & 0xff) << 8 | ((uint32_t)p6_ftoi(col[2]) & 0xff) << 16 |
         ((uint32_t)p6_ftoi(col[3]) & 0xff) << 24;
#undef O
}
static int p6_big(float f) { uint32_t u; memcpy(&u, &f, 4); return (u >> 23 & 0xff) >= 127 + 20; }   /* |f| >= 2^20 */
/* one vertex of the GE: type bit 0 a float UV (else VcVert's 16-bit one), bit 1 a float position (else 16-bit) */
static void p6_put(uint8_t *o, int type, uint32_t u16, uint32_t v16, const float *uvf, uint32_t c, const uint32_t *P, const float *p)
{
  switch (type) {
    case 0: { VcVert *v = (VcVert *)o; v->u = (uint16_t)u16; v->v = (uint16_t)v16; v->c = c;
              v->x = (int16_t)P[0]; v->y = (int16_t)P[1]; v->z = (int16_t)P[2]; v->pad = 0; break; }
    case 1: { VcVertE *v = (VcVertE *)o; v->u = uvf[0]; v->v = uvf[1]; v->c = c;
              v->x = (int16_t)P[0]; v->y = (int16_t)P[1]; v->z = (int16_t)P[2]; v->pad = 0; break; }
    case 2: { VcVertS *v = (VcVertS *)o; v->u = (uint16_t)u16; v->v = (uint16_t)v16; v->c = c; v->x = p[0]; v->y = p[1]; v->z = p[2]; break; }
    default: { VcVertF *v = (VcVertF *)o; v->u = uvf[0]; v->v = uvf[1]; v->c = c; v->x = p[0]; v->y = p[1]; v->z = p[2]; break; }
  }
}
static const int P6SZ[4] = { 16, 20, 20, 24 };
int vc_p06_exact;
int vc_p06_size1(const VcLit *L) { return P6SZ[(L->uv16 ? 0 : 1) | (L->pos16 ? 0 : 2)]; }
int vc_p06_size3(const VcLit *L) { return P6SZ[1 | (L->pos16 ? 0 : 2)]; }

/* every call (vc_p06's first form): the per-operation flush where a constant needs it, any vertex type, every pass-3 kind */
static int p06_any(const uint8_t *vm, uint32_t top, uint32_t entry, VcLit *L, void *v1, void *v3, uint16_t *flags, uint32_t *rgba, int max)
{
  int kind = entry == VC_P06 ? 0 : entry == VC_P06S ? 1 : entry == VC_P06T ? 2 : entry == VC_P06ST ? 3 : -1;
  if (kind < 0) return -5;
  const uint32_t *h = vq(vm, top + 0x14);
  int n = (int)(h[0] & 0xffff);
  if (n > max) return -1;
  memset(L, 0, sizeof *L);
  L->kind = kind; L->n = n; L->hy = h[1] & 0xffff;
  uint32_t vi6 = ((h[2] & 0xffff) - 0x8000u) & 0xffff;   /* XYZF2 w = (flags & 0x8000) | (header z - 0x8000) */
  L->fog = (int)(vi6 >> 4 & 0xff);
  P6K k; memset(&k, 0, sizeof k);
  k.skin = kind & 1; k.tint = kind >> 1;
  /* the vertex types: 16-bit positions for the unskinned entries when every position fits; 16-bit UVs when every UV1 is in 0..32767 */
  L->pos16 = !k.skin; L->uv16 = 1;
  for (int i = 0; i < n; i++) {
    const uint32_t *P = vq(vm, top + 0x100 + 6 * i), *U = vq(vm, top + 0x102 + 6 * i);
    for (int j = 0; j < 3; j++) if ((int32_t)P[j] != (int16_t)P[j]) L->pos16 = 0;
    for (int j = 0; j < 2; j++) if (U[j] > 32767) L->uv16 = 0;
    if (U[2] != 0x1000) return -6;   /* a UV1 z other than 1.0 (the GS's texel u / z; none seen) */
  }
  vc_xform(vm, top, 0, L->pos16 ? 32768.0f : 1.0f, &L->x);
  L->x.n = n; L->x.clip = h[3] != 0;
  for (int j = 0; j < 3; j++) for (int c = 0; c < 3; c++) k.L[j][c] = vfl(vq(vm, top + 4 + j)[c]);
  for (int j = 0; j < 4; j++) for (int c = 0; c < 3; c++) k.C[j][c] = vfl(vq(vm, top + 8 + j)[c]);
  if (k.skin) for (int j = 0; j < 4; j++) for (int c = 0; c < 3; c++) k.B[j][c] = vfl(vq(vm, top + 0xc + j)[c]);
  k.cmax = vfl(vq(vm, 0x03)[0]); k.alpha = vfl(vq(vm, 0x03)[1]);
  /* pass 3 (0x1f3): the light ramp (0x400: light 0's direction normalized with RSQRT, the y lane 0) or the normal projection (0x200:
     R = T+0x10..0x12); 0x100 (from T+0x104, whose last vertex the program writes into pass 2's slot) is not drawn */
  if (L->hy & 0x100) L->p3 = -1;
  else if (L->hy & 0x600) {
    L->p3 = L->hy & 0x400 ? 2 : 1;
    if (L->p3 == 2) {
      float a = vc_vu(k.L[0][0] * k.L[0][0]); a = vc_vu(a + k.L[1][0] * k.L[1][0]); a = vc_vu(a + k.L[2][0] * k.L[2][0]);
      float q = a == 0.0f ? 3.40282347e38f : vc_vu(1.0f / sqrtf(a < 0 ? -a : a));
      for (int j = 0; j < 3; j++) { k.R[j][0] = vc_vu(k.L[j][0] * q); k.R[j][1] = 0; }
    } else for (int j = 0; j < 3; j++) for (int c = 0; c < 2; c++) k.R[j][c] = vfl(vq(vm, top + 0x10 + j)[c]);
    for (int c = 0; c < 2; c++) { k.s[c] = vfl(vq(vm, top + 0xfe)[c]); k.o[c] = vfl(vq(vm, top + 0xff)[c]); }
  }
  k.p3 = L->p3;
  /* the fast form when every constant the vertices use is below 2^20 (p6_vertex) */
  int big = p6_big(k.cmax) || p6_big(k.alpha);
  for (int j = 0; j < 3; j++) for (int c = 0; c < 3; c++) big |= p6_big(k.L[j][c]);
  for (int j = 0; j < 4; j++) for (int c = 0; c < 3; c++) big |= p6_big(k.C[j][c]) | p6_big(k.B[j][c]);
  for (int j = 0; j < 3; j++) for (int c = 0; c < 2; c++) big |= p6_big(k.R[j][c]);
  for (int c = 0; c < 2; c++) big |= p6_big(k.s[c]) | p6_big(k.o[c]);
  L->fast = !big && !vc_p06_exact;
  for (int j = 0; j < 3; j++)
    k.lit[j] = (k.L[0][j] != 0 || k.L[1][j] != 0 || k.L[2][j] != 0) && (k.C[j][0] != 0 || k.C[j][1] != 0 || k.C[j][2] != 0);
  int t1 = (L->uv16 ? 0 : 1) | (L->pos16 ? 0 : 2), t3 = 1 | (L->pos16 ? 0 : 2), s1 = P6SZ[t1], s3 = P6SZ[t3];
  int amin = 255, cull = 0, var = 0, badz = 0;
  uint8_t *o1 = (uint8_t *)v1, *o3 = (uint8_t *)v3;
  for (int i = 0; i < n; i++) {
    const uint32_t *P = vq(vm, top + 0x100 + 6 * i), *N = vq(vm, top + 0x101 + 6 * i), *U = vq(vm, top + 0x102 + 6 * i);
    const uint32_t *S5 = vq(vm, top + 0x105 + 6 * i);
    float p[3], uv[2], uvf[2];
    uint32_t c = L->fast ? p6_vertex(&k, P, N, S5, 0, p, uv) : p6_vertex(&k, P, N, S5, 1, p, uv);
    if (rgba) rgba[i] = c;
    int a = (int)(c >> 24); if (a < amin) amin = a;
    c = (c & 0xffffffu) | (a >= 0x80 ? 0xffu : (uint32_t)a * 2) << 24;   /* GE alpha: the PS2's doubled */
    uvf[0] = (float)(int32_t)U[0] * (1.0f / 4096); uvf[1] = (float)(int32_t)U[1] * (1.0f / 4096);
    p6_put(o1 + s1 * i, t1, U[0], U[1], uvf, c, P, p);
    if (L->p3 > 0) p6_put(o3 + s3 * i, t3, 0, 0, uv, c, P, p);
    flags[i] = (uint16_t)((N[3] & 0xffff) | (vi6 & 0x8000));
    if ((N[3] & 0x30) != 0x30) cull = 1;
    if (L->hy & 0x10) {
      const uint32_t *U2 = vq(vm, top + 0x103 + 6 * i);
      int du = (int)(U2[0] - U[0]), dv = (int)(U2[1] - U[1]);
      if (!i) { L->du = du; L->dv = dv; }
      else if (du != L->du || dv != L->dv) var = 1;
      if (U2[2] != 0x1000) badz = 1;
    }
  }
  L->uv2 = !(L->hy & 0x10) ? 0 : badz ? -1 : var ? 2 : 1;
  if (L->uv2 != 1) L->du = L->dv = 0;
  L->amin = amin; L->cull = (L->hy & 4) && cull;
  return n;
}

/* ---- P06's loop for the usual calls (vc_p06) ----
   A call whose constants give the fast form, with a lit light, no lit colour below zero, no light ramp (pass 3 kind 2) and its vertices
   in one run of VU memory (no wrap past quadword 1023), as every call of the eight captures (21,740 calls, renderer_notes 11.14), goes
   through a loop made for its kind, its number of lit lights and its passes (p6_loop), with the constants in locals.  The loop computes
   what p6_vertex's fast form computes, every product and sum as there and in the same order, and gives the same bits; what it does
   another way:
   - passes 2 and 3 are both drawn or neither (the rest go to p06_any);
   - the lit lights are packed, keeping their order (an unlit light only adds zeros: p6_vertex);
   - d = max(d, 0) need not keep the sign of a zero d: only the colours use d, and they go through FTOI0;
   - FTOI0 without its saturation: with lit colours not below zero, a colour is at least the ambient, and after the clamp at most cmax,
     both below 2^20 in size in the fast form; a factor (+738, +ab0: S5 / 4096) of at most 2^10 in size keeps it below 2^30, and an S5
     outside [-2^22, 2^22) is checked as the loop goes;
   - the vertex types are taken to be the usual ones (16-bit positions for the unskinned entries, 16-bit UVs) and checked as the loop
     goes, with the UV z.
   When a check fails the call is done again by p06_any, whose result is then the result. */
typedef uint32_t __attribute__((may_alias)) p6u32;
typedef float __attribute__((may_alias)) p6f32;
typedef struct {
  float lx[3], ly[3], lz[3], cr[3], cg[3], cb[3];   /* the lit lights in their order: direction (lanes of T+4, 5, 6), colour (T+8 + j) */
  float ar, ag, ab, cmax, acl;                      /* ambient (T+0xb), the clamp (abs 3 x), the clamped alpha (abs 3 y) */
  float b[4][3];                                    /* the skin matrix (P6K.B) */
  float r[3][2], s[2], o[2];                        /* pass 3 (P6K.R, s, o) */
  uint32_t a1, a2;                                  /* without a factor: the alpha byte as the program kicks it, and the GE's */
} P6F;
typedef struct { uint32_t pos, uv, z, z2, var, fac, andn, amin, du, dv; } P6Chk;
/* sb2, Stage B lane 2: a call's vertices in the game's data (vc_p06_rec): streams A, B, F V4-16 signed, 4 halfwords a vertex (the VU's
   words are those halfwords sign-extended), C, D V2-16 signed, 2 halfwords a vertex (the VU's x, y; its z is ROW z: cz, dz) */
typedef int16_t __attribute__((may_alias)) p6h16;
typedef struct { const p6h16 *a, *b, *c, *d, *f; uint32_t cz, dz; } P6Rec;
static inline __attribute__((always_inline)) void p6_loop(const P6F *k, const uint32_t *q, const P6Rec *r, int n, uint32_t adc, uint8_t *o1,
  uint8_t *o3, uint16_t *flags, uint32_t *rgba, P6Chk *ck, const int SKIN, const int TINT, const int NL, const int P3, const int CHK2,
  const int REC)
{
  const float i12 = 1.0f / 4096;
  const float lx0 = k->lx[0], ly0 = k->ly[0], lz0 = k->lz[0], cr0 = k->cr[0], cg0 = k->cg[0], cb0 = k->cb[0];
#define P6K1(v, e) const float v = NL > 1 ? (e) : 0
#define P6K2(v, e) const float v = NL > 2 ? (e) : 0
#define P6KS(v, e) const float v = SKIN ? (e) : 0
#define P6K3(v, e) const float v = P3 ? (e) : 0
  P6K1(lx1, k->lx[1]); P6K1(ly1, k->ly[1]); P6K1(lz1, k->lz[1]); P6K1(cr1, k->cr[1]); P6K1(cg1, k->cg[1]); P6K1(cb1, k->cb[1]);
  P6K2(lx2, k->lx[2]); P6K2(ly2, k->ly[2]); P6K2(lz2, k->lz[2]); P6K2(cr2, k->cr[2]); P6K2(cg2, k->cg[2]); P6K2(cb2, k->cb[2]);
  const float ar = k->ar, ag = k->ag, ab = k->ab, cmax = k->cmax, acl = TINT ? k->acl : 0;
  P6KS(b00, k->b[0][0]); P6KS(b01, k->b[0][1]); P6KS(b02, k->b[0][2]); P6KS(b10, k->b[1][0]); P6KS(b11, k->b[1][1]); P6KS(b12, k->b[1][2]);
  P6KS(b20, k->b[2][0]); P6KS(b21, k->b[2][1]); P6KS(b22, k->b[2][2]); P6KS(b30, k->b[3][0]); P6KS(b31, k->b[3][1]); P6KS(b32, k->b[3][2]);
  P6K3(r00, k->r[0][0]); P6K3(r01, k->r[0][1]); P6K3(r10, k->r[1][0]); P6K3(r11, k->r[1][1]); P6K3(r20, k->r[2][0]); P6K3(r21, k->r[2][1]);
  P6K3(s0, k->s[0]); P6K3(s1, k->s[1]); P6K3(t0, k->o[0]); P6K3(t1, k->o[1]);
#undef P6K1
#undef P6K2
#undef P6KS
#undef P6K3
  const uint32_t a1 = k->a1, a2 = k->a2;
  uint32_t pos = 0, uv = 0, z = 0, z2 = 0, var = 0, fac = 0, andn = ~0u, amin = 255, du0 = 0, dv0 = 0;
  const p6h16 *ra = 0, *rb = 0, *rc = 0, *rd = 0, *rf = 0;
  if (REC) { ra = r->a; rb = r->b; rc = r->c; rd = r->d; rf = r->f; z = r->cz ^ 0x1000u; z2 = CHK2 ? r->dz ^ 0x1000u : 0; }   /* every
                                                    vertex's UV z is ROW z (n > 0 here) */
#define P6H(p, j) ((uint32_t)(int32_t)(p)[j])
  if (CHK2) { du0 = REC ? P6H(rd, 0) - P6H(rc, 0) : q[12] - q[8]; dv0 = REC ? P6H(rd, 1) - P6H(rc, 1) : q[13] - q[9]; }
  for (int i = 0; i < n; i++) {
    const uint32_t P0 = REC ? P6H(ra, 0) : q[0], P1 = REC ? P6H(ra, 1) : q[1], P2 = REC ? P6H(ra, 2) : q[2], PW = REC ? P6H(ra, 3) : q[3];
    const uint32_t N0 = REC ? P6H(rb, 0) : q[4], N1 = REC ? P6H(rb, 1) : q[5], N2 = REC ? P6H(rb, 2) : q[6], f = REC ? P6H(rb, 3) : q[7];
    const uint32_t U0 = REC ? P6H(rc, 0) : q[8], U1 = REC ? P6H(rc, 1) : q[9];
    float n0 = (float)(int32_t)N0 * i12, n1 = (float)(int32_t)N1 * i12, n2 = (float)(int32_t)N2 * i12, p0 = 0, p1 = 0, p2 = 0;
    if (SKIN) {   /* p = p w + (B (p, 1)) (1 - w), n = n w + (B3 n) (1 - w) */
      p0 = (float)(int32_t)P0; p1 = (float)(int32_t)P1; p2 = (float)(int32_t)P2;
      const float w = (float)(int32_t)PW * i12, w1 = 1.0f - w;
      const float bp0 = ((b00 * p0 + b10 * p1) + b20 * p2) + b30, bp1 = ((b01 * p0 + b11 * p1) + b21 * p2) + b31;
      const float bp2 = ((b02 * p0 + b12 * p1) + b22 * p2) + b32;
      const float bn0 = (b00 * n0 + b10 * n1) + b20 * n2, bn1 = (b01 * n0 + b11 * n1) + b21 * n2, bn2 = (b02 * n0 + b12 * n1) + b22 * n2;
      p0 = p0 * w + bp0 * w1; p1 = p1 * w + bp1 * w1; p2 = p2 * w + bp2 * w1;
      n0 = n0 * w + bn0 * w1; n1 = n1 * w + bn1 * w1; n2 = n2 * w + bn2 * w1;
    }
    float d0 = (lx0 * n0 + ly0 * n1) + lz0 * n2, d1 = 0, d2 = 0;
    d0 = d0 < 0.0f ? 0.0f : d0;
    if (NL > 1) { d1 = (lx1 * n0 + ly1 * n1) + lz1 * n2; d1 = d1 < 0.0f ? 0.0f : d1; }
    if (NL > 2) { d2 = (lx2 * n0 + ly2 * n1) + lz2 * n2; d2 = d2 < 0.0f ? 0.0f : d2; }
    float cr = cr0 * d0, cg = cg0 * d0, cb = cb0 * d0;
    if (NL > 1) { cr = cr + cr1 * d1; cg = cg + cg1 * d1; cb = cb + cb1 * d1; }
    if (NL > 2) { cr = cr + cr2 * d2; cg = cg + cg2 * d2; cb = cb + cb2 * d2; }
    cr = cr + ar; cg = cg + ag; cb = cb + ab;
    cr = cr < cmax ? cr : cmax; cg = cg < cmax ? cg : cmax; cb = cb < cmax ? cb : cmax;
    uint32_t A1 = a1, A2 = a2;
    if (TINT) {
      const uint32_t f0 = REC ? P6H(rf, 0) : q[20], f1 = REC ? P6H(rf, 1) : q[21], f2 = REC ? P6H(rf, 2) : q[22], f3 = REC ? P6H(rf, 3) : q[23];
      fac |= (f0 + 0x400000u) | (f1 + 0x400000u) | (f2 + 0x400000u) | (f3 + 0x400000u);
      cr = (float)(int32_t)f0 * i12 * cr; cg = (float)(int32_t)f1 * i12 * cg; cb = (float)(int32_t)f2 * i12 * cb;
      A1 = (uint32_t)(int32_t)((float)(int32_t)f3 * i12 * acl) & 0xffu; A2 = A1 >= 0x80 ? 0xffu : A1 * 2;
      if (A1 < amin) amin = A1;
    }
    const uint32_t rgb = ((uint32_t)(int32_t)cr & 0xffu) | ((uint32_t)(int32_t)cg & 0xffu) << 8 | ((uint32_t)(int32_t)cb & 0xffu) << 16;
    const uint32_t c = rgb | A2 << 24;
    if (rgba) rgba[i] = rgb | A1 << 24;
    float u3 = 0, v3 = 0;
    if (P3) { u3 = ((r00 * n0 + r10 * n1) + r20 * n2) * s0 + t0; v3 = ((r01 * n0 + r11 * n1) + r21 * n2) * s1 + t1; }
    uv |= U0 | U1; if (!REC) z |= q[10] ^ 0x1000u;
    p6u32 *w = (p6u32 *)o1; w[0] = U0 | U1 << 16; w[1] = c;
    if (!SKIN) {
      pos |= (P0 + 0x8000u) | (P1 + 0x8000u) | (P2 + 0x8000u);
      const uint32_t xy = (P0 & 0xffffu) | P1 << 16, zp = P2 & 0xffffu;
      w[2] = xy; w[3] = zp; o1 += 16;
      if (P3) { p6f32 *e = (p6f32 *)o3; e[0] = u3; e[1] = v3; p6u32 *ei = (p6u32 *)o3; ei[2] = c; ei[3] = xy; ei[4] = zp; o3 += 20; }
    } else {
      p6f32 *wf = (p6f32 *)o1; wf[2] = p0; wf[3] = p1; wf[4] = p2; o1 += 20;
      if (P3) { p6f32 *e = (p6f32 *)o3; e[0] = u3; e[1] = v3; ((p6u32 *)o3)[2] = c; e[3] = p0; e[4] = p1; e[5] = p2; o3 += 24; }
    }
    flags[i] = (uint16_t)(f | adc); andn &= f;
    if (CHK2) {
      const uint32_t V0 = REC ? P6H(rd, 0) : q[12], V1 = REC ? P6H(rd, 1) : q[13];
      var |= ((V0 - U0) ^ du0) | ((V1 - U1) ^ dv0); if (!REC) z2 |= q[14] ^ 0x1000u;
    }
    if (REC) { ra += 4; rb += 4; rc += 2; if (CHK2) rd += 2; if (TINT) rf += 4; } else q += 24;
  }
#undef P6H
  ck->pos = pos; ck->uv = uv; ck->z = z; ck->z2 = z2; ck->var = var; ck->fac = fac; ck->andn = andn; ck->amin = TINT ? amin : a1;
  ck->du = du0; ck->dv = dv0;
}
static uint32_t p6_fl(uint32_t u) { return (u & 0x7f800000u) ? u : u & 0x80000000u; }   /* vfl for a value not above 2^20 in size */
/* vc_p06's usual form: the vertices from the VU memory (REC 0, q) or, sb2, from a lane's streams in the game's data (REC 1, r: where the
   VU memory form goes to p06_any, -8: the caller decodes the VU memory and calls vc_p06) */
static inline __attribute__((always_inline)) int p06_main(const uint8_t *vm, uint32_t top, uint32_t entry, const P6Rec *r, VcLit *L,
  void *v1, void *v3, uint16_t *flags, uint32_t *rgba, int max, const int REC)
{
  int kind = entry == VC_P06 ? 0 : entry == VC_P06S ? 1 : entry == VC_P06T ? 2 : entry == VC_P06ST ? 3 : -1;
  if (kind < 0) return -5;
  const uint32_t *h = vq(vm, top + 0x14);
  int n = (int)(h[0] & 0xffff);
  if (n > max) return -1;
  uint32_t hy = h[1] & 0xffff, q0 = (top + 0x100) & 0x3ffu;
  int skin = kind & 1, tint = kind >> 1, p3 = (hy & 0x100) ? -1 : (hy & 0x600) ? ((hy & 0x400) ? 2 : 1) : 0, chk2 = (hy & 0x10) != 0;
  if (!n || vc_p06_exact || p3 == 2 || (p3 == 1) != chk2 || (!REC && q0 + 6u * (uint32_t)n > 1024u))
    return REC ? -8 : p06_any(vm, top, entry, L, v1, v3, flags, rgba, max);
  /* the constants, each read once: mx the largest size (p6_big: an exponent of 147 or more, 255 included, is 0x49800000 or more),
     vfl's flush (no constant reaches exponent 255 here), the lit lights packed (P6K.lit), neg: a lit colour below zero */
  P6F k; uint32_t mx = 0, neg = 0; int nl = 0;
#define P6MX(u) do { const int32_t a_ = (int32_t)((u) & 0x7fffffffu); mx = a_ > (int32_t)mx ? (uint32_t)a_ : mx; } while (0)
#define P6ST(d, u) do { const uint32_t f_ = p6_fl(u); memcpy(&(d), &f_, 4); } while (0)
  const uint32_t *A = vq(vm, 0x03), *L0 = vq(vm, top + 4), *L1 = vq(vm, top + 5), *L2 = vq(vm, top + 6), *C3 = vq(vm, top + 0xb);
  P6MX(A[0]); P6MX(A[1]); P6MX(C3[0]); P6MX(C3[1]); P6MX(C3[2]);
  for (int j = 0; j < 3; j++) {
    const uint32_t *C = vq(vm, top + 8 + j), lx = L0[j], ly = L1[j], lz = L2[j], c0 = C[0], c1 = C[1], c2 = C[2];
    P6MX(lx); P6MX(ly); P6MX(lz); P6MX(c0); P6MX(c1); P6MX(c2);
    if (!((lx | ly | lz) & 0x7f800000u) || !((c0 | c1 | c2) & 0x7f800000u)) continue;
    neg |= ((c0 & 0x7f800000u) ? c0 : 0) | ((c1 & 0x7f800000u) ? c1 : 0) | ((c2 & 0x7f800000u) ? c2 : 0);
    P6ST(k.lx[nl], lx); P6ST(k.ly[nl], ly); P6ST(k.lz[nl], lz); P6ST(k.cr[nl], c0); P6ST(k.cg[nl], c1); P6ST(k.cb[nl], c2); nl++;
  }
  P6ST(k.ar, C3[0]); P6ST(k.ag, C3[1]); P6ST(k.ab, C3[2]); P6ST(k.cmax, A[0]);
  { float al; P6ST(al, A[1]); k.acl = al < k.cmax ? al : k.cmax; }
  if (skin) for (int j = 0; j < 4; j++) { const uint32_t *B = vq(vm, top + 0xc + j);
    P6MX(B[0]); P6MX(B[1]); P6MX(B[2]); P6ST(k.b[j][0], B[0]); P6ST(k.b[j][1], B[1]); P6ST(k.b[j][2], B[2]); }
  if (p3 == 1) {
    for (int j = 0; j < 3; j++) { const uint32_t *R = vq(vm, top + 0x10 + j); P6MX(R[0]); P6MX(R[1]); P6ST(k.r[j][0], R[0]); P6ST(k.r[j][1], R[1]); }
    const uint32_t *S = vq(vm, top + 0xfe), *O = vq(vm, top + 0xff);
    P6MX(S[0]); P6MX(S[1]); P6MX(O[0]); P6MX(O[1]); P6ST(k.s[0], S[0]); P6ST(k.s[1], S[1]); P6ST(k.o[0], O[0]); P6ST(k.o[1], O[1]);
  }
#undef P6ST
#undef P6MX
  if (mx >= 0x49800000u || (neg >> 31) || !nl) return REC ? -8 : p06_any(vm, top, entry, L, v1, v3, flags, rgba, max);
  k.a1 = (uint32_t)p6_ftoi(k.acl) & 0xffu; k.a2 = k.a1 >= 0x80 ? 0xffu : k.a1 * 2;
  uint32_t vi6 = ((h[2] & 0xffff) - 0x8000u) & 0xffff;
  const uint32_t *q = REC ? 0 : (const uint32_t *)(vm + (q0 << 4));
  P6Chk ck;
  /* a loop for each kind, number of lit lights and passes (2 and 3, or neither) */
  uint8_t *o1 = (uint8_t *)v1, *o3 = (uint8_t *)v3; uint32_t adc = vi6 & 0x8000;
  int sel = skin | tint << 1 | (nl - 1) << 2 | chk2 << 4;
#define P6L(sk, ti, nl_, pc) case (sk | ti << 1 | (nl_ - 1) << 2 | pc << 4): \
    p6_loop(&k, q, r, n, adc, o1, o3, flags, rgba, &ck, sk, ti, nl_, pc, pc, REC); break;
#define P6L4(nl_, pc) P6L(0, 0, nl_, pc) P6L(1, 0, nl_, pc) P6L(0, 1, nl_, pc) P6L(1, 1, nl_, pc)
  switch (sel) { P6L4(1, 0) P6L4(2, 0) P6L4(3, 0) P6L4(1, 1) P6L4(2, 1) P6L4(3, 1) default: return -5; }
#undef P6L4
#undef P6L
  const uint32_t any = (ck.uv >> 15) | ck.z | (skin ? 0 : ck.pos >> 16) | (tint ? ck.fac >> 23 : 0);
  if (any) return REC ? -8 : p06_any(vm, top, entry, L, v1, v3, flags, rgba, max);
  L->kind = kind; L->n = n; L->hy = hy; L->fog = (int)(vi6 >> 4 & 0xff); L->pos16 = !skin; L->uv16 = 1;
  vc_xform(vm, top, 0, skin ? 1.0f : 32768.0f, &L->x);
  L->x.n = n; L->x.clip = h[3] != 0;
  L->p3 = p3;
  L->fast = 1;
  L->uv2 = !chk2 ? 0 : ck.z2 ? -1 : ck.var ? 2 : 1;
  L->du = L->uv2 == 1 ? (int)ck.du : 0; L->dv = L->uv2 == 1 ? (int)ck.dv : 0;
  L->amin = (int)ck.amin; L->cull = (hy & 4) && (ck.andn & 0x30) != 0x30;
  return n;
}
int vc_p06(const uint8_t *vm, uint32_t top, uint32_t entry, VcLit *L, void *v1, void *v3, uint16_t *flags, uint32_t *rgba, int max)
{
  return p06_main(vm, top, entry, 0, L, v1, v3, flags, rgba, max, 0);
}
int vc_p06_rec(const uint8_t *vm, uint32_t top, uint32_t entry, const void *const *p, const uint8_t *kind, const uint16_t *num,
               const uint32_t *row, VcLit *L, void *v1, void *v3, uint16_t *flags, uint32_t *rgba, int max)
{
  /* the streams the call reads, each sent in its group with at least the header's count of vectors: A, B (kind 2), C (kind 4), D on
     pass 2 (header y 0x10), F for the factor (+738, +ab0) */
  const uint32_t *h = vq(vm, top + 0x14); uint32_t n = h[0] & 0xffff;
  int tint = entry == VC_P06T || entry == VC_P06ST;
#define P6S(s, kd) (p[s] && kind[s] == (kd) && num[s] >= n)
  if (!P6S(0, 2) || !P6S(1, 2) || !P6S(2, 4) || ((h[1] & 0x10) && !P6S(3, 4)) || (tint && !P6S(5, 2))) return -8;
#undef P6S
  const P6Rec r = { (const p6h16 *)p[0], (const p6h16 *)p[1], (const p6h16 *)p[2], (const p6h16 *)p[3], (const p6h16 *)p[5], row[2], row[2] };
  return p06_main(vm, top, entry, &r, L, v1, v3, flags, rgba, max, 1);
}
/* pass 2's own vertices from the VU memory (d 0) or, sb2, from the lane's stream D (d: 2 halfwords a vertex) */
static int p06_uv2(const uint8_t *vm, uint32_t top, const p6h16 *d, const int ds, const VcLit *L, const void *v1, void *v2)
{
  const uint32_t st = L->kind == 4 ? 7 : 6;   /* P07 (kind 4): 7 quadwords a vertex */
#define P6U2(i, j) (d ? (uint32_t)(int32_t)d[ds * (i) + (j)] : vq(vm, top + 0x103 + st * (uint32_t)(i))[j])   /* (ds: halfwords a vertex in d) */
  int u16 = 1, t1 = (L->uv16 ? 0 : 1) | (L->pos16 ? 0 : 2);
  for (int i = 0; i < L->n; i++) if (P6U2(i, 0) > 32767 || P6U2(i, 1) > 32767) u16 = 0;
  int t2 = (u16 ? 0 : 1) | (L->pos16 ? 0 : 2), s1 = P6SZ[t1], s2 = P6SZ[t2];
  for (int i = 0; i < L->n; i++) {
    const uint32_t U2[2] = { P6U2(i, 0), P6U2(i, 1) };
    const uint8_t *a = (const uint8_t *)v1 + s1 * i;
    uint32_t c, P[3] = { 0, 0, 0 }; float p[3] = { 0, 0, 0 }, uvf[2];
    memcpy(&c, a + 4 + 4 * (t1 & 1), 4);   /* the colour follows the UV: 4 bytes (16-bit) or 8 (floats) */
    if (L->pos16) { int16_t q[3]; memcpy(q, a + 8 + 4 * (t1 & 1), 6); for (int j = 0; j < 3; j++) P[j] = (uint32_t)(int32_t)q[j]; }
    else memcpy(p, a + 8 + 4 * (t1 & 1), 12);
    uvf[0] = (float)(int32_t)U2[0] * (1.0f / 4096); uvf[1] = (float)(int32_t)U2[1] * (1.0f / 4096);
    p6_put((uint8_t *)v2 + s2 * i, t2, U2[0], U2[1], uvf, c, P, p);
  }
#undef P6U2
  return t2;
}
int vc_p06_uv2(const uint8_t *vm, uint32_t top, const VcLit *L, const void *v1, void *v2) { return p06_uv2(vm, top, 0, 2, L, v1, v2); }
int vc_p06_uv2_rec(const void *d, const VcLit *L, const void *v1, void *v2) { return p06_uv2(0, 0, (const p6h16 *)d, 2, L, v1, v2); }

/* ---- P07 (vu1conv.h; re/vu1_programs.md P07) ---- */
/* ITOF of an integer as gsref's interpreter computes it ((float)(int32_t)u / 2^sh), through the VU's clamp */
static float p7_itof(uint32_t u, int sh) { return vc_vu((float)(int32_t)u / (float)(1 << sh)); }
/* the call's constants as the program reads them (each through vfl): L[j][k] = abs 24 + j lane k (light k is column k), C[k][c] =
   abs 28 + k lane c (light k's colour), the ambient (abs 31), pass 3's R[j][c] = abs 16 + j lane c with its scale s and offset o
   (T+0xfe, T+0xff x and y), the clamp (abs 3 x) and the alpha (MULy: 1.0 times abs 3 y) */
typedef struct { float L[3][3], C[3][3], amb[3], R[3][2], s[2], o[2], cmax, alpha; int bones, p3; } P7K;
/* One vertex as the program computes it (phase 1: 0x0098-0x0230 and its loop 0x02d0-0x04b0 for 2 to 4 bones, 0x0518-0x05d0 for one;
   phase 2: 0x1188-0x1308; pass 3: 0x1b28-0x1c20), every product and sum rounded on its own and through vc_vu, in the program's
   order.  q[j]: the vertex's quadword j; p: the skinned position; uv: pass 3's texture coordinates (k->p3 > 0); returns the RGBAQ
   colour as the program kicks it (r | g << 8 | b << 16 | a << 24) */
static uint32_t p7_vertex(const uint8_t *vm, const P7K *k, const uint32_t *const q[7], float *p, float *uv)
{
#define O(x) vc_vu(x)
  const uint32_t *P = q[0], *N = q[1], *W = q[5], *B = q[6];
  float pi[3], ni[3], w[4] = { 0, 0, 0, 0 }, n[3] = { 0, 0, 0 };
  for (int j = 0; j < 3; j++) { pi[j] = p7_itof(P[j], 4); ni[j] = p7_itof(N[j], 12); }
  if (k->bones > 1) for (int j = 0; j < 4; j++) w[j] = O(p7_itof(W[j], 0) * 0.0078125f);   /* MULi, I = 1 / 128 */
  for (int b = 0; b < k->bones; b++) {
    /* bone b: its matrix's columns at abs (offset + 32) (ILW and IADDIU: 16 bits); s_b = M (p, 1) (MULAx, MADDAy, MADDAz, MADDw),
       n_b = M3 n (MULAx, MADDAy, MADDz) */
    const uint32_t a = ((B[b] & 0xffffu) + 32u) & 0xffffu;
    const uint32_t *c0 = vq(vm, a), *c1 = vq(vm, a + 1), *c2 = vq(vm, a + 2), *c3 = vq(vm, a + 3);
    float sb[3], nb[3];
    for (int l = 0; l < 3; l++) {
      float e = O(vfl(c0[l]) * pi[0]); e = O(e + vfl(c1[l]) * pi[1]); e = O(e + vfl(c2[l]) * pi[2]); sb[l] = O(e + vfl(c3[l]) * 1.0f);
      float f = O(vfl(c0[l]) * ni[0]); f = O(f + vfl(c1[l]) * ni[1]); nb[l] = O(f + vfl(c2[l]) * ni[2]);
    }
    /* the blend: s0 w.x + s1 w.y (MULAx, MADDy), then + s2 w.z and + s3 w.w (ADDAx with 0.0, MADDz / MADDw); the normal the same */
    for (int l = 0; l < 3; l++)
      if (!b) { p[l] = sb[l]; n[l] = nb[l]; }
      else if (b == 1) { p[l] = O(O(p[l] * w[0]) + sb[l] * w[1]); n[l] = O(O(n[l] * w[0]) + nb[l] * w[1]); }
      else { p[l] = O(O(p[l] + 0.0f) + sb[l] * w[b]); n[l] = O(O(n[l] + 0.0f) + nb[l] * w[b]); }
  }
  if (k->bones > 1) {   /* ERLENG as gsref's EFU computes it (the squares and their sum in float, 1 / sqrt, the clamp), then MULw */
    const float sq = n[0] * n[0] + n[1] * n[1] + n[2] * n[2], r = O(sq > 0 ? 1.0f / sqrtf(sq) : 3.4028235e38f);
    for (int l = 0; l < 3; l++) n[l] = O(n[l] * r);
  }
  /* d = max(L n, 0) (MULAx, MADDAy, MADDz, MAXx); colour = C0 d.x + C1 d.y + C2 d.z + ambient (MULAx, MADDAy, MADDAz, MADDw), MINIx
     with abs 3 x; the alpha is not clamped */
  float d[3], col[4];
  for (int j = 0; j < 3; j++) {
    float e = O(k->L[0][j] * n[0]); e = O(e + k->L[1][j] * n[1]); e = O(e + k->L[2][j] * n[2]);
    d[j] = e > 0.0f ? e : 0.0f;
  }
  for (int c = 0; c < 3; c++) {
    float e = O(k->C[0][c] * d[0]); e = O(e + k->C[1][c] * d[1]); e = O(e + k->C[2][c] * d[2]); e = O(e + k->amb[c] * 1.0f);
    col[c] = e < k->cmax ? e : k->cmax;
  }
  col[3] = k->alpha;
  if (k->p3 > 0)   /* pass 3: (R n).xy s + o (MULAx, MADDAy, MADDz; MULA, MADDw) */
    for (int j = 0; j < 2; j++) {
      float e = O(k->R[0][j] * n[0]); e = O(e + k->R[1][j] * n[1]); e = O(e + k->R[2][j] * n[2]);
      uv[j] = O(O(e * k->s[j]) + k->o[j] * 1.0f);
    }
  return ((uint32_t)p6_ftoi(col[0]) & 0xff) | ((uint32_t)p6_ftoi(col[1]) & 0xff) << 8 | ((uint32_t)p6_ftoi(col[2]) & 0xff) << 16 |
         ((uint32_t)p6_ftoi(col[3]) & 0xff) << 24;
#undef O
}
/* every call (vc_p07's first form): each result through the VU's flush and clamp, any vertex type, every pass */
static int p07_any(const uint8_t *vm, uint32_t top, VcLit *L, void *v1, void *v3, uint16_t *flags, uint32_t *rgba, int max)
{
  const uint32_t *h = vq(vm, top);
  const int n = (int)(h[0] & 0xffff);
  const uint32_t fl = h[3] & 0xffff, nb = h[2] & 0xffff;
  if (n > max) return -1;
  if ((fl & 0x804) || (vq(vm, top + 1)[0] & 0xffff) || nb < 1 || nb > 4) return -7;   /* paths the captures never took */
  memset(L, 0, sizeof *L);
  L->kind = 4; L->n = n; L->hy = fl; L->uv16 = 1;
  for (int i = 0; i < n; i++) {
    const uint32_t *U = vq(vm, top + 0x102 + 7 * i);
    for (int j = 0; j < 2; j++) if (U[j] > 32767) L->uv16 = 0;
    if (U[2] != 0x1000) return -6;   /* a UV1 z other than 1.0 (the GS's texel u / z; none seen) */
  }
  vc_xform(vm, 0, 8, 1.0f, &L->x);   /* V: abs 8..11; the positions are floats */
  L->x.n = n; L->x.clip = 1;
  P7K k; memset(&k, 0, sizeof k);
  k.bones = (int)nb;
  for (int j = 0; j < 3; j++) for (int c = 0; c < 3; c++) { k.L[j][c] = vfl(vq(vm, 24 + j)[c]); k.C[j][c] = vfl(vq(vm, 28 + j)[c]); }
  for (int c = 0; c < 3; c++) k.amb[c] = vfl(vq(vm, 31)[c]);
  k.cmax = vfl(vq(vm, 0x03)[0]); k.alpha = vc_vu(1.0f * vfl(vq(vm, 0x03)[1]));
  /* pass 3: the normal projection (0x200); 0x100 and 0x400 take paths not seen (not drawn) */
  L->p3 = (fl & 0x500) ? -1 : (fl & 0x200) ? 1 : 0;
  if (L->p3 > 0) {
    for (int j = 0; j < 3; j++) for (int c = 0; c < 2; c++) k.R[j][c] = vfl(vq(vm, 16 + j)[c]);
    for (int c = 0; c < 2; c++) { k.s[c] = vfl(vq(vm, top + 0xfe)[c]); k.o[c] = vfl(vq(vm, top + 0xff)[c]); }
  }
  k.p3 = L->p3;
  /* the fog F = clamp(a w + b, 0, abs 3 x) (MULAw, MADDy, MAXx, MINIx; FTOI4, bits 4-11): per vertex, which the GE's fog makes from
     its view z (= the PS2's w); with a = 0 the constant clamp(b, 0, abs 3 x) */
  L->fog = -1;
  if (L->x.fog_a == 0.0f) { float f = L->x.fog_b > 0.0f ? L->x.fog_b : 0.0f; f = f < k.cmax ? f : k.cmax; L->fog = (int)((uint32_t)p6_ftoi(f * 16.0f) >> 4 & 0xff); }
  const int t1 = L->uv16 ? 2 : 3, s1 = P6SZ[t1];
  int amin = 255, var = 0, badz = 0;
  uint8_t *o1 = (uint8_t *)v1, *o3 = (uint8_t *)v3;
  for (int i = 0; i < n; i++) {
    const uint32_t *q[7];
    for (int j = 0; j < 7; j++) q[j] = vq(vm, top + 0x100 + 7 * i + j);
    float p[3], uv[2] = { 0, 0 }, uvf[2];
    uint32_t c = p7_vertex(vm, &k, q, p, uv);
    if (rgba) rgba[i] = c;
    int a = (int)(c >> 24); if (a < amin) amin = a;
    c = (c & 0xffffffu) | (a >= 0x80 ? 0xffu : (uint32_t)a * 2) << 24;   /* GE alpha: the PS2's doubled */
    const uint32_t *U = q[2];
    uvf[0] = (float)(int32_t)U[0] * (1.0f / 4096); uvf[1] = (float)(int32_t)U[1] * (1.0f / 4096);
    p6_put(o1 + s1 * i, t1, U[0], U[1], uvf, c, 0, p);
    if (L->p3 > 0) p6_put(o3 + P6SZ[3] * i, 3, 0, 0, uv, c, 0, p);
    flags[i] = (uint16_t)q[0][3];   /* ILW.w: bit 15 ADC (the program adds -2048 to the fog: FTOI4 sets bit 15) */
    if (fl & 0x10) {
      const uint32_t *U2 = q[3];
      int du = (int)(U2[0] - U[0]), dv = (int)(U2[1] - U[1]);
      if (!i) { L->du = du; L->dv = dv; }
      else if (du != L->du || dv != L->dv) var = 1;
      if (U2[2] != 0x1000) badz = 1;
    }
  }
  L->uv2 = !(fl & 0x10) ? 0 : badz ? -1 : var ? 2 : 1;
  if (L->uv2 != 1) L->du = L->dv = 0;
  L->amin = amin;
  return n;
}

/* ---- P07's loop for the usual calls (vc_p07) ----
   A call whose constants (the lights, their colours, the ambient, the clamp, pass 3's matrix, scale and offset) are below 2^20 in size
   with no light colour below zero, whose vertices' bone matrices hold zeros or normal floats below 2^20 in the 12 lanes a bone reads,
   whose normals and weights are in [-2^16, 2^16), whose UV1s are in 0..32767 with z 1.0, with passes 2 and 3 both drawn or neither and
   its vertices in one run of VU memory, as every call of the captures, goes through a loop made for its number of bones and passes
   (p7_loop).  No product or sum can then reach 2^128 (a position stays below 2^61: 2^27 x 2^20 x 8 x 2^9 x 4; a normal below 2^38
   before ERLENG and 2^65 after it, when its squares flush to a zero length; a light term below 2^87, a colour below 2^110), so the VU's
   clamp never acts, and a result too small for a float is made zero by the FPU's own mode as vc_vu makes it (p6_vertex): the loop
   computes what p7_vertex computes, every product and sum as there and in the same order, and gives the same bits.  FTOI0 needs no
   saturation: a colour lies between the ambient and the clamp, both below 2^20 in size.  When a check fails the call is done again by
   p07_any, whose result is then the result. */
typedef struct {
  float l[3][3], c[3][3], amb[3], cmax;   /* P7K.L, C, amb, cmax */
  float r[3][2], s[2], o[2];              /* pass 3 (P7K.R, s, o) */
  uint32_t a1, a2;                        /* the alpha byte as the program kicks it, and the GE's */
} P7F;
typedef struct { uint32_t uv, z, z2, var, nw, du, dv, bad; } P7Chk;
/* a bone matrix's 12 lanes (abs m..m+3, x y z): each a zero or a normal float below 2^20 in size */
static int p7_mok(const uint8_t *vm, uint32_t m)
{
  const uint32_t *c = (const uint32_t *)(vm + (m << 4));
  for (int i = 0; i < 16; i++) { const uint32_t u = c[i] & 0x7fffffffu; if ((i & 3) != 3 && u && u - 0x00800000u >= 0x49000000u) return 0; }
  return 1;
}
/* sb5, Stage B lane 4: a call's vertices in the game's data (vc_p07_rec): streams A, B, C, D V4-16 signed, 4 halfwords a vertex (the VU's
   words are those halfwords sign-extended), F V4-8 unsigned, 2 words a vertex (the weights', then the bone offsets' four bytes) */
typedef struct { const p6h16 *a, *b, *c, *d; const p6u32 *f; } P7Rec;
static inline __attribute__((always_inline)) void p7_loop(const uint8_t *vm, const P7F *k, const uint32_t *q, const P7Rec *r, int n,
  uint8_t *o1, uint8_t *o3, uint16_t *flags, uint32_t *rgba, P7Chk *ck, const int NB, const int P23, const int REC)
{
  const float l00 = k->l[0][0], l01 = k->l[0][1], l02 = k->l[0][2], l10 = k->l[1][0], l11 = k->l[1][1], l12 = k->l[1][2];
  const float l20 = k->l[2][0], l21 = k->l[2][1], l22 = k->l[2][2];
  const float c00 = k->c[0][0], c01 = k->c[0][1], c02 = k->c[0][2], c10 = k->c[1][0], c11 = k->c[1][1], c12 = k->c[1][2];
  const float c20 = k->c[2][0], c21 = k->c[2][1], c22 = k->c[2][2], ar = k->amb[0], ag = k->amb[1], ab = k->amb[2], cmax = k->cmax;
#define P7K3(v, e) const float v = P23 ? (e) : 0
  P7K3(r00, k->r[0][0]); P7K3(r01, k->r[0][1]); P7K3(r10, k->r[1][0]); P7K3(r11, k->r[1][1]); P7K3(r20, k->r[2][0]); P7K3(r21, k->r[2][1]);
  P7K3(s0, k->s[0]); P7K3(s1, k->s[1]); P7K3(t0, k->o[0]); P7K3(t1, k->o[1]);
#undef P7K3
  const uint32_t a1 = k->a1, a2 = k->a2;
  uint32_t ok[32], uv = 0, z = 0, z2 = 0, var = 0, nw = 0, du0 = 0, dv0 = 0;   /* ok: the bone matrices checked (p7_mok), by abs address */
  const p6h16 *ra = 0, *rb = 0, *rc = 0, *rd = 0; const p6u32 *rf = 0;
  if (REC) { ra = r->a; rb = r->b; rc = r->c; rd = r->d; rf = r->f; }
  /* vertex i's quadword j lane l as the VU memory holds it: q[4 j + l], or from the streams (A j 0, B 1, C 2, D 3; F: j 5 the weights'
     word, j 6 the bone offsets') */
#define P7H(p, l) ((uint32_t)(int32_t)(p)[l])
#define P7F8(w, l) ((w) >> (8 * (l)) & 0xffu)
  memset(ok, 0, sizeof ok);
  if (P23) { du0 = REC ? P7H(rd, 0) - P7H(rc, 0) : q[12] - q[8]; dv0 = REC ? P7H(rd, 1) - P7H(rc, 1) : q[13] - q[9]; }
  ck->bad = 1;
  for (int i = 0; i < n; i++) {
    const uint32_t Q0 = REC ? P7H(ra, 0) : q[0], Q1 = REC ? P7H(ra, 1) : q[1], Q2 = REC ? P7H(ra, 2) : q[2], Q3 = REC ? P7H(ra, 3) : q[3];
    const uint32_t N0 = REC ? P7H(rb, 0) : q[4], N1 = REC ? P7H(rb, 1) : q[5], N2 = REC ? P7H(rb, 2) : q[6];
    const uint32_t FW = REC ? rf[0] : 0, FB = REC ? rf[1] : 0;
    const float pi0 = (float)(int32_t)Q0 * 0.0625f, pi1 = (float)(int32_t)Q1 * 0.0625f, pi2 = (float)(int32_t)Q2 * 0.0625f;
    const float ni0 = (float)(int32_t)N0 * (1.0f / 4096), ni1 = (float)(int32_t)N1 * (1.0f / 4096), ni2 = (float)(int32_t)N2 * (1.0f / 4096);
    nw |= (N0 + 0x10000u) | (N1 + 0x10000u) | (N2 + 0x10000u);
    float p0 = 0, p1 = 0, p2 = 0, n0 = 0, n1 = 0, n2 = 0, w0 = 0;
    for (int b = 0; b < NB; b++) {   /* bone b: s_b = M (p, 1), n_b = M3 n, then the blend (p7_vertex) */
      const uint32_t m = (((REC ? P7F8(FB, b) : q[24 + b]) & 0xffffu) + 32u) & 0x3ffu;
      if (!(ok[m >> 5] >> (m & 31) & 1u)) {   /* (REC: the matrix must lie in abs 32 .. 63, which call() decoded) */
        if (m > (REC ? 60u : 1020u) || !p7_mok(vm, m)) return;
        ok[m >> 5] |= 1u << (m & 31);
      }
      const p6f32 *M = (const p6f32 *)(vm + (m << 4));
      const float e0 = ((M[0] * pi0 + M[4] * pi1) + M[8] * pi2) + M[12], e1 = ((M[1] * pi0 + M[5] * pi1) + M[9] * pi2) + M[13];
      const float e2 = ((M[2] * pi0 + M[6] * pi1) + M[10] * pi2) + M[14];
      const float f0 = (M[0] * ni0 + M[4] * ni1) + M[8] * ni2, f1 = (M[1] * ni0 + M[5] * ni1) + M[9] * ni2;
      const float f2 = (M[2] * ni0 + M[6] * ni1) + M[10] * ni2;
      if (b == 0) { p0 = e0; p1 = e1; p2 = e2; n0 = f0; n1 = f1; n2 = f2; continue; }
      const uint32_t Wb = REC ? P7F8(FW, b) : q[20 + b];
      nw |= Wb + 0x10000u;
      const float wb = (float)(int32_t)Wb * 0.0078125f;
      if (b == 1) {
        const uint32_t W0 = REC ? P7F8(FW, 0) : q[20];
        nw |= W0 + 0x10000u; w0 = (float)(int32_t)W0 * 0.0078125f;
        p0 = p0 * w0 + e0 * wb; p1 = p1 * w0 + e1 * wb; p2 = p2 * w0 + e2 * wb;
        n0 = n0 * w0 + f0 * wb; n1 = n1 * w0 + f1 * wb; n2 = n2 * w0 + f2 * wb;
      } else {
        p0 = (p0 + 0.0f) + e0 * wb; p1 = (p1 + 0.0f) + e1 * wb; p2 = (p2 + 0.0f) + e2 * wb;
        n0 = (n0 + 0.0f) + f0 * wb; n1 = (n1 + 0.0f) + f1 * wb; n2 = (n2 + 0.0f) + f2 * wb;
      }
    }
    if (NB > 1) {   /* ERLENG, then MULw */
      const float sq = (n0 * n0 + n1 * n1) + n2 * n2, r_ = sq > 0.0f ? 1.0f / sqrtf(sq) : 3.4028235e38f;
      n0 = n0 * r_; n1 = n1 * r_; n2 = n2 * r_;
    }
    float d0 = (l00 * n0 + l10 * n1) + l20 * n2, d1 = (l01 * n0 + l11 * n1) + l21 * n2, d2 = (l02 * n0 + l12 * n1) + l22 * n2;
    d0 = d0 > 0.0f ? d0 : 0.0f; d1 = d1 > 0.0f ? d1 : 0.0f; d2 = d2 > 0.0f ? d2 : 0.0f;
    float cr = ((c00 * d0 + c10 * d1) + c20 * d2) + ar, cg = ((c01 * d0 + c11 * d1) + c21 * d2) + ag;
    float cb = ((c02 * d0 + c12 * d1) + c22 * d2) + ab;
    cr = cr < cmax ? cr : cmax; cg = cg < cmax ? cg : cmax; cb = cb < cmax ? cb : cmax;
    const uint32_t rgb = ((uint32_t)(int32_t)cr & 0xffu) | ((uint32_t)(int32_t)cg & 0xffu) << 8 | ((uint32_t)(int32_t)cb & 0xffu) << 16;
    const uint32_t c = rgb | a2 << 24, U0 = REC ? P7H(rc, 0) : q[8], U1 = REC ? P7H(rc, 1) : q[9];
    if (rgba) rgba[i] = rgb | a1 << 24;
    uv |= U0 | U1; z |= (REC ? P7H(rc, 2) : q[10]) ^ 0x1000u;
    p6u32 *w = (p6u32 *)o1; w[0] = U0 | U1 << 16; w[1] = c;
    p6f32 *wf = (p6f32 *)o1; wf[2] = p0; wf[3] = p1; wf[4] = p2; o1 += 20;
    if (P23) {
      const float u3 = ((r00 * n0 + r10 * n1) + r20 * n2) * s0 + t0, v3 = ((r01 * n0 + r11 * n1) + r21 * n2) * s1 + t1;
      p6f32 *e = (p6f32 *)o3; e[0] = u3; e[1] = v3; ((p6u32 *)o3)[2] = c; e[3] = p0; e[4] = p1; e[5] = p2; o3 += 24;
      const uint32_t V0 = REC ? P7H(rd, 0) : q[12], V1 = REC ? P7H(rd, 1) : q[13];
      var |= ((V0 - U0) ^ du0) | ((V1 - U1) ^ dv0); z2 |= (REC ? P7H(rd, 2) : q[14]) ^ 0x1000u;
    }
    flags[i] = (uint16_t)Q3;
    if (REC) { ra += 4; rb += 4; rc += 4; if (P23) rd += 4; rf += 2; } else q += 28;
  }
#undef P7H
#undef P7F8
  ck->uv = uv; ck->z = z; ck->z2 = z2; ck->var = var; ck->nw = nw; ck->du = du0; ck->dv = dv0; ck->bad = 0;
}
static uint32_t p7_mx(uint32_t mx, uint32_t u) { u &= 0x7fffffffu; return u > mx ? u : mx; }
/* vc_p07's usual form: the vertices from the VU memory (REC 0) or, sb5, from a lane's streams in the game's data (REC 1, r: where the VU
   memory form goes to p07_any, -8: the caller decodes the VU memory and calls vc_p07) */
static inline __attribute__((always_inline)) int p07_main(const uint8_t *vm, uint32_t top, const P7Rec *r, VcLit *L, void *v1, void *v3,
  uint16_t *flags, uint32_t *rgba, int max, const int REC)
{
  const uint32_t *h = vq(vm, top);
  const int n = (int)(h[0] & 0xffff);
  const uint32_t fl = h[3] & 0xffff, nb = h[2] & 0xffff, q0 = (top + 0x100) & 0x3ffu;
  const int p23 = (fl & 0x210) == 0x210;
  if (n > max) return -1;
  /* (REC: the vertices at T + 0x100 .. T + 0x100 + 7 n - 1 without a wrap, so no other part of the slot wrote over them) */
  if (!n || vc_p06_exact || (fl & 0xd04) || (vq(vm, top + 1)[0] & 0xffff) || nb < 1 || nb > 4 || ((fl & 0x210) && !p23) ||
      q0 + 7u * (uint32_t)n > 1024u || (REC && (top & 0x3ffu) + 0x100u + 7u * (uint32_t)n > 1024u))
    return REC ? -8 : p07_any(vm, top, L, v1, v3, flags, rgba, max);
  /* the constants, each read once: mx the largest size (2^20 is 0x49800000; exponent 255 is above it), neg: a colour below zero */
  P7F k; uint32_t mx = 0, neg = 0;
  for (int j = 0; j < 3; j++) for (int c = 0; c < 3; c++) {
    const uint32_t lu = vq(vm, 24 + j)[c], cu = vq(vm, 28 + j)[c];
    mx = p7_mx(p7_mx(mx, lu), cu); k.l[j][c] = vfl(lu); k.c[j][c] = vfl(cu);
    neg |= (cu & 0x7f800000u) ? cu : 0;
  }
  for (int c = 0; c < 3; c++) { const uint32_t u = vq(vm, 31)[c]; mx = p7_mx(mx, u); k.amb[c] = vfl(u); }
  const uint32_t *A = vq(vm, 0x03);
  mx = p7_mx(mx, A[0]); k.cmax = vfl(A[0]);
  k.a1 = (uint32_t)p6_ftoi(vc_vu(1.0f * vfl(A[1]))) & 0xffu; k.a2 = k.a1 >= 0x80 ? 0xffu : k.a1 * 2;   /* MULy, not clamped */
  if (p23) {
    for (int j = 0; j < 3; j++) for (int c = 0; c < 2; c++) { const uint32_t u = vq(vm, 16 + j)[c]; mx = p7_mx(mx, u); k.r[j][c] = vfl(u); }
    for (int c = 0; c < 2; c++) {
      const uint32_t su = vq(vm, top + 0xfe)[c], ou = vq(vm, top + 0xff)[c];
      mx = p7_mx(p7_mx(mx, su), ou); k.s[c] = vfl(su); k.o[c] = vfl(ou);
    }
  }
  if (mx >= 0x49800000u || (neg >> 31)) return REC ? -8 : p07_any(vm, top, L, v1, v3, flags, rgba, max);
  const uint32_t *q = REC ? 0 : (const uint32_t *)(vm + (q0 << 4));
  uint8_t *o1 = (uint8_t *)v1, *o3 = (uint8_t *)v3;
  P7Chk ck;
#define P7L(nb_, pc) case (nb_ | pc << 3): p7_loop(vm, &k, q, r, n, o1, o3, flags, rgba, &ck, nb_, pc, REC); break;
  switch ((int)nb | p23 << 3) { P7L(1, 0) P7L(2, 0) P7L(3, 0) P7L(4, 0) P7L(1, 1) P7L(2, 1) P7L(3, 1) P7L(4, 1) default: return -5; }
#undef P7L
  if (ck.bad || (ck.nw >> 17) || (ck.uv >> 15) || ck.z) return REC ? -8 : p07_any(vm, top, L, v1, v3, flags, rgba, max);
  memset(L, 0, sizeof *L);
  L->kind = 4; L->n = n; L->hy = fl; L->uv16 = 1; L->fast = 1;
  vc_xform(vm, 0, 8, 1.0f, &L->x);
  L->x.n = n; L->x.clip = 1;
  L->p3 = p23;
  L->fog = -1;   /* as p07_any */
  if (L->x.fog_a == 0.0f) { float f = L->x.fog_b > 0.0f ? L->x.fog_b : 0.0f; f = f < k.cmax ? f : k.cmax; L->fog = (int)((uint32_t)p6_ftoi(f * 16.0f) >> 4 & 0xff); }
  L->uv2 = !p23 ? 0 : ck.z2 ? -1 : ck.var ? 2 : 1;
  L->du = L->uv2 == 1 ? (int)ck.du : 0; L->dv = L->uv2 == 1 ? (int)ck.dv : 0;
  L->amin = (int)k.a1;
  return n;
}
int vc_p07(const uint8_t *vm, uint32_t top, VcLit *L, void *v1, void *v3, uint16_t *flags, uint32_t *rgba, int max)
{
  return p07_main(vm, top, 0, L, v1, v3, flags, rgba, max, 0);
}
int vc_p07_rec(const uint8_t *vm, uint32_t top, const void *const *p, const uint8_t *kind, const uint16_t *num, VcLit *L, void *v1, void *v3,
               uint16_t *flags, uint32_t *rgba, int max)
{
  /* the streams the call reads, each sent in its slot with at least the header's count of vertices: A, B, C (kind 2), D on passes 2 and 3
     (flags 0x210), F (kind 5, two vectors a vertex); stream E not sent (never seen: renderer_notes 11.32) */
  const uint32_t *h = vq(vm, top); uint32_t n = h[0] & 0xffff, fl = h[3] & 0xffff;
#define P7S(s, kd, m) (p[s] && kind[s] == (kd) && num[s] >= (m))
  if (!P7S(0, 2, n) || !P7S(1, 2, n) || !P7S(2, 2, n) || ((fl & 0x210) == 0x210 && !P7S(3, 2, n)) || !P7S(5, 5, 2 * n) || p[4]) return -8;
#undef P7S
  const P7Rec r = { (const p6h16 *)p[0], (const p6h16 *)p[1], (const p6h16 *)p[2], (const p6h16 *)p[3], (const p6u32 *)p[5] };
  return p07_main(vm, top, &r, L, v1, v3, flags, rgba, max, 1);
}
int vc_p07_uv2_rec(const void *d, const VcLit *L, const void *v1, void *v2) { return p06_uv2(0, 0, (const p6h16 *)d, 4, L, v1, v2); }
/* sb11 C1 (vu1conv.h): p07_main's checks and constants, as it reads them, and the GE's form of them.  The constants (abs 3, the lights
   and colours abs 24 .. 31, with passes 2 and 3 abs 16 .. 18 and T + 0xfe, 0xff) are an object's, the same for all its calls: the last
   outcome is kept with the words it came from (C1m) and taken again while they are all equal (c1opt, 2026-11-02: the same results) */
static struct {
  int have, p23, r;                    /* r: the constants' outcome, 0 or -8 or -9 */
  uint32_t key[33];
  float cmax; uint32_t a1;
  float ldir[3][3]; uint32_t lcol[3], amb; float tgen[12];
} C1m;
static int p7_c1k(const uint8_t *vm, uint32_t top, int p23, uint32_t *key)   /* the words the constants come from */
{
  int nk = 0;
  for (int j = 0; j < 3; j++) for (int c = 0; c < 3; c++) { key[nk++] = vq(vm, 24 + j)[c]; key[nk++] = vq(vm, 28 + j)[c]; }
  for (int c = 0; c < 3; c++) key[nk++] = vq(vm, 31)[c];
  key[nk++] = vq(vm, 0x03)[0]; key[nk++] = vq(vm, 0x03)[1];
  if (p23) {
    for (int j = 0; j < 3; j++) for (int c = 0; c < 2; c++) key[nk++] = vq(vm, 16 + j)[c];
    for (int c = 0; c < 2; c++) { key[nk++] = vq(vm, top + 0xfe)[c]; key[nk++] = vq(vm, top + 0xff)[c]; }
  }
  return nk;
}
static uint32_t p7_c1x(const uint8_t *vm, uint32_t top, int p23)   /* c1opt2: 0 when p7_c1k's words equal C1m.key (straight-line, no gather) */
{
  const uint32_t *K = C1m.key, *a = vq(vm, 24), *b = vq(vm, 28), *q = vq(vm, 31), *t = vq(vm, 0x03);
  uint32_t d = (a[0] ^ K[0]) | (b[0] ^ K[1]) | (a[1] ^ K[2]) | (b[1] ^ K[3]) | (a[2] ^ K[4]) | (b[2] ^ K[5]) |
               (a[4] ^ K[6]) | (b[4] ^ K[7]) | (a[5] ^ K[8]) | (b[5] ^ K[9]) | (a[6] ^ K[10]) | (b[6] ^ K[11]) |
               (a[8] ^ K[12]) | (b[8] ^ K[13]) | (a[9] ^ K[14]) | (b[9] ^ K[15]) | (a[10] ^ K[16]) | (b[10] ^ K[17]) |
               (q[0] ^ K[18]) | (q[1] ^ K[19]) | (q[2] ^ K[20]) | (t[0] ^ K[21]) | (t[1] ^ K[22]);
  if (p23) {
    const uint32_t *r = vq(vm, 16), *u = vq(vm, top + 0xfe), *w = vq(vm, top + 0xff);
    d |= (r[0] ^ K[23]) | (r[1] ^ K[24]) | (r[4] ^ K[25]) | (r[5] ^ K[26]) | (r[8] ^ K[27]) | (r[9] ^ K[28]) |
         (u[0] ^ K[29]) | (w[0] ^ K[30]) | (u[1] ^ K[31]) | (w[1] ^ K[32]);
  }
  return d;
}
static void p7_c1m(const uint8_t *vm, uint32_t top, int p23)   /* the constants as p07_main reads them, into C1m (C1m.r -8: p07_main's fast
                                                                 form would not take them, -9: the GE cannot) */
{
  P7F k; uint32_t mx = 0, neg = 0;   /* (p07_main's) */
  C1m.r = 0;
  for (int j = 0; j < 3; j++) for (int c = 0; c < 3; c++) {
    const uint32_t lu = vq(vm, 24 + j)[c], cu = vq(vm, 28 + j)[c];
    mx = p7_mx(p7_mx(mx, lu), cu); k.l[j][c] = vfl(lu); k.c[j][c] = vfl(cu);
    neg |= (cu & 0x7f800000u) ? cu : 0;
  }
  for (int c = 0; c < 3; c++) { const uint32_t u = vq(vm, 31)[c]; mx = p7_mx(mx, u); k.amb[c] = vfl(u); }
  const uint32_t *A = vq(vm, 0x03);
  mx = p7_mx(mx, A[0]); k.cmax = vfl(A[0]);
  k.a1 = (uint32_t)p6_ftoi(vc_vu(1.0f * vfl(A[1]))) & 0xffu; k.a2 = k.a1 >= 0x80 ? 0xffu : k.a1 * 2;
  if (p23) {
    for (int j = 0; j < 3; j++) for (int c = 0; c < 2; c++) { const uint32_t u = vq(vm, 16 + j)[c]; mx = p7_mx(mx, u); k.r[j][c] = vfl(u); }
    for (int c = 0; c < 2; c++) {
      const uint32_t su = vq(vm, top + 0xfe)[c], ou = vq(vm, top + 0xff)[c];
      mx = p7_mx(p7_mx(mx, su), ou); k.s[c] = vfl(su); k.o[c] = vfl(ou);
    }
  }
  C1m.cmax = k.cmax; C1m.a1 = k.a1;
  if (mx >= 0x49800000u || (neg >> 31)) { C1m.r = -8; return; }
  /* the GE's: its clamp is 255; a light's colour and the ambient are 8 bits */
  if (k.cmax != 255.0f) { C1m.r = -9; return; }
  for (int j = 0; j < 3; j++) {   /* light j: d_j = column j of abs 24..26 . n (p7_loop's d0 = l00 n0 + l10 n1 + l20 n2), colour row j */
    const float x = k.l[0][j], y = k.l[1][j], z = k.l[2][j], q = (x * x + y * y) + z * z, len = q > 0.0f ? sqrtf(q) : 0.0f;
    uint32_t col = 0;
    for (int c = 0; c < 3; c++) {
      const float v = k.c[j][c] * len;   /* (>= 0: neg) */
      if (!(v < 255.5f)) { C1m.r = -9; return; }
      col |= (uint32_t)(int32_t)(v + 0.5f) << (8 * c);
    }
    C1m.lcol[j] = len > 0.0f ? col : 0;
    C1m.ldir[j][0] = len > 0.0f ? x / len : 0.0f; C1m.ldir[j][1] = len > 0.0f ? y / len : 0.0f; C1m.ldir[j][2] = len > 0.0f ? z / len : 0.0f;
  }
  uint32_t amb = 0;
  for (int c = 0; c < 3; c++) { const float a = k.amb[c]; if (!(a < 255.5f) || a < 0.0f) { C1m.r = -9; return; } amb |= (uint32_t)(int32_t)(a + 0.5f) << (8 * c); }
  C1m.amb = amb | k.a2 << 24;
  for (int j = 0; j < 3; j++) {   /* pass 3: u = (r00 n0 + r10 n1 + r20 n2) s0 + o0 (p7_loop), so row j = (r[j][0] s0, r[j][1] s1, 0) */
    C1m.tgen[3 * j] = p23 ? k.r[j][0] * k.s[0] : 0.0f; C1m.tgen[3 * j + 1] = p23 ? k.r[j][1] * k.s[1] : 0.0f; C1m.tgen[3 * j + 2] = 0.0f;
  }
  C1m.tgen[9] = p23 ? k.o[0] : 0.0f; C1m.tgen[10] = p23 ? k.o[1] : 0.0f; C1m.tgen[11] = 1.0f;
}
int vc_p07_c1(const uint8_t *vm, uint32_t top, const void *const *p, const uint8_t *kind, const uint16_t *num, VcLit *L, VcP7C1 *o)
{
  const uint32_t *h = vq(vm, top);
  const int n = (int)(h[0] & 0xffff);
  const uint32_t fl = h[3] & 0xffff, nb = h[2] & 0xffff, q0 = (top + 0x100) & 0x3ffu;
  const int p23 = (fl & 0x210) == 0x210;
#define P7S(s, kd, m) (p[s] && kind[s] == (kd) && num[s] >= (m))
  if (!P7S(0, 2, n) || !P7S(1, 2, n) || !P7S(2, 2, n) || (p23 && !P7S(3, 2, n)) || !P7S(5, 5, 2 * n) || p[4]) return -8;
#undef P7S
  if (!n || n > 64 || vc_p06_exact || (fl & 0xd04) || (vq(vm, top + 1)[0] & 0xffff) || nb < 1 || nb > 4 || ((fl & 0x210) && !p23) ||
      q0 + 7u * (uint32_t)n > 1024u || (top & 0x3ffu) + 0x100u + 7u * (uint32_t)n > 1024u)
    return -8;
  if (!C1m.have || C1m.p23 != p23 || p7_c1x(vm, top, p23)) {   /* c1opt2: the key compared in place, gathered only when it changed */
    uint32_t key[33]; p7_c1k(vm, top, p23, key); p7_c1m(vm, top, p23); memcpy(C1m.key, key, sizeof key); C1m.p23 = p23; C1m.have = 1; }
  if (C1m.r) return C1m.r;
  o->nb = (int)nb;
  o->ldir = (const float (*)[3])C1m.ldir; o->lcol = C1m.lcol; o->amb = C1m.amb; o->tgen = C1m.tgen;   /* (C1m's, until the next call) */
  memset(L, 0, sizeof *L);   /* (p07_main's L but uv2, du, dv) */
  L->kind = 4; L->n = n; L->hy = fl; L->uv16 = 1; L->fast = 1;
  vc_xform(vm, 0, 8, 1.0f, &L->x);
  L->x.n = n; L->x.clip = 1;
  L->p3 = p23;
  L->fog = -1;
  if (L->x.fog_a == 0.0f) { float f = L->x.fog_b > 0.0f ? L->x.fog_b : 0.0f; f = f < C1m.cmax ? f : C1m.cmax; L->fog = (int)((uint32_t)p6_ftoi(f * 16.0f) >> 4 & 0xff); }
  L->amin = (int)C1m.a1;
  return n;
}
int vc_p07_bone(const uint8_t *vm, uint32_t m, float *b)
{
  if (m > 60u) return 0;
  const p6u32 *c = (const p6u32 *)(vm + (m << 4)); uint32_t bad = 0;   /* p7_mok's test without its branches: the floats are then as vfl
                                                                          reads them (zeros and normals below 2^20) */
  /* c1opt2: unrolled (the loops cost 273 instructions a bone on the PSP) */
#define C1BT(i) ((uint32_t)((c[i] & 0x7fffffffu) != 0) & (uint32_t)((c[i] & 0x7fffffffu) - 0x00800000u >= 0x49000000u))
  bad = C1BT(0) | C1BT(1) | C1BT(2) | C1BT(4) | C1BT(5) | C1BT(6) | C1BT(8) | C1BT(9) | C1BT(10) | C1BT(12) | C1BT(13) | C1BT(14);
#undef C1BT
  if (bad) return 0;
  const p6f32 *f = (const p6f32 *)c;
  b[0] = f[0] * 2048.0f; b[1] = f[1] * 2048.0f; b[2] = f[2] * 2048.0f; b[3] = f[4] * 2048.0f; b[4] = f[5] * 2048.0f; b[5] = f[6] * 2048.0f;
  b[6] = f[8] * 2048.0f; b[7] = f[9] * 2048.0f; b[8] = f[10] * 2048.0f; b[9] = f[12]; b[10] = f[13]; b[11] = f[14];
  return 1;
}
typedef uint16_t __attribute__((may_alias)) c1u16;
/* (vu1conv.h) the slots each vertex names (offset / 4, with a weight above 0 or the one bone) and the checks */
int vc_p07_c1s(const void *const *p, VcLit *L, VcP7C1 *k)
{
  const int n = L->n, nb = k->nb, p23 = L->p3 > 0;
  const p6h16 *C = (const p6h16 *)p[2], *D = (const p6h16 *)p[3];
  const p6u32 *F = (const p6u32 *)p[5];
  const uint32_t bm = nb >= 4 ? 0xffffffffu : (1u << (8 * nb)) - 1u;   /* the bones' bytes of a word of F */
  uint32_t used = 0, bad = 0, uv = 0, z = 0, z2 = 0, var = 0, u2 = 0, du0 = 0, dv0 = 0;
  if (p23) { du0 = (uint32_t)(int32_t)D[0] - (uint32_t)(int32_t)C[0]; dv0 = (uint32_t)(int32_t)D[1] - (uint32_t)(int32_t)C[1]; }
  const p6h16 *A = (const p6h16 *)p[0];   /* c1opt2: the box of the positions and the weights' sums, for xconv.c xc_c1_zin */
  int x0 = 32767, y0 = 32767, z0 = 32767, x1 = -32768, y1 = -32768, z1 = -32768, s0 = nb == 1 ? 128 : 0x7fff, s1 = nb == 1 ? 128 : 0;
  for (int i = 0; i < n; i++) {
    const uint32_t fw = F[2 * i] & bm, fb = F[2 * i + 1] & bm;
    const int px = A[4 * i], py = A[4 * i + 1], pz = A[4 * i + 2];
    x0 = px < x0 ? px : x0; x1 = px > x1 ? px : x1; y0 = py < y0 ? py : y0; y1 = py > y1 ? py : y1; z0 = pz < z0 ? pz : z0; z1 = pz > z1 ? pz : z1;
    if (nb != 1) { const uint32_t t = (fw & 0x00ff00ffu) + (fw >> 8 & 0x00ff00ffu); const int ws = (int)((t & 0xffffu) + (t >> 16)); s0 = ws < s0 ? ws : s0; s1 = ws > s1 ? ws : s1; }
    bad |= fb & 0xe3e3e3e3u;   /* an offset not a multiple of 4, or 32 and above (with the first: above 28) */
    if (nb == 1) used |= 1u << (fb >> 2 & 7);
    else   /* c1opt2: the 4 bytes unrolled (a byte above nb is 0 in fw, so it adds nothing) */
      used |= (uint32_t)((fw & 0xffu) != 0) << (fb >> 2 & 7) | (uint32_t)((fw >> 8 & 0xffu) != 0) << (fb >> 10 & 7) |
              (uint32_t)((fw >> 16 & 0xffu) != 0) << (fb >> 18 & 7) | (uint32_t)((fw >> 24) != 0) << (fb >> 26 & 7);
    const uint32_t U0 = (uint32_t)(int32_t)C[4 * i], U1 = (uint32_t)(int32_t)C[4 * i + 1];
    uv |= U0 | U1; z |= (uint32_t)(uint16_t)C[4 * i + 2] ^ 0x1000u;
    if (p23) {
      const uint32_t V0 = (uint32_t)(int32_t)D[4 * i], V1 = (uint32_t)(int32_t)D[4 * i + 1];
      var |= ((V0 - U0) ^ du0) | ((V1 - U1) ^ dv0); z2 |= (uint32_t)(uint16_t)D[4 * i + 2] ^ 0x1000u; u2 |= V0 | V1;
    }
  }
  if (bad || (uv >> 15) || z || !used) return -8;
  L->uv2 = !p23 ? 0 : z2 ? -1 : var ? 2 : 1;
  L->du = L->uv2 == 1 ? (int)du0 : 0; L->dv = L->uv2 == 1 ? (int)dv0 : 0;
  if (L->uv2 == 2 && (u2 >> 15)) return -8;   /* pass 2's own UV2 above 32767 */
  int nw = 0;
  for (int s = 0; s < 8; s++) if (used >> s & 1) { k->idx[s] = (uint8_t)nw; k->slot[nw++] = (uint8_t)s; } else k->idx[s] = 0;
  const int st = ((nw + 1) & ~1) + 16;
  const uint32_t a1 = ((uint32_t)(n * st) + 15u) & ~15u;
  k->nw = nw; k->stride = st; k->v2 = L->uv2 == 2 ? a1 : 0;
  k->blo[0] = (int16_t)x0; k->blo[1] = (int16_t)y0; k->blo[2] = (int16_t)z0; k->bhi[0] = (int16_t)x1; k->bhi[1] = (int16_t)y1; k->bhi[2] = (int16_t)z1;
  k->smin = (uint16_t)s0; k->smax = (uint16_t)s1;
  return n;
}
/* (vu1conv.h) the vertices; with zf each one's w as xconv.c's depth range takes it */
int vc_p07_c1b(const void *const *p, const VcLit *L, const VcP7C1 *k, uint8_t *v, uint16_t *flags, const float (*zf)[4], float zt, float *zw,
               long *zn)
{
  const int n = L->n, nb = k->nb, w2h = (k->nw + 1) >> 1, st = k->stride, own2 = L->uv2 == 2;
  const p6h16 *A = (const p6h16 *)p[0], *B = (const p6h16 *)p[1], *C = (const p6h16 *)p[2], *D = (const p6h16 *)p[3];
  const p6u32 *F = (const p6u32 *)p[5];
  const uint32_t a1 = ((uint32_t)(n * st) + 15u) & ~15u;
  float lo = 3.0e38f, hi = 0; long cnt = 0;
  for (int i = 0; i < n; i++) {
    union { uint32_t w[2]; uint8_t b[8]; } wb; uint32_t m = 0;
    wb.w[0] = wb.w[1] = 0;
    const uint32_t fw = F[2 * i], fb = F[2 * i + 1];
    if (nb == 1) { const uint32_t j = k->idx[fb >> 2 & 7]; wb.b[j] = 128; m = 1u << j; }
    else for (int b = 0; b < nb; b++) {
      const uint32_t w = fw >> (8 * b) & 0xffu, j = k->idx[fb >> (8 * b + 2) & 7], t = wb.b[j] + w;
      if (t > 255u) return -8;
      wb.b[j] = (uint8_t)t; m |= (uint32_t)(w != 0) << j;
    }
    c1u16 *h = (c1u16 *)(v + st * i);   /* the weights' halfwords, then UV1, the normal, the position */
    h[0] = (uint16_t)wb.w[0];
    if (w2h > 1) { h[1] = (uint16_t)(wb.w[0] >> 16); if (w2h > 2) { h[2] = (uint16_t)wb.w[1]; if (w2h > 3) h[3] = (uint16_t)(wb.w[1] >> 16); } }
    c1u16 *e = h + w2h;
    e[0] = (uint16_t)C[4 * i]; e[1] = (uint16_t)C[4 * i + 1]; e[2] = (uint16_t)B[4 * i]; e[3] = (uint16_t)B[4 * i + 1]; e[4] = (uint16_t)B[4 * i + 2];
    e[5] = (uint16_t)A[4 * i]; e[6] = (uint16_t)A[4 * i + 1]; e[7] = (uint16_t)A[4 * i + 2];
    flags[i] = (uint16_t)A[4 * i + 3];
    if (own2) {   /* pass 2's copy: UV2 */
      c1u16 *h2 = (c1u16 *)(v + a1 + st * i), *e2 = h2 + w2h;
      for (int q = 0; q < w2h; q++) h2[q] = h[q];
      e2[0] = (uint16_t)D[4 * i]; e2[1] = (uint16_t)D[4 * i + 1];
      for (int q = 2; q < 8; q++) e2[q] = e[q];
    }
    if (zf) {   /* w = t + the weights' sum of each slot's bone w row . (p, 1), the slots in ascending order (xconv.c xc_zr_c1's first form) */
      const float p0 = A[4 * i], p1 = A[4 * i + 1], p2 = A[4 * i + 2];
      float w = zt;
      for (uint32_t mm = m; mm; mm &= mm - 1) {
        const int j = __builtin_ctz(mm);
        w += (float)wb.b[j] * (zf[j][0] * p0 + zf[j][1] * p1 + zf[j][2] * p2 + zf[j][3]);
      }
      if (w > 0) { if (w < lo) lo = w; if (w > hi) hi = w; cnt++; }
    }
  }
  for (uint32_t o = (uint32_t)(n * st); o < a1; o++) { v[o] = 0; if (own2) v[a1 + o] = 0; }
  if (zf) { zw[0] = lo; zw[1] = hi; *zn = cnt; }
  return n;
}

/* ---- sb12 C2 (vu1conv.h; stagec_notes.md step 2): a lane 2 P06 call drawn by the GE's own skinning, lighting and texture matrix ---- */
/* VC_C2T[d] (d = 0..254): the threshold between the colours d and d + 1, float(((d + 0.5) / 255)^2) rounded to nearest (made by
   tools/dev/sb12/c2tab.py); d is the number of thresholds at or below the normal's squared length */
static const union { uint32_t u[255]; float f[255]; } VC_C2T = { {
  0x36810182u, 0x381121b2u, 0x38c9925bu, 0x39458a4fu, 0x39a345e9u, 0x39f3e6dau,
  0x3a2a53feu, 0x3a62c4a7u, 0x3a91a2b4u, 0x3ab5eb20u, 0x3ade3b99u, 0x3b054a0fu,
  0x3b1d7a57u, 0x3b37aea6u, 0x3b53e6fau, 0x3b722355u, 0x3b8931dbu, 0x3b9a540eu,
  0x3bac7844u, 0x3bbf9e7du, 0x3bd3c6bau, 0x3be8f0f9u, 0x3bff1d3bu, 0x3c0b25c0u,
  0x3c173de5u, 0x3c23d70au, 0x3c30f131u, 0x3c3e8c5au, 0x3c4ca884u, 0x3c5b45b0u,
  0x3c6a63ddu, 0x3c7a030cu, 0x3c85119eu, 0x3c8d6237u, 0x3c95f351u, 0x3c9ec4ebu,
  0x3ca7d706u, 0x3cb129a2u, 0x3cbabcbfu, 0x3cc4905cu, 0x3ccea47au, 0x3cd8f919u,
  0x3ce38e39u, 0x3cee63d9u, 0x3cf979fau, 0x3d02684eu, 0x3d0833e0u, 0x3d0e1fb1u,
  0x3d142bc3u, 0x3d1a5816u, 0x3d20a4a9u, 0x3d27117cu, 0x3d2d9e90u, 0x3d344be3u,
  0x3d3b1978u, 0x3d42074du, 0x3d491562u, 0x3d5043b7u, 0x3d57924du, 0x3d5f0123u,
  0x3d66903au, 0x3d6e3f91u, 0x3d760f28u, 0x3d7dff00u, 0x3d83078cu, 0x3d871fb8u,
  0x3d8b4805u, 0x3d8f8071u, 0x3d93c8feu, 0x3d9821abu, 0x3d9c8a78u, 0x3da10366u,
  0x3da58c73u, 0x3daa25a1u, 0x3daeceefu, 0x3db3885du, 0x3db851ecu, 0x3dbd2b9au,
  0x3dc21569u, 0x3dc70f58u, 0x3dcc1967u, 0x3dd13396u, 0x3dd65de5u, 0x3ddb9855u,
  0x3de0e2e5u, 0x3de63d95u, 0x3deba865u, 0x3df12356u, 0x3df6ae66u, 0x3dfc4997u,
  0x3e00fa74u, 0x3e03d82du, 0x3e06bdf5u, 0x3e09abceu, 0x3e0ca1b7u, 0x3e0f9fb0u,
  0x3e12a5b9u, 0x3e15b3d2u, 0x3e18c9fbu, 0x3e1be835u, 0x3e1f0e7eu, 0x3e223cd7u,
  0x3e257341u, 0x3e28b1bbu, 0x3e2bf845u, 0x3e2f46deu, 0x3e329d88u, 0x3e35fc43u,
  0x3e39630du, 0x3e3cd1e7u, 0x3e4048d1u, 0x3e43c7ccu, 0x3e474ed6u, 0x3e4addf1u,
  0x3e4e751cu, 0x3e521457u, 0x3e55bba2u, 0x3e596afdu, 0x3e5d2268u, 0x3e60e1e3u,
  0x3e64a96eu, 0x3e68790au, 0x3e6c50b5u, 0x3e703071u, 0x3e74183cu, 0x3e780818u,
  0x3e7c0004u, 0x3e800000u, 0x3e820406u, 0x3e840c14u, 0x3e86182au, 0x3e882848u,
  0x3e8a3c6fu, 0x3e8c549du, 0x3e8e70d3u, 0x3e909112u, 0x3e92b558u, 0x3e94dda6u,
  0x3e9709fdu, 0x3e993a5bu, 0x3e9b6ec2u, 0x3e9da731u, 0x3e9fe3a7u, 0x3ea22426u,
  0x3ea468adu, 0x3ea6b13cu, 0x3ea8fdd3u, 0x3eab4e72u, 0x3eada319u, 0x3eaffbc8u,
  0x3eb2587fu, 0x3eb4b93eu, 0x3eb71e05u, 0x3eb986d4u, 0x3ebbf3abu, 0x3ebe648bu,
  0x3ec0d972u, 0x3ec35261u, 0x3ec5cf59u, 0x3ec85058u, 0x3ecad560u, 0x3ecd5e70u,
  0x3ecfeb87u, 0x3ed27ca7u, 0x3ed511cfu, 0x3ed7aafeu, 0x3eda4836u, 0x3edce976u,
  0x3edf8ebeu, 0x3ee2380eu, 0x3ee4e566u, 0x3ee796c6u, 0x3eea4c2eu, 0x3eed059eu,
  0x3eefc316u, 0x3ef28497u, 0x3ef54a1fu, 0x3ef813afu, 0x3efae148u, 0x3efdb2e8u,
  0x3f004448u, 0x3f01b121u, 0x3f031ffdu, 0x3f0490ddu, 0x3f0603c1u, 0x3f0778aau,
  0x3f08ef96u, 0x3f0a6887u, 0x3f0be37bu, 0x3f0d6074u, 0x3f0edf70u, 0x3f106071u,
  0x3f11e375u, 0x3f13687eu, 0x3f14ef8au, 0x3f16789bu, 0x3f1803afu, 0x3f1990c8u,
  0x3f1b1fe5u, 0x3f1cb105u, 0x3f1e442au, 0x3f1fd953u, 0x3f217080u, 0x3f2309b0u,
  0x3f24a4e5u, 0x3f26421eu, 0x3f27e15bu, 0x3f29829cu, 0x3f2b25e1u, 0x3f2ccb2au,
  0x3f2e7276u, 0x3f301bc7u, 0x3f31c71cu, 0x3f337475u, 0x3f3523d3u, 0x3f36d534u,
  0x3f388899u, 0x3f3a3e02u, 0x3f3bf56fu, 0x3f3daee0u, 0x3f3f6a55u, 0x3f4127ceu,
  0x3f42e74cu, 0x3f44a8cdu, 0x3f466c52u, 0x3f4831dcu, 0x3f49f969u, 0x3f4bc2fau,
  0x3f4d8e90u, 0x3f4f5c29u, 0x3f512bc6u, 0x3f52fd68u, 0x3f54d10du, 0x3f56a6b7u,
  0x3f587e64u, 0x3f5a5816u, 0x3f5c33cbu, 0x3f5e1185u, 0x3f5ff143u, 0x3f61d304u,
  0x3f63b6cau, 0x3f659c94u, 0x3f678461u, 0x3f696e33u, 0x3f6b5a09u, 0x3f6d47e2u,
  0x3f6f37c0u, 0x3f7129a2u, 0x3f731d88u, 0x3f751372u, 0x3f770b60u, 0x3f790552u,
  0x3f7b0148u, 0x3f7cff42u, 0x3f7eff3fu
} };
#define C2_SHI 16711504u   /* a normal (4.12) whose integer squared length is at least this has d 255: its float one (x^2 + y^2) + z^2 of
                              n / 4096 in round toward zero is above S / 2^24 (1 - 2^-21), at least VC_C2T[254] then (c2tab.py) */
static uint32_t c2_d(float s)   /* the thresholds at or below s (a binary search: 8 compares) */
{
  uint32_t lo = 0, hi = 255;
  while (lo < hi) { const uint32_t m = (lo + hi) >> 1; if (s >= VC_C2T.f[m]) lo = m + 1; else hi = m; }
  return lo;
}
static float p6_flf(uint32_t u) { const uint32_t f = p6_fl(u); float r; memcpy(&r, &f, 4); return r; }   /* (p06_main's P6ST) */
/* The constants (T + 4 .. 0xb and abs 3, with pass 3 T + 0x10 .. 0x12, 0xfe, 0xff) are an object's, the same for many of its calls: the
   last outcome is kept with the words it came from (C2m, as C1m) and taken again while they are all equal */
static struct {
  int have, p3, r;                     /* p3: pass 3's words are in the key; r: the constants' outcome, 0 or -8 or -9 */
  uint32_t key[33];
  float acl; uint32_t a1, a2;
  float ldir[3][3]; uint32_t lcol[3], emis; float tgen[12];
} C2m H39;
static int p6_c2k(const uint8_t *vm, uint32_t top, int p3, uint32_t *key)   /* the words the constants come from */
{
  int nk = 0;
  for (uint32_t j = 0; j < 3; j++) for (int c = 0; c < 3; c++) key[nk++] = vq(vm, top + 4 + j)[c];   /* the lights' rows */
  for (uint32_t j = 0; j < 4; j++) for (int c = 0; c < 3; c++) key[nk++] = vq(vm, top + 8 + j)[c];   /* the colours, the ambient */
  key[nk++] = vq(vm, 0x03)[0]; key[nk++] = vq(vm, 0x03)[1];
  if (p3) {
    for (uint32_t j = 0; j < 3; j++) for (int c = 0; c < 2; c++) key[nk++] = vq(vm, top + 0x10 + j)[c];
    for (int c = 0; c < 2; c++) { key[nk++] = vq(vm, top + 0xfe)[c]; key[nk++] = vq(vm, top + 0xff)[c]; }
  }
  return nk;
}
static uint32_t p6_c2x(const uint8_t *vm, uint32_t top, int p3)   /* 0 when p6_c2k's words equal C2m.key (straight-line, no gather) */
{
  const uint32_t *K = C2m.key, *a = vq(vm, top + 4), *b = vq(vm, top + 5), *c = vq(vm, top + 6), *e = vq(vm, top + 8), *f = vq(vm, top + 9),
                 *g = vq(vm, top + 10), *q = vq(vm, top + 11), *t = vq(vm, 0x03);
  uint32_t d = (a[0] ^ K[0]) | (a[1] ^ K[1]) | (a[2] ^ K[2]) | (b[0] ^ K[3]) | (b[1] ^ K[4]) | (b[2] ^ K[5]) | (c[0] ^ K[6]) | (c[1] ^ K[7]) |
               (c[2] ^ K[8]) | (e[0] ^ K[9]) | (e[1] ^ K[10]) | (e[2] ^ K[11]) | (f[0] ^ K[12]) | (f[1] ^ K[13]) | (f[2] ^ K[14]) |
               (g[0] ^ K[15]) | (g[1] ^ K[16]) | (g[2] ^ K[17]) | (q[0] ^ K[18]) | (q[1] ^ K[19]) | (q[2] ^ K[20]) | (t[0] ^ K[21]) |
               (t[1] ^ K[22]);
  if (p3) {
    const uint32_t *r0 = vq(vm, top + 0x10), *r1 = vq(vm, top + 0x11), *r2 = vq(vm, top + 0x12), *u = vq(vm, top + 0xfe), *w = vq(vm, top + 0xff);
    d |= (r0[0] ^ K[23]) | (r0[1] ^ K[24]) | (r1[0] ^ K[25]) | (r1[1] ^ K[26]) | (r2[0] ^ K[27]) | (r2[1] ^ K[28]) | (u[0] ^ K[29]) |
         (w[0] ^ K[30]) | (u[1] ^ K[31]) | (w[1] ^ K[32]);
  }
  return d;
}
/* the constants as p06_main reads them and checks them (its mx, neg and lit lights), and the GE's form of them, into C2m */
static void p6_c2m(const uint8_t *vm, uint32_t top, int p3)
{
  uint32_t mx = 0, neg = 0; int nl = 0, lit[3];
  const uint32_t *A = vq(vm, 0x03), *L0 = vq(vm, top + 4), *L1 = vq(vm, top + 5), *L2 = vq(vm, top + 6), *C3 = vq(vm, top + 0xb);
  mx = p7_mx(p7_mx(mx, A[0]), A[1]); mx = p7_mx(p7_mx(p7_mx(mx, C3[0]), C3[1]), C3[2]);
  float lx[3], ly[3], lz[3], col[3][3];
  for (int j = 0; j < 3; j++) {
    const uint32_t *C = vq(vm, top + 8 + (uint32_t)j), x = L0[j], y = L1[j], z = L2[j], c0 = C[0], c1 = C[1], c2 = C[2];
    mx = p7_mx(p7_mx(p7_mx(mx, x), y), z); mx = p7_mx(p7_mx(p7_mx(mx, c0), c1), c2);
    lit[j] = ((x | y | z) & 0x7f800000u) && ((c0 | c1 | c2) & 0x7f800000u);
    if (!lit[j]) continue;
    neg |= ((c0 & 0x7f800000u) ? c0 : 0) | ((c1 & 0x7f800000u) ? c1 : 0) | ((c2 & 0x7f800000u) ? c2 : 0);
    lx[j] = p6_flf(x); ly[j] = p6_flf(y); lz[j] = p6_flf(z); col[j][0] = p6_flf(c0); col[j][1] = p6_flf(c1); col[j][2] = p6_flf(c2); nl++;
  }
  float R[3][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 } }, s[2] = { 0, 0 }, o[2] = { 0, 0 };
  if (p3) {
    for (uint32_t j = 0; j < 3; j++) {
      const uint32_t *r = vq(vm, top + 0x10 + j); mx = p7_mx(p7_mx(mx, r[0]), r[1]); R[j][0] = p6_flf(r[0]); R[j][1] = p6_flf(r[1]); }
    const uint32_t *S = vq(vm, top + 0xfe), *O = vq(vm, top + 0xff);
    mx = p7_mx(p7_mx(p7_mx(p7_mx(mx, S[0]), S[1]), O[0]), O[1]);
    s[0] = p6_flf(S[0]); s[1] = p6_flf(S[1]); o[0] = p6_flf(O[0]); o[1] = p6_flf(O[1]);
  }
  C2m.r = 0;
  if (mx >= 0x49800000u || (neg >> 31) || !nl) { C2m.r = -8; return; }
  const float cmax = p6_flf(A[0]), al = p6_flf(A[1]);
  C2m.acl = al < cmax ? al : cmax;
  C2m.a1 = (uint32_t)p6_ftoi(C2m.acl) & 0xffu; C2m.a2 = C2m.a1 >= 0x80 ? 0xffu : C2m.a1 * 2;
  /* the GE's: its clamp is 255; a light's colour and the emissive are 8 bits */
  if (cmax != 255.0f) { C2m.r = -9; return; }
  for (int j = 0; j < 3; j++) {   /* light j: d_j = column j of T + 4..6 . n (p6_loop's d0 = lx0 n0 + ly0 n1 + lz0 n2), colour row j */
    C2m.lcol[j] = 0; C2m.ldir[j][0] = C2m.ldir[j][1] = C2m.ldir[j][2] = 0.0f;
    if (!lit[j]) continue;
    const float x = lx[j], y = ly[j], z = lz[j], q = (x * x + y * y) + z * z, len = q > 0.0f ? sqrtf(q) : 0.0f;
    if (!(len > 0.0f)) continue;
    uint32_t cc = 0;
    for (int c = 0; c < 3; c++) {
      const float v = col[j][c] * len;   /* (>= 0: neg) */
      if (!(v < 255.5f)) { C2m.r = -9; return; }
      cc |= (uint32_t)(int32_t)(v + 0.5f) << (8 * c);
    }
    C2m.lcol[j] = cc; C2m.ldir[j][0] = x / len; C2m.ldir[j][1] = y / len; C2m.ldir[j][2] = z / len;
  }
  uint32_t em = 0;
  for (int c = 0; c < 3; c++) {
    const float a = p6_flf(C3[c]);
    if (!(a < 255.5f) || a < 0.0f) { C2m.r = -9; return; }
    em |= (uint32_t)(int32_t)(a + 0.5f) << (8 * c);
  }
  C2m.emis = em;
  for (int j = 0; j < 3; j++) { C2m.tgen[3 * j] = R[j][0] * s[0]; C2m.tgen[3 * j + 1] = R[j][1] * s[1]; C2m.tgen[3 * j + 2] = 0.0f; }   /* (p6_loop:
                                                    u3 = (r00 n0 + r10 n1 + r20 n2) s0 + t0) */
  C2m.tgen[9] = o[0]; C2m.tgen[10] = o[1]; C2m.tgen[11] = 1.0f;
}
int vc_p06_c2(const uint8_t *vm, uint32_t top, uint32_t entry, const void *const *p, const uint8_t *kind, const uint16_t *num,
              const uint32_t *row, VcLit *L, VcP6C2 *k)
{
  const int kd = entry == VC_P06 ? 0 : entry == VC_P06S ? 1 : entry == VC_P06T ? 2 : entry == VC_P06ST ? 3 : -1;
  if (kd < 0) return -8;
  const uint32_t *h = vq(vm, top + 0x14);
  const uint32_t n = h[0] & 0xffff, hy = h[1] & 0xffff;
  const int skin = kd & 1, tint = kd >> 1, chk2 = (hy & 0x10) != 0, p3 = (hy & 0x100) ? -1 : (hy & 0x600) ? ((hy & 0x400) ? 2 : 1) : 0;
#define P6S(s, kd_) (p[s] && kind[s] == (kd_) && num[s] >= n)   /* (vc_p06_rec's) */
  if (!P6S(0, 2) || !P6S(1, 2) || !P6S(2, 4) || (chk2 && !P6S(3, 4)) || (tint && !P6S(5, 2))) return -8;
#undef P6S
  if (!n || n > 64 || vc_p06_exact || p3 == 2 || (p3 == 1) != chk2 || row[2] != 0x1000u) return -8;   /* (p06_main's with REC; ROW z:
                                                    every UV z, its z check) */
  const int p31 = p3 == 1;
  if (!C2m.have || C2m.p3 != p31 || p6_c2x(vm, top, p31)) {
    uint32_t key[33]; const int nk = p6_c2k(vm, top, p31, key); p6_c2m(vm, top, p31);
    memcpy(C2m.key, key, (size_t)nk * 4); C2m.p3 = p31; C2m.have = 1; }
  if (C2m.r) return C2m.r;
  if (skin) {   /* the skin matrix (p06_main's mx and P6ST) and the GE's bone 1 */
    uint32_t mx = 0;
    for (uint32_t j = 0; j < 4; j++) {
      const uint32_t *B = vq(vm, top + 0xc + j);
      mx = p7_mx(p7_mx(p7_mx(mx, B[0]), B[1]), B[2]); k->b[j][0] = p6_flf(B[0]); k->b[j][1] = p6_flf(B[1]); k->b[j][2] = p6_flf(B[2]); }
    if (mx >= 0x49800000u) return -8;
    for (int j = 0; j < 3; j++) for (int c = 0; c < 3; c++) k->bone1[3 * j + c] = k->b[j][c] * 32768.0f;
    for (int c = 0; c < 3; c++) k->bone1[9 + c] = k->b[3][c];
  }
  k->ldir = (const float (*)[3])C2m.ldir; k->lcol = C2m.lcol; k->emis = C2m.emis; k->tgen = C2m.tgen;
  k->a1 = C2m.a1; k->a2 = C2m.a2; k->acl = C2m.acl; k->skin = skin; k->tint = tint; k->chk2 = chk2;
  const uint32_t vi6 = ((h[2] & 0xffff) - 0x8000u) & 0xffff;
  k->adc = vi6 & 0x8000;
  memset(L, 0, sizeof *L);   /* (p06_main's L but uv2, du, dv, amin, cull: vc_p06_c2v's) */
  L->kind = kd; L->n = (int)n; L->hy = hy; L->fog = (int)(vi6 >> 4 & 0xff); L->pos16 = !skin; L->uv16 = 1;
  vc_xform(vm, top, 0, skin ? 1.0f : 32768.0f, &L->x);
  L->x.n = (int)n; L->x.clip = h[3] != 0;
  L->p3 = p3; L->fast = 1;
  return (int)n;
}
void vc_p06_c2box(const void *const *p, int n, VcP6C2 *k)
{
  const p6h16 *A = (const p6h16 *)p[0];
  int x0 = 32767, y0 = 32767, z0 = 32767, x1 = -32768, y1 = -32768, z1 = -32768;
  for (int i = 0; i < n; i++) {
    const int px = A[4 * i], py = A[4 * i + 1], pz = A[4 * i + 2];
    x0 = px < x0 ? px : x0; x1 = px > x1 ? px : x1; y0 = py < y0 ? py : y0; y1 = py > y1 ? py : y1; z0 = pz < z0 ? pz : z0; z1 = pz > z1 ? pz : z1;
  }
  k->blo[0] = (int16_t)x0; k->blo[1] = (int16_t)y0; k->blo[2] = (int16_t)z0; k->bhi[0] = (int16_t)x1; k->bhi[1] = (int16_t)y1; k->bhi[2] = (int16_t)z1;
}
/* the vertex loop of each kind (SKIN, TINT); its float arithmetic is p6_loop's where it computes the same things (the weight, the skin's
   blend of the normal and of the position) */
static inline __attribute__((always_inline)) int p6_c2loop(const void *const *p, VcLit *L, const VcP6C2 *k, uint8_t *v, uint16_t *flags,
  const float *zr, float *zw, long *zn, const int SKIN, const int TINT)
{
  const int n = L->n, chk2 = k->chk2, st = SKIN ? 24 : 20;
  const p6h16 *A = (const p6h16 *)p[0], *B = (const p6h16 *)p[1], *C = (const p6h16 *)p[2], *D = (const p6h16 *)p[3], *F = (const p6h16 *)p[5];
  const float i12 = 1.0f / 4096, T254 = VC_C2T.f[254];
#define C2KS(v_, e) const float v_ = SKIN ? (e) : 0
  C2KS(b00, k->b[0][0]); C2KS(b01, k->b[0][1]); C2KS(b02, k->b[0][2]); C2KS(b10, k->b[1][0]); C2KS(b11, k->b[1][1]); C2KS(b12, k->b[1][2]);
  C2KS(b20, k->b[2][0]); C2KS(b21, k->b[2][1]); C2KS(b22, k->b[2][2]); C2KS(b30, k->b[3][0]); C2KS(b31, k->b[3][1]); C2KS(b32, k->b[3][2]);
#undef C2KS
  const uint32_t adc = k->adc, ac = k->a2 << 24;
  const float acl = TINT ? k->acl : 0;
  const float za = zr ? zr[0] : 0, zb = zr ? zr[1] : 0, zc = zr ? zr[2] : 0, zt = zr ? zr[3] : 0;
  uint32_t uv = 0, var = 0, u2 = 0, bad = 0, andn = ~0u, amin = TINT ? 255u : k->a1, du0 = 0, dv0 = 0;
  float lo = 3.0e38f, hi = 0; long cnt = 0;
  if (chk2) { du0 = (uint32_t)(int32_t)D[0] - (uint32_t)(int32_t)C[0]; dv0 = (uint32_t)(int32_t)D[1] - (uint32_t)(int32_t)C[1]; }
  uint8_t *o = v;
  for (int i = 0; i < n; i++, o += st) {
    const int32_t P0 = A[4 * i], P1 = A[4 * i + 1], P2 = A[4 * i + 2], PW = A[4 * i + 3], N0 = B[4 * i], N1 = B[4 * i + 1], N2 = B[4 * i + 2];
    const uint32_t f = (uint32_t)(int32_t)B[4 * i + 3], U0 = (uint32_t)(int32_t)C[2 * i], U1 = (uint32_t)(int32_t)C[2 * i + 1];
    uv |= U0 | U1;
    if (chk2) {   /* (p6_loop's CHK2) */
      const uint32_t V0 = (uint32_t)(int32_t)D[2 * i], V1 = (uint32_t)(int32_t)D[2 * i + 1];
      var |= ((V0 - U0) ^ du0) | ((V1 - U1) ^ dv0); u2 |= V0 | V1;
    }
    /* d: the normal's squared length.  The data's (unskinned, or a weight of 1.0: p6_loop's n w + bn 0 is n exactly) by its integer one
       where that settles 255, else as a float; a blend p6_loop's (with a weight of 0 that is B3 n exactly) */
    uint32_t d;
    if (!SKIN || PW == 4096) {
      const uint32_t S = (uint32_t)(N0 * N0) + (uint32_t)(N1 * N1) + (uint32_t)(N2 * N2);
      if (S >= C2_SHI) d = 255;
      else { const float m0 = (float)N0 * i12, m1 = (float)N1 * i12, m2 = (float)N2 * i12; d = c2_d((m0 * m0 + m1 * m1) + m2 * m2); }
    } else {
      float n0 = (float)N0 * i12, n1 = (float)N1 * i12, n2 = (float)N2 * i12;
      const float w = (float)PW * i12, w1 = 1.0f - w;
      const float bn0 = (b00 * n0 + b10 * n1) + b20 * n2, bn1 = (b01 * n0 + b11 * n1) + b21 * n2, bn2 = (b02 * n0 + b12 * n1) + b22 * n2;
      n0 = n0 * w + bn0 * w1; n1 = n1 * w + bn1 * w1; n2 = n2 * w + bn2 * w1;
      const float s = (n0 * n0 + n1 * n1) + n2 * n2;
      d = s >= T254 ? 255u : c2_d(s);
    }
    uint32_t a = ac;
    if (TINT) {   /* the factor: its colour 1.0 (else the GE cannot), its alpha p6_loop's */
      const int32_t f0 = F[4 * i], f1 = F[4 * i + 1], f2 = F[4 * i + 2], f3 = F[4 * i + 3];
      bad |= (uint32_t)(f0 ^ 4096) | (uint32_t)(f1 ^ 4096) | (uint32_t)(f2 ^ 4096);
      const uint32_t A1 = (uint32_t)(int32_t)((float)(int32_t)f3 * i12 * acl) & 0xffu;
      a = (A1 >= 0x80 ? 0xffu : A1 * 2) << 24;
      if (A1 < amin) amin = A1;
    }
    if (SKIN) bad |= (uint32_t)PW > 4096u;
    p6u32 *w4 = (p6u32 *)o;
    if (SKIN) { const uint32_t w8 = (uint32_t)PW * 8u; w4[0] = (w8 & 0xffffu) | (32768u - w8) << 16; w4++; }
    w4[0] = (U0 & 0xffffu) | U1 << 16; w4[1] = d * 0x010101u | a;
    w4[2] = ((uint32_t)N0 & 0xffffu) | (uint32_t)N1 << 16; w4[3] = ((uint32_t)N2 & 0xffffu) | (uint32_t)P0 << 16;
    w4[4] = ((uint32_t)P1 & 0xffffu) | (uint32_t)P2 << 16;
    flags[i] = (uint16_t)(f | adc); andn &= f;
    if (zr) {   /* w as xconv.c xc_zr_lit takes it: of the position as the stream holds it, or of p6_loop's blend */
      float wz;
      if (SKIN) {
        const float p0 = (float)P0, p1 = (float)P1, p2 = (float)P2, w = (float)PW * i12, w1 = 1.0f - w;
        const float bp0 = ((b00 * p0 + b10 * p1) + b20 * p2) + b30, bp1 = ((b01 * p0 + b11 * p1) + b21 * p2) + b31;
        const float bp2 = ((b02 * p0 + b12 * p1) + b22 * p2) + b32;
        const float q0 = p0 * w + bp0 * w1, q1 = p1 * w + bp1 * w1, q2 = p2 * w + bp2 * w1;
        wz = q0 * za + q1 * zb + q2 * zc + zt;
      } else wz = (float)P0 * za + (float)P1 * zb + (float)P2 * zc + zt;
      if (wz > 0) { if (wz < lo) lo = wz; if (wz > hi) hi = wz; cnt++; }
    }
  }
  if ((uv >> 15) || bad) return -8;
  const int uv2 = !chk2 ? 0 : var ? 2 : 1;
  const uint32_t a1 = ((uint32_t)(n * st) + 15u) & ~15u;
  if (uv2 == 2) {   /* pass 2's own copy: UV2 */
    if (u2 >> 15) return -8;
    for (int i = 0; i < n; i++) {
      const p6u32 *s4 = (const p6u32 *)(v + st * i); p6u32 *d4 = (p6u32 *)(v + a1 + st * i);
      for (int j = 0; j < st / 4; j++) d4[j] = s4[j];
      d4[SKIN ? 1 : 0] = (uint32_t)(uint16_t)D[2 * i] | (uint32_t)(uint16_t)D[2 * i + 1] << 16;
    }
  }
  for (uint32_t q = (uint32_t)(n * st); q < a1; q++) { v[q] = 0; if (uv2 == 2) v[a1 + q] = 0; }
  L->uv2 = uv2; L->du = uv2 == 1 ? (int)du0 : 0; L->dv = uv2 == 1 ? (int)dv0 : 0;
  L->amin = (int)amin; L->cull = (L->hy & 4) && (andn & 0x30) != 0x30;
  if (zr) { zw[0] = lo; zw[1] = hi; *zn = cnt; }
  return n;
}
int vc_p06_c2v(const void *const *p, VcLit *L, const VcP6C2 *k, uint8_t *v, uint16_t *flags, const float *zr, float *zw, long *zn)
{
  switch (k->skin | k->tint << 1) {
    case 0: return p6_c2loop(p, L, k, v, flags, zr, zw, zn, 0, 0);
    case 1: return p6_c2loop(p, L, k, v, flags, zr, zw, zn, 1, 0);
    case 2: return p6_c2loop(p, L, k, v, flags, zr, zw, zn, 0, 1);
    default: return p6_c2loop(p, L, k, v, flags, zr, zw, zn, 1, 1);
  }
}
/* native step 2 (ge/nge.h NgeP6G): the parts of vc_p06_c2v's vertices that depend on the call, on a copy of a cooked group's (made by
   vc_p06_c2v with alpha 255): the alpha a2 of every vertex when it is not 255, then the d of the nb blended vertices bl[] (skinned,
   weight not 1.0) from the call's skin matrix, p6_c2loop's blend in its order and float mode (the normal and the weight read back from
   the vertex: the weight word's low half is 8 w); both in VB1 and, uv2 2, in VB2 at v + a1 */
void vc_p06_c2bl(uint8_t *v, int n, uint32_t a1, int uv2, const uint8_t *bl, int nb, const VcP6C2 *k)
{
  const int st = k->skin ? 24 : 20, co = k->skin ? 8 : 4, nc = uv2 == 2 ? 2 : 1;
  const uint32_t ac = k->a2 << 24;
  if (k->a2 != 0xffu)
    for (int c = 0; c < nc; c++) for (int i = 0; i < n; i++) { p6u32 *w = (p6u32 *)(v + c * a1 + (uint32_t)(st * i + co)); *w = (*w & 0xffffffu) | ac; }
  if (!k->skin || !nb) return;
  const float i12 = 1.0f / 4096, T254 = VC_C2T.f[254];
  const float b00 = k->b[0][0], b01 = k->b[0][1], b02 = k->b[0][2], b10 = k->b[1][0], b11 = k->b[1][1], b12 = k->b[1][2];
  const float b20 = k->b[2][0], b21 = k->b[2][1], b22 = k->b[2][2];
  for (int j = 0; j < nb; j++) {
    uint8_t *o = v + 24u * bl[j];
    const p6h16 *h = (const p6h16 *)o;
    const int32_t PW = (int32_t)((uint32_t)(uint16_t)h[0] >> 3), N0 = h[6], N1 = h[7], N2 = h[8];
    float n0 = (float)N0 * i12, n1 = (float)N1 * i12, n2 = (float)N2 * i12;
    const float w = (float)PW * i12, w1 = 1.0f - w;
    const float bn0 = (b00 * n0 + b10 * n1) + b20 * n2, bn1 = (b01 * n0 + b11 * n1) + b21 * n2, bn2 = (b02 * n0 + b12 * n1) + b22 * n2;
    n0 = n0 * w + bn0 * w1; n1 = n1 * w + bn1 * w1; n2 = n2 * w + bn2 * w1;
    const float s = (n0 * n0 + n1 * n1) + n2 * n2;
    const uint32_t d = s >= T254 ? 255u : c2_d(s), c = d * 0x010101u | ac;
    *(p6u32 *)(o + 8) = c;
    if (uv2 == 2) *(p6u32 *)(o + a1 + 8) = c;
  }
}
/* native step 2: vc_p06_c2v's depth range part (zr given) for a cooked group's n vertices at v (the position and, skinned, the weight
   read back from the vertex), in p6_c2loop's order and float mode: zw[0], zw[1] the least and largest w above 0, *zn their count */
void vc_p06_c2z(const uint8_t *v, int n, const VcP6C2 *k, const float *zr, float *zw, long *zn)
{
  const float i12 = 1.0f / 4096, za = zr[0], zb = zr[1], zc = zr[2], zt = zr[3];
  float lo = 3.0e38f, hi = 0; long cnt = 0;
  if (k->skin) {
    const float b00 = k->b[0][0], b01 = k->b[0][1], b02 = k->b[0][2], b10 = k->b[1][0], b11 = k->b[1][1], b12 = k->b[1][2];
    const float b20 = k->b[2][0], b21 = k->b[2][1], b22 = k->b[2][2], b30 = k->b[3][0], b31 = k->b[3][1], b32 = k->b[3][2];
    for (int i = 0; i < n; i++) {
      const p6h16 *h = (const p6h16 *)(v + 24 * i);
      const int32_t PW = (int32_t)((uint32_t)(uint16_t)h[0] >> 3), P0 = h[9], P1 = h[10], P2 = h[11];
      const float p0 = (float)P0, p1 = (float)P1, p2 = (float)P2, w = (float)PW * i12, w1 = 1.0f - w;
      const float bp0 = ((b00 * p0 + b10 * p1) + b20 * p2) + b30, bp1 = ((b01 * p0 + b11 * p1) + b21 * p2) + b31;
      const float bp2 = ((b02 * p0 + b12 * p1) + b22 * p2) + b32;
      const float q0 = p0 * w + bp0 * w1, q1 = p1 * w + bp1 * w1, q2 = p2 * w + bp2 * w1;
      const float wz = q0 * za + q1 * zb + q2 * zc + zt;
      if (wz > 0) { if (wz < lo) lo = wz; if (wz > hi) hi = wz; cnt++; }
    }
  } else
    for (int i = 0; i < n; i++) {
      const p6h16 *h = (const p6h16 *)(v + 20 * i);
      const int32_t P0 = h[7], P1 = h[8], P2 = h[9];
      const float wz = (float)P0 * za + (float)P1 * zb + (float)P2 * zc + zt;
      if (wz > 0) { if (wz < lo) lo = wz; if (wz > hi) hi = wz; cnt++; }
    }
  zw[0] = lo; zw[1] = hi; *zn = cnt;
}

/* ---- Stage C step 4 (vu1conv.h): P04+0x3740, the sprites ---- */
static float vdiv(float a, float b)   /* the VU's DIV as the interpreters model it: b = 0 gives the largest value of a's sign times b's */
{
  if (b == 0.0f) { uint32_t ua, ub, u; memcpy(&ua, &a, 4); memcpy(&ub, &b, 4); u = ((ua ^ ub) & 0x80000000u) | 0x7f7fffffu; float r; memcpy(&r, &u, 4); return r; }
  return vc_vu(a / b);
}
int vc_spr(const uint8_t *vm, uint32_t top, VcXform *x, VcVertF *v, uint8_t *drop, int max)
{
  const uint32_t *h = vq(vm, top + 0x14);
  int n = (int)(h[0] & 0xffff);
  if (n > max) return -1;
  vc_xform(vm, top, 0x10, 1.0f, x);
  static const float ID[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
  memcpy(x->world, ID, sizeof ID);
  x->n = n; x->clip = 1;
  const uint32_t *k7 = vq(vm, 0x07);
  const float fx = vfl(k7[0]), fy = vfl(k7[1]);
  int bad = 0, m = 0;
  for (int i = 0; i < n; i++) {
    const uint32_t *P = vq(vm, top + 0x100 + 6 * i), *C = vq(vm, top + 0x101 + 6 * i), *U0 = vq(vm, top + 0x102 + 6 * i),
                   *S = vq(vm, top + 0x104 + 6 * i), *U1 = vq(vm, top + 0x105 + 6 * i);
    for (int k = 0; k < 4; k++) if (C[k] > 255) bad = 1;
    if (U0[2] != 0x1000) bad = 1;
    const float p[3] = { vfl(P[0]), vfl(P[1]), vfl(P[2]) };
    float c[4]; vc_vu_clip(x->m, p, c);
    /* MSUB / MADD: corner = ACC (= c) -+ size f, the product rounded, then the sum (z: c.z -+ 0 f.z = c.z; w: c.w) */
    const float px = vc_vu((float)(int32_t)S[0] * fx), py = vc_vu((float)(int32_t)S[1] * fy);
    float x0 = vc_vu(c[0] - px), y0 = vc_vu(c[1] - py), x1 = vc_vu(c[0] + px), y1 = vc_vu(c[1] + py);
    const float aw = fabs_(c[3]), z = c[2];
    const int fz = (z > aw) << 4 | (z < -aw) << 5;
    const int f0 = (x0 > aw) | (x0 < -aw) << 1 | (y0 > aw) << 2 | (y0 < -aw) << 3 | fz, f1 = (x1 > aw) | (x1 < -aw) << 1 | (y1 > aw) << 2 | (y1 < -aw) << 3 | fz;
    const int fl = f0 << 6 | f1;   /* the VU's clip flags after both CLIPs: corner 0 in bits 6..11, corner 1 in 0..5 */
    if (fl & 0xd7a) { drop[i] = 1; continue; }
    drop[i] = 0;
    float u0 = (float)(int32_t)U0[0] / 4096.0f, v0 = (float)(int32_t)U0[1] / 4096.0f, u1 = (float)(int32_t)U1[0] / 4096.0f, v1 = (float)(int32_t)U1[1] / 4096.0f;
    if (fl) {
      const float Q = vdiv(1.0f, c[3]);
      float n0x = vc_vu(x0 * Q), n0y = vc_vu(y0 * Q), n1x = vc_vu(x1 * Q), n1y = vc_vu(y1 * Q);
      if (fl & 0x080) { const float q = vdiv(vc_vu(n1x + 1.0f), vc_vu(n1x - n0x)); n0x = -1.0f; u0 = vc_vu(u1 + vc_vu(vc_vu(u0 - u1) * q)); x0 = -c[3]; }
      if (fl & 0x200) { const float q = vdiv(vc_vu(n1y + 1.0f), vc_vu(n1y - n0y)); n0y = -1.0f; v0 = vc_vu(v1 + vc_vu(vc_vu(v0 - v1) * q)); y0 = -c[3]; }
      if (fl & 0x001) { const float q = vdiv(vc_vu(n0x - 1.0f), vc_vu(n0x - n1x)); n1x = 1.0f; u1 = vc_vu(u0 + vc_vu(vc_vu(u1 - u0) * q)); x1 = c[3]; }
      if (fl & 0x004) { const float q = vdiv(vc_vu(n0y - 1.0f), vc_vu(n0y - n1y)); n1y = 1.0f; v1 = vc_vu(v0 + vc_vu(vc_vu(v1 - v0) * q)); y1 = c[3]; }
    }
    const uint32_t col = (C[0] & 0xffu) | (C[1] & 0xffu) << 8 | (C[2] & 0xffu) << 16 | vc_a2((int)(C[3] & 0xffu)) << 24;
    VcVertF *a = &v[2 * m], *b = a + 1;
    a->u = u0; a->v = v0; a->c = col; a->x = x0; a->y = y0; a->z = c[3];
    b->u = u1; b->v = v1; b->c = col; b->x = x1; b->y = y1; b->z = c[3];
    m++;
  }
  return bad ? -4 : n;
}

/* ---- Stage C step 4 (vu1conv.h): P04+0x3a38, the rotated quads ---- */
int vc_quad(const uint8_t *vm, uint32_t top, VcXform *x, VcVertF *v, VcCv *cv, uint8_t *st, int max)
{
  const uint32_t *h = vq(vm, top + 0x14);
  int n = (int)(h[0] & 0xffff);
  if (n > max) return -1;
  vc_xform(vm, top, 0x10, 1.0f, x);
  static const float ID[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
  memcpy(x->world, ID, sizeof ID);
  x->n = 4 * n; x->clip = 1;
  const uint32_t *k7 = vq(vm, 0x07);
  const float fx = vfl(k7[0]), fy = vfl(k7[1]);
  int bad = 0;
  for (int i = 0; i < n; i++) {
    const uint32_t *P = vq(vm, top + 0x100 + 12 * i), *C = vq(vm, top + 0x101 + 12 * i), *U0 = vq(vm, top + 0x102 + 12 * i),
                   *S = vq(vm, top + 0x103 + 12 * i), *U1 = vq(vm, top + 0x104 + 12 * i);
    for (int k = 0; k < 4; k++) if (C[k] > 255) bad = 1;
    if (U0[2] != 0x1000 || U1[2] != 0x1000) bad = 1;
    const float p[3] = { vfl(P[0]), vfl(P[1]), vfl(P[2]) };
    float c[4]; vc_vu_clip(x->m, p, c);
    const float sx = (float)(int32_t)S[0], sy = (float)(int32_t)S[1];
    const float axx = vc_vu(fx * sx), ayy = vc_vu(fy * sy), axy = vc_vu(fx * sy), ayx = vc_vu(fy * sx);
    /* MSUB / MADD with ACC = c: corner 0 c - f s; 1 (c.x + fx sy, c.y - fy sx); 2 (c.x - fx sy, c.y + fy sx); 3 c + f s */
    const float X[4] = { vc_vu(c[0] - axx), vc_vu(c[0] + axy), vc_vu(c[0] - axy), vc_vu(c[0] + axx) };
    const float Y[4] = { vc_vu(c[1] - ayy), vc_vu(c[1] - ayx), vc_vu(c[1] + ayx), vc_vu(c[1] + ayy) };
    const float u0 = (float)(int32_t)U0[0] / 4096.0f, v0 = (float)(int32_t)U0[1] / 4096.0f, u1 = (float)(int32_t)U1[0] / 4096.0f, v1 = (float)(int32_t)U1[1] / 4096.0f;
    const float U[4] = { u0, u1, u0, u1 }, V[4] = { v0, v0, v1, v1 };
    const float aw = fabs_(c[3]), z = c[2];
    const int fz = (z > aw) << 4 | (z < -aw) << 5;
    int fa = 0x3f, fo = 0;
    for (int k = 0; k < 4; k++) { const int f = (X[k] > aw) | (X[k] < -aw) << 1 | (Y[k] > aw) << 2 | (Y[k] < -aw) << 3 | fz; fa &= f; fo |= f; }
    st[i] = fa ? 1 : fo ? 2 : 0;
    const uint32_t col = (C[0] & 0xffu) | (C[1] & 0xffu) << 8 | (C[2] & 0xffu) << 16 | vc_a2((int)(C[3] & 0xffu)) << 24;
    for (int k = 0; k < 4; k++) {
      VcVertF *o = &v[4 * i + k]; o->u = U[k]; o->v = V[k]; o->c = col; o->x = X[k]; o->y = Y[k]; o->z = c[3];
      if (st[i] == 2) {
        VcCv *q = &cv[4 * i + k];
        q->c[0] = X[k]; q->c[1] = Y[k]; q->c[2] = z; q->c[3] = c[3];
        for (int j = 0; j < 4; j++) q->col[j] = (float)(int32_t)C[j];
        q->uv[0] = U[k]; q->uv[1] = V[k]; q->uv[2] = 1.0f;
        q->p[0] = X[k]; q->p[1] = Y[k]; q->p[2] = c[3];
      }
    }
  }
  return bad ? -4 : n;
}

/* ---- Stage C step 4 (vu1conv.h): P04+0x2b60, the line strips, and its clipper ---- */
int vc_line(const uint8_t *vm, uint32_t top, VcXform *x, VcVertF *v, uint16_t *flags, float (*cc)[4], uint8_t *cls, int max, int tme)
{
  const uint32_t *h = vq(vm, top + 0x14);
  int n = (int)(h[0] & 0xffff);
  if (n > max) return -1;
  vc_xform(vm, top, 0x10, 1.0f, x);
  x->n = n; x->clip = 1;
  int bad = 0, pf = 0;
  for (int i = 0; i < n; i++) {
    const uint32_t *p = vq(vm, top + 0x100 + 3 * i), *c = vq(vm, top + 0x101 + 3 * i), *t = vq(vm, top + 0x102 + 3 * i);
    for (int k = 0; k < 4; k++) if (c[k] > 255) bad = 1;
    if (tme && t[2] != 0x1000) bad = 1;   /* untextured: the GS takes no ST or Q from it (the deck's lines at vb 24,667 hold old data there) */
    v[i].x = vfl(p[0]); v[i].y = vfl(p[1]); v[i].z = vfl(p[2]);
    v[i].u = (float)(int32_t)t[0] / 4096.0f; v[i].v = (float)(int32_t)t[1] / 4096.0f;
    v[i].c = (c[0] & 0xffu) | (c[1] & 0xffu) << 8 | (c[2] & 0xffu) << 16 | vc_a2((int)(c[3] & 0xffu)) << 24;
    flags[i] = (uint16_t)t[3];
    const float pp[3] = { v[i].x, v[i].y, v[i].z };
    vc_vu_clip(x->m, pp, cc[i]);
    const int f = vc_cflags(cc[i]);
    cls[i] = i == 0 || (flags[i] & 0x8000) ? 0 : !(f | pf) ? 1 : (f & pf) ? 2 : 4;
    pf = f;
  }
  return bad ? -4 : n;
}
/* one plane of the line clipper (p4.lst 0x31a0 .. 0x36d8): bit (0..5) and axis of the plane, s = +1 / -1 */
static void seg_plane(VcCv *e, int bit, int axis, float s)
{
  VcCv *a = &e[0], *b = &e[1];
  const float da = vc_vu(a->c[3] - s * a->c[axis]), db = vc_vu(b->c[3] - s * b->c[axis]);
  const float q = vdiv(1.0f, vc_vu(db - da));
  VcCv r;
  for (int j = 0; j < 4; j++) r.c[j] = vc_vu(vc_vu(a->c[j] * db) - vc_vu(b->c[j] * da));
  r.c[axis] = s * r.c[3];   /* ADDW / SUBW: 0 +- the new w (exact) */
  for (int j = 0; j < 4; j++) r.c[j] = vc_vu(r.c[j] * q);
  for (int j = 0; j < 4; j++) r.col[j] = vc_vu(vc_vu(vc_vu(a->col[j] * db) - vc_vu(b->col[j] * da)) * q);
  for (int j = 0; j < 3; j++) r.uv[j] = vc_vu(vc_vu(vc_vu(a->uv[j] * db) - vc_vu(b->uv[j] * da)) * q);
  for (int j = 0; j < 3; j++) r.p[j] = (a->p[j] * db - b->p[j] * da) * q;   /* (ours: the world position for the GE, as uv) */
  /* the end replaced: b if b is outside the plane, else a */
  if (vc_cflags(b->c) >> bit & 1) *b = r; else *a = r;
}
int vc_clip_seg(const VcCv in[2], VcCv out[2])
{
  VcCv e[2] = { in[0], in[1] };
  int fa = vc_cflags(e[0].c), fb = vc_cflags(e[1].c);
  if (fa & fb) return 0;
  if ((fa | fb) & 0x10) { seg_plane(e, 4, 2, 1.0f); fa = vc_cflags(e[0].c); fb = vc_cflags(e[1].c); if (fa & fb) return 0; }
  for (int k = 0; k < 2; k++) for (int j = 0; j < 4; j++) e[k].col[j] = vc_vu(e[k].col[j] * e[k].c[3]);   /* 0x2508 */
  fa = vc_cflags(e[0].c); fb = vc_cflags(e[1].c);
  static const int BIT[5] = { 5, 0, 1, 2, 3 }, AX[5] = { 2, 0, 0, 1, 1 }; static const float SG[5] = { -1, 1, -1, 1, -1 };
  for (int p = 0; p < 5; p++) if ((fa | fb) >> BIT[p] & 1) {
    seg_plane(e, BIT[p], AX[p], SG[p]);
    fa = vc_cflags(e[0].c); fb = vc_cflags(e[1].c);
    if (fa & fb) return 0;
  }
  out[0] = e[0]; out[1] = e[1];
  return 2;
}
