/* m1den.c (dev, audit M1): records the FPU's denormal operands of the generated code per site, and the PSP zero rule's uses
   (guest.h M1DEN).  MGS2_M1_PS2=1: build B, the R5900's rule (MXCSR DAZ set before boot.c ORs in RZ + FTZ; FADD / FSUB give the zero sum).
   Report at exit on stderr: "m1:" lines. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <xmmintrin.h>
unsigned long m1_nload, m1_nmtc1; int g_m1_ps2;
extern long g_hash_vb;
static long *m1_kvb;   /* boot.c's static k_vblanks, found from g_hash_vb and the offset in MGS2_M1_KVB (nm k_vblanks - nm g_hash_vb) */
typedef struct { unsigned site; uint32_t pc, a0, u0, a1, u1; int kind; long n, vb0, vb1; } Rec;
#define NREC 4096
static Rec rec[NREC]; static int nrec; static long ndrop, nden[3];
typedef struct { void *ra; int sub; uint32_t a0, b0; long n, vb0, vb1; } Rule;
static Rule rule[1024]; static int nrule; static long nrule_all;
static long vbnow(void) { return m1_kvb ? *m1_kvb : g_hash_vb; }
float m1_den(unsigned site, uint32_t pc, uint32_t a, uint32_t u, int kind)
{
  int i; long vb = vbnow(); nden[kind]++;
  for (i = 0; i < nrec; i++) if (rec[i].site == site) break;
  if (i == nrec) { if (nrec == NREC) { ndrop++; goto out; } nrec++; rec[i].site = site; rec[i].pc = pc; rec[i].kind = kind; rec[i].a0 = a; rec[i].u0 = u; rec[i].vb0 = vb; }
  rec[i].n++; rec[i].a1 = a; rec[i].u1 = u; rec[i].vb1 = vb;
out: { float f; memcpy(&f, &u, 4); return f; }
}
__attribute__((noinline)) void m1_rule(int sub, float a, float b)
{
  void *ra = __builtin_return_address(0); int i; long vb = vbnow(); nrule_all++;
  for (i = 0; i < nrule; i++) if (rule[i].ra == ra && rule[i].sub == sub) break;
  if (i == nrule) { if (nrule == 1024) return; nrule++; rule[i].ra = ra; rule[i].sub = sub; memcpy(&rule[i].a0, &a, 4); memcpy(&rule[i].b0, &b, 4); rule[i].vb0 = vb; }
  rule[i].n++; rule[i].vb1 = vb;
}
/* guest.h M1VGT: each vu_vgrp run while k_vblanks is in MGS2_M1_VG=<a>:<b>: "m1vg: <seq> vb <vb> pc <pc> fb <fallbacks> h <FNV-1a 32 of VF
   before the run>"; the runs whose seq is in MGS2_M1_VGSEQ=<s>,... or whose pc is in MGS2_M1_VGPC=<pc>,..., and every run that fell back,
   also "m1vgd: <seq> pre <VF before> post <VF after>" */
extern uint32_t VF[32][4];
static long m1_vgdummy = -1; volatile long *m1_vgk = &m1_vgdummy; long m1_vga = 1, m1_vgb = 0;
static uint32_t vg_snap[32][4]; static long vg_seq, vg_dseq[64]; static uint32_t vg_dpc[64]; static int vg_nds, vg_ndp;
void m1_vgpre(void) { memcpy(vg_snap, VF, sizeof vg_snap); }
void m1_vgnote(uint32_t pc, uint32_t fb)
{
  uint32_t h = 2166136261u; const uint8_t *p = (const uint8_t *)vg_snap; int i, d = fb != 0; long s = vg_seq++;
  for (i = 0; i < (int)sizeof vg_snap; i++) { h ^= p[i]; h *= 16777619u; }
  for (i = 0; i < vg_nds; i++) if (vg_dseq[i] == s) d = 1;
  for (i = 0; i < vg_ndp; i++) if (vg_dpc[i] == pc) d = 1;
  fprintf(stderr, "m1vg: %ld vb %ld pc %x fb %u h %08x\n", s, vbnow(), pc, fb, h);
  if (d) { fprintf(stderr, "m1vgd: %ld pre", s); for (i = 0; i < 32; i++) fprintf(stderr, " %08x,%08x,%08x,%08x", vg_snap[i][0], vg_snap[i][1], vg_snap[i][2], vg_snap[i][3]);
    fprintf(stderr, " post"); for (i = 0; i < 32; i++) fprintf(stderr, " %08x,%08x,%08x,%08x", VF[i][0], VF[i][1], VF[i][2], VF[i][3]); fputc('\n', stderr); }
}
static void report(void)
{
  int i;
  fprintf(stderr, "m1: mode %s; float loads %lu, mtc1 %lu; denormal loads %ld (stack %ld), denormal mtc1 %ld; sites %d (dropped %ld); zero-rule uses %ld at %d places\n",
          g_m1_ps2 ? "PS2 (DAZ)" : "PSP rule", m1_nload, m1_nmtc1, nden[0] + nden[2], nden[2], nden[1], nrec, ndrop, nrule_all, nrule);
  for (i = 0; i < nrec; i++) fprintf(stderr, "m1: site %u pc %08x %s n %ld vb %ld-%ld first a %08x u %08x last a %08x u %08x\n", rec[i].site, rec[i].pc,
          rec[i].kind == 1 ? "mtc1" : rec[i].kind == 2 ? "lwc1s" : "lwc1", rec[i].n, rec[i].vb0, rec[i].vb1, rec[i].a0, rec[i].u0, rec[i].a1, rec[i].u1);
  for (i = 0; i < nrule; i++) fprintf(stderr, "m1: rule %s at %p n %ld vb %ld-%ld first %08x %08x\n", rule[i].sub ? "sub" : "add", rule[i].ra, rule[i].n,
          rule[i].vb0, rule[i].vb1, rule[i].a0, rule[i].b0);
  fprintf(stderr, "m1: base %p (g_hash_vb)\n", (void *)&g_hash_vb);
}
__attribute__((constructor)) static void m1_init(void)
{
  const char *k = getenv("MGS2_M1_KVB");
  if (k) m1_kvb = (long *)((char *)&g_hash_vb + strtol(k, 0, 0));
  { const char *e = getenv("MGS2_M1_VG"); if (e && m1_kvb && sscanf(e, "%ld:%ld", &m1_vga, &m1_vgb) == 2) m1_vgk = m1_kvb; }
  { const char *e = getenv("MGS2_M1_VGSEQ"); while (e && *e && vg_nds < 64) { vg_dseq[vg_nds++] = strtol(e, (char **)&e, 0); if (*e == ',') e++; else break; } }
  { const char *e = getenv("MGS2_M1_VGPC"); while (e && *e && vg_ndp < 64) { vg_dpc[vg_ndp++] = strtoul(e, (char **)&e, 16); if (*e == ',') e++; else break; } }
  if (getenv("MGS2_M1_PS2")) { g_m1_ps2 = 1; _mm_setcsr(_mm_getcsr() | 0x0040u); }   /* DAZ: denormal operands are zeros, as on the R5900 */
  atexit(report);
}
