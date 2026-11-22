/* p06bench.c: ge/vu1conv.c vc_p06 (and vc_p06_uv2) on gsref's P06 dumps (tools/gsref.c GSREF_P06DUMP), for the converter's cost on the
   PSP and the sameness of its outputs across builds (task #70; renderer_notes 11.14).  Every call's outputs (the return value, VcLit, the
   vertices of passes 1, 2 and 3, the flags and the RGBA words) go into one FNV-1a 64 hash, in call order.  The x86 build with -rz (the
   PSP's float mode, as p06chk -rz) and the PSP build must print the same hash for the same dumps; tools/dev/p06chk.c checks the x86
   outputs against the vertices the PS2 kicked.  So a change to the converter keeps its bits when p06chk -rz still finds 0 wrong and
   this hash is unchanged on both builds.
   PSP (PPSSPP with PspProf, tools/ppsspp_mgs2.diff; tools/dev/p06bench.sh): the working directory holds list.txt (one dump file name a
   line) and the dumps; pass 1 runs under the JIT and gives the hash, pass 2 sets g_emuprof to 1 around the converter calls only (2,
   paused, elsewhere; 0 at the end, when PspProf writes its profile), so the profile counts the converter alone.  Both passes go to
   bench.txt.
   x86: p06bench [-rz] <dump>...
   Prints counts and the hash only: the dumps are game data (personal use, kept in the container). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../ge/vu1conv.h"
#ifdef __psp__
#include <pspkernel.h>
#include <pspfpu.h>
PSP_MODULE_INFO("p06bench", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);
volatile uint32_t g_emuprof;
#define PROF(v) (g_emuprof = (v))
#else
#include <xmmintrin.h>
#define PROF(v) ((void)0)
#endif

static uint8_t VM[16384] __attribute__((aligned(16)));
static uint8_t V1[64 * 24] __attribute__((aligned(16))), V2[64 * 24] __attribute__((aligned(16))), V3[64 * 24] __attribute__((aligned(16)));
static uint64_t H = 0xcbf29ce484222325ull;
static void hb(const void *p, size_t n) { const uint8_t *b = p; for (size_t i = 0; i < n; i++) { H ^= b[i]; H *= 0x100000001b3ull; } }
static int rd(FILE *f, void *p, size_t n) { return fread(p, 1, n, f) == n; }

/* one dump: every call through the converter; prof: count the converter calls in PspProf */
static int run(const char *path, int prof, long *calls, long *verts, long *bad)
{
  FILE *f = fopen(path, "rb"); if (!f) return -1;
  static uint8_t T[16 * 1024 + 16];
  for (;;) {
    uint32_t hd[8], ab[16][4];
    if (!rd(f, hd, 32)) break;
    if (hd[0] != 0x32433650u) { fclose(f); return -2; }
    uint32_t ent = hd[3], top = hd[4]; int nt = (int)hd[5], nr = (int)hd[6], nq = (int)hd[7];
    if (nq > 1024 || !rd(f, ab, 256) || !rd(f, T, 16 * (size_t)nq)) { fclose(f); return -3; }
    if (fseek(f, 112L * nt + 56L * nr, SEEK_CUR)) { fclose(f); return -3; }
    memset(VM, 0, sizeof VM); memcpy(VM, ab, 256);
    for (int q = 0; q < nq; q++) memcpy(VM + (((top + q) & 0x3ff) << 4), T + 16 * q, 16);
    VcLit L; uint16_t fl[64]; uint32_t rgba[64]; int t2 = 0;
    if (prof) PROF(1);
    int n = vc_p06(VM, top, ent, &L, V1, V3, fl, rgba, 64);
    if (n >= 0 && L.uv2 == 2) t2 = vc_p06_uv2(VM, top, &L, V1, V2);
    if (prof) PROF(2);
    (*calls)++;
    hb(&n, sizeof n);
    if (n < 0) { (*bad)++; continue; }
    *verts += n;
    hb(&L, sizeof L);
    hb(V1, (size_t)vc_p06_size1(&L) * n);
    if (L.p3 > 0) hb(V3, (size_t)vc_p06_size3(&L) * n);
    if (L.uv2 == 2) { hb(&t2, sizeof t2); hb(V2, (size_t)(16 + 4 * ((t2 & 1) + (t2 >> 1))) * n); }
    hb(fl, 2 * (size_t)n); hb(rgba, 4 * (size_t)n);
  }
  fclose(f);
  return 0;
}

#ifdef __psp__
int main(void)
{
  pspFpuSetFCR31(0x01000001u);   /* the harness's float mode (psp/main_psp.c, lessons A9): round toward zero, flush to zero */
  FILE *o = fopen("bench.txt", "w"), *lf = fopen("list.txt", "r");
  if (!o || !lf) { sceKernelExitGame(); return 1; }
  char names[8][64]; int nn = 0;
  while (nn < 8 && fgets(names[nn], sizeof names[nn], lf)) { names[nn][strcspn(names[nn], "\r\n")] = 0; if (names[nn][0]) nn++; }
  fclose(lf);
  for (int pass = 1; pass <= 2; pass++) {
    long calls = 0, verts = 0, bad = 0; H = 0xcbf29ce484222325ull;
    SceInt64 t0 = sceKernelGetSystemTimeWide();
    if (pass == 2) PROF(2);
    for (int i = 0; i < nn; i++) { int r = run(names[i], pass == 2, &calls, &verts, &bad); if (r) fprintf(o, "pass %d: %s: error %d\n", pass, names[i], r); }
    if (pass == 2) PROF(0);
    fprintf(o, "pass %d (%s): calls %ld, vertices %ld, refused %ld, hash %016llx, %lld us\n", pass, pass == 1 ? "JIT" : "profiled",
            calls, verts, bad, (unsigned long long)H, (long long)(sceKernelGetSystemTimeWide() - t0));
    fflush(o);
  }
  fclose(o);
  sceKernelExitGame();
  return 0;
}
#else
int main(int argc, char **argv)
{
  int a0 = 1;
  if (a0 < argc && !strcmp(argv[a0], "-rz")) { _mm_setcsr(_mm_getcsr() | 0x6000u | 0x8000u); a0++; }
  long calls = 0, verts = 0, bad = 0;
  for (int i = a0; i < argc; i++) { int r = run(argv[i], 0, &calls, &verts, &bad); if (r) { fprintf(stderr, "%s: error %d\n", argv[i], r); return 1; } }
  printf("calls %ld, vertices %ld, refused %ld, hash %016llx\n", calls, verts, bad, (unsigned long long)H);
  return 0;
}
#endif
