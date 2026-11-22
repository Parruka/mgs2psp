/* PSP stand-in for <ucontext.h>: the coroutines of the runtime (kernel.h guest threads, iop_rt_ucontext.c IOP threads) switch with
   psp_ctx.S.  Only what they use: getcontext before makecontext, makecontext with no arguments, swapcontext, setcontext, uc_link. */
#ifndef MGS2_PSP_UCONTEXT_H
#define MGS2_PSP_UCONTEXT_H
#include <stddef.h>
#include <stdint.h>
typedef struct psp_uc {
  uint32_t regs[12];                       /* s0..s7, fp, gp, sp, ra (the resume address) */
  float fregs[12];                         /* f20..f31 */
  uint32_t fcr31;
  struct { void *ss_sp; size_t ss_size; } uc_stack;
  struct psp_uc *uc_link;                  /* where a context goes when its function returns (0: the runtime stops) */
} psp_uc_t;
#define ucontext_t psp_uc_t
int psp_getcontext(psp_uc_t *uc);
int psp_setcontext(const psp_uc_t *uc);
int psp_swapcontext(psp_uc_t *from, const psp_uc_t *to);
void psp_uc_tramp(void);
void psp_uc_end(void);
static inline void psp_makecontext(psp_uc_t *uc, void (*fn)(void), int argc)
{
  (void)argc;
  uc->regs[0] = (uint32_t)fn; uc->regs[1] = (uint32_t)uc;
  uc->regs[10] = ((uint32_t)uc->uc_stack.ss_sp + uc->uc_stack.ss_size - 32) & ~15u;
  uc->regs[11] = (uint32_t)psp_uc_tramp;
}
#define getcontext psp_getcontext
#define setcontext psp_setcontext
#define swapcontext psp_swapcontext
#define makecontext(uc, fn, n) psp_makecontext((uc), (void (*)(void))(fn), (n))
#endif
