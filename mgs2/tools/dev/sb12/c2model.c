/* c2model.c (sb12, task #140; x86 only): C2 for the people (P06: Snake, the crew, the guards) sized before any GE code.  For each
   P06 call the picture code converts (xconv.c xc_lit), with its exact results in hand, this file
   - counts what the GE would have to represent (the census): the clamp abs 3 x and the alpha abs 3 y, the lit lights (direction
     lengths, colour times length, colours below 0), the ambient, the skin's weight (the position's w, 4.12) and its matrix (column
     lengths, angles, translation), the normals' lengths (as the data holds them, and blended by the skin), the guards' factor (4.12 per
     channel), the vertices whose lit sum reaches the clamp, passes 2 and 3 by kind, and the volumes in the gl14 model windows;
   - computes the same vertices as the GE would under C2's mapping (stagec_notes.md step 2) in PPSSPP master's GE arithmetic
     (83e616ea, GPU/Software/GEMath.h, TransformUnit.cpp ApplyGESkinning, Lighting.cpp ProcessSIMD; its authors matched them to hardware
     probes), scalar replicas here: float24 products and adds (GEMulFloat24, GEAdd), row sums (GERowSum4), dot products and normalising
     (GEDot3, GENormalize4) with the reciprocal square root approximated (1 / sqrtf truncated to a float24: master uses measured segment
     tables), the integer light sum (LightColorFactor 2c + 1, GELightColorProduct, GELightColorScale, the ambient product):
       - WORLD identity, VIEW the call's world rows (today's, so the unskinned positions are today's bit for bit);
       - skinned entries: two bones, bone 0 = 32768 I, bone 1 = (32768 B3, B translation), rounded to nearest float24, 16-bit weights
         w * 8 and 32768 - w * 8 (exact: w is 4.12);
       - lights: the lit ones, direction = the light's column normalised (sent rounded to nearest float24, the GE normalises again),
         diffuse colour = the PS2's colour row times the column's length rounded to 8 bits; global ambient = the ambient rounded; material
         white (alpha 255) and the global ambient's alpha the call's GE alpha, or for the factor entries the material's ambient and diffuse
         from the vertex colour: the factor in 8 bits (two encodings, A round(f 255 / 4096), B round(f / 16)) with the PS2's GE alpha per
         vertex and an ambient alpha of 255;
       - pass 3 by the texture matrix from the normal (mode 3, as the data holds it: x 8 or / 4096 folded into the matrix) and, to compare,
         from the normalised normal (mode 2);
   - compares those with today's exact vertices: colours (levels, GE minus exact, by entry), alpha, skinned positions (world units and GE
     pixels through the call's own transform), pass 3's UVs.
   It only reads: every run with it stays exact.  MGS2_C2_MODEL=1 turns it on; c2m_report prints the totals. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vu1conv.h"
#include "c2model.h"

int c2m_on;
long c2m_vb;
static const uint32_t *Q(const uint8_t *vm, uint32_t qa) { return (const uint32_t *)(vm + ((qa & 0x3ffu) << 4)); }
static float F(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint32_t U(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
/* a float as the VU reads it (vu1conv.c vfl) */
static float vfl(uint32_t u)
{
  uint32_t e = u & 0x7f800000u;
  if (!e) u &= 0x80000000u; else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu;
  return F(u);
}

/* ---- PPSSPP master's GE arithmetic, scalar (GEMath.h) ---- */
static float f24t(float f) { return F(U(f) & 0xffffff00u); }                         /* TruncateToFloat24 */
static float f24r(float f) { uint32_t u = U(f); if ((u & 0x7f800000u) == 0x7f800000u) return f; return F((u + 0x80u) & ~0xffu); }   /* putr */
static float pdf24(double d)                                                         /* ProductToFloat24 */
{
  uint64_t b; memcpy(&b, &d, 8);
  if (((b >> 52) & 0x7ff) != 0x7ff) b &= ~((1ull << (52 - 15)) - 1);
  memcpy(&d, &b, 8); return (float)d;
}
static float ge_add0(float a, float b)                                               /* GEAdd */
{
  uint32_t ba = U(a), bb = U(b); const int ea = (int)(ba >> 23 & 0xff), eb = (int)(bb >> 23 & 0xff);
  if (ea == 0 || eb == 0) return ea == 0 ? (eb == 0 ? 0.0f : f24t(b)) : f24t(a);
  const int em = ea > eb ? ea : eb, na = 8 + em - ea, nb = 8 + em - eb;
  ba = na >= 24 ? (ba & 0x80000000u) : (ba & ~((1u << na) - 1));
  bb = nb >= 24 ? (bb & 0x80000000u) : (bb & ~((1u << nb) - 1));
  return F(ba) + F(bb);
}
static float ge_add(float a, float b) { return f24t(ge_add0(a, b)); }                /* GEAddFloat24 */
static float ge_mul(float a, float b)                                                /* GEMulFloat24 (operands truncated first) */
{
  const uint32_t ba = U(a) & 0xffffff00u, bb = U(b) & 0xffffff00u;
  const int ea = (int)(ba >> 23 & 0xff), eb = (int)(bb >> 23 & 0xff);
  if (!(ba & 0x7fffffffu) || !(bb & 0x7fffffffu) || ea == 0 || eb == 0) return 0.0f;
  const int32_t af = (int32_t)(ba >> 8 & 0x7fff), bf = (int32_t)(bb >> 8 & 0x7fff);
  const int32_t prod = af + 32768 + bf + ((bf * af) >> 15);
  const float r = ldexpf((float)prod, ea + eb - 254 - 15);
  return F((U(r) & 0xffffff00u) | ((ba ^ bb) & 0x80000000u));
}
/* GERowSum: sum_k a[k] b[k], each term the significands' product to 15 fraction bits, all aligned to the largest term's lsb (toward
   zero), added exactly, the sum made a float24 */
static float ge_rowsum(const float *a, const float *b, int count)
{
  int32_t m[4]; int lsb[4], lmax = -100000;
  for (int k = 0; k < count; k++) {
    const uint32_t ba = U(a[k]) & 0xffffff00u, bb = U(b[k]) & 0xffffff00u;
    const int ea = (int)(ba >> 23 & 0xff), eb = (int)(bb >> 23 & 0xff);
    if (ea == 0 || eb == 0) { m[k] = 0; lsb[k] = -100000; continue; }
    const int32_t af = (int32_t)(ba >> 8 & 0x7fff), bf = (int32_t)(bb >> 8 & 0x7fff);
    int32_t p = af + 32768 + bf + ((bf * af) >> 15);
    if ((ba ^ bb) & 0x80000000u) p = -p;
    m[k] = p; lsb[k] = ea + eb - 254 - 15;
    if (lsb[k] > lmax) lmax = lsb[k];
  }
  if (lmax == -100000) return 0.0f;
  int64_t s = 0;
  for (int k = 0; k < count; k++) {
    if (lsb[k] == -100000) continue;
    const int sh = lmax - lsb[k];
    if (sh >= 40) continue;
    const double v = (double)m[k] / (double)(1ll << sh);
    s += (int64_t)v;   /* toward zero */
  }
  if (!s) return 0.0f;
  return f24t(ldexpf((float)s, lmax));
}
static float ge_dot3(const float *a, const float *b) { return ge_rowsum(a, b, 3); }
static float ge_rsqrt(float d) { return f24t(1.0f / sqrtf(d)); }                   /* (approximation: master's segment tables) */
static float ge_norm(float *v)                                                       /* GENormalize4: v made unit, returns its length */
{
  const float d2 = ge_dot3(v, v);
  if (!(d2 > 0.0f) || !isfinite(d2)) return 0.0f;
  const float r = ge_rsqrt(d2);
  for (int i = 0; i < 3; i++) v[i] = ge_mul(r, v[i]);
  return pdf24((double)d2 * r);
}
static int lfac(int c) { return 2 * c + 1; }                                         /* LightColorFactor */
static int lprod(int lf, int mf) { return (lf * mf) >> 10; }                         /* GELightColorProduct */
static int lscale(int x, float f) { int s = (int)(256.0f * f); s = s < 0 ? 0 : s > 256 ? 256 : s; return ((2 * x + 1) * (2 * s + 1)) >> 10; }

/* ---- the totals ---- */
#define NV 8
typedef struct { uint32_t v; long n; } Val;
static void val_add(Val *t, int k, uint32_t v) { for (int i = 0; i < k; i++) { if (t[i].n && t[i].v == v) { t[i].n++; return; } if (!t[i].n) { t[i].v = v; t[i].n = 1; return; } } t[k - 1].n++; }
#define NWIN 2
static const long WIN[NWIN][2] = { { 30810, 30890 }, { 33410, 33490 } };   /* gl14's model windows: the waiting deck, inside */
static struct {
  long calls, verts, kind[4], kind_v[4], fast, any, nl[4], uv2h[4], p3h[4], cull;
  Val cmax[NV], alpha[NV];
  double llen_min, llen_max, ld_max, amb_min, amb_max;
  long ld_frac, ld_over, ld_neg, amb_frac, amb_over, lights;
  /* skin */
  long w_n, w_out, w_zero, w_one; int w_min, w_max; double bcol_min, bcol_max, borth, bt_max; long bcalls;
  /* normals: |n| - 1 in buckets (< 1e-3, < 1e-2, < 0.05, < 0.1, < 0.25, from 0.25), the data's (unskinned) and the blended */
  long nlen_u[6], nlen_s[6]; double nmin_u, nmax_u, nmin_s, nmax_s;
  /* the factor */
  int f_min[4], f_max[4]; long f_over, f_neg, f_n;
  /* the clamp */
  long clamp_v, clamp_tv;
  /* the model */
  long elig, elig_v, why[10];
  long dch[4][11], dchB[11]; double dc_sum[4], dc_sumB; long dc_n[4], dc_max[4], dc_nB, dc_maxB;
  long da_bad, da_n;
  double dp_max, dp_sum; long dp_n, dsh[6], behind;
  double du3_max[2], du3_sum[2]; long du3_n, du3h[2][6];
  /* model (b): emissive = the ambient, the material's ambient and diffuse from the vertex colour (d, d, d, a), d the PS2's normal length
     in 8 bits, two encodings (0: round(255 len), 1: min(255, (int)(256 len))); fb_c, fb_v: factor calls left to the converter (an RGB
     factor other than 4096) */
  long dchE[2][4][11]; double dcE_sum[2][4]; long dcE_n[2][4], dcE_max[2][4], fb_c, fb_v, dhist[9];
  /* the passes' GS state: pass, tfx, tcc, a GE texture; blending; fog */
  long ps[4][4][2][2], pab[4][2], pfg[4][2], ptm[4][2]; Val pbl[4][NV];
  /* the vertex cache census: a call's streams by address (A, B, C, D when pass 2 has its own UVs, F with a factor), its count and entry;
     the bytes behind them hashed (FNV-1a 64) */
  long ck_calls, ck_new, ck_same, ck_other, ck_full, ck_bytes, ck_vb, ck_wnew[NWIN], ck_wcall[NWIN], ck_skb, ck_skbv;
  /* windows */
  long wc[NWIN], wv[NWIN], wsk[NWIN], wp2[NWIN], wp3[NWIN], wnl[NWIN][4], wfac[NWIN];
} T;
static int first = 1;
#define CKN (1 << 18)
typedef struct { const void *a, *b, *c, *d, *f; int n, kind; uint64_t h; long vb; } CkE;
static CkE *ck;
static uint64_t fnv(uint64_t h, const void *p, size_t len) { const uint8_t *q = (const uint8_t *)p; for (size_t i = 0; i < len; i++) { h ^= q[i]; h *= 0x100000001b3ull; } return h; }
/* one call into the cache census (sp: the lane's streams; own2: pass 2 has its own UVs) */
static void ck_call(const void *const *sp, int n, int kind, int own2, int blended)
{
  if (!sp) return;
  if (!ck) { ck = calloc(CKN, sizeof *ck); if (!ck) return; }
  const int tint = kind >> 1;
  const void *a = sp[0], *b = sp[1], *c = sp[2], *d = own2 ? sp[3] : 0, *f = tint ? sp[5] : 0;
  uint64_t h = 0xcbf29ce484222325ull;
  h = fnv(h, a, 8 * (size_t)n); h = fnv(h, b, 8 * (size_t)n); h = fnv(h, c, 4 * (size_t)n);
  if (d) h = fnv(h, d, 4 * (size_t)n);
  if (f) h = fnv(h, f, 8 * (size_t)n);
  uint64_t k = (uint64_t)(uintptr_t)a * 0x9e3779b97f4a7c15ull ^ (uint64_t)(uintptr_t)b * 0xc2b2ae3d27d4eb4full ^ (uint64_t)(uintptr_t)c ^
               (uint64_t)(uintptr_t)d * 31 ^ (uint64_t)(uintptr_t)f * 17 ^ (uint64_t)n << 40 ^ (uint64_t)kind << 56;
  T.ck_calls++;
  int w = -1; for (int j = 0; j < NWIN; j++) if (c2m_vb >= WIN[j][0] && c2m_vb < WIN[j][1]) w = j;
  if (w >= 0) T.ck_wcall[w]++;
  if (kind & 1) { T.ck_skb += blended > 0; T.ck_skbv += blended; }
  for (uint32_t i = (uint32_t)(k ^ k >> 29) & (CKN - 1), t = 0; t < CKN; t++, i = (i + 1) & (CKN - 1)) {
    CkE *e = &ck[i];
    if (!e->a) {   /* a new key */
      e->a = a; e->b = b; e->c = c; e->d = d; e->f = f; e->n = n; e->kind = kind; e->h = h; e->vb = c2m_vb;
      T.ck_new++; T.ck_bytes += (long)n * ((kind & 1) ? 24 : 20);
      if (w >= 0) T.ck_wnew[w]++;
      return;
    }
    if (e->a == a && e->b == b && e->c == c && e->d == d && e->f == f && e->n == n && e->kind == kind) {
      if (e->h == h) T.ck_same++; else { T.ck_other++; e->h = h; }
      return;
    }
  }
  T.ck_full++;
}
static void init(void)
{
  first = 0;
  T.llen_min = 1e30; T.llen_max = 0; T.ld_max = -1e30; T.amb_min = 1e30; T.amb_max = -1e30;
  T.w_min = 1 << 30; T.w_max = -(1 << 30); T.bcol_min = 1e30; T.bcol_max = 0;
  T.nmin_u = T.nmin_s = 1e30; T.nmax_u = T.nmax_s = 0;
  for (int c = 0; c < 4; c++) { T.f_min[c] = 1 << 30; T.f_max[c] = -(1 << 30); }
}
static int lbucket(double d) { d = fabs(d); return d < 1e-3 ? 0 : d < 1e-2 ? 1 : d < 0.05 ? 2 : d < 0.1 ? 3 : d < 0.25 ? 4 : 5; }
static int bucket_px(double d) { return d < 1.0 / 64 ? 0 : d < 1.0 / 16 ? 1 : d < 0.25 ? 2 : d < 0.5 ? 3 : d < 1 ? 4 : 5; }
static int dbin(int d) { return d < -5 ? 0 : d > 5 ? 10 : d + 5; }

/* vertex i's input words: the streams (A, B, C: halfwords, F) or the VU memory (T + 0x100 + 6 i: P, N, U, U2, -, S5) */
typedef struct { int32_t p[4], n[4], u[2], f[4]; } In;
static void get_in(const uint8_t *vm, uint32_t top, const void *const *sp, int tint, int i, In *r)
{
  if (sp) {
    const int16_t *a = (const int16_t *)sp[0] + 4 * i, *b = (const int16_t *)sp[1] + 4 * i, *c = (const int16_t *)sp[2] + 2 * i;
    for (int j = 0; j < 4; j++) { r->p[j] = a[j]; r->n[j] = b[j]; }
    r->u[0] = c[0]; r->u[1] = c[1];
    if (tint && sp[5]) { const int16_t *f = (const int16_t *)sp[5] + 4 * i; for (int j = 0; j < 4; j++) r->f[j] = f[j]; }
    else for (int j = 0; j < 4; j++) r->f[j] = 4096;
  } else {
    const uint32_t *P = Q(vm, top + 0x100 + 6 * (uint32_t)i), *N = Q(vm, top + 0x101 + 6 * (uint32_t)i), *C = Q(vm, top + 0x102 + 6 * (uint32_t)i);
    const uint32_t *S = Q(vm, top + 0x105 + 6 * (uint32_t)i);
    for (int j = 0; j < 4; j++) { r->p[j] = (int32_t)P[j]; r->n[j] = (int32_t)N[j]; r->f[j] = tint ? (int32_t)S[j] : 4096; }
    r->u[0] = (int32_t)C[0]; r->u[1] = (int32_t)C[1];
  }
}

void c2m_pass(int pass, int tme, int tfx, int tcc, int abe, uint32_t bl, int fge, int tex)
{
  if (first) init();
  if (pass < 1 || pass > 3) return;
  T.ps[pass][tfx & 3][tcc & 1][tex != 0]++; T.pab[pass][abe & 1]++; T.pfg[pass][fge & 1]++; T.ptm[pass][tme & 1]++;
  if (abe) val_add(T.pbl[pass], NV, bl);
}

void c2m_call(const uint8_t *vm, uint32_t top, uint32_t entry, const void *const *sp, int n, const VcLit *L, const void *v1, const void *v3)
{
  if (first) init();
  if (n <= 0 || L->kind < 0 || L->kind > 3) return;
  const int kind = L->kind, skin = kind & 1, tint = kind >> 1;
  (void)entry;
  T.calls++; T.verts += n; T.kind[kind]++; T.kind_v[kind] += n; T.fast += L->fast; T.any += !L->fast;
  T.uv2h[L->uv2 < 0 ? 3 : L->uv2]++; T.p3h[L->p3 < 0 ? 3 : L->p3]++; T.cull += L->cull;
  /* the constants as the program reads them (vu1conv.c p06_any) */
  float Lm[3][3], C[4][3], B[4][3] = { { 0 } }, R[3][2] = { { 0 } }, s[2] = { 0 }, o[2] = { 0 };
  for (int j = 0; j < 3; j++) for (int c = 0; c < 3; c++) Lm[j][c] = vfl(Q(vm, top + 4 + (uint32_t)j)[c]);
  for (int j = 0; j < 4; j++) for (int c = 0; c < 3; c++) C[j][c] = vfl(Q(vm, top + 8 + (uint32_t)j)[c]);
  if (skin) for (int j = 0; j < 4; j++) for (int c = 0; c < 3; c++) B[j][c] = vfl(Q(vm, top + 0xc + (uint32_t)j)[c]);
  const float cmax = vfl(Q(vm, 3)[0]), alpha = vfl(Q(vm, 3)[1]), acl = alpha < cmax ? alpha : cmax;
  val_add(T.cmax, NV, Q(vm, 3)[0]); val_add(T.alpha, NV, Q(vm, 3)[1]);
  if (L->p3 == 1) for (int j = 0; j < 3; j++) for (int c = 0; c < 2; c++) R[j][c] = vfl(Q(vm, top + 0x10 + (uint32_t)j)[c]);
  else if (L->p3 == 2) {   /* the light ramp: light 0's direction normalised (p06_any) */
    const float a = Lm[0][0] * Lm[0][0] + Lm[1][0] * Lm[1][0] + Lm[2][0] * Lm[2][0], q = a == 0.0f ? 3.40282347e38f : 1.0f / sqrtf(a);
    for (int j = 0; j < 3; j++) { R[j][0] = Lm[j][0] * q; R[j][1] = 0; }
  }
  if (L->p3 > 0) for (int c = 0; c < 2; c++) { s[c] = vfl(Q(vm, top + 0xfe)[c]); o[c] = vfl(Q(vm, top + 0xff)[c]); }
  /* the lit lights (p06_main: a direction and a colour not zero), their GE form */
  int ok = 1, why = -1, nl = 0, lit[3] = { 0, 0, 0 }, LD[3][3] = { { 0 } }, AMB[3];
  float ldir[3][3] = { { 0 } }, lgen[3][3] = { { 0 } };
  for (int j = 0; j < 3; j++) {
    const float x = Lm[0][j], y = Lm[1][j], z = Lm[2][j];
    if ((x == 0 && y == 0 && z == 0) || (C[j][0] == 0 && C[j][1] == 0 && C[j][2] == 0)) continue;
    lit[j] = 1; nl++; T.lights++;
    const double len = sqrt((double)x * x + (double)y * y + (double)z * z);
    if (len < T.llen_min) T.llen_min = len;
    if (len > T.llen_max) T.llen_max = len;
    ldir[j][0] = f24r((float)(x / len)); ldir[j][1] = f24r((float)(y / len)); ldir[j][2] = f24r((float)(z / len));
    for (int r = 0; r < 3; r++) lgen[j][r] = ldir[j][r];
    ge_norm(lgen[j]);   /* the GE's own normalising of what it is sent (Lighting.cpp ComputeState) */
    for (int c = 0; c < 3; c++) {
      const double v = (double)C[j][c] * len;
      if (v > T.ld_max) T.ld_max = v;
      if (v < 0) { T.ld_neg++; ok = 0; why = 0; }
      if (v >= 255.5) { T.ld_over++; ok = 0; why = 1; }
      if (v != floor(v)) T.ld_frac++;
      LD[j][c] = v < 0 ? 0 : v >= 255 ? 255 : (int)floor(v + 0.5);
    }
  }
  T.nl[nl]++;
  for (int c = 0; c < 3; c++) {
    const float a = C[3][c];
    if (a < T.amb_min) T.amb_min = a;
    if (a > T.amb_max) T.amb_max = a;
    if (a != floorf(a)) T.amb_frac++;
    if (a < 0 || a >= 255.5f) { T.amb_over++; ok = 0; why = 2; }
    AMB[c] = a < 0 ? 0 : a >= 255 ? 255 : (int)floorf(a + 0.5f);
  }
  if (cmax != 255.0f) { ok = 0; why = 3; }
  if (!L->fast) { ok = 0; why = 4; }
  if (!nl) { ok = 0; why = 5; }
  /* the skin matrix: column lengths and angles, translation */
  float Bg[2][12];   /* the GE's bones (rounded to nearest float24): 0 = 32768 I, 1 = (32768 B3, B translation) */
  if (skin) {
    T.bcalls++;
    double cl[3];
    for (int c = 0; c < 3; c++) {
      cl[c] = sqrt((double)B[c][0] * B[c][0] + (double)B[c][1] * B[c][1] + (double)B[c][2] * B[c][2]);
      if (cl[c] < T.bcol_min) T.bcol_min = cl[c];
      if (cl[c] > T.bcol_max) T.bcol_max = cl[c];
    }
    for (int a = 0; a < 3; a++) for (int b = a + 1; b < 3; b++) {
      const double d = ((double)B[a][0] * B[b][0] + (double)B[a][1] * B[b][1] + (double)B[a][2] * B[b][2]) / (cl[a] * cl[b] + 1e-30);
      if (fabs(d) > T.borth) T.borth = fabs(d);
    }
    const double tl = sqrt((double)B[3][0] * B[3][0] + (double)B[3][1] * B[3][1] + (double)B[3][2] * B[3][2]);
    if (tl > T.bt_max) T.bt_max = tl;
    for (int e = 0; e < 12; e++) Bg[0][e] = 0;
    Bg[0][0] = Bg[0][4] = Bg[0][8] = 32768.0f;
    for (int col = 0; col < 3; col++) for (int r = 0; r < 3; r++) Bg[1][3 * col + r] = f24r(B[col][r] * 32768.0f);
    for (int r = 0; r < 3; r++) Bg[1][9 + r] = f24r(B[3][r]);
  }
  /* the inputs, and the census of weights, normals, factors and the clamp */
  In in[64];
  int wbad = 0, fbad = 0;
  for (int i = 0; i < n && i < 64; i++) {
    get_in(vm, top, sp, tint, i, &in[i]);
    const In *v = &in[i];
    float nn[3]; for (int j = 0; j < 3; j++) nn[j] = (float)v->n[j] / 4096.0f;
    const double lu = sqrt((double)nn[0] * nn[0] + (double)nn[1] * nn[1] + (double)nn[2] * nn[2]);
    if (!skin) {
      T.nlen_u[lbucket(lu - 1)]++;
      if (lu < T.nmin_u) T.nmin_u = lu;
      if (lu > T.nmax_u) T.nmax_u = lu;
    } else {
      const int w = v->p[3];
      T.w_n++; if (w < T.w_min) T.w_min = w; if (w > T.w_max) T.w_max = w;
      if (w < 0 || w > 4096) { T.w_out++; wbad = 1; }
      T.w_zero += w == 0; T.w_one += w == 4096;
      const float wf = (float)w / 4096.0f, w1 = 1.0f - wf;
      float bn[3];
      for (int j = 0; j < 3; j++) bn[j] = (B[0][j] * nn[0] + B[1][j] * nn[1]) + B[2][j] * nn[2];
      float m[3]; for (int j = 0; j < 3; j++) m[j] = nn[j] * wf + bn[j] * w1;
      const double ls = sqrt((double)m[0] * m[0] + (double)m[1] * m[1] + (double)m[2] * m[2]);
      T.nlen_s[lbucket(ls - 1)]++;
      if (ls < T.nmin_s) T.nmin_s = ls;
      if (ls > T.nmax_s) T.nmax_s = ls;
    }
    if (tint) {
      T.f_n++;
      for (int c = 0; c < 4; c++) { if (v->f[c] < T.f_min[c]) T.f_min[c] = v->f[c]; if (v->f[c] > T.f_max[c]) T.f_max[c] = v->f[c]; }
      for (int c = 0; c < 3; c++) { if (v->f[c] > 4096) { T.f_over++; fbad = 1; } if (v->f[c] < 0) { T.f_neg++; fbad = 1; } }
    }
  }
  { int bl = 0; if (skin) for (int i = 0; i < n && i < 64; i++) bl += in[i].p[3] > 0 && in[i].p[3] < 4096;
    ck_call(sp, n, kind, L->uv2 == 2, bl); }
  if (wbad) { ok = 0; why = 6; }
  if (fbad) { ok = 0; why = 7; }
  if (n > 64) { ok = 0; why = 8; }
  /* the windows */
  for (int w = 0; w < NWIN; w++) if (c2m_vb >= WIN[w][0] && c2m_vb < WIN[w][1]) {
    T.wc[w]++; T.wv[w] += n; T.wsk[w] += skin ? n : 0; T.wp2[w] += L->uv2 > 0 ? n : 0; T.wp3[w] += L->p3 > 0 ? n : 0; T.wnl[w][nl] += n;
    T.wfac[w] += tint ? n : 0; }
  /* the exact vertices: colour and position offsets by type (VcVert, VcVertE, VcVertS, VcVertF) */
  const int t1 = (L->uv16 ? 0 : 1) | (L->pos16 ? 0 : 2), s1 = vc_p06_size1(L), co = (t1 & 1) ? 8 : 4, po = (t1 & 1) ? 12 : 8;
  /* the PS2's sum before the clamp (the fast form's order), for the clamp census: every call */
  for (int i = 0; i < n && i < 64; i++) {
    const In *v = &in[i];
    float nn[3]; for (int j = 0; j < 3; j++) nn[j] = (float)v->n[j] / 4096.0f;
    if (skin) {
      const float wf = (float)v->p[3] / 4096.0f, w1 = 1.0f - wf; float bn[3];
      for (int j = 0; j < 3; j++) bn[j] = (B[0][j] * nn[0] + B[1][j] * nn[1]) + B[2][j] * nn[2];
      for (int j = 0; j < 3; j++) nn[j] = nn[j] * wf + bn[j] * w1;
    }
    int hit = 0;
    for (int c = 0; c < 3; c++) {
      float a = 0;
      for (int j = 0; j < 3; j++) if (lit[j]) { float d = (Lm[0][j] * nn[0] + Lm[1][j] * nn[1]) + Lm[2][j] * nn[2]; d = d < 0 ? 0 : d; a += C[j][c] * d; }
      a += C[3][c];
      if (a > cmax) hit = 1;
    }
    T.clamp_v += hit; if (tint) T.clamp_tv += hit;
  }
  if (!ok) { if (why >= 0) T.why[why]++; return; }
  T.elig++; T.elig_v += n;
  /* ---- the GE model ---- */
  const VcXform *x = &L->x;
  const uint32_t a1c = (uint32_t)(int32_t)acl & 0xffu, a2c = a1c >= 0x80 ? 0xffu : a1c * 2;   /* the call's GE alpha (no factor) */
  for (int i = 0; i < n; i++) {
    const In *v = &in[i];
    const uint8_t *e = (const uint8_t *)v1 + (size_t)s1 * (size_t)i;
    uint32_t ce; memcpy(&ce, e + co, 4);
    /* the GE's model-space normal (and skinned position) */
    float Ng[3], Pg[3] = { 0, 0, 0 };
    const float pin[3] = { (float)v->p[0] / 32768.0f, (float)v->p[1] / 32768.0f, (float)v->p[2] / 32768.0f };
    const float nin[3] = { (float)v->n[0] / 32768.0f, (float)v->n[1] / 32768.0f, (float)v->n[2] / 32768.0f };
    if (!skin) for (int c = 0; c < 3; c++) Ng[c] = nin[c];
    else {   /* ApplyGESkinning: per bone, its translation then x, y and z times its column, each product a float24, the GE's adds */
      const float w[2] = { f24t((float)(v->p[3] * 8) / 32768.0f), f24t((float)(32768 - v->p[3] * 8) / 32768.0f) };
      for (int c = 0; c < 3; c++) {
        float ap = 0, an = 0;
        for (int b = 0; b < 2; b++) {
          if (w[b] == 0.0f) continue;
          ap = ge_add(ap, ge_mul(w[b], Bg[b][9 + c]));
          for (int j = 0; j < 3; j++) {
            const float row = ge_mul(w[b], Bg[b][3 * j + c]);
            ap = ge_add(ap, ge_mul(pin[j], row));
            an = ge_add(an, ge_mul(nin[j], row));
          }
        }
        Pg[c] = ap; Ng[c] = an;
      }
      /* positions: against the exact (VcVertS / VcVertF floats) */
      float pe[3]; memcpy(pe, e + po, 12);
      const double dp = sqrt(((double)Pg[0] - pe[0]) * (Pg[0] - pe[0]) + ((double)Pg[1] - pe[1]) * (Pg[1] - pe[1]) + ((double)Pg[2] - pe[2]) * (Pg[2] - pe[2]));
      if (dp > T.dp_max) T.dp_max = dp;
      T.dp_sum += dp; T.dp_n++;
      const double Xe = (double)pe[0] * x->world[0] + (double)pe[1] * x->world[3] + (double)pe[2] * x->world[6] + x->world[9];
      const double Ye = (double)pe[0] * x->world[1] + (double)pe[1] * x->world[4] + (double)pe[2] * x->world[7] + x->world[10];
      const double We = (double)pe[0] * x->world[2] + (double)pe[1] * x->world[5] + (double)pe[2] * x->world[8] + x->world[11];
      const double Xg = (double)Pg[0] * x->world[0] + (double)Pg[1] * x->world[3] + (double)Pg[2] * x->world[6] + x->world[9];
      const double Yg = (double)Pg[0] * x->world[1] + (double)Pg[1] * x->world[4] + (double)Pg[2] * x->world[7] + x->world[10];
      const double Wg = (double)Pg[0] * x->world[2] + (double)Pg[1] * x->world[5] + (double)Pg[2] * x->world[8] + x->world[11];
      if (We > 0 && Wg > 0) {
        const double dx = fabs(0.9375 * x->vs[0] * (Xg / Wg - Xe / We)), dy = fabs(0.53125 * x->vs[1] * (Yg / Wg - Ye / We)), d = dx > dy ? dx : dy;
        T.dsh[bucket_px(d)]++;
      } else T.behind++;
    }
    /* the lighting (WORLD identity: the world normal is Ng; Lighting.cpp, the dot products scaled by its reciprocal length) */
    const float len2 = ge_dot3(Ng, Ng);
    const int nz = len2 > 0.0f && isfinite(len2);
    const float nr = nz ? ge_rsqrt(len2) : 1.0f;
    int fs[3] = { 0, 0, 0 };   /* each lit light's 8-bit factor input */
    float fl[3] = { 0, 0, 0 };
    for (int j = 0; j < 3; j++) if (lit[j] && nz) { fl[j] = pdf24((double)ge_dot3(lgen[j], Ng) * nr); fs[j] = 1; }
    /* the PS2's GE alpha (exact) of this vertex */
    uint32_t a2 = a2c;
    if (tint) { const uint32_t a1 = (uint32_t)(int32_t)((float)v->f[3] / 4096.0f * acl) & 0xffu; a2 = a1 >= 0x80 ? 0xffu : a1 * 2; }
    /* model (b): the PS2's normal (p6_loop's order) and its length */
    if (!tint || (v->f[0] == 4096 && v->f[1] == 4096 && v->f[2] == 4096)) {
      float m[3]; for (int j = 0; j < 3; j++) m[j] = (float)v->n[j] * (1.0f / 4096);
      if (skin) {
        const float wf = (float)v->p[3] * (1.0f / 4096), w1 = 1.0f - wf; float bn[3];
        for (int j = 0; j < 3; j++) bn[j] = (B[0][j] * m[0] + B[1][j] * m[1]) + B[2][j] * m[2];
        for (int j = 0; j < 3; j++) m[j] = m[j] * wf + bn[j] * w1;
      }
      const float len = sqrtf((m[0] * m[0] + m[1] * m[1]) + m[2] * m[2]);
      for (int enc = 0; enc < 2; enc++) {
        int d = enc ? (int)(len * 256.0f) : (int)(len * 255.0f + 0.5f); d = d < 0 ? 0 : d > 255 ? 255 : d;
        if (!enc) T.dhist[d >= 255 ? 8 : d >= 250 ? 7 : d >= 230 ? 6 : d >= 200 ? 5 : d >= 160 ? 4 : d >= 128 ? 3 : d >= 64 ? 2 : d ? 1 : 0]++;
        for (int c = 0; c < 3; c++) {
          int sum = AMB[c];   /* the emissive */
          for (int j = 0; j < 3; j++) if (lit[j] && fs[j]) sum += lscale(lprod(lfac(LD[j][c]), lfac(d)), fl[j]);
          sum = sum > 255 ? 255 : sum;
          const int ex = (int)(ce >> (8 * c) & 0xff), dd = sum - ex;
          T.dchE[enc][kind][dbin(dd)]++; T.dcE_sum[enc][kind] += dd; T.dcE_n[enc][kind]++; if (abs(dd) > T.dcE_max[enc][kind]) T.dcE_max[enc][kind] = abs(dd);
        }
      }
    } else { T.fb_v++; if (!i) T.fb_c++; }
    for (int enc = 0; enc < (tint ? 2 : 1); enc++) {
      int mat[3];   /* the material (ambient and diffuse): white, or the factor in 8 bits */
      for (int c = 0; c < 3; c++) {
        if (!tint) { mat[c] = 255; continue; }
        const int f = v->f[c];
        int q = enc == 0 ? (int)floor((double)f * 255.0 / 4096.0 + 0.5) : (f + 8) >> 4;
        mat[c] = q < 0 ? 0 : q > 255 ? 255 : q;
      }
      int col[4];
      for (int c = 0; c < 3; c++) {
        int sum = lprod(lfac(mat[c]), lfac(AMB[c]));   /* the ambient: ((2 m + 1)(2 A + 1)) >> 10 */
        for (int j = 0; j < 3; j++) if (lit[j] && fs[j]) sum += lscale(lprod(lfac(LD[j][c]), lfac(mat[c])), fl[j]);
        col[c] = sum > 255 ? 255 : sum;
      }
      col[3] = tint ? lprod(lfac((int)a2), lfac(255)) : lprod(lfac(255), lfac((int)a2c));
      if (!enc) { T.da_n++; T.da_bad += (uint32_t)col[3] != (ce >> 24); }
      for (int c = 0; c < 3; c++) {
        const int ex = (int)(ce >> (8 * c) & 0xff), d = col[c] - ex;
        if (enc == 0) { T.dch[kind][dbin(d)]++; T.dc_sum[kind] += d; T.dc_n[kind]++; if (abs(d) > T.dc_max[kind]) T.dc_max[kind] = abs(d); }
        else { T.dchB[dbin(d)]++; T.dc_sumB += d; T.dc_nB++; if (abs(d) > T.dc_maxB) T.dc_maxB = abs(d); }
      }
    }
    /* pass 3: the texture matrix from the normal as the GE holds it (mode 3) and from the normalised normal (mode 2) */
    if (L->p3 > 0 && v3) {
      const int s3 = vc_p06_size3(L);
      float ue[2]; memcpy(ue, (const uint8_t *)v3 + (size_t)s3 * (size_t)i, 8);
      for (int mode = 0; mode < 2; mode++) {
        const float sc = mode ? 1.0f : skin ? 1.0f / 4096.0f : 8.0f;
        float src[4] = { Ng[0], Ng[1], Ng[2], 1.0f };
        if (mode) ge_norm(src);
        float uv[2];
        for (int k = 0; k < 2; k++) {
          const float b[4] = { f24r(R[0][k] * s[k] * sc), f24r(R[1][k] * s[k] * sc), f24r(R[2][k] * s[k] * sc), f24r(o[k]) };
          uv[k] = ge_rowsum(src, b, 4);
        }
        const double du = fabs((double)uv[0] - ue[0]), dv = fabs((double)uv[1] - ue[1]), d = du > dv ? du : dv;
        if (d > T.du3_max[mode]) T.du3_max[mode] = d;
        T.du3_sum[mode] += d;
        const double t = d * 256;
        T.du3h[mode][t < 1.0 / 64 ? 0 : t < 1.0 / 16 ? 1 : t < 0.25 ? 2 : t < 0.5 ? 3 : t < 1 ? 4 : 5]++;
      }
      T.du3_n++;
    }
  }
}

static void pvals(FILE *f, const char *name, const Val *t)
{
  fprintf(f, " %s", name);
  for (int i = 0; i < NV && t[i].n; i++) fprintf(f, " %g x%ld", (double)F(t[i].v), t[i].n);
  fprintf(f, ";");
}
void c2m_report(FILE *f)
{
  if (!c2m_on || first) return;
  fprintf(f, "c2 census: P06 calls %ld (%ld vertices; +0 %ld/%ld, +358 %ld/%ld, +738 %ld/%ld, +ab0 %ld/%ld calls/vertices; fast %ld, exact form %ld); "
          "lit lights 0..3: %ld %ld %ld %ld; pass 2 none/1/own/-1: %ld %ld %ld %ld; pass 3 none/normal/ramp/0x100: %ld %ld %ld %ld; cull bit %ld\n",
          T.calls, T.verts, T.kind[0], T.kind_v[0], T.kind[1], T.kind_v[1], T.kind[2], T.kind_v[2], T.kind[3], T.kind_v[3], T.fast, T.any,
          T.nl[0], T.nl[1], T.nl[2], T.nl[3], T.uv2h[0], T.uv2h[1], T.uv2h[2], T.uv2h[3], T.p3h[0], T.p3h[1], T.p3h[2], T.p3h[3], T.cull);
  fprintf(f, "c2 census:"); pvals(f, "clamp abs 3 x", T.cmax); pvals(f, "alpha abs 3 y", T.alpha); fprintf(f, "\n");
  fprintf(f, "c2 census: lit lights %ld, lengths %.6g..%.6g; colour x length max %.6g, not whole %ld, from 255.5 %ld, below 0 %ld; ambient %.6g..%.6g, "
          "not whole %ld, outside 0..255 %ld\n", T.lights, T.llen_min, T.llen_max, T.ld_max, T.ld_frac, T.ld_over, T.ld_neg, T.amb_min, T.amb_max,
          T.amb_frac, T.amb_over);
  fprintf(f, "c2 census: skin: %ld calls, weight w %d..%d (4.12) over %ld vertices, outside 0..4096 %ld, 0 %ld, 4096 %ld; matrix column lengths "
          "%.6g..%.6g, largest |cos| %.3g, translation up to %.6g\n", T.bcalls, T.w_min, T.w_max, T.w_n, T.w_out, T.w_zero, T.w_one, T.bcol_min,
          T.bcol_max, T.borth, T.bt_max);
  fprintf(f, "c2 census: normal length - 1 (<1e-3, <1e-2, <0.05, <0.1, <0.25, more): data (unskinned) %ld %ld %ld %ld %ld %ld (%.5g..%.5g); "
          "blended (skinned) %ld %ld %ld %ld %ld %ld (%.5g..%.5g)\n", T.nlen_u[0], T.nlen_u[1], T.nlen_u[2], T.nlen_u[3], T.nlen_u[4], T.nlen_u[5],
          T.nmin_u, T.nmax_u, T.nlen_s[0], T.nlen_s[1], T.nlen_s[2], T.nlen_s[3], T.nlen_s[4], T.nlen_s[5], T.nmin_s, T.nmax_s);
  fprintf(f, "c2 census: factor (4.12) over %ld vertices: r %d..%d, g %d..%d, b %d..%d, a %d..%d; a channel above 4096 %ld, below 0 %ld; "
          "vertices whose lit sum passes the clamp %ld (of them with a factor %ld)\n", T.f_n, T.f_min[0], T.f_max[0], T.f_min[1], T.f_max[1],
          T.f_min[2], T.f_max[2], T.f_min[3], T.f_max[3], T.f_over, T.f_neg, T.clamp_v, T.clamp_tv);
  for (int w = 0; w < NWIN; w++)
    fprintf(f, "c2 window vb %ld-%ld: P06 calls %ld, vertices %ld (skinned %ld, with a factor %ld), drawn again in pass 2 %ld and pass 3 %ld; by lit "
            "lights 0..3: %ld %ld %ld %ld\n", WIN[w][0], WIN[w][1], T.wc[w], T.wv[w], T.wsk[w], T.wfac[w], T.wp2[w], T.wp3[w], T.wnl[w][0],
            T.wnl[w][1], T.wnl[w][2], T.wnl[w][3]);
  fprintf(f, "c2 model: calls C2 takes %ld (%ld vertices); left: light colour below 0 %ld, from 255.5 %ld, ambient %ld, clamp not 255 %ld, not "
          "the fast form %ld, no lit light %ld, weight outside 0..4096 %ld, factor outside 0..4096 %ld, over 64 %ld\n", T.elig, T.elig_v, T.why[0],
          T.why[1], T.why[2], T.why[3], T.why[4], T.why[5], T.why[6], T.why[7], T.why[8]);
  static const char *KN[4] = { "+0", "+358", "+738 (factor A)", "+ab0 (factor A)" };
  for (int k = 0; k < 4; k++) {
    fprintf(f, "c2 model: colour %s, GE - exact: mean %.3f, max |d| %ld; -5 or below .. +5 or above:", KN[k], T.dc_n[k] ? T.dc_sum[k] / T.dc_n[k] : 0, T.dc_max[k]);
    for (int b = 0; b < 11; b++) fprintf(f, " %ld", T.dch[k][b]);
    fprintf(f, "\n");
  }
  fprintf(f, "c2 model: colour with a factor, encoding B (f / 16): mean %.3f, max |d| %ld:", T.dc_nB ? T.dc_sumB / T.dc_nB : 0, T.dc_maxB);
  for (int b = 0; b < 11; b++) fprintf(f, " %ld", T.dchB[b]);
  fprintf(f, "\nc2 model: alpha differing %ld of %ld\n", T.da_bad, T.da_n);
  for (int e = 0; e < 2; e++) for (int k = 0; k < 4; k++) {
    fprintf(f, "c2 model (b) %s: colour %s, GE - exact: mean %.3f, max |d| %ld; -5 or below .. +5 or above:", e ? "min(255, 256 len)" : "round(255 len)",
            KN[k], T.dcE_n[e][k] ? T.dcE_sum[e][k] / T.dcE_n[e][k] : 0, T.dcE_max[e][k]);
    for (int b = 0; b < 11; b++) fprintf(f, " %ld", T.dchE[e][k][b]);
    fprintf(f, "\n");
  }
  fprintf(f, "c2 model (b): factor calls left to the converter %ld (%ld vertices); d = round(255 len) 0, 1-63, 64-127, 128-159, 160-199, 200-229, "
          "230-249, 250-254, 255: %ld %ld %ld %ld %ld %ld %ld %ld %ld\n", T.fb_c, T.fb_v, T.dhist[0], T.dhist[1], T.dhist[2], T.dhist[3], T.dhist[4],
          T.dhist[5], T.dhist[6], T.dhist[7], T.dhist[8]);
  fprintf(f, "c2 cache: calls %ld from streams: keys %ld (GE vertex bytes %ld at 20 or 24 a vertex), the same bytes again %ld, other bytes at a "
          "known key %ld, table full %ld; skinned calls with a blended vertex %ld (%ld blended vertices: their colour each frame)\n", T.ck_calls,
          T.ck_new, T.ck_bytes, T.ck_same, T.ck_other, T.ck_full, T.ck_skb, T.ck_skbv);
  for (int w = 0; w < NWIN; w++) fprintf(f, "c2 cache window vb %ld-%ld: calls %ld, new keys %ld\n", WIN[w][0], WIN[w][1], T.ck_wcall[w], T.ck_wnew[w]);
  static const char *TF[4] = { "modulate", "decal", "highlight", "highlight2" };
  for (int p = 1; p <= 3; p++) {
    fprintf(f, "c2 passes: pass %d: tme 0/1 %ld %ld; tfx/tcc/GE texture:", p, T.ptm[p][0], T.ptm[p][1]);
    for (int a = 0; a < 4; a++) for (int b = 0; b < 2; b++) for (int c = 0; c < 2; c++) if (T.ps[p][a][b][c]) fprintf(f, " %s tcc %d tex %d x%ld", TF[a], b, c, T.ps[p][a][b][c]);
    fprintf(f, "; blend off/on %ld %ld", T.pab[p][0], T.pab[p][1]);
    for (int i = 0; i < NV && T.pbl[p][i].n; i++) fprintf(f, " (A %u B %u C %u D %u FIX %u) x%ld", T.pbl[p][i].v & 3, T.pbl[p][i].v >> 2 & 3, T.pbl[p][i].v >> 4 & 3,
                                                          T.pbl[p][i].v >> 6 & 3, T.pbl[p][i].v >> 8 & 0xff, T.pbl[p][i].n);
    fprintf(f, "; fog off/on %ld %ld\n", T.pfg[p][0], T.pfg[p][1]);
  }
  fprintf(f, "c2 model: skinned positions: world distance max %.4g, mean %.4g; GE pixels (larger of x, y) below 1/64 %ld, 1/64-1/16 %ld, 1/16-1/4 %ld, "
          "1/4-1/2 %ld, 1/2-1 %ld, from 1 %ld; behind the eye %ld\n", T.dp_max, T.dp_n ? T.dp_sum / T.dp_n : 0, T.dsh[0], T.dsh[1], T.dsh[2], T.dsh[3],
          T.dsh[4], T.dsh[5], T.behind);
  for (int m = 0; m < 2; m++)
    fprintf(f, "c2 model: pass 3 UVs, %s: %ld vertices, max |d| %.4g, mean %.4g; in texels of a 256 texture: below 1/64 %ld, 1/64-1/16 %ld, "
            "1/16-1/4 %ld, 1/4-1/2 %ld, 1/2-1 %ld, from 1 %ld\n", m ? "normalised normal (mode 2)" : "the normal as held (mode 3)", T.du3_n,
            T.du3_max[m], T.du3_n ? T.du3_sum[m] / T.du3_n : 0, T.du3h[m][0], T.du3h[m][1], T.du3h[m][2], T.du3h[m][3], T.du3h[m][4], T.du3h[m][5]);
}
