/* vu1conv.c: see vu1conv.h. */
#include "vu1conv.h"
#include <math.h>
#include <string.h>

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

int vc_fpos(const uint8_t *vm, uint32_t top, VcXform *x, VcVertF *v, uint16_t *flags, int max)
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
    if (t[2] != 0x1000) bad = 1;
    v[i].x = vfl(p[0]); v[i].y = vfl(p[1]); v[i].z = vfl(p[2]);
    v[i].u = (float)(int32_t)t[0] / 4096.0f; v[i].v = (float)(int32_t)t[1] / 4096.0f;
    v[i].c = (c[0] & 0xffu) | (c[1] & 0xffu) << 8 | (c[2] & 0xffu) << 16 | vc_a2((int)(c[3] & 0xffu)) << 24;
    flags[i] = (uint16_t)t[3];
  }
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
static inline __attribute__((always_inline)) void p6_loop(const P6F *k, const uint32_t *q, int n, uint32_t adc, uint8_t *o1, uint8_t *o3,
  uint16_t *flags, uint32_t *rgba, P6Chk *ck, const int SKIN, const int TINT, const int NL, const int P3, const int CHK2)
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
  if (CHK2) { du0 = q[12] - q[8]; dv0 = q[13] - q[9]; }
  for (int i = 0; i < n; i++, q += 24) {
    const uint32_t P0 = q[0], P1 = q[1], P2 = q[2], U0 = q[8], U1 = q[9], f = q[7];
    float n0 = (float)(int32_t)q[4] * i12, n1 = (float)(int32_t)q[5] * i12, n2 = (float)(int32_t)q[6] * i12, p0 = 0, p1 = 0, p2 = 0;
    if (SKIN) {   /* p = p w + (B (p, 1)) (1 - w), n = n w + (B3 n) (1 - w) */
      p0 = (float)(int32_t)P0; p1 = (float)(int32_t)P1; p2 = (float)(int32_t)P2;
      const float w = (float)(int32_t)q[3] * i12, w1 = 1.0f - w;
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
      const uint32_t f0 = q[20], f1 = q[21], f2 = q[22], f3 = q[23];
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
    uv |= U0 | U1; z |= q[10] ^ 0x1000u;
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
    if (CHK2) { var |= ((q[12] - U0) ^ du0) | ((q[13] - U1) ^ dv0); z2 |= q[14] ^ 0x1000u; }
  }
  ck->pos = pos; ck->uv = uv; ck->z = z; ck->z2 = z2; ck->var = var; ck->fac = fac; ck->andn = andn; ck->amin = TINT ? amin : a1;
  ck->du = du0; ck->dv = dv0;
}
static uint32_t p6_fl(uint32_t u) { return (u & 0x7f800000u) ? u : u & 0x80000000u; }   /* vfl for a value not above 2^20 in size */
int vc_p06(const uint8_t *vm, uint32_t top, uint32_t entry, VcLit *L, void *v1, void *v3, uint16_t *flags, uint32_t *rgba, int max)
{
  int kind = entry == VC_P06 ? 0 : entry == VC_P06S ? 1 : entry == VC_P06T ? 2 : entry == VC_P06ST ? 3 : -1;
  if (kind < 0) return -5;
  const uint32_t *h = vq(vm, top + 0x14);
  int n = (int)(h[0] & 0xffff);
  if (n > max) return -1;
  uint32_t hy = h[1] & 0xffff, q0 = (top + 0x100) & 0x3ffu;
  int skin = kind & 1, tint = kind >> 1, p3 = (hy & 0x100) ? -1 : (hy & 0x600) ? ((hy & 0x400) ? 2 : 1) : 0, chk2 = (hy & 0x10) != 0;
  if (!n || vc_p06_exact || p3 == 2 || (p3 == 1) != chk2 || q0 + 6u * (uint32_t)n > 1024u) return p06_any(vm, top, entry, L, v1, v3, flags, rgba, max);
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
  if (mx >= 0x49800000u || (neg >> 31) || !nl) return p06_any(vm, top, entry, L, v1, v3, flags, rgba, max);
  k.a1 = (uint32_t)p6_ftoi(k.acl) & 0xffu; k.a2 = k.a1 >= 0x80 ? 0xffu : k.a1 * 2;
  uint32_t vi6 = ((h[2] & 0xffff) - 0x8000u) & 0xffff;
  const uint32_t *q = (const uint32_t *)(vm + (q0 << 4));
  P6Chk ck;
  /* a loop for each kind, number of lit lights and passes (2 and 3, or neither) */
  uint8_t *o1 = (uint8_t *)v1, *o3 = (uint8_t *)v3; uint32_t adc = vi6 & 0x8000;
  int sel = skin | tint << 1 | (nl - 1) << 2 | chk2 << 4;
#define P6L(sk, ti, nl_, pc) case (sk | ti << 1 | (nl_ - 1) << 2 | pc << 4): \
    p6_loop(&k, q, n, adc, o1, o3, flags, rgba, &ck, sk, ti, nl_, pc, pc); break;
#define P6L4(nl_, pc) P6L(0, 0, nl_, pc) P6L(1, 0, nl_, pc) P6L(0, 1, nl_, pc) P6L(1, 1, nl_, pc)
  switch (sel) { P6L4(1, 0) P6L4(2, 0) P6L4(3, 0) P6L4(1, 1) P6L4(2, 1) P6L4(3, 1) default: return -5; }
#undef P6L4
#undef P6L
  const uint32_t any = (ck.uv >> 15) | ck.z | (skin ? 0 : ck.pos >> 16) | (tint ? ck.fac >> 23 : 0);
  if (any) return p06_any(vm, top, entry, L, v1, v3, flags, rgba, max);
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
int vc_p06_uv2(const uint8_t *vm, uint32_t top, const VcLit *L, const void *v1, void *v2)
{
  const uint32_t st = L->kind == 4 ? 7 : 6;   /* P07 (kind 4): 7 quadwords a vertex */
  int u16 = 1, t1 = (L->uv16 ? 0 : 1) | (L->pos16 ? 0 : 2);
  for (int i = 0; i < L->n; i++) { const uint32_t *U2 = vq(vm, top + 0x103 + st * i); if (U2[0] > 32767 || U2[1] > 32767) u16 = 0; }
  int t2 = (u16 ? 0 : 1) | (L->pos16 ? 0 : 2), s1 = P6SZ[t1], s2 = P6SZ[t2];
  for (int i = 0; i < L->n; i++) {
    const uint32_t *U2 = vq(vm, top + 0x103 + st * i);
    const uint8_t *a = (const uint8_t *)v1 + s1 * i;
    uint32_t c, P[3] = { 0, 0, 0 }; float p[3] = { 0, 0, 0 }, uvf[2];
    memcpy(&c, a + 4 + 4 * (t1 & 1), 4);   /* the colour follows the UV: 4 bytes (16-bit) or 8 (floats) */
    if (L->pos16) { int16_t q[3]; memcpy(q, a + 8 + 4 * (t1 & 1), 6); for (int j = 0; j < 3; j++) P[j] = (uint32_t)(int32_t)q[j]; }
    else memcpy(p, a + 8 + 4 * (t1 & 1), 12);
    uvf[0] = (float)(int32_t)U2[0] * (1.0f / 4096); uvf[1] = (float)(int32_t)U2[1] * (1.0f / 4096);
    p6_put((uint8_t *)v2 + s2 * i, t2, U2[0], U2[1], uvf, c, P, p);
  }
  return t2;
}

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
static inline __attribute__((always_inline)) void p7_loop(const uint8_t *vm, const P7F *k, const uint32_t *q, int n, uint8_t *o1,
  uint8_t *o3, uint16_t *flags, uint32_t *rgba, P7Chk *ck, const int NB, const int P23)
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
  memset(ok, 0, sizeof ok);
  if (P23) { du0 = q[12] - q[8]; dv0 = q[13] - q[9]; }
  ck->bad = 1;
  for (int i = 0; i < n; i++, q += 28) {
    const float pi0 = (float)(int32_t)q[0] * 0.0625f, pi1 = (float)(int32_t)q[1] * 0.0625f, pi2 = (float)(int32_t)q[2] * 0.0625f;
    const float ni0 = (float)(int32_t)q[4] * (1.0f / 4096), ni1 = (float)(int32_t)q[5] * (1.0f / 4096), ni2 = (float)(int32_t)q[6] * (1.0f / 4096);
    nw |= (q[4] + 0x10000u) | (q[5] + 0x10000u) | (q[6] + 0x10000u);
    float p0 = 0, p1 = 0, p2 = 0, n0 = 0, n1 = 0, n2 = 0, w0 = 0;
    for (int b = 0; b < NB; b++) {   /* bone b: s_b = M (p, 1), n_b = M3 n, then the blend (p7_vertex) */
      const uint32_t m = ((q[24 + b] & 0xffffu) + 32u) & 0x3ffu;
      if (!(ok[m >> 5] >> (m & 31) & 1u)) {
        if (m > 1020u || !p7_mok(vm, m)) return;
        ok[m >> 5] |= 1u << (m & 31);
      }
      const p6f32 *M = (const p6f32 *)(vm + (m << 4));
      const float e0 = ((M[0] * pi0 + M[4] * pi1) + M[8] * pi2) + M[12], e1 = ((M[1] * pi0 + M[5] * pi1) + M[9] * pi2) + M[13];
      const float e2 = ((M[2] * pi0 + M[6] * pi1) + M[10] * pi2) + M[14];
      const float f0 = (M[0] * ni0 + M[4] * ni1) + M[8] * ni2, f1 = (M[1] * ni0 + M[5] * ni1) + M[9] * ni2;
      const float f2 = (M[2] * ni0 + M[6] * ni1) + M[10] * ni2;
      if (b == 0) { p0 = e0; p1 = e1; p2 = e2; n0 = f0; n1 = f1; n2 = f2; continue; }
      nw |= q[20 + b] + 0x10000u;
      const float wb = (float)(int32_t)q[20 + b] * 0.0078125f;
      if (b == 1) {
        nw |= q[20] + 0x10000u; w0 = (float)(int32_t)q[20] * 0.0078125f;
        p0 = p0 * w0 + e0 * wb; p1 = p1 * w0 + e1 * wb; p2 = p2 * w0 + e2 * wb;
        n0 = n0 * w0 + f0 * wb; n1 = n1 * w0 + f1 * wb; n2 = n2 * w0 + f2 * wb;
      } else {
        p0 = (p0 + 0.0f) + e0 * wb; p1 = (p1 + 0.0f) + e1 * wb; p2 = (p2 + 0.0f) + e2 * wb;
        n0 = (n0 + 0.0f) + f0 * wb; n1 = (n1 + 0.0f) + f1 * wb; n2 = (n2 + 0.0f) + f2 * wb;
      }
    }
    if (NB > 1) {   /* ERLENG, then MULw */
      const float sq = (n0 * n0 + n1 * n1) + n2 * n2, r = sq > 0.0f ? 1.0f / sqrtf(sq) : 3.4028235e38f;
      n0 = n0 * r; n1 = n1 * r; n2 = n2 * r;
    }
    float d0 = (l00 * n0 + l10 * n1) + l20 * n2, d1 = (l01 * n0 + l11 * n1) + l21 * n2, d2 = (l02 * n0 + l12 * n1) + l22 * n2;
    d0 = d0 > 0.0f ? d0 : 0.0f; d1 = d1 > 0.0f ? d1 : 0.0f; d2 = d2 > 0.0f ? d2 : 0.0f;
    float cr = ((c00 * d0 + c10 * d1) + c20 * d2) + ar, cg = ((c01 * d0 + c11 * d1) + c21 * d2) + ag;
    float cb = ((c02 * d0 + c12 * d1) + c22 * d2) + ab;
    cr = cr < cmax ? cr : cmax; cg = cg < cmax ? cg : cmax; cb = cb < cmax ? cb : cmax;
    const uint32_t rgb = ((uint32_t)(int32_t)cr & 0xffu) | ((uint32_t)(int32_t)cg & 0xffu) << 8 | ((uint32_t)(int32_t)cb & 0xffu) << 16;
    const uint32_t c = rgb | a2 << 24, U0 = q[8], U1 = q[9];
    if (rgba) rgba[i] = rgb | a1 << 24;
    uv |= U0 | U1; z |= q[10] ^ 0x1000u;
    p6u32 *w = (p6u32 *)o1; w[0] = U0 | U1 << 16; w[1] = c;
    p6f32 *wf = (p6f32 *)o1; wf[2] = p0; wf[3] = p1; wf[4] = p2; o1 += 20;
    if (P23) {
      const float u3 = ((r00 * n0 + r10 * n1) + r20 * n2) * s0 + t0, v3 = ((r01 * n0 + r11 * n1) + r21 * n2) * s1 + t1;
      p6f32 *e = (p6f32 *)o3; e[0] = u3; e[1] = v3; ((p6u32 *)o3)[2] = c; e[3] = p0; e[4] = p1; e[5] = p2; o3 += 24;
      var |= ((q[12] - U0) ^ du0) | ((q[13] - U1) ^ dv0); z2 |= q[14] ^ 0x1000u;
    }
    flags[i] = (uint16_t)q[3];
  }
  ck->uv = uv; ck->z = z; ck->z2 = z2; ck->var = var; ck->nw = nw; ck->du = du0; ck->dv = dv0; ck->bad = 0;
}
static uint32_t p7_mx(uint32_t mx, uint32_t u) { u &= 0x7fffffffu; return u > mx ? u : mx; }
int vc_p07(const uint8_t *vm, uint32_t top, VcLit *L, void *v1, void *v3, uint16_t *flags, uint32_t *rgba, int max)
{
  const uint32_t *h = vq(vm, top);
  const int n = (int)(h[0] & 0xffff);
  const uint32_t fl = h[3] & 0xffff, nb = h[2] & 0xffff, q0 = (top + 0x100) & 0x3ffu;
  const int p23 = (fl & 0x210) == 0x210;
  if (n > max) return -1;
  if (!n || vc_p06_exact || (fl & 0xd04) || (vq(vm, top + 1)[0] & 0xffff) || nb < 1 || nb > 4 || ((fl & 0x210) && !p23) ||
      q0 + 7u * (uint32_t)n > 1024u) return p07_any(vm, top, L, v1, v3, flags, rgba, max);
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
  if (mx >= 0x49800000u || (neg >> 31)) return p07_any(vm, top, L, v1, v3, flags, rgba, max);
  const uint32_t *q = (const uint32_t *)(vm + (q0 << 4));
  uint8_t *o1 = (uint8_t *)v1, *o3 = (uint8_t *)v3;
  P7Chk ck;
#define P7L(nb_, pc) case (nb_ | pc << 3): p7_loop(vm, &k, q, n, o1, o3, flags, rgba, &ck, nb_, pc); break;
  switch ((int)nb | p23 << 3) { P7L(1, 0) P7L(2, 0) P7L(3, 0) P7L(4, 0) P7L(1, 1) P7L(2, 1) P7L(3, 1) P7L(4, 1) default: return -5; }
#undef P7L
  if (ck.bad || (ck.nw >> 17) || (ck.uv >> 15) || ck.z) return p07_any(vm, top, L, v1, v3, flags, rgba, max);
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
