/* c1model.c (sb11, task #134; x86 only): C1 for the codec faces (P07) sized before any GE code.  For each P07 call the picture code
   converts (xconv.c xc_lit), with its exact results in hand, this file
   - counts what the GE would have to represent (the census): the clamp abs 3 x, the alpha, the three lights' lengths and colours times
     those lengths, the ambient, pass 2's kind, the bones per vertex, the bone offsets and the slots they name, the weight sums, the
     bone matrices' column lengths, a 1-bone vertex's normal length;
   - computes the same vertices as the GE would under C1's mapping (stagec_notes.md 1.7), in PPSSPP 1.20.4's software GPU arithmetic
     (GPU/Common/VertexDecoderCommon.cpp ComputeSkinMatrix and the S16 skin steps; GPU/Software/Lighting.cpp ProcessSIMD;
     TransformUnit.cpp's texture-matrix mapping): bones as float24 (rounded to nearest) with x 2048 in the 3 x 3 for the GE's s16 / 32768
     positions, 8-bit weights / 128, the skin matrix blended first, world identity; three directional lights (direction normalised,
     diffuse colour = the PS2's colour row times the direction's length rounded to 8 bits), global ambient = abs 31 rounded, material
     white, the integer light sum ((2 c + 1) factors, ceil(512 d + 1) capped at 512, each light's term >> 19, the sum clamped at 255);
     pass 3 from the normalised skinned normal through a float24 texture matrix (rows s R, translation o);
   - compares those with today's exact vertices: positions (world units, and GE pixels through the call's own transform), colours
     (levels, GE minus exact; also with the ambient one level up), pass 3's UVs.
   It only reads: every run with it stays exact.  MGS2_C1_MODEL=1 turns it on; c1m_report prints the totals. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vu1conv.h"
#include "c1model.h"

int c1m_on;
static const uint32_t *Q(const uint8_t *vm, uint32_t qa) { return (const uint32_t *)(vm + ((qa & 0x3ffu) << 4)); }
static float F(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint32_t U(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
/* the GE's 24-bit floats: the top 24 bits, rounded to nearest (what a driver would send) */
static float f24(float f) { uint32_t u = U(f); if ((u & 0x7f800000u) == 0x7f800000u) return f; u = (u + 0x80u) & ~0xffu; return F(u); }

/* ---- the totals ---- */
#define NV 8
typedef struct { uint32_t v; long n; } Val;
static void val_add(Val *t, int k, uint32_t v) { for (int i = 0; i < k; i++) { if (t[i].n && t[i].v == v) { t[i].n++; return; } if (!t[i].n) { t[i].v = v; t[i].n = 1; return; } } t[k - 1].n++; }
static struct {
  long calls, verts, fast, any, nbh[5], uv2h[4], p3h[3], noslot, elig, elig_v, why[8];
  Val cmax[NV], alpha[NV];
  double llen_min[3], llen_max[3], ld_max, amb_min, amb_max, coll_min, coll_max, orth_max, n1_dev;
  long ld_frac, ld_over, ld_neg, amb_frac, amb_over, off_unal, off_out, wsum_bad, slots[9], wdup;
  /* the model */
  double dp_max, dp_sum, ds_max; long dp_n, dsh[6], ds_n, behind;
  long dch[2][11]; double dc_sum[2]; long dc_n, dc_max[2];
  long dch1[11], dch2[11];   /* bias 0, by bones: 1 and 2-4 */
  double du_max, du_sum; long du_n, duh[6];
  /* the cache's census (sb11 b): calls by the number of distinct slots; the streams' addresses as the key, a 64-bit hash of the bytes */
  long nslot[9], ck_nosp, ck_new, ck_same, ck_diff, ck_cseen, ck_bytes, ck_keys, ck_keyv, ck_full;
} T;
#define CK (1u << 21)
typedef struct { uint64_t key, h; } Ce;
static Ce *ckt, *cct;   /* by key; by content alone */
static uint64_t hb(uint64_t h, const void *p, size_t len) { const uint8_t *b = (const uint8_t *)p; for (size_t i = 0; i < len; i++) { h ^= b[i]; h *= 0x100000001b3ull; } return h; }
static uint64_t fin(uint64_t h) { h ^= h >> 33; h *= 0xff51afd7ed558ccdull; h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ull; h ^= h >> 33; return h ? h : 1; }
/* find or add: 0 new, 1 same value, 2 another value (replaced) */
static int ck_put(Ce *t, uint64_t key, uint64_t h)
{
  for (uint32_t i = (uint32_t)key & (CK - 1), k = 0; k < CK; k++, i = (i + 1) & (CK - 1)) {
    if (!t[i].key) { t[i].key = key; t[i].h = h; return 0; }
    if (t[i].key == key) { if (t[i].h == h) return 1; t[i].h = h; return 2; }
  }
  T.ck_full++; return 0;
}
static int first = 1;
static void init(void)
{
  first = 0;
  for (int j = 0; j < 3; j++) { T.llen_min[j] = 1e30; T.llen_max[j] = 0; }
  T.amb_min = 1e30; T.amb_max = -1e30; T.coll_min = 1e30; T.coll_max = 0; T.ld_max = -1e30;
}

/* vertex i's input words: the streams (sp: A, B as halfwords, F as two words) or the VU memory (T + 0x100 + 7 i) */
typedef struct { int32_t p[3], n[3]; uint32_t w[4], o[4]; } In;
static void get_in(const uint8_t *vm, uint32_t top, const void *const *sp, int i, In *r)
{
  if (sp) {
    const int16_t *a = (const int16_t *)sp[0] + 4 * i, *b = (const int16_t *)sp[1] + 4 * i; const uint32_t *f = (const uint32_t *)sp[5] + 2 * i;
    for (int j = 0; j < 3; j++) { r->p[j] = a[j]; r->n[j] = b[j]; }
    for (int j = 0; j < 4; j++) { r->w[j] = f[0] >> (8 * j) & 0xff; r->o[j] = f[1] >> (8 * j) & 0xff; }
  } else {
    const uint32_t *a = Q(vm, top + 0x100 + 7 * (uint32_t)i), *b = Q(vm, top + 0x101 + 7 * (uint32_t)i);
    const uint32_t *w = Q(vm, top + 0x105 + 7 * (uint32_t)i), *o = Q(vm, top + 0x106 + 7 * (uint32_t)i);
    for (int j = 0; j < 3; j++) { r->p[j] = (int32_t)a[j]; r->n[j] = (int32_t)b[j]; }
    for (int j = 0; j < 4; j++) { r->w[j] = w[j]; r->o[j] = o[j]; }
  }
}
static int bucket_px(double d) { return d < 1.0 / 64 ? 0 : d < 1.0 / 16 ? 1 : d < 0.25 ? 2 : d < 0.5 ? 3 : d < 1 ? 4 : 5; }

void c1m_call(const uint8_t *vm, uint32_t top, const void *const *sp, int n, const VcLit *L, const void *v1, const void *v3)
{
  if (first) init();
  if (n <= 0 || L->kind != 4) return;
  T.calls++; T.verts += n; T.fast += L->fast; T.any += !L->fast;
  const uint32_t *h = Q(vm, top);
  const int nb = (int)(h[2] & 0xffff); const uint32_t fl = h[3] & 0xffff;
  if (nb >= 1 && nb <= 4) T.nbh[nb]++;
  T.uv2h[L->uv2 < 0 ? 3 : L->uv2]++; T.p3h[L->p3 < 0 ? 2 : L->p3 > 0]++;
  /* the constants as the program reads them (vu1conv.c p07_main) */
  float l[3][3], c[3][3], amb[3], R[3][2] = { { 0 } }, s[2] = { 0 }, o[2] = { 0 };
  for (int j = 0; j < 3; j++) for (int k = 0; k < 3; k++) { l[j][k] = F(Q(vm, 24 + j)[k]); c[j][k] = F(Q(vm, 28 + j)[k]); }
  for (int k = 0; k < 3; k++) amb[k] = F(Q(vm, 31)[k]);
  const float cmax = F(Q(vm, 3)[0]);
  val_add(T.cmax, NV, Q(vm, 3)[0]); val_add(T.alpha, NV, (uint32_t)L->amin);
  if (L->p3 > 0) {
    for (int j = 0; j < 3; j++) for (int k = 0; k < 2; k++) R[j][k] = F(Q(vm, 16 + j)[k]);
    for (int k = 0; k < 2; k++) { s[k] = F(Q(vm, top + 0xfe)[k]); o[k] = F(Q(vm, top + 0xff)[k]); }
  }
  /* light j: direction = column j of abs 24..26 (d_j = sum over rows of l[row][j] n[row]), colour = row j of abs 28..30 */
  float dir[3][3], len[3]; int LD[3][3], AMB[3]; int ok = 1, why = -1;
  for (int j = 0; j < 3; j++) {
    double q = (double)l[0][j] * l[0][j] + (double)l[1][j] * l[1][j] + (double)l[2][j] * l[2][j];
    len[j] = (float)sqrt(q);
    if (len[j] < T.llen_min[j]) T.llen_min[j] = len[j];
    if (len[j] > T.llen_max[j]) T.llen_max[j] = len[j];
    for (int r = 0; r < 3; r++) dir[j][r] = len[j] > 0 ? f24(l[r][j] / len[j]) : (r == 2 ? 1.0f : 0.0f);
    for (int k = 0; k < 3; k++) {
      double v = (double)c[j][k] * len[j];
      if (v > T.ld_max) T.ld_max = v;
      if (v < 0) { T.ld_neg++; ok = 0; why = 0; }
      if (v >= 255.5) { T.ld_over++; ok = 0; why = 1; }
      if (v != floor(v)) T.ld_frac++;
      LD[j][k] = v < 0 ? 0 : v >= 255 ? 255 : (int)floor(v + 0.5);
    }
  }
  for (int k = 0; k < 3; k++) {
    if (amb[k] < T.amb_min) T.amb_min = amb[k];
    if (amb[k] > T.amb_max) T.amb_max = amb[k];
    if (amb[k] != floorf(amb[k])) T.amb_frac++;
    if (amb[k] < 0 || amb[k] >= 255.5f) { T.amb_over++; ok = 0; why = 2; }
    AMB[k] = amb[k] < 0 ? 0 : amb[k] >= 255 ? 255 : (int)floorf(amb[k] + 0.5f);
  }
  if (cmax != 255.0f) { ok = 0; why = 3; }
  if (nb < 1 || nb > 4) { ok = 0; why = 4; }
  /* the bones: every vertex's pairs b < nb; each slot's matrix as the GE gets it */
  In in[64];
  int used = 0;
  for (int i = 0; i < n && i < 64; i++) {
    get_in(vm, top, sp, i, &in[i]);
    unsigned ws = 0;
    for (int b = 0; b < nb; b++) {
      const uint32_t of = in[i].o[b];
      if (of & 3) { T.off_unal++; ok = 0; why = 5; }
      if (of > 28) { T.off_out++; ok = 0; why = 6; }
      if (!(of & 3) && of <= 28) used |= 1 << (of >> 2);
      ws += in[i].w[b];
      for (int b2 = 0; b2 < b; b2++) if (in[i].o[b2] == of) T.wdup++;
    }
    if (nb > 1 && ws != 128) T.wsum_bad++;
  }
  if (n > 64) { ok = 0; why = 7; }
  int top_slot = 0; for (int k = 0; k < 8; k++) if (used >> k & 1) top_slot = k + 1;
  T.slots[top_slot]++;
  float B[8][12];
  for (int k = 0; k < 8; k++) {
    if (!(used >> k & 1)) continue;
    const uint32_t a = 32 + 4 * (uint32_t)k;
    double cl[3];
    for (int col = 0; col < 4; col++) {
      const uint32_t *cc = Q(vm, a + (uint32_t)col);
      for (int r = 0; r < 3; r++) B[k][3 * col + r] = f24(col < 3 ? F(cc[r]) * 2048.0f : F(cc[r]));
      if (col < 3) cl[col] = sqrt((double)F(cc[0]) * F(cc[0]) + (double)F(cc[1]) * F(cc[1]) + (double)F(cc[2]) * F(cc[2]));
    }
    for (int col = 0; col < 3; col++) { if (cl[col] < T.coll_min) T.coll_min = cl[col]; if (cl[col] > T.coll_max) T.coll_max = cl[col]; }
    for (int x = 0; x < 3; x++) for (int y = x + 1; y < 3; y++) {
      const uint32_t *p = Q(vm, a + (uint32_t)x), *q = Q(vm, a + (uint32_t)y);
      double d = ((double)F(p[0]) * F(q[0]) + (double)F(p[1]) * F(q[1]) + (double)F(p[2]) * F(q[2])) / (cl[x] * cl[y] + 1e-30);
      if (fabs(d) > T.orth_max) T.orth_max = fabs(d);
    }
  }
  if (!ok) { if (why >= 0) T.why[why]++; return; }
  T.noslot += !used;
  T.elig++; T.elig_v += n;
  T.nslot[__builtin_popcount((unsigned)used)]++;
  if (!sp || !sp[0] || !sp[1] || !sp[2] || !sp[5]) T.ck_nosp++;
  else {
    if (!ckt) { ckt = (Ce *)calloc(CK, sizeof(Ce)); cct = (Ce *)calloc(CK, sizeof(Ce)); }
    const struct { const void *p[6]; int n, nb; } kk = { { sp[0], sp[1], sp[2], sp[3], 0, sp[5] }, n, nb };
    const uint64_t key = fin(hb(0xcbf29ce484222325ull, &kk, sizeof kk));
    uint64_t h = 0xcbf29ce484222325ull;
    for (int s2 = 0; s2 < 6; s2++) if (s2 != 4 && sp[s2]) { h = hb(h, sp[s2], 8 * (size_t)n); T.ck_bytes += 8 * n; }
    h = fin(h);
    const int r = ckt ? ck_put(ckt, key, h) : 0;
    if (r == 0) { T.ck_new++; T.ck_keys++; T.ck_keyv += n; } else if (r == 1) T.ck_same++; else T.ck_diff++;
    if (cct && ck_put(cct, h, 1) == 1) T.ck_cseen++;
  }
  /* the call's own transform for the pixel distances (xconv.c xc_xform: GE x = 0.9375 vs0 X / W, y = 0.53125 vs1 Y / W) */
  const VcXform *x = &L->x;
  const int t1 = L->uv16 ? 2 : 3;
  for (int i = 0; i < n; i++) {
    const In *v = &in[i];
    /* exact: today's vertex */
    float pe[3]; uint32_t ce;
    if (t1 == 2) { const VcVertS *e = (const VcVertS *)v1 + i; pe[0] = e->x; pe[1] = e->y; pe[2] = e->z; ce = e->c; }
    else { const VcVertF *e = (const VcVertF *)v1 + i; pe[0] = e->x; pe[1] = e->y; pe[2] = e->z; ce = e->c; }
    /* GE: weights by slot, the skin matrix blended first, then the s16 / 32768 position and normal through it */
    float w[8] = { 0 };
    if (nb == 1) w[v->o[0] >> 2] = 1.0f;
    else for (int b = 0; b < nb; b++) w[v->o[b] >> 2] += (float)v->w[b];
    if (nb > 1) for (int k = 0; k < 8; k++) w[k] = w[k] * (1.0f / 128.0f);
    float S[12] = { 0 };
    for (int k = 0; k < top_slot; k++) if (w[k] != 0.0f) for (int e = 0; e < 12; e++) S[e] = S[e] + w[k] * B[k][e];
    const float fp[3] = { v->p[0] * (1.0f / 32768.0f), v->p[1] * (1.0f / 32768.0f), v->p[2] * (1.0f / 32768.0f) };
    const float fn[3] = { v->n[0] * (1.0f / 32768.0f), v->n[1] * (1.0f / 32768.0f), v->n[2] * (1.0f / 32768.0f) };
    float pg[3], ng[3];
    for (int r = 0; r < 3; r++) {
      pg[r] = ((fp[0] * S[r] + fp[1] * S[3 + r]) + fp[2] * S[6 + r]) + S[9 + r];
      ng[r] = (fn[0] * S[r] + fn[1] * S[3 + r]) + fn[2] * S[6 + r];
    }
    /* positions: world units, then pixels */
    double dp = sqrt(((double)pg[0] - pe[0]) * (pg[0] - pe[0]) + ((double)pg[1] - pe[1]) * (pg[1] - pe[1]) + ((double)pg[2] - pe[2]) * (pg[2] - pe[2]));
    if (dp > T.dp_max) T.dp_max = dp;
    T.dp_sum += dp; T.dp_n++;
    double Xe = (double)pe[0] * x->world[0] + (double)pe[1] * x->world[3] + (double)pe[2] * x->world[6] + x->world[9];
    double Ye = (double)pe[0] * x->world[1] + (double)pe[1] * x->world[4] + (double)pe[2] * x->world[7] + x->world[10];
    double We = (double)pe[0] * x->world[2] + (double)pe[1] * x->world[5] + (double)pe[2] * x->world[8] + x->world[11];
    double Xg = (double)pg[0] * x->world[0] + (double)pg[1] * x->world[3] + (double)pg[2] * x->world[6] + x->world[9];
    double Yg = (double)pg[0] * x->world[1] + (double)pg[1] * x->world[4] + (double)pg[2] * x->world[7] + x->world[10];
    double Wg = (double)pg[0] * x->world[2] + (double)pg[1] * x->world[5] + (double)pg[2] * x->world[8] + x->world[11];
    if (We > 0 && Wg > 0) {
      double dx = fabs(0.9375 * x->vs[0] * (Xg / Wg - Xe / We)), dy = fabs(0.53125 * x->vs[1] * (Yg / Wg - Ye / We)), d = dx > dy ? dx : dy;
      if (d > T.ds_max) T.ds_max = d;
      T.dsh[bucket_px(d)]++; T.ds_n++;
    } else T.behind++;
    /* the normal as the GE lights it (world identity: normalised, (0, 0, 1) if zero) */
    float nl = sqrtf((ng[0] * ng[0] + ng[1] * ng[1]) + ng[2] * ng[2]), nn[3];
    for (int r = 0; r < 3; r++) nn[r] = nl > 0 ? ng[r] / nl : (r == 2 ? 1.0f : 0.0f);
    if (nb == 1) {   /* the PS2 does not normalise a 1-bone normal: its length, before the GE's normalising */
      const uint32_t a = 32 + v->o[0];
      double m[3] = { 0, 0, 0 };
      for (int r = 0; r < 3; r++) for (int col = 0; col < 3; col++) m[r] += (double)F(Q(vm, a + (uint32_t)col)[r]) * (v->n[col] / 4096.0);
      double dev = fabs(sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]) - 1.0);
      if (dev > T.n1_dev) T.n1_dev = dev;
    }
    /* the light sum, PPSSPP's integer form, with the ambient as is (0) and one level up (1) */
    for (int bias = 0; bias < 2; bias++) {
      int col[3];
      for (int k = 0; k < 3; k++) { int a = AMB[k] + bias; if (a > 255) a = 255; col[k] = (511 * (2 * a + 1)) >> 10; }
      for (int j = 0; j < 3; j++) {
        float dl = sqrtf((dir[j][0] * dir[j][0] + dir[j][1] * dir[j][1]) + dir[j][2] * dir[j][2]), dn[3];
        for (int r = 0; r < 3; r++) dn[r] = dl > 0 ? dir[j][r] / dl : (r == 2 ? 1.0f : 0.0f);
        const float d = (dn[0] * nn[0] + dn[1] * nn[1]) + dn[2] * nn[2];
        if (!(d > 0.0f)) continue;
        int att = (int)ceilf(256 * 2 * d + 1); if (att > 512) att = 512;
        for (int k = 0; k < 3; k++) col[k] += (int)(((int64_t)(2 * LD[j][k] + 1) * 511 * att) >> 19);
      }
      for (int k = 0; k < 3; k++) {
        const int g = col[k] > 255 ? 255 : col[k], e = (int)(ce >> (8 * k) & 0xff), dd = g - e;
        const int bi = dd < -5 ? 0 : dd > 5 ? 10 : dd + 5;
        T.dch[bias][bi]++; T.dc_sum[bias] += dd;
        if (abs(dd) > T.dc_max[bias]) T.dc_max[bias] = abs(dd);
        if (!bias) { if (nb == 1) T.dch1[bi]++; else T.dch2[bi]++; }
      }
    }
    T.dc_n += 3;
    /* pass 3: the texture matrix on the normalised skinned normal (PPSSPP: Normalized(), world identity) */
    if (L->p3 > 0 && v3) {
      const VcVertF *e = (const VcVertF *)v3 + i;
      float m[12];
      for (int r = 0; r < 3; r++) { m[3 * r] = f24(s[0] * R[r][0]); m[3 * r + 1] = f24(s[1] * R[r][1]); m[3 * r + 2] = 0; }
      m[9] = f24(o[0]); m[10] = f24(o[1]); m[11] = 1;
      const float src[3] = { ng[0] / nl, ng[1] / nl, ng[2] / nl };
      float uv[2];
      for (int k = 0; k < 2; k++) uv[k] = ((src[0] * m[k] + src[1] * m[3 + k]) + src[2] * m[6 + k]) + m[9 + k];
      double du = fabs((double)uv[0] - e->u), dv = fabs((double)uv[1] - e->v), d = du > dv ? du : dv;
      if (d > T.du_max) T.du_max = d;
      T.du_sum += d; T.du_n++;
      const double t = d * 256;   /* texels of a 256-texel texture */
      T.duh[t < 1.0 / 64 ? 0 : t < 1.0 / 16 ? 1 : t < 0.25 ? 2 : t < 0.5 ? 3 : t < 1 ? 4 : 5]++;
    }
  }
  (void)fl;
}

static void pvals(FILE *f, const char *name, const Val *t, int asfloat)
{
  fprintf(f, " %s", name);
  for (int i = 0; i < NV && t[i].n; i++) { if (asfloat) fprintf(f, " %g x%ld", (double)F(t[i].v), t[i].n); else fprintf(f, " %u x%ld", t[i].v, t[i].n); }
  fprintf(f, ";");
}
void c1m_report(FILE *f)
{
  if (!c1m_on || first) return;
  fprintf(f, "c1 census: P07 calls %ld (%ld vertices; fast %ld, the exact form %ld); bones 1/2/3/4: %ld %ld %ld %ld; pass 2 none/1/2/-1: %ld %ld %ld %ld; "
          "pass 3 none/normal/other: %ld %ld %ld\n", T.calls, T.verts, T.fast, T.any, T.nbh[1], T.nbh[2], T.nbh[3], T.nbh[4],
          T.uv2h[0], T.uv2h[1], T.uv2h[2], T.uv2h[3], T.p3h[0], T.p3h[1], T.p3h[2]);
  fprintf(f, "c1 census:"); pvals(f, "clamp abs 3 x", T.cmax, 1); pvals(f, "alpha", T.alpha, 0); fprintf(f, "\n");
  fprintf(f, "c1 census: light lengths %.6g..%.6g, %.6g..%.6g, %.6g..%.6g; colour x length max %.6g, not whole %ld, from 255.5 %ld, below 0 %ld (of %ld); "
          "ambient %.6g..%.6g, not whole %ld, outside 0..255 %ld\n", T.llen_min[0], T.llen_max[0], T.llen_min[1], T.llen_max[1], T.llen_min[2],
          T.llen_max[2], T.ld_max, T.ld_frac, T.ld_over, T.ld_neg, 9 * T.calls, T.amb_min, T.amb_max, T.amb_frac, T.amb_over);
  fprintf(f, "c1 census: bone offsets not a multiple of 4 %ld, above 28 %ld; pairs naming a slot twice %ld; 2-4 bone weight sums other than 128 %ld; "
          "highest slot 0..8: %ld %ld %ld %ld %ld %ld %ld %ld %ld; bone columns' lengths %.6g..%.6g, largest |cos| between columns %.3g; "
          "1-bone normal length - 1 up to %.3g\n", T.off_unal, T.off_out, T.wdup, T.wsum_bad, T.slots[0], T.slots[1], T.slots[2], T.slots[3],
          T.slots[4], T.slots[5], T.slots[6], T.slots[7], T.slots[8], T.coll_min, T.coll_max, T.orth_max, T.n1_dev);
  fprintf(f, "c1 model: calls C1 takes %ld (%ld vertices); left: light colour below 0 %ld, above 255 %ld, ambient %ld, clamp not 255 %ld, bones %ld, "
          "offset %ld / %ld, over 64 %ld; no slot %ld\n", T.elig, T.elig_v, T.why[0], T.why[1], T.why[2], T.why[3], T.why[4], T.why[5], T.why[6], T.why[7], T.noslot);
  fprintf(f, "c1 model: positions: world distance max %.4g, mean %.4g; GE pixels (larger of x, y) max %.4g, below 1/64 %ld, 1/64-1/16 %ld, 1/16-1/4 %ld, "
          "1/4-1/2 %ld, 1/2-1 %ld, from 1 %ld; behind the eye %ld\n", T.dp_max, T.dp_n ? T.dp_sum / T.dp_n : 0, T.ds_max, T.dsh[0], T.dsh[1],
          T.dsh[2], T.dsh[3], T.dsh[4], T.dsh[5], T.behind);
  for (int b = 0; b < 2; b++) {
    fprintf(f, "c1 model: colour, GE - exact, ambient +%d: mean %.3f, max |d| %ld; -5 or below .. +5 or above:", b, T.dc_n ? T.dc_sum[b] / T.dc_n : 0, T.dc_max[b]);
    for (int k = 0; k < 11; k++) fprintf(f, " %ld", T.dch[b][k]);
    fprintf(f, "\n");
  }
  fprintf(f, "c1 model: colour (ambient +0) 1 bone:"); for (int k = 0; k < 11; k++) fprintf(f, " %ld", T.dch1[k]);
  fprintf(f, "; 2-4 bones:"); for (int k = 0; k < 11; k++) fprintf(f, " %ld", T.dch2[k]); fprintf(f, "\n");
  fprintf(f, "c1 census b: distinct slots per call 0..8:"); for (int k = 0; k < 9; k++) fprintf(f, " %ld", T.nslot[k]);
  fprintf(f, "; cache by the streams' addresses: calls without streams %ld, new keys %ld (%ld vertices, %ld KB at 24 bytes), same bytes %ld, "
          "other bytes %ld, bytes seen before under any key %ld; bytes hashed a call %.1f; table full %ld\n", T.ck_nosp, T.ck_new, T.ck_keyv,
          T.ck_keyv * 24 / 1024, T.ck_same, T.ck_diff, T.ck_cseen, T.elig ? (double)T.ck_bytes / T.elig : 0, T.ck_full);
  fprintf(f, "c1 model: pass 3 UVs: %ld vertices, max |d| %.4g, mean %.4g; in texels of a 256 texture: below 1/64 %ld, 1/64-1/16 %ld, 1/16-1/4 %ld, "
          "1/4-1/2 %ld, 1/2-1 %ld, from 1 %ld\n", T.du_n, T.du_max, T.du_n ? T.du_sum / T.du_n : 0, T.duh[0], T.duh[1], T.duh[2], T.duh[3], T.duh[4], T.duh[5]);
}
