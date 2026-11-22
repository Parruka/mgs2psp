/* gl1_glue.c - MGS2T_gl1 (2026-10-27; psp/gl1.sh, task #56): the crash catcher (psp/v26me4_exc.c / .S, in the harness since Ffm's
   choice of 2026-10-24) and the battery log (psp/v26me_bat.c, lines every 5 s with the clocks, the program break and the free memory)
   of the Media Engine builds, here without the Media Engine: what psp/me_mix.c gives those two units there, and nothing else.  The
   link wraps (psp/gl1.sh): host_vb_begin (the latest vblank; MGS2_EXC_DRY's dry run), mgs2_exit (battery.txt's "end" line),
   sceKernelExitGame (the handler given back before anything else, tt_port lessons A10; the "HOME" line) and mgs2_main (v26me_bat.c:
   the battery thread and the catcher's install).  The game state is not touched. */
#include <pspkernel.h>
#include <stdint.h>
#include <string.h>

volatile long me_mix_last_vb = -1;      /* the latest vblank (-1 before the first): battery.txt and the crash record */
void me_mix_state(uint32_t v[6]) { memset(v, 0, 6 * sizeof *v); }   /* "Media Engine off" */
void me_mix_diag(uint32_t d[7]) { memset(d, 0, 7 * sizeof *d); }
int me_bat_line(const char *why);       /* psp/v26me_bat.c */
void me_bat_exit(void);                 /* psp/v26me_bat.c: SYS_DIAG's power callback out (gl2d); nothing in the other builds */
void me_exc_vb(long vb);                /* psp/v26me4_exc.c */
void me_exc_release(void);

void __real_host_vb_begin(long vb);
void __wrap_host_vb_begin(long vb)
{
  __real_host_vb_begin(vb);             /* main_psp.c's step timing first, as psp/me_mix.c's wrap */
  me_mix_last_vb = vb;
  me_exc_vb(vb);
}
void __real_mgs2_exit(int code);
void __wrap_mgs2_exit(int code) { me_bat_line("end"); __real_mgs2_exit(code); }
int __real_sceKernelExitGame(void);
int __wrap_sceKernelExitGame(void)
{
  me_exc_release();
  me_bat_line("HOME");
  me_bat_exit();
  return __real_sceKernelExitGame();
}
