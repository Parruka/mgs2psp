/* gl45 study: the switch word, the counters and the x86 check mode of boot/native45.h.  tools/n45_apply.py includes this file at the end
   of gen_elf.h, after every generated function. */
#if N45_ON
#undef static
uint32_t g_n45;                          /* one bit per native form (native45.h); the PSP harness sets it (a cfg key, a cycle by turns) */
#if !defined(__psp__)
#include <stdio.h>
static const char *const n45_name[N45_NFN] = { "MT_QuatNormalize 11cd68", "sceVu0MulMatrix 165d78", "MT_QuatToMat 11c828", "_sceVu0ecossin 166090", "sceVu0RotMatrixZ 166108",
                                               "sceVu0RotMatrixX 1661b0", "sceVu0RotMatrixY 166258", "pointer walk 14b008" };
long n45_hzs[4];
long n45_nc[N45_NFN][3];
#ifdef N45_BCNT
long n45_bc[512];
#endif
#ifdef N45_TICKS
uint64_t n45_tk[N45_NFN][3];
#endif
#ifdef N45_CHECK
extern uint32_t g_vgrp_fb; extern long g_hash_vb;
struct n45_st { int64_t R[32], RH[32], HI, LO, HI1, LO1; float F[32], ACC; int CC; VQ VF[32]; uint32_t VACC[4], VIr[16], VQr, VPr, VIreg, VRr, VMAC, VSTAT,
                VCLIP, VCMSAR0, vmac, fb; uint64_t t; };
#define N45_DEPTH 8
#define N45_NREG 8
static struct n45_ctx { struct n45_st s0, s1; int nreg, full; struct { uint32_t a, n; uint8_t pre[256], post[256]; } reg[N45_NREG]; } n45_cx[N45_DEPTH];
static int n45_d = -1, n45_full_busy; static long n45_full_left[N45_NFN], n45_bad[N45_NFN], n45_decl[N45_NFN], n45_ok[N45_NFN], n45_fulln[N45_NFN];
static uint8_t *n45_ram0, *n45_ram1; static int n45_inited;
/* batch 2: a form that calls translated functions and polls cannot be run twice from declared regions: it is compared only with the whole RAM,
   every MGS2_N45_EVERY-th call, and only when no poll can be due inside the call (a poll that runs handlers cannot be put back); every other
   call runs the native form alone.  g_vu_ext (boot.c: counts rt_store and rt_poll_slow) proves that none ran inside a compared call. */
#define N45_FULLONLY (1u << N45_HZX)
extern uint32_t g_vu_ext;
static long n45_every = 1, n45_calls[N45_NFN], n45_unchk[N45_NFN]; static uint32_t n45_ext0;
static void n45_get(struct n45_st *s)
{
  memcpy(s->R, R, sizeof R); memcpy(s->RH, RH, sizeof RH); s->HI = HI; s->LO = LO; s->HI1 = HI1; s->LO1 = LO1; memcpy(s->F, F, sizeof F); s->ACC = ACC; s->CC = CC;
  memcpy(s->VF, VF, sizeof VF); memcpy(s->VACC, VACC, 16); memcpy(s->VIr, VIr, sizeof VIr); s->VQr = VQr; s->VPr = VPr; s->VIreg = VIreg; s->VRr = VRr;
  s->VMAC = VMAC; s->VSTAT = VSTAT; s->VCLIP = VCLIP; s->VCMSAR0 = VCMSAR0; s->vmac = g_vmac; s->fb = g_vgrp_fb; s->t = k_time;
}
static void n45_put(const struct n45_st *s)
{
  memcpy(R, s->R, sizeof R); memcpy(RH, s->RH, sizeof RH); HI = s->HI; LO = s->LO; HI1 = s->HI1; LO1 = s->LO1; memcpy(F, s->F, sizeof F); ACC = s->ACC; CC = s->CC;
  memcpy(VF, s->VF, sizeof VF); memcpy(VACC, s->VACC, 16); memcpy(VIr, s->VIr, sizeof VIr); VQr = s->VQr; VPr = s->VPr; VIreg = s->VIreg; VRr = s->VRr;
  VMAC = s->VMAC; VSTAT = s->VSTAT; VCLIP = s->VCLIP; VCMSAR0 = s->VCMSAR0; g_vmac = s->vmac; g_vgrp_fb = s->fb; k_time = s->t;
}
static void n45_say(int i, const char *what, int k, uint64_t tr, uint64_t nat)
{
  if (n45_bad[i] < 12) fprintf(stderr, "n45chk %s: call %ld (after the hash of vb %ld): %s[%d] translation %016llx native %016llx\n", n45_name[i],
                               n45_ok[i] + n45_bad[i], g_hash_vb, what, k, (unsigned long long)tr, (unsigned long long)nat);
}
/* the translation's state now against the native result b; 1 = different */
static int n45_cmp(int i, const struct n45_st *b)
{
  struct n45_st a; int k, bad = 0; uint32_t x, y;
  n45_get(&a);
#define N45_C(what, k, u, v) do { if ((u) != (v)) { n45_say(i, what, k, (uint64_t)(u), (uint64_t)(v)); bad = 1; } } while (0)
  for (k = 0; k < 32; k++) { N45_C("R", k, a.R[k], b->R[k]); N45_C("RH", k, a.RH[k], b->RH[k]); memcpy(&x, &a.F[k], 4); memcpy(&y, &b->F[k], 4); N45_C("F", k, x, y); }
  N45_C("HI", 0, a.HI, b->HI); N45_C("LO", 0, a.LO, b->LO); N45_C("HI1", 0, a.HI1, b->HI1); N45_C("LO1", 0, a.LO1, b->LO1);
  memcpy(&x, &a.ACC, 4); memcpy(&y, &b->ACC, 4); N45_C("FPU ACC", 0, x, y); N45_C("CC", 0, a.CC, b->CC);
  for (k = 0; k < 128; k++) N45_C("VF (register * 4 + lane)", k, a.VF[k / 4].u[k & 3], b->VF[k / 4].u[k & 3]);
  for (k = 0; k < 4; k++) N45_C("VU ACC", k, a.VACC[k], b->VACC[k]);
  for (k = 0; k < 16; k++) N45_C("VI", k, a.VIr[k], b->VIr[k]);
  N45_C("Q", 0, a.VQr, b->VQr); N45_C("P", 0, a.VPr, b->VPr); N45_C("I", 0, a.VIreg, b->VIreg); N45_C("VU R", 0, a.VRr, b->VRr); N45_C("MAC", 0, a.VMAC, b->VMAC);
  N45_C("STATUS", 0, a.VSTAT, b->VSTAT); N45_C("CLIP", 0, a.VCLIP, b->VCLIP); N45_C("CMSAR0", 0, a.VCMSAR0, b->VCMSAR0); N45_C("g_vmac", 0, a.vmac, b->vmac);
  N45_C("group fallbacks", 0, a.fb, b->fb); N45_C("time", 0, a.t, b->t);
  return bad;
}
int n45_chk_begin(int i)
{
  struct n45_ctx *c;
  if (!n45_inited) { const char *e = getenv("MGS2_N45_FULL"); long n = e ? atol(e) : 50; int k; for (k = 0; k < N45_NFN; k++) n45_full_left[k] = n; n45_inited = 1; }
  if (N45_FULLONLY >> i & 1) {
    static int got; if (!got) { const char *e = getenv("MGS2_N45_EVERY"); if (e) n45_every = atol(e); got = 1; }
    static int due; if (n45_calls[i]++ % n45_every == 0) due = 1;   /* a sample that cannot be taken now is taken at the next call that can */
    if (!due || n45_full_busy || N45_FREE() < 4096) { n45_unchk[i]++; return 0; }
    due = 0;
    n45_full_left[i] = 1; n45_ext0 = g_vu_ext;
  }
  if (++n45_d >= N45_DEPTH) { fprintf(stderr, "n45chk: nesting deeper than %d\n", N45_DEPTH); abort(); }
  c = &n45_cx[n45_d]; c->nreg = 0; c->full = 0; n45_get(&c->s0);
  if (!n45_full_busy && n45_full_left[i] > 0) {
    if (!n45_ram0) { n45_ram0 = malloc(GUEST_SIZE); n45_ram1 = malloc(GUEST_SIZE); if (!n45_ram0 || !n45_ram1) abort(); }
    memcpy(n45_ram0, g_mem, GUEST_SIZE); c->full = 1; n45_full_busy = 1; n45_full_left[i]--; n45_fulln[i]++;
  }
  return 1;
}
void n45_chk_w(uint32_t a, uint32_t n)   /* the native form is about to write [a, a + n) */
{
  struct n45_ctx *c = &n45_cx[n45_d];
  if (c->nreg >= N45_NREG || n > 256) { fprintf(stderr, "n45chk: too many / too long declared writes\n"); abort(); }
  c->reg[c->nreg].a = a; c->reg[c->nreg].n = n; memcpy(c->reg[c->nreg].pre, N45_HOST(a), n); c->nreg++;
}
void n45_chk_declined(int i)             /* the form returned 0: nothing may have changed */
{
  struct n45_ctx *c = &n45_cx[n45_d];
  if (n45_cmp(i, &c->s0) || c->nreg || (c->full && memcmp(n45_ram0, g_mem, GUEST_SIZE))) { n45_say(i, "state changed by a declined call", 0, 0, 0); n45_bad[i]++; }
  if (c->full) n45_full_busy = 0;
  n45_decl[i]++; n45_d--;
}
void n45_chk_mid(int i)                  /* the form accepted: keep its result, put everything back, the translation runs next */
{
  struct n45_ctx *c = &n45_cx[n45_d]; int k;
  if ((N45_FULLONLY >> i & 1) && g_vu_ext != n45_ext0) { fprintf(stderr, "n45chk %s: a poll or an I/O store ran inside a compared call\n", n45_name[i]); abort(); }
  n45_get(&c->s1);
  for (k = 0; k < c->nreg; k++) memcpy(c->reg[k].post, N45_HOST(c->reg[k].a), c->reg[k].n);
  if (c->full) { memcpy(n45_ram1, g_mem, GUEST_SIZE); memcpy(g_mem, n45_ram0, GUEST_SIZE); }
  else for (k = c->nreg - 1; k >= 0; k--) memcpy(N45_HOST(c->reg[k].a), c->reg[k].pre, c->reg[k].n);
  n45_put(&c->s0);
}
void n45_chk_end(int i)
{
  struct n45_ctx *c = &n45_cx[n45_d]; int k, bad; uint32_t j;
  bad = n45_cmp(i, &c->s1);
  for (k = 0; k < c->nreg; k++) for (j = 0; j < c->reg[k].n; j++) if (N45_HOST(c->reg[k].a)[j] != c->reg[k].post[j]) {
    n45_say(i, "RAM byte at", (int)j, ((uint64_t)c->reg[k].a << 32) | N45_HOST(c->reg[k].a)[j], c->reg[k].post[j]); bad = 1; break; }
  if (c->full) {
    if (memcmp(n45_ram1, g_mem, GUEST_SIZE)) {
      uint32_t a, n = 0, sp = (uint32_t)c->s0.R[29] & (GUEST_SIZE - 1);
      for (a = 0; a < GUEST_SIZE; a += 4) if (memcmp(n45_ram1 + a, g_mem + a, 4)) {
        uint32_t x, y; memcpy(&x, g_mem + a, 4); memcpy(&y, n45_ram1 + a, 4);
        if (n++ < 6) n45_say(i, a < sp && a + 1024 >= sp ? "RAM word below $sp (stack leftover), address" : "RAM word, address", (int)a, x, y);
      }
      bad = 1;
    }
    n45_full_busy = 0;
  }
  if (bad) n45_bad[i]++; else n45_ok[i]++;
  n45_d--;
}
#endif
void n45_mark(long vb)                   /* boot_x86.c state_hash: MGS2_N45_MARK=1 prints the counters (and ticks) so far, for per-window numbers */
{
  static int on = -1; int i, k;
#ifdef N45_BCNT
  fprintf(stderr, "n45bc %ld", vb); for (i = 0; i < 512; i++) fprintf(stderr, " %ld", n45_bc[i]); fprintf(stderr, "\n");
#endif
  if (on < 0) on = getenv("MGS2_N45_MARK") != 0;
  if (!on) return;
  fprintf(stderr, "n45mark %ld", vb);
  for (i = 0; i < N45_NFN; i++) if (n45_name[i]) for (k = 0; k < 3; k++) {
    fprintf(stderr, " %ld", n45_nc[i][k]);
#ifdef N45_TICKS
    fprintf(stderr, ":%llu", (unsigned long long)n45_tk[i][k]);
#endif
  }
  fprintf(stderr, "\n");
}
__attribute__((constructor)) static void n45_init(void)
{ const char *e = getenv("MGS2_N45"); if (e) g_n45 = (uint32_t)strtoul(e, 0, 16); }
__attribute__((destructor)) static void n45_report(void)
{
  int i;
  fprintf(stderr, "n45 pointer walk: %ld scans, %ld entries skipped, %ld scans cut by a poll about to be due, %ld refused (no room)\n", n45_hzs[0], n45_hzs[1], n45_hzs[2], n45_hzs[3]);
  for (i = 0; i < N45_NFN; i++) {
    if (!n45_name[i]) continue;
#ifdef N45_CHECK
    fprintf(stderr, "n45chk %s: %ld calls equal, %ld different, %ld declined (%ld with the whole RAM compared)\n", n45_name[i], n45_ok[i], n45_bad[i], n45_decl[i],
            n45_fulln[i]);
    if (n45_unchk[i]) fprintf(stderr, "n45chk %s: %ld more calls ran the native form alone (compared: every %ld-th call with no poll near)\n", n45_name[i], n45_unchk[i], n45_every);
#elif defined(N45_TICKS)
    fprintf(stderr, "n45 %s: native %ld calls %llu ticks; declined %ld calls %llu ticks; translated (bit off) %ld calls %llu ticks\n", n45_name[i], n45_nc[i][0],
            (unsigned long long)n45_tk[i][0], n45_nc[i][1], (unsigned long long)n45_tk[i][1], n45_nc[i][2], (unsigned long long)n45_tk[i][2]);
#else
    fprintf(stderr, "n45 %s: native %ld calls, declined %ld, translated (bit off) %ld\n", n45_name[i], n45_nc[i][0], n45_nc[i][1], n45_nc[i][2]);
#endif
  }
}
#endif
#define static
#endif
