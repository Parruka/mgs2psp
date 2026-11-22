/* p06chk.c: ge/vu1conv.c vc_p06 against gsref's P06 dumps (tools/gsref.c GSREF_P06DUMP; renderer_notes 11.14): for every call, the
   strip vertices each pass kicked against the converter's vertices, with the Q and XYZ the program computes from them in its own order
   (the GE draws with its own transform; this recomputes the PS2's to check the positions the converter hands over): RGBA, XYZ, F and ADC,
   ST and Q of every pass (pass 2 from pass 1's UV plus the call's (du, dv), or from its own vertices (vc_p06_uv2), pass 3 from its own
   vertices).  Prints counts only (the
   dumps are game data, personal use, kept in the container).
   cc -O2 -ffp-contract=off -o p06chk p06chk.c ../../ge/vu1conv.c -lm
   p06chk [-rz] [-exact] <dump>...   -rz: the PSP's float mode (round toward zero, denormal results flushed: MXCSR as boot.c's
   MGS2_FPU_RZ), for dumps made with GSREF_RZ=1; -exact: every call through the exact form (else the fast one where the converter
   takes it) */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>
#include "../../ge/vu1conv.h"

static float vfl(uint32_t u) { uint32_t e = u & 0x7f800000u; float f; if (!e) u &= 0x80000000u; else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu; memcpy(&f, &u, 4); return f; }
static float vo(float f) { uint32_t u; memcpy(&u, &f, 4); return vfl(u); }
static uint32_t fu(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static int32_t ftoi(float a, int sh) { float v = a * (float)(1 << sh); return v >= 2147483648.0f ? 0x7fffffff : v <= -2147483648.0f ? (int32_t)0x80000000u : (int32_t)v; }

/* the dump's kicked vertex (tools/gsref.c P6Rec: gsref7's GfVtx order, then adc, prim, tag, register) */
typedef struct { int x, y; uint32_t z; int f, r, g, b, a, u, v; float s, t, q; uint8_t adc, prim, tag, reg; } Rec;
static int rd(FILE *f, void *p, size_t n) { return fread(p, 1, n, f) == n; }
static uint8_t VM[16384] __attribute__((aligned(16)));
static uint8_t V1[64 * 24], V2[64 * 24], V3[64 * 24];

int main(int argc, char **argv)
{
  int a0 = 1;
  for (; a0 < argc && argv[a0][0] == '-'; a0++) {
    if (!strcmp(argv[a0], "-rz")) _mm_setcsr(_mm_getcsr() | 0x6000u | 0x8000u);
    else if (!strcmp(argv[a0], "-exact")) vc_p06_exact = 1;
  }
  long calls = 0, kinds[4] = { 0 }, fast = 0, bad = 0, verts = 0, wrong = 0, w_xyz = 0, w_f = 0, w_rgba = 0, w_st = 0, w_q = 0, w_cnt = 0;
  long pass2 = 0, pass2_own = 0, pass2_no = 0, pass3 = 0, pass3_no = 0, cull = 0, pos16 = 0, uv16 = 0, shown = 0, rets[8] = { 0 };
  for (int ai = a0; ai < argc; ai++) {
    FILE *f = fopen(argv[ai], "rb"); if (!f) { perror(argv[ai]); return 1; }
    for (;;) {
      uint32_t hd[8], ab[16][4];
      if (!rd(f, hd, 32)) break;
      if (hd[0] != 0x32433650u) { fprintf(stderr, "bad magic\n"); return 1; }
      uint32_t vb = hd[1], call = hd[2], ent = hd[3], top = hd[4]; int nt = (int)hd[5], nr = (int)hd[6], nq = (int)hd[7];
      if (!rd(f, ab, 256)) return 1;
      memset(VM, 0, sizeof VM); memcpy(VM, ab, 256);
      uint8_t *T = malloc(16 * (size_t)nq + 16); uint8_t *tags = malloc(112 * (size_t)nt + 112); Rec *rec = malloc(sizeof(Rec) * (size_t)nr + sizeof(Rec));
      if (!rd(f, T, 16 * (size_t)nq) || !rd(f, tags, 112 * (size_t)nt)) return 1;
      for (int i = 0; i < nr; i++) { uint8_t b[56]; if (!rd(f, b, 56)) return 1; memcpy(&rec[i], b, 52); rec[i].adc = b[52]; rec[i].prim = b[53]; rec[i].tag = b[54]; rec[i].reg = b[55]; }
      for (int q = 0; q < nq; q++) memcpy(VM + (((top + q) & 0x3ff) << 4), T + 16 * q, 16);
      calls++;
      VcLit L; uint16_t fl[64]; uint32_t rgba[64];
      int n = vc_p06(VM, top, ent, &L, V1, V3, fl, rgba, 64);
      if (n < 0) { rets[-n & 7]++; bad++; goto next; }
      kinds[L.kind]++; fast += L.fast; cull += L.cull; pos16 += L.pos16; uv16 += L.uv16;
      if (L.hy & 0x10) { if (L.uv2 > 0) pass2++; else pass2_no++; if (L.uv2 == 2) pass2_own++; }
      if (L.hy & 0x700) { if (L.p3 > 0) pass3++; else pass3_no++; }
      {
        /* the program's Q and XYZ from the converter's positions: M = T+0..3, on the no-clip path with the viewport folded in */
        float M[4][4], vs[3], vofs[3];
        for (int j = 0; j < 4; j++) for (int k = 0; k < 4; k++) M[j][k] = vfl(((uint32_t *)(VM + (((top + j) & 0x3ff) << 4)))[k]);
        for (int k = 0; k < 3; k++) { vs[k] = vfl(ab[4][k]); vofs[k] = vfl(ab[5][k]); }
        int clip = L.x.clip;
        if (!clip) for (int j = 0; j < 4; j++) for (int k = 0; k < 3; k++) M[j][k] = vo(vo(M[j][k] * vs[k]) + vofs[k] * M[j][3]);
        int s1 = vc_p06_size1(&L), s3 = vc_p06_size3(&L), t1 = (L.uv16 ? 0 : 1) | (L.pos16 ? 0 : 2);
        float Q[64], u1[64][2], u2[64][2], u3[64][2]; int X[64], Y[64], A[64]; uint32_t Z[64]; uint32_t clipf = 0;
        int t2 = L.uv2 == 2 ? vc_p06_uv2(VM, top, &L, V1, V2) : 0, s2 = 16 + 4 * ((t2 & 1) + (t2 >> 1));
        for (int i = 0; i < n; i++) {
          const uint8_t *o = V1 + s1 * i; float p[3];
          switch (t1) {
            case 0: { const VcVert *v = (const VcVert *)o; p[0] = v->x; p[1] = v->y; p[2] = v->z; u1[i][0] = v->u * (1.0f / 4096); u1[i][1] = v->v * (1.0f / 4096); break; }
            case 1: { const VcVertE *v = (const VcVertE *)o; p[0] = v->x; p[1] = v->y; p[2] = v->z; u1[i][0] = v->u; u1[i][1] = v->v; break; }
            case 2: { const VcVertS *v = (const VcVertS *)o; p[0] = v->x; p[1] = v->y; p[2] = v->z; u1[i][0] = v->u * (1.0f / 4096); u1[i][1] = v->v * (1.0f / 4096); break; }
            default: { const VcVertF *v = (const VcVertF *)o; p[0] = v->x; p[1] = v->y; p[2] = v->z; u1[i][0] = v->u; u1[i][1] = v->v; break; }
          }
          if (L.uv2 == 2) { const uint8_t *o2 = V2 + s2 * i;   /* its own UVs: a 16-bit 4.12 value or the float UV2 / 4096 */
            if (t2 & 1) memcpy(u2[i], o2, 8);
            else { uint16_t q[2]; memcpy(q, o2, 4); u2[i][0] = q[0] * (1.0f / 4096); u2[i][1] = q[1] * (1.0f / 4096); } }
          if (L.p3 > 0) { const uint8_t *o3 = V3 + s3 * i;
            if (L.pos16) { const VcVertE *v = (const VcVertE *)o3; u3[i][0] = v->u; u3[i][1] = v->v; }
            else { const VcVertF *v = (const VcVertF *)o3; u3[i][0] = v->u; u3[i][1] = v->v; } }
          float cl[4];
          for (int k = 0; k < 4; k++) { float a = vo(M[0][k] * p[0]); a = vo(a + M[1][k] * p[1]); a = vo(a + M[2][k] * p[2]); cl[k] = vo(a + M[3][k] * 1.0f); }
          Q[i] = cl[3] == 0.0f ? vfl((fu(cl[3]) & 0x80000000u) | 0x7f7fffffu) : vo(1.0f / cl[3]);
          float sc[3];
          for (int k = 0; k < 3; k++) sc[k] = vo(cl[k] * Q[i]);
          if (clip) for (int k = 0; k < 3; k++) sc[k] = vo(vo(sc[k] * vs[k]) + vofs[k] * 1.0f);
          X[i] = ftoi(sc[0], 4) & 0xffff; Y[i] = ftoi(sc[1], 4) & 0xffff; Z[i] = (uint32_t)ftoi(sc[2], 4) >> 4 & 0xffffff;
          A[i] = (fl[i] >> 15) & 1;
          if (clip) {   /* the clip path: ADC also when one of the last three vertices is outside (CLIPw, FCAND 0x03ffff) */
            float aw = fabsf(cl[3]); uint32_t b = 0;
            b |= (cl[0] > aw) | (cl[0] < -aw) << 1 | (cl[1] > aw) << 2 | (cl[1] < -aw) << 3 | (cl[2] > aw) << 4 | (cl[2] < -aw) << 5;
            clipf = ((clipf << 6) | b) & 0xffffff;
            if (clipf & 0x03ffff) A[i] = 1;
          }
        }
        /* the passes the program kicked: pass 1, pass 2 (0x10), pass 3 (0x700), tags 1, 3, 5 in that order */
        int np = 1, ps_kind[3] = { 1, 0, 0 };
        if (L.hy & 0x10) ps_kind[np++] = 2;
        if (L.hy & 0x700) ps_kind[np++] = 3;
        for (int ps = 0; ps < np; ps++) {
          int kind = ps_kind[ps], tag = 2 * ps + 1, m = 0;
          if ((kind == 2 && L.uv2 <= 0) || (kind == 3 && L.p3 <= 0)) continue;   /* not drawn by the converter: counted above */
          for (int i = 0; i < nr; i++) {
            const Rec *x = &rec[i];
            if (x->tag != tag || x->prim != 4) continue;
            if (m >= n) { w_cnt++; m++; continue; }
            float qz = vo(1.0f * Q[m]), s, t, q;
            if (kind == 1) { s = vo(u1[m][0] * Q[m]); t = vo(u1[m][1] * Q[m]); q = qz; }
            else if (kind == 2 && L.uv2 == 2) { s = vo(u2[m][0] * qz); t = vo(u2[m][1] * qz); q = vo(1.0f * qz); }
            else if (kind == 2) {   /* UV2 = UV1 + (du, dv): the integers again */
              float a = (float)((int32_t)lrintf(u1[m][0] * 4096) + L.du) * (1.0f / 4096), b = (float)((int32_t)lrintf(u1[m][1] * 4096) + L.dv) * (1.0f / 4096);
              s = vo(a * qz); t = vo(b * qz); q = vo(1.0f * qz);
            } else { s = vo(u3[m][0] * qz); t = vo(u3[m][1] * qz); q = vo(1.0f * qz); }
            int w = 0;
            if (x->x != X[m] || x->y != Y[m] || x->z != Z[m]) { w_xyz++; w = 1; }
            if (x->f != L.fog || x->adc != A[m]) { w_f++; w = 1; }
            uint32_t c = rgba[m];
            if (x->r != (int)(c & 0xff) || x->g != (int)(c >> 8 & 0xff) || x->b != (int)(c >> 16 & 0xff) || x->a != (int)(c >> 24)) { w_rgba++; w = 1; }
            if (fu(x->s) != fu(s) || fu(x->t) != fu(t)) { w_st++; w = 1; }
            if (fu(x->q) != fu(q)) { w_q++; w = 1; }
            if (w && shown++ < 12)
              printf("vb %u call %u ent %06x pass %d v %d/%d: ps2 xyz %04x %04x %06x f %d adc %d rgba %d %d %d %d st %.9g %.9g %.9g | conv xyz %04x %04x %06x f %d adc %d rgba %u %u %u %u st %.9g %.9g %.9g\n",
                     vb, call, ent, kind, m, n, x->x, x->y, x->z, x->f, x->adc, x->r, x->g, x->b, x->a, x->s, x->t, x->q,
                     X[m], Y[m], Z[m], L.fog, A[m], c & 0xff, c >> 8 & 0xff, c >> 16 & 0xff, c >> 24, s, t, q);
            wrong += w; verts++; m++;
          }
          if (m != n) { w_cnt++; if (shown++ < 12) printf("vb %u call %u ent %06x pass %d: %d strip vertices kicked, %d converted\n", vb, call, ent, kind, m, n); }
        }
      }
    next:
      free(T); free(tags); free(rec);
    }
    fclose(f);
  }
  printf("calls %ld (+0 %ld, +358 %ld, +738 %ld, +ab0 %ld; fast form %ld; 16-bit positions %ld, 16-bit UVs %ld; refused %ld: n above 64 %ld, "
         "UV1 z %ld); pass 2 drawn %ld (its own vertices %ld), not %ld; pass 3 drawn %ld, not %ld; cull bit with cullable data %ld\n",
         calls, kinds[0], kinds[1], kinds[2], kinds[3], fast, pos16, uv16, bad, rets[1], rets[6], pass2, pass2_own, pass2_no, pass3, pass3_no, cull);
  printf("strip vertices compared %ld, wrong %ld: xyz %ld, f/adc %ld, rgba %ld, st %ld, q %ld; passes with another count %ld\n",
         verts, wrong, w_xyz, w_f, w_rgba, w_st, w_q, w_cnt);
  return 0;
}
