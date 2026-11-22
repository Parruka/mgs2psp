/* v25f (2026-10-23, audit H1): what the free-play timing build adds to v24's harness, in units linked after all of v24's objects
   (gen_x.o, xhook.o, then pad.o: psp/v25f.sh), so v24's code keeps its addresses and its data keeps its cache sets.  v24's objects
   are used as they are (main_psp.o rebuilt only for its title, the same length); this file reaches the harness through the link's
   --wrap of three functions that other objects call: rt_missing (the stubs), mgs2_main (main_psp.c's call of the machine: the start)
   and mgs2_exit (the machine's stop: the end).
   1. Free play's extra functions (tools/xunit.py): v24's reduced units keep a stub for every function they left out, which calls
      boot.c rt_missing.  The links wrap it (-Wl,--wrap=rt_missing, also for the overlays), so the stubs call __wrap_rt_missing here
      and boot_rt.o stays v24's object (any edit of boot.c moves PGO's choices in its hot functions: tried 2026-10-23).  x_call finds
      the function in the ELF's table (gen_xtab, gen_x.o) or in the loaded overlay's extra part (its OvlX header right after the
      overlay's own end, psp/ovl_x.ld) and calls it; calls between extra functions are direct.  A small direct-mapped cache keeps the
      lookup short; every call is counted per 100 vblanks for the log (so the hook's own time can be taken off).  The vblank count is
      boot.c's static k_vblanks, named for this file by the link (--defsym xh_k_vblanks=<its address>, read from the first link).
   2. The battery log: v25's was in the vblank hook, which here keeps v24's code; this thread writes battery.txt (next to the EBOOT)
      every 60 s and when external power / charging / the low flag changes, each line opened, written and closed (it survives a
      power-off), with the time since the start and the latest vblank the hook saw.  Reads only (scePower), Ffm's rule.  Eight power
      functions, so the import tables grow by a multiple of 16 bytes and the pads can put v24's read-only data back on its offsets.
   Built with -fno-merge-constants: no literal joins the merged string block of v24's objects (that would move their read-only data). */
#include <pspkernel.h>
#include <psppower.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define XD __attribute__((section(".data")))   /* (zeroed variables too: nothing new in .bss, so no library .bss moves) */
struct GenFn { uint32_t a; void (*f)(void); };
extern const struct GenFn gen_xtab[] __attribute__((weak)); extern const int gen_xtab_n __attribute__((weak));
extern uint8_t ovl_arena[];
struct OvlHdr { uint32_t magic; char name[12]; void (*init)(void); int (*call)(uint32_t); uint8_t *bss, *end; const void *chk[3];
  void (*const *ctor_a)(void), (*const *ctor_b)(void); };
struct OvlX { uint32_t magic; int n; const struct GenFn *tab; };
#define OVL_MAGIC 0x314c564fu
#define OVLX_MAGIC 0x31425458u
#define XWIN 400                                 /* per-100-vblank counts up to vb 40,000 */
static struct { uint32_t a; const struct OvlX *x; void (*f)(void); } x_c[512] XD;
static uint32_t x_win[XWIN] XD, x_calls XD, x_elf XD, x_ovl XD, x_none XD;
static volatile long x_last_vb XD = -1;
static uint64_t x_t0 XD;

static void (*x_find(const struct GenFn *t, int n, uint32_t a))(void)
{
  int lo = 0, hi = n - 1;
  while (lo <= hi) { int m = (lo + hi) >> 1; if (t[m].a == a) return t[m].f; if (t[m].a < a) lo = m + 1; else hi = m - 1; }
  return 0;
}
/* the loaded overlay's extra part, or 0 */
static const struct OvlX *x_ovl_hdr(void)
{
  const struct OvlHdr *h = (const struct OvlHdr *)ovl_arena; const struct OvlX *x;
  if (h->magic != OVL_MAGIC || h->end < ovl_arena || h->end > ovl_arena + OVL_SIZE - sizeof *x) return 0;
  x = (const struct OvlX *)(((uintptr_t)h->end + 15) & ~(uintptr_t)15);
  return x->magic == OVLX_MAGIC ? x : 0;
}
static int x_call(uint32_t a, long vb)
{
  const struct OvlX *x = x_ovl_hdr(); void (*f)(void) = 0;
  unsigned k = ((a >> 3) ^ (a >> 12)) & 511;
  if (x_c[k].a == a && x_c[k].x == x) f = x_c[k].f;
  else {
    /* an address in the ELF: the ELF's table, then the loaded stage's (a stage's translation can hold functions at ELF addresses that the
       ELF's does not have, whose stubs are the stage's: w00a's 106020, 112fb8, 155370; d01t's 108520, 118b30, 11eb78, 12a7c0); a stage
       address: the loaded stage's table */
    if (a < 0x1f1c00u && &gen_xtab_n) f = x_find(gen_xtab, gen_xtab_n, a);
    if (!f && x) f = x_find(x->tab, x->n, a);
    if (!f) { x_none++; return 0; }
    x_c[k].a = a; x_c[k].x = x; x_c[k].f = f;
  }
  x_calls++; if ((uint8_t *)f >= ovl_arena && (uint8_t *)f < ovl_arena + OVL_SIZE) x_ovl++; else x_elf++;   /* (where the function lives) */
  if (vb >= 0 && vb / 100 < XWIN) x_win[vb / 100]++;
  x_last_vb = vb;
  f();
  return 1;
}
extern long xh_k_vblanks __attribute__((weak));   /* boot.c's k_vblanks (--defsym; without it no vblank: counts go to "unknown") */
void __real_rt_missing(uint32_t a);
void __wrap_rt_missing(uint32_t a) { if (x_call(a, &xh_k_vblanks ? xh_k_vblanks : -1)) return; __real_rt_missing(a); }

/* ---- battery.txt ---- */
static char x_bpath[256] XD;
static char *x_put(char *p, const char *s) { while (*s) *p++ = *s++; return p; }
static char *x_num(char *p, long v) { char t[16]; int n = 0; unsigned long u = v < 0 ? -(unsigned long)v : (unsigned long)v; if (v < 0) *p++ = '-'; do t[n++] = '0' + u % 10; while (u /= 10); while (n) *p++ = t[--n]; return p; }
static int x_bat_line(const char *why)
{
  char l[240], *p = l; SceUID f;
  int on = scePowerIsPowerOnline(), ch = scePowerIsBatteryCharging(), lo = scePowerIsLowBattery();
  uint64_t t = (uint64_t)sceKernelGetSystemTimeWide() - x_t0;
  p = x_put(p, why); p = x_put(p, " t "); p = x_num(p, (long)(t / 1000000)); p = x_put(p, " s, vb "); p = x_num(p, x_last_vb);
  p = x_put(p, ": "); p = x_num(p, scePowerGetBatteryLifePercent()); p = x_put(p, "%, "); p = x_num(p, scePowerGetBatteryVolt());
  p = x_put(p, " mV, "); p = x_num(p, scePowerGetBatteryTemp()); p = x_put(p, " C, "); p = x_num(p, scePowerGetBatteryLifeTime());
  p = x_put(p, " min left (charging status "); p = x_num(p, scePowerGetBatteryChargingStatus()); p = x_put(p, "); external power "); p = x_num(p, on > 0);
  p = x_put(p, ", charging "); p = x_num(p, ch > 0); p = x_put(p, ", low "); p = x_num(p, lo > 0);
  p = x_put(p, "; hook calls "); p = x_num(p, (long)x_calls); *p++ = '\n';
  if ((f = sceIoOpen(x_bpath, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777)) >= 0) { sceIoWrite(f, l, p - l); sceIoClose(f); }
  return (on > 0) << 2 | (ch > 0) << 1 | (lo > 0);
}
static int x_bat_thread(SceSize n, void *arg)
{
  int last = x_bat_line("start"), i; (void)n; (void)arg;
  for (;;) {                                      /* checks every 5 s (reads only), writes every 60 s or on a change */
    for (i = 0; i < 12; i++) {
      int k; sceKernelDelayThread(5000000);
      k = (scePowerIsPowerOnline() > 0) << 2 | (scePowerIsBatteryCharging() > 0) << 1 | (scePowerIsLowBattery() > 0);
      if (k != last) { last = x_bat_line("change"); i = -1; }
    }
    last = x_bat_line("every 60 s");
  }
  return 0;
}
/* the start (main_psp.c main's call of the machine, argv[1] = <EBOOT folder>/SLES_503.83): the battery thread, above the game thread's
   priority as the HOME thread (it sleeps all but a few ms a minute) */
int __real_mgs2_main(int argc, char **argv);
int __wrap_mgs2_main(int argc, char **argv)
{
  SceUID th, f; const char *e = argc > 1 && argv[1] ? strrchr(argv[1], '/') : 0; int n = e ? (int)(e - argv[1]) + 1 : 0;
  x_t0 = (uint64_t)sceKernelGetSystemTimeWide();
  snprintf(x_bpath, sizeof x_bpath, "%.*sbattery.txt", n, n ? argv[1] : "");
  if ((f = sceIoOpen(x_bpath, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777)) >= 0) sceIoClose(f);
  fprintf(stderr, "v25f: extra functions: ELF %d in the main program, the stages' in their overlay files; battery log to %s every 60 s\n",
          &gen_xtab_n ? gen_xtab_n : 0, x_bpath);
  if ((th = sceKernelCreateThread("battery", x_bat_thread, 0x11, 0x2000, 0, 0)) >= 0) sceKernelStartThread(th, 0, 0);
  else fprintf(stderr, "v25f: battery thread not started (%08x)\n", (unsigned)th);
  return __real_mgs2_main(argc, argv);
}
/* the end (the machine stops: main_psp.c mgs2_exit, which writes the summary and waits for X): the hook's counts to the log, a last
   battery line */
void __real_mgs2_exit(int code);
void __wrap_mgs2_exit(int code)
{
  int i, k = 0; char l[1200];
  fprintf(stderr, "v25f: extra functions called through the hook %u times (ELF %u, overlays %u), %u not found\n", x_calls, x_elf, x_ovl, x_none);
  for (i = 0; i < XWIN; i++) if (x_win[i]) {
    k += snprintf(l + k, sizeof l - k, " %d:%u", i * 100, x_win[i]);
    if (k > (int)sizeof l - 40) { fprintf(stderr, "v25f: hook calls per 100 vblanks (window start: calls):%s\n", l); k = 0; }
  }
  if (k) fprintf(stderr, "v25f: hook calls per 100 vblanks (window start: calls):%s\n", l);
  x_bat_line("end");
  __real_mgs2_exit(code);
}
