/* p07chk.c: ge/vu1conv.c vc_p07 against gsref's P07 dumps (the study gsref's GSREF_COV=1a31e0 GSREF_P7DUMP=<file>; renderer_notes
   11.17): for every call, the strip vertices each pass kicked against the converter's vertices, with what the program computes from
   them in its own order (the GE draws with its own transform and fog; this recomputes the PS2's to check what the converter hands
   over): clip = V s (V: abs 8..11), Q = 1 / w (DIV), XYZ = (clip.xyz Q) x abs 4 + abs 5 (FTOI4), F and ADC (clamp(a w + b, 0, abs 3 x)
   (abs 2), -2048 with the data's ADC bit, FTOI4), RGBA, and ST and Q of every pass: pass 1 UV1 Q, pass 2 UV2 times pass 1's q (from
   UV1 plus the call's (du, dv), or its own vertices: vc_p06_uv2), pass 3 (u, v, 1) times pass 1's q.  Prints counts only (the dumps
   are game data, personal use, kept in the container).
   cc -O2 -ffp-contract=off -o p07chk p07chk.c ../../ge/vu1conv.c -lm
   p07chk [-rz] [-exact] <dump>...   -rz: the PSP's float mode (round toward zero, denormal results flushed: MXCSR as boot.c's
   MGS2_FPU_RZ), for dumps made with GSREF_RZ=1; -exact: every call in the exact form (vc_p06_exact).  Without -exact, each call the
   fast form (p7_loop) took is converted again in the exact form, and the two outputs are compared byte for byte */
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

/* the dump's kicked vertex (tools/gsref.c P6Rec: gsref's GfVtx order, then adc, prim, tag, register) */
typedef struct { int x, y; uint32_t z; int f, r, g, b, a, u, v; float s, t, q; uint8_t adc, prim, tag, reg; } Rec;
static int rd(FILE *f, void *p, size_t n) { return fread(p, 1, n, f) == n; }
static uint8_t VM[16384] __attribute__((aligned(16)));
static uint8_t V1[64 * 24], V2[64 * 24], V3[64 * 24];
static const uint32_t *vq(uint32_t qa) { return (const uint32_t *)(VM + ((qa & 0x3ffu) << 4)); }

int main(int argc, char **argv)
{
  int a0 = 1;
  for (; a0 < argc && argv[a0][0] == '-'; a0++)
    if (!strcmp(argv[a0], "-rz")) _mm_setcsr(_mm_getcsr() | 0x6000u | 0x8000u);
    else if (!strcmp(argv[a0], "-exact")) vc_p06_exact = 1;
  long fastc = 0, fdiff = 0;
  long calls = 0, bones[5] = { 0 }, bad = 0, verts = 0, wrong = 0, w_xyz = 0, w_f = 0, w_rgba = 0, w_st = 0, w_q = 0, w_cnt = 0, rets[8] = { 0 };
  long pass2 = 0, pass2_own = 0, pass2_no = 0, pass3 = 0, pass3_no = 0, uv16 = 0, fogk = 0, prims[8] = { 0 }, shown = 0, flagv[16] = { 0 };
  for (int ai = a0; ai < argc; ai++) {
    FILE *f = fopen(argv[ai], "rb"); if (!f) { perror(argv[ai]); return 1; }
    for (;;) {
      uint32_t hd[8];
      if (!rd(f, hd, 32)) break;
      if (hd[0] != 0x31443750u) { fprintf(stderr, "bad magic\n"); return 1; }
      uint32_t vb = hd[1], call = hd[2], ent = hd[3], top = hd[4]; int nt = (int)hd[6], nr = (int)hd[7];
      uint8_t skip[32 * 16 + 16 * 4];
      if (!rd(f, VM, sizeof VM) || !rd(f, skip, sizeof skip)) return 1;
      uint8_t *tags = malloc(112 * (size_t)nt + 112); Rec *rec = malloc(sizeof(Rec) * (size_t)nr + sizeof(Rec));
      if (!rd(f, tags, 112 * (size_t)nt)) return 1;
      for (int i = 0; i < nr; i++) { uint8_t b[56]; if (!rd(f, b, 56)) return 1; memcpy(&rec[i], b, 52); rec[i].adc = b[52]; rec[i].prim = b[53]; rec[i].tag = b[54]; rec[i].reg = b[55]; }
      calls++;
      if (ent != VC_P07) { rets[0]++; bad++; goto next; }
      VcLit L; uint16_t fl[64]; uint32_t rgba[64];
      int n = vc_p07(VM, top, &L, V1, V3, fl, rgba, 64);
      if (n < 0) { rets[-n & 7]++; bad++; goto next; }
      if (L.fast) {   /* the fast form against the exact form, byte for byte */
        static uint8_t E1[64 * 24], E3[64 * 24]; VcLit LE; uint16_t fe[64]; uint32_t re[64];
        vc_p06_exact = 1; int ne = vc_p07(VM, top, &LE, E1, E3, fe, re, 64); vc_p06_exact = 0;
        fastc++; LE.fast = 1;
        if (ne != n || memcmp(&LE, &L, sizeof L) || memcmp(E1, V1, (size_t)vc_p06_size1(&L) * (size_t)n) ||
            (L.p3 > 0 && memcmp(E3, V3, 24 * (size_t)n)) || memcmp(fe, fl, 2 * (size_t)n) || memcmp(re, rgba, 4 * (size_t)n)) fdiff++;
      }
      bones[vq(top)[2] & 0xffff]++; uv16 += L.uv16; fogk += L.fog >= 0;
      for (int b = 0; b < 16; b++) flagv[b] += L.hy >> b & 1;
      if (L.hy & 0x10) { if (L.uv2 > 0) pass2++; else pass2_no++; if (L.uv2 == 2) pass2_own++; }
      if (L.hy & 0x700) { if (L.p3 > 0) pass3++; else pass3_no++; }
      {
        float V[4][4], vs[3], vofs[3], fa = vfl(vq(2)[0]), fb = vfl(vq(2)[1]), cmax = vfl(vq(3)[0]);
        for (int j = 0; j < 4; j++) for (int k = 0; k < 4; k++) V[j][k] = vfl(vq(8 + j)[k]);
        for (int k = 0; k < 3; k++) { vs[k] = vfl(vq(4)[k]); vofs[k] = vfl(vq(5)[k]); }
        const int t1 = L.uv16 ? 2 : 3, s1 = vc_p06_size1(&L);
        float Q[64], q1[64], u1[64][2], u2[64][2], u3[64][2]; int X[64], Y[64], F[64], A[64]; uint32_t Z[64];
        int t2 = L.uv2 == 2 ? vc_p06_uv2(VM, top, &L, V1, V2) : 0, s2 = 16 + 4 * ((t2 & 1) + (t2 >> 1));
        for (int i = 0; i < n; i++) {
          const uint8_t *o = V1 + s1 * i; float p[3];
          if (t1 == 2) { const VcVertS *v = (const VcVertS *)o; p[0] = v->x; p[1] = v->y; p[2] = v->z; u1[i][0] = v->u * (1.0f / 4096); u1[i][1] = v->v * (1.0f / 4096); }
          else { const VcVertF *v = (const VcVertF *)o; p[0] = v->x; p[1] = v->y; p[2] = v->z; u1[i][0] = v->u; u1[i][1] = v->v; }
          if (L.uv2 == 2) { const uint8_t *o2 = V2 + s2 * i;   /* its own UVs: a 16-bit 4.12 value or the float UV2 / 4096 */
            if (t2 & 1) memcpy(u2[i], o2, 8);
            else { uint16_t q[2]; memcpy(q, o2, 4); u2[i][0] = q[0] * (1.0f / 4096); u2[i][1] = q[1] * (1.0f / 4096); } }
          else if (L.uv2 == 1) {   /* UV2 = UV1 + (du, dv): the integers again */
            u2[i][0] = (float)((int32_t)lrintf(u1[i][0] * 4096) + L.du) * (1.0f / 4096); u2[i][1] = (float)((int32_t)lrintf(u1[i][1] * 4096) + L.dv) * (1.0f / 4096); }
          if (L.p3 > 0) { const VcVertF *v = (const VcVertF *)(V3 + 24 * i); u3[i][0] = v->u; u3[i][1] = v->v; }
          /* phase 1's clip (MULAx, MADDAy, MADDAz, MADDw), phase 2's DIV, MULq, MULA / MADDw (the viewport), the fog (MULAw, MADDy, MAXx,
             MINIx, ADDw with ITOF4 of MFIR(in[0].w & 0x8000)) */
          float cl[4];
          for (int k = 0; k < 4; k++) { float a = vo(V[0][k] * p[0]); a = vo(a + V[1][k] * p[1]); a = vo(a + V[2][k] * p[2]); cl[k] = vo(a + V[3][k] * 1.0f); }
          Q[i] = cl[3] == 0.0f ? vfl((fu(cl[3]) & 0x80000000u) | 0x7f7fffffu) : vo(1.0f / cl[3]);
          float sc[3];
          for (int k = 0; k < 3; k++) { sc[k] = vo(cl[k] * Q[i]); sc[k] = vo(vo(sc[k] * vs[k]) + vofs[k] * 1.0f); }
          X[i] = ftoi(sc[0], 4) & 0xffff; Y[i] = ftoi(sc[1], 4) & 0xffff; Z[i] = (uint32_t)ftoi(sc[2], 4) >> 4 & 0xffffff;
          float fg = vo(vo(cl[3] * fa) + 1.0f * fb);
          fg = fg > 0.0f ? fg : 0.0f; fg = fg < cmax ? fg : cmax;
          fg = vo(fg + ((fl[i] & 0x8000) ? -2048.0f : 0.0f));
          int32_t fi = ftoi(fg, 4); F[i] = fi >> 4 & 0xff; A[i] = fi >> 15 & 1;
          q1[i] = vo(1.0f * Q[i]);   /* pass 1's q: ITOF12 of UV1 z (1.0, checked by the converter) times Q */
        }
        /* the passes the program kicked: pass 1, pass 2 (0x10), pass 3 (0x700), tags 1, 3, 5 in that order */
        int np = 1, ps_kind[3] = { 1, 0, 0 };
        if (L.hy & 0x10) ps_kind[np++] = 2;
        if (L.hy & 0x700) ps_kind[np++] = 3;
        for (int i = 0; i < nr; i++) prims[rec[i].prim & 7]++;
        for (int ps = 0; ps < np; ps++) {
          int kind = ps_kind[ps], tag = 2 * ps + 1, m = 0;
          if ((kind == 2 && L.uv2 <= 0) || (kind == 3 && L.p3 <= 0)) continue;   /* not drawn by the converter: counted above */
          for (int i = 0; i < nr; i++) {
            const Rec *x = &rec[i];
            if (x->tag != tag) continue;
            if (m >= n) { w_cnt++; m++; continue; }
            float s, t, q;
            if (kind == 1) { s = vo(u1[m][0] * Q[m]); t = vo(u1[m][1] * Q[m]); q = q1[m]; }
            else if (kind == 2) { s = vo(u2[m][0] * q1[m]); t = vo(u2[m][1] * q1[m]); q = vo(1.0f * q1[m]); }
            else { s = vo(u3[m][0] * q1[m]); t = vo(u3[m][1] * q1[m]); q = vo(1.0f * q1[m]); }
            int w = 0;
            if (x->x != X[m] || x->y != Y[m] || x->z != Z[m]) { w_xyz++; w = 1; }
            if (x->f != F[m] || x->adc != A[m]) { w_f++; w = 1; }
            uint32_t c = rgba[m];
            if (x->r != (int)(c & 0xff) || x->g != (int)(c >> 8 & 0xff) || x->b != (int)(c >> 16 & 0xff) || x->a != (int)(c >> 24)) { w_rgba++; w = 1; }
            if (fu(x->s) != fu(s) || fu(x->t) != fu(t)) { w_st++; w = 1; }
            if (fu(x->q) != fu(q)) { w_q++; w = 1; }
            if (w && shown++ < 12)
              printf("vb %u call %u pass %d v %d/%d: ps2 xyz %04x %04x %06x f %d adc %d rgba %d %d %d %d st %.9g %.9g %.9g | conv xyz %04x %04x %06x f %d adc %d rgba %u %u %u %u st %.9g %.9g %.9g\n",
                     vb, call, kind, m, n, x->x, x->y, x->z, x->f, x->adc, x->r, x->g, x->b, x->a, x->s, x->t, x->q,
                     X[m], Y[m], Z[m], F[m], A[m], c & 0xff, c >> 8 & 0xff, c >> 16 & 0xff, c >> 24, s, t, q);
            wrong += w; verts++; m++;
          }
          if (m != n) { w_cnt++; if (shown++ < 12) printf("vb %u call %u pass %d: %d vertices kicked, %d converted\n", vb, call, kind, m, n); }
        }
      }
    next:
      free(tags); free(rec);
    }
    fclose(f);
  }
  printf("calls %ld (bones 1 %ld, 2 %ld, 3 %ld, 4 %ld; 16-bit UVs %ld; constant fog %ld; refused %ld: not P07 %ld, n above 64 %ld, UV1 z %ld, "
         "paths not seen %ld); pass 2 drawn %ld (its own vertices %ld), not %ld; pass 3 drawn %ld, not %ld\n",
         calls, bones[1], bones[2], bones[3], bones[4], uv16, fogk, bad, rets[0], rets[1], rets[6], rets[7], pass2, pass2_own, pass2_no, pass3, pass3_no);
  printf("flag bits set (calls):"); for (int b = 0; b < 16; b++) if (flagv[b]) printf(" %x:%ld", 1 << b, flagv[b]);
  printf("; kicked vertices by prim:"); for (int k = 0; k < 8; k++) if (prims[k]) printf(" %d:%ld", k, prims[k]);
  printf("\nstrip vertices compared %ld, wrong %ld: xyz %ld, f/adc %ld, rgba %ld, st %ld, q %ld; passes with another count %ld\n",
         verts, wrong, w_xyz, w_f, w_rgba, w_st, w_q, w_cnt);
  printf("fast form: calls %ld, their output differing from the exact form's %ld\n", fastc, fdiff);
  return 0;
}
