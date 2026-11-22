/* dispatch of guest calls through a register (included after the generated code: boot.c in one unit, gen.c when split) */
#include "host_time.h"
#ifdef STAGES
#include STAGES
static int stage_cur = -1;
static int stage_call(uint32_t a)              /* a call into the stage image at 0x1F1C00: find which translated image is in memory */
{
  int i, k, n = sizeof stage_code / sizeof *stage_code;
  for (i = stage_cur >= 0 ? -1 : 0; i < n; i++) {
    int j = i < 0 ? stage_cur : i;
    for (k = 0; k < STAGE_SIG_N; k++) if (RD32(0x1f1c00 + stage_code[j].off[k]) != stage_code[j].sig[k]) break;
    if (k < STAGE_SIG_N) continue;
    if (j != stage_cur) { fprintf(stderr, "stage code: %s\n", stage_code[j].name); stage_cur = j; }
#ifdef STAGE_OVL
    { int stage_ovl_call(const char *name, uint32_t a);   /* psp/main_psp.c: the stage's code is an overlay file, one in memory at a time */
      return stage_ovl_call(stage_code[j].name, a); }
#else
    return stage_code[j].call(a);
#endif
  }
  stage_cur = -1; return 0;
}
#else
static int stage_call(uint32_t a) { (void)a; return 0; }
#endif
/* v26 (OPT_STAGE_TAB): gen_call's cases all lie below the stage image (tools/jalr_tab.py checks it), so a stage address skips its search */
static int guest_call(uint32_t a) { return ((a < 0x1f1c00u || !(g_opt & OPT_STAGE_TAB)) && gen_call(a)) || stage_call(a); }
#ifdef GUEST_SPLIT_GEN
/* tools/jalr_tab.py: gen_ftab lists gen_call's cases that call one translated function, sorted.  JALR looks a target up in a small
   direct-mapped cache first: a listed target is called directly (the call the switch makes), an unlisted one (a stage's function, a case
   with an inlined body) is remembered as such and goes to guest_call as before.  Weak: headers without the table keep the switch alone */
extern const struct GenFn gen_ftab[] __attribute__((weak)); extern const int gen_ftab_n __attribute__((weak));
static struct GenFn jalr_c[256];
static inline void (*jalr_find(uint32_t a))(void)
{
  struct GenFn *e = &jalr_c[((a >> 3) ^ (a >> 11)) & 255];
  if (__builtin_expect(e->a != a, 0)) {
    int lo = 0, hi = &gen_ftab_n ? gen_ftab_n - 1 : -1;
    e->a = a; e->f = 0;
    while (lo <= hi) { int m = (lo + hi) >> 1; if (gen_ftab[m].a == a) { e->f = gen_ftab[m].f; break; } if (gen_ftab[m].a < a) lo = m + 1; else hi = m - 1; }
  }
  return e->f;
}
#endif
#ifndef DISPATCH_FN
#define DISPATCH_FN static
#endif
#ifdef GEN_STUB_HOOK
int rt_stub(uint32_t a);              /* dev (boot.c MGS2_STUB): 1 = skip this call through a register */
#endif
#ifdef GEN_ACTOR_PROF
/* dev builds only: the actor scheduler's call of each actor's act (0x13f140 at 0x13f1ac, elf_notes.md) names the actor in g_emuactor
   (psp/main_psp.c; boot.c on x86) while it runs: the emulator profiler's per-actor costs (PSPPROF_BUCKET), boot.c MGS2_ACT_TRACE */
static void jalr_do(uint32_t a);
DISPATCH_FN void JALR(uint32_t a)
{
  if ((uint32_t)R[31] == 0x13f1b4u) { extern volatile uint32_t g_emuactor; uint32_t o_ = g_emuactor; g_emuactor = a; jalr_do(a); g_emuactor = o_; return; }
  jalr_do(a);
}
static void jalr_do(uint32_t a)
#else
DISPATCH_FN void JALR(uint32_t a)
#endif
{
#ifdef GEN_STUB_HOOK
  if (rt_stub(a)) return;
#endif
#ifdef GUEST_SPLIT_GEN
  if (a != 0x11da90u) { void (*f_)(void) = jalr_find(a); if (f_) { f_(); return; } }   /* (the draw pipeline's call goes through rt_dg_call) */
#endif
#if defined(DG_EXP) && !defined(__psp__)
  if (a == 0x11da90u) { int dg_exp_call(uint32_t a); if (dg_exp_call(a)) return; } else   /* boot.c: x86 dev probe of the draw pipeline */
#endif
  if (a == 0x11da90u) { int rt_dg_call(uint32_t a), ok_; HOST_ENTER(HOST_DG); ok_ = rt_dg_call(a); HOST_LEAVE(HOST_DG); if (ok_) return; } else   /* the draw pipeline: no game time (boot.c) */
  if (guest_call(a)) return;     /* (a stage's pointers can name ELF code the ELF alone does not reveal) */
  fprintf(stderr, "JALR to %08x: not a translated function (ra %06x); stage image header %08x %08x %08x %08x\n", a, (uint32_t)R[31],
          RD32(0x1f1c00), RD32(0x1f1c04), RD32(0x1f1c08), RD32(0x1f1c10));
#ifdef STAGES
  { int j, k; for (j = 0; j < (int)(sizeof stage_code / sizeof *stage_code); j++) for (k = 0; k < STAGE_SIG_N; k++)   /* which signature word differs */
      if (RD32(0x1f1c00 + stage_code[j].off[k]) != stage_code[j].sig[k]) { if (k) fprintf(stderr, "  %s: word +%x is %08x, image has %08x\n",
          stage_code[j].name, stage_code[j].off[k], RD32(0x1f1c00 + stage_code[j].off[k]), stage_code[j].sig[k]); break; } }
#endif
  exit(4);
}
static void gen_start(void) { gen_init(); P100010(); }
