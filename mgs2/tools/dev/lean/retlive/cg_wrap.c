/* cg_wrap.c (lean_plan.md 1c, dev, x86 only): linked with -Wl,--wrap=gen_jalr,--wrap=gen_guest_call.  A call through a register (JALR,
   JSIF) or from the kernel (thread entries, handlers, alarms, the draw pipeline's call) must never reach a closed function (regcache.py
   RC_RETLIVE): its returns store only what its direct callers read.  Exit 8 when one does.  The stage in memory is found as
   dispatch.h's stage_call finds it. */
#include "gen_pre.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STAGES_EXTERN
#include STAGES
#include CG_CLOSED
void __real_gen_jalr(uint32_t a); int __real_gen_guest_call(uint32_t a);
static int cg_in(const uint32_t *t, int n, uint32_t a)
{
  int lo = 0, hi = n - 1;
  while (lo <= hi) { int m = (lo + hi) >> 1; if (t[m] == a) return 1; if (t[m] < a) lo = m + 1; else hi = m - 1; }
  return 0;
}
static void cg_check(uint32_t a, const char *via)
{
  int j, k, i;
  if (a < 0x1f1c00u) {
    if (cg_in(cg_elf, (int)(sizeof cg_elf / sizeof *cg_elf), a)) { fprintf(stderr, "retlive guard: %s to closed ELF function %06x (ra %06x)\n", via, a, (uint32_t)R[31]); exit(8); }
    return;
  }
  for (j = 0; j < (int)(sizeof stage_code / sizeof *stage_code); j++) {
    for (k = 0; k < STAGE_SIG_N; k++) if (RD32(0x1f1c00 + stage_code[j].off[k]) != stage_code[j].sig[k]) break;
    if (k < STAGE_SIG_N) continue;
    for (i = 0; i < (int)(sizeof cg_stage / sizeof *cg_stage); i++)
      if (!strcmp(cg_stage[i].name, stage_code[j].name) && cg_in(cg_stage[i].a, cg_stage[i].n, a)) {
        fprintf(stderr, "retlive guard: %s to closed %s function %06x (ra %06x)\n", via, stage_code[j].name, a, (uint32_t)R[31]); exit(8); }
    return;
  }
}
void __wrap_gen_jalr(uint32_t a) { cg_check(a, "JALR"); __real_gen_jalr(a); }
int __wrap_gen_guest_call(uint32_t a) { cg_check(a, "guest call"); return __real_gen_guest_call(a); }
