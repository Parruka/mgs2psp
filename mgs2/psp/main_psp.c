/* MGS2 PSP timing harness: the recompiled game (boot/boot.c's machine with the boot-path code) headless on the PSP-3000.  It runs the
   boot path with the scripted pad to the Tanker deck (w00a) exactly as the x86 probe does, prints the state hash every MGS2_HASH_EVERY
   vblanks (compare with boot/hashes_x86_rz.txt) and times every guest vblank, i.e. one game step, with sceKernelGetSystemTimeWide.
   Rules (tt_port/PS2_to_PSP_optimisation_lessons.md): FCR31 0x01000001 before any guest code (A9); no hardware counters (A10); no
   exception handler; the clock is set to 333 MHz with scePowerSetClockFrequency (Ffm's call, 2026-10-18: 333 is the ceiling, nothing above;
   "clock default" in mgs2.cfg keeps the firmware's clock); HOME asks the run to stop and
   the program exits in order, the callback thread exits by itself after 2 s if the run does not answer; at the end of a run, or when the
   machine stops on an error, it waits for X and never exits on its own.
   Files next to the EBOOT (personal use, never published): SLES_503.83, mgs2disc.pak (tools/make_pak.py), pad.txt (the MGS2_PAD script),
   mgs2.cfg (optional); written: log.txt (everything the machine prints), steps.csv (one line per vblank of the timed range), memcard0/.
   v2: the exact host speedups (runtime/host_time.h OPT_*) take turns over the deck range, 100 vblanks each (boot.c MGS2_OPT_CYCLE): all on,
   each one off alone, all off (= v1), so one run times each of them on the same scenes; steps.csv also has the configuration and the
   game's end-of-frame wait word 0x18db74 (non-zero: the 25 Hz cutscene pacing, web_research.md section 4).
   v8: hwprobe.c measures VFPU rounding against the FPU, VFPU denormals and load speeds (scratchpad, cached, uncached, miss) at the start
   v9: code layout redone for v8's code (tools/layout_sim -opt), and hwprobe.c also measures the cost of an instruction-cache miss
   ("probe off" in mgs2.cfg skips it).
   v10: mgs2.cfg's MGS2_POKE loads the crew deck (w01a) after the opening; the timed steps are w01a's.
   v11: w01a's own overlay code order (tools/layout_sim -fix0), and hwprobe.c measures the VFPU on the VU's special values and the time
   of a VU0 macro op on the VFPU against the FPU.
   v12: runs of VU0 macro ops on the VFPU with one NaN / infinity check per run and the exact C forms as fallback (VU_VFPU, Ffm's
   consent 2026-10-20 to the VFPU's rounding; compare with prof/hashes_x86_w01a_vfpu.txt), lqc2 / sqc2 as one VFPU move, code layouts
   redone for that code (deck ELF + w00a, w01a overlay); the v11 probes stay.
   v13: the interrupt poll counts down a 32-bit distance to the next due event (K_COUNTDOWN, runtime/ee_io.h) instead of adding to the
   64-bit time and comparing, two more VU0 micro programs compiled (w01a's f28803a2 entries 153 / 155, recomp/vupipe.py VP_KEEP so the
   PGO profile still matches), code layouts redone (deck ELF + w00a, w01a overlay); the v11 probes stay.
   v14: the checked VFPU groups read the condition code with a branch (bvtl) instead of mfvc: v12 on hardware missed failed checks
   (inferred: mfvc read the code before vcmp wrote it; hwprobe.c probe 7 measures it); compiled VU0 programs keep VF / ACC in memory
   (recomp/vupipe.py VP_MEM, fewer spills); code layouts redone.
   v21: steps.csv also counts each step's full and reduced draw-pipeline calls (boot.c rt_dg_call); log.txt is appended with an open /
   write / close every window (FAT keeps a file's size only from its close: research_review.md M13) and scePowerTick keeps a long run
   from the auto sleep (M18).
   v22: the VU0 micro programs the interpreter still ran on the two test routes are compiled (recomp/vupipe.py, speed_notes section 37:
   dbbe42cf in the opening, d43ba0fb on the deck at vb 24400-24700, the four of the 1.4 s step at vb 24667, f6aa3d14 / 35e6a92a on the
   crew deck); the PC hashes do not move.
   v23: the VIF1 chain walk in one loop (runtime/ee_io.h vif1_chain_fast, OPT_GS_FAST) and the sort-key loop of P12dbc0 natively
   (boot/native_elf.c, OPT_NAT_KEY), both exact; mgs2.cfg's MGS2_OPT_CYCLE times each against the rest (cfg 9 and 10 in steps.csv).
   v24 (after the 2026-10-23 audit; no change to the game code): the log's first line is the build's own title (audit L8); the memory
   report adds newlib's mallinfo, the heap block and the program break at the start and the end, and at the end every block malloc can
   still give, largest first (M2); steps.csv and the log count the disc pack reads per step (reads, KB, host time: runtime/iop_host.h
   g_disc_*), for the IOP / sound spikes (M3); mgs2.cfg hashes every 100 vblanks (M5).
   v25f (2026-10-23, audit H1: free play timed): this file is v24's (the code is the same; only the title differs, at the same length);
   free play's extra functions (tools/xunit.py) are separate units linked after v24's objects (gen_x.o in this program, an extra part
   after each overlay's own end, psp/ovl_x.ld), reached from v24's stubs through the link's --wrap of rt_missing, so v24's code keeps
   its addresses and its data its cache sets (psp/v25f.sh).  The battery log (level, voltage, temperature, external power, charging,
   low flag; v24's first deck run switched the PSP off on USB power with nothing in the log to say why) is psp/xhook.c's thread writing
   battery.txt every 60 s and on a change: here, in the vblank hook, it would have moved v24's code. */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psppower.h>
#include <pspfpu.h>
#include <pspsuspend.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include "host_time.h"
#if defined(MGS2_GL) && !defined(MGS2_XL)
#error "an MGS2_GL build needs MGS2_XL: the translator makes the picture's frames (psp/xl_psp.c)"
#endif

PSP_MODULE_INFO("MGS2T", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);
#ifndef POOL_KB
#define POOL_KB 1152                       /* the host stacks of the other guest threads and the IOP threads (host_stack_alloc below) */
#endif
PSP_MAIN_THREAD_STACK_SIZE_KB(1024 + POOL_KB + 16);   /* guest thread 1 runs on the top 1 MB (runtime/kernel.h, the recompiled code nests deep);
                                                         the bottom holds the pool and a 16 KB guard */

#ifndef HARNESS_TITLE                      /* psp/Makefile passes the EBOOT title (rebuild_use.sh TITLE), so the log names the build it came from */
#define HARNESS_TITLE "MGS2 timing harness (title not set by the build)"
#endif
register uint8_t *g_memb asm("$23");     /* gen_pre.h GEN_PIN23: the guest memory base, set before any guest code */
extern uint8_t g_mem[];
extern long g_hash_vb; extern uint64_t g_hash_ee, g_hash_iop;
extern uint32_t g_opt;                   /* boot.c: host_time.h OPT_* switched on */
extern uint32_t g_dg_calls[2];           /* boot.c rt_dg_call: draw-pipeline calls since the last vblank hook, full / reduced */
int mgs2_main(int argc, char **argv);

static char g_dir[192] = "ms0:/PSP/GAME/MGS2T/";
static volatile int g_quit;              /* 1: HOME was pressed; 2: the program is exiting in order */
static FILE *g_log; static char g_logpath[256], g_logbuf[1 << 17]; static int g_logn, g_logfail;
static uint64_t now(void) { return (uint64_t)sceKernelGetSystemTimeWide(); }
/* log.txt.  research_review.md M13: FAT records a file's size only when the file is closed, so a crash or a power-off loses what was written
   after the last close, fflush or not.  stderr is a stream into g_logbuf (newlib funopen, unbuffered: each print is copied there at once);
   log_put appends what it holds to log.txt with sceIoOpen / sceIoWrite / sceIoClose: every window in the hook (outside the timed steps),
   after the boot report, at the end, and at once when the buffer is full.  Not freopen: pspsdk's libcglue starts the std descriptors with a
   reference count of 0, so a file reopened on stderr's descriptor is never closed by close(); each reopen leaked one I/O handle, and v21's
   first build stopped logging after about 60 windows, when the emulator's 64 handles ran out. */
static void log_put(void)
{
  SceUID f;
  if (!g_logn) return;
  if ((f = sceIoOpen(g_logpath, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777)) < 0) { g_logfail++; return; }   /* kept for the next try */
  if (sceIoWrite(f, g_logbuf, g_logn) != g_logn) g_logfail++;
  sceIoClose(f); g_logn = 0;
}
static int log_write(void *cookie, const char *b, int n)
{
  int k = 0, m;
  while (k < n) {
    if ((m = n - k) > (int)sizeof g_logbuf - g_logn) m = (int)sizeof g_logbuf - g_logn;
    memcpy(g_logbuf + g_logn, b + k, m); g_logn += m; k += m;
    if (g_logn == (int)sizeof g_logbuf) { log_put(); g_logn = 0; }   /* full: to the stick now (dropped if the stick cannot be written) */
  }
  return n;
}
static void log_commit(void) { if (g_log) { fflush(g_log); log_put(); } }
/* psp/exc3.c's writer thread after a crash (crash catcher 3): the run's unwritten lines when the stopped thread is the main thread (their
   only writer, so g_logbuf holds still), then the record; raw sceIo only: the stopped thread may hold libc's locks */
void mgs2_log_crash(const char *b, int n, int pending)
{
  SceUID f;
  if (!g_logpath[0] || (f = sceIoOpen(g_logpath, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777)) < 0) return;
  if (pending && g_logn > 0 && g_logn <= (int)sizeof g_logbuf) { sceIoWrite(f, g_logbuf, g_logn); g_logn = 0; }
  sceIoWrite(f, b, n); sceIoClose(f);
}

/* ---- screen + log ---- */
static void out(const char *fmt, ...)
{
  char b[512]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
  pspDebugScreenPrintf("%s", b);
  if (g_log) { fputs(b, g_log); fflush(g_log); }
}

/* ---- the environment the machine reads (boot.c and the runtime call getenv; built with -Dgetenv=mgs2_getenv) ---- */
static struct { char k[32]; char *v; } g_env[48]; static int g_nenv;
char *mgs2_getenv(const char *k) { int i; for (i = 0; i < g_nenv; i++) if (!strcmp(g_env[i].k, k)) return g_env[i].v; return 0; }
static void env_set(const char *k, const char *v)
{
  int i; for (i = 0; i < g_nenv && strcmp(g_env[i].k, k); i++) {}
  if (i == g_nenv) { if (g_nenv == 48) return; snprintf(g_env[g_nenv++].k, sizeof g_env[0].k, "%s", k); } else free(g_env[i].v);
  g_env[i].v = strdup(v);
}
static char *read_text(const char *name)
{
  char p[256]; FILE *f; long n; char *b;
  snprintf(p, sizeof p, "%s%s", g_dir, name); if (!(f = fopen(p, "rb"))) return 0;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  if (!(b = malloc(n + 1)) || fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return 0; }
  fclose(f); b[n] = 0; return b;
}

/* ---- timing (host_time.h regions, boot.c host_vb_begin / host_vb_end) ---- */
/* exclusive regions: entering one pauses the region it is entered from (a stack), so IOP / GS / DG times never overlap */
#if defined(MGS2_GL)
#define NREG 6                          /* + HOST_XL, HOST_GL (the live picture's work) and HOST_GW (waits for the GE): psp/xl_psp.c */
#elif defined(MGS2_XL)
#define NREG 4                          /* + HOST_XL: the live packet translator (psp/xl_psp.c) */
#else
#define NREG 3
#endif
static uint64_t t_mark, reg_t[NREG], reg_since; static int reg_stk[16], reg_sp;
/* emulator profiling only (the container's PPSSPP with PspProf, tools/ppsspp_mgs2.diff): "prof A B" in mgs2.cfg (up to 8 lines) sets this
   word from vblank A to B: 1 = count, 2 = paused (inside the draw pipeline or the GS walk, unless "prof A B all"), 0 = off (the emulator
   dumps a profile per range).  A plain word in RAM: on the PSP it does nothing. */
volatile uint32_t g_emuactor;   /* boot/dispatch.h GEN_ACTOR_PROF: the act function of the actor running (dev builds) */
volatile uint32_t g_emuprof; static long g_prof_a[8], g_prof_b[8]; static int g_nprof, g_prof_all;
#ifdef MGS2_XL
static int g_prof_xl;                   /* "prof A B xl": count only inside the translator (HOST_XL); "prof A B xg" (2): inside the
                                           translator or the GS walk (HOST_GS), which holds the translator in the one walk (psp/xl3.sh);
                                           MGS2_GL builds: "prof A B gl" (3): inside the picture's work (HOST_GL); "prof A B xgl" (4):
                                           inside any of the three */
#endif
static void prof_mark(void)
{ if (g_emuprof) { int top = reg_sp > 0 && reg_sp <= 16 ? reg_stk[reg_sp - 1] : -1;
#if defined(MGS2_GL)
    if (g_prof_xl) { g_emuprof = (g_prof_xl != 3 && top == HOST_XL) || ((g_prof_xl == 2 || g_prof_xl == 4) && top == HOST_GS)
                                 || (g_prof_xl >= 3 && top == HOST_GL) ? 1 : 2; return; }
#elif defined(MGS2_XL)
    if (g_prof_xl) { g_emuprof = top == HOST_XL || (g_prof_xl == 2 && top == HOST_GS) ? 1 : 2; return; }
#endif
    g_emuprof = !g_prof_all && (top == HOST_DG || top == HOST_GS) ? 2 : 1; } }
void host_enter(int k) { uint64_t t = now(); if (reg_sp) reg_t[reg_stk[reg_sp - 1]] += t - reg_since; if (reg_sp < 16) reg_stk[reg_sp] = k; reg_sp++; reg_since = t; prof_mark(); }
void host_leave(int k) { uint64_t t = now(); (void)k; if (reg_sp > 0 && reg_sp <= 16) reg_t[reg_stk[reg_sp - 1]] += t - reg_since; if (reg_sp > 0) reg_sp--; reg_since = t; prof_mark(); }
static long g_deck_from = 24000, g_win = 100;
#if defined(MGS2_GL)
typedef struct { long n, over20, over40; uint64_t sum, max, iop, gs, dg, xl, gl, gw; } Win;
#elif defined(MGS2_XL)
typedef struct { long n, over20, over40; uint64_t sum, max, iop, gs, dg, xl; } Win;
#else
typedef struct { long n, over20, over40; uint64_t sum, max, iop, gs, dg; } Win;
#endif
static Win w_cur, w_deck;
/* the configurations boot.c opt_window cycles through (its cfg[] order), plus anything else */
#ifdef MGS2_GL
#define NCFG 22
static const char *const cfg_name[NCFG] = { "all speedups on", "GS bulk walk off", "poll scan skip off", "all off (= v1)", "other", "VIF0 fast off",
                                            "SPR bulk off", "native forms off", "reduced renderer off", "GS one-loop walk off", "native sort key off",
                                            "stage table off", "translator off", "picture off", "GE overlap off", "VU sync by scan",
                                            "block tex conv off", "area memo off", "flags-only test off", "palette memo off", "V4-8 decode off",
                                            "codec speedups off" };   /* 17-21: codec_speed.md A-D (2026-10-30) */
#else
#define NCFG 13
static const char *const cfg_name[NCFG] = { "all speedups on", "GS bulk walk off", "poll scan skip off", "all off (= v1)", "other", "VIF0 fast off",
                                            "SPR bulk off", "native forms off", "reduced renderer off", "GS one-loop walk off", "native sort key off",
                                            "stage table off", "translator off" };
#endif
static Win w_cfg[NCFG]; static int g_step_cfg;
#ifdef MGS2_GL
static int cfg_index(uint32_t o) { return o == ~0u ? 0 : o == ~1u ? 1 : o == ~2u ? 2 : o == 0 ? 3 : o == ~4u ? 5 : o == ~8u ? 6 : o == ~16u ? 7 : o == ~64u ? 8 :
                                         o == ~128u ? 9 : o == ~256u ? 10 : o == ~512u ? 11 : o == ~1024u ? 12 : o == ~2048u ? 13 : o == ~4096u ? 14 :
                                         o == ~8192u ? 15 : o == ~16384u ? 16 : o == ~65536u ? 17 : o == ~131072u ? 18 : o == ~262144u ? 19 :
                                         o == ~524288u ? 20 : o == ~0xf0000u ? 21 : 4; }
#else
static int cfg_index(uint32_t o) { return o == ~0u ? 0 : o == ~1u ? 1 : o == ~2u ? 2 : o == 0 ? 3 : o == ~4u ? 5 : o == ~8u ? 6 : o == ~16u ? 7 : o == ~64u ? 8 :
                                         o == ~128u ? 9 : o == ~256u ? 10 : o == ~512u ? 11 : o == ~1024u ? 12 : 4; }
#endif
#if defined(MGS2_GL)
#define NSTEPF 14                       /* + the translator's, the picture's and the GE waits' us */
#elif defined(MGS2_XL)
#define NSTEPF 12                       /* + the translator's us */
#else
#define NSTEPF 11
#endif
static uint32_t (*g_steps)[NSTEPF]; static long g_nsteps, g_cap_steps;  /* the timed range, one entry per vblank: step, IOP, GS, DG (us), configuration, wait word,
                                                                       full and reduced pipeline calls, disc pack reads, sectors, host us */
extern uint32_t g_disc_reads, g_disc_sectors; extern uint64_t g_disc_us; extern uint64_t (*g_disc_clock)(void);   /* runtime/iop_host.h */
static uint32_t g_dr0, g_ds0; static uint64_t g_du0; static uint32_t w_dr, w_ds; static uint64_t w_du;   /* at the step's start; in the window */
static void win_add(Win *w, uint64_t st)
{ w->n++; w->sum += st; w->iop += reg_t[HOST_IOP]; w->gs += reg_t[HOST_GS]; w->dg += reg_t[HOST_DG]; if (st > w->max) w->max = st; if (st > 20000) w->over20++; if (st > 40000) w->over40++;
#ifdef MGS2_XL
  w->xl += reg_t[HOST_XL];
#endif
#ifdef MGS2_GL
  w->gl += reg_t[HOST_GL]; w->gw += reg_t[HOST_GW];
#endif
}
static void win_print(const char *what, long vb, const Win *w)
{
  double n = w->n ? (double)w->n : 1;
#if defined(MGS2_GL)
  if (g_log) fprintf(g_log, "%s vb %ld: %ld steps, mean %.2f ms (logic %.2f, draw pipeline %.2f, IOP+sound %.2f, GS walk %.2f, translator %.2f, picture %.2f, GE wait %.2f), max %.2f ms, over 20 ms %ld, over 40 ms %ld\n",
                     what, vb, w->n, w->sum / n / 1000, (w->sum - w->iop - w->gs - w->dg - w->xl - w->gl - w->gw) / n / 1000, w->dg / n / 1000, w->iop / n / 1000,
                     w->gs / n / 1000, w->xl / n / 1000, w->gl / n / 1000, w->gw / n / 1000, w->max / 1000.0, w->over20, w->over40);
#elif defined(MGS2_XL)
  if (g_log) fprintf(g_log, "%s vb %ld: %ld steps, mean %.2f ms (logic %.2f, draw pipeline %.2f, IOP+sound %.2f, GS walk %.2f, translator %.2f), max %.2f ms, over 20 ms %ld, over 40 ms %ld\n",
                     what, vb, w->n, w->sum / n / 1000, (w->sum - w->iop - w->gs - w->dg - w->xl) / n / 1000, w->dg / n / 1000, w->iop / n / 1000, w->gs / n / 1000,
                     w->xl / n / 1000, w->max / 1000.0, w->over20, w->over40);
#else
  if (g_log) fprintf(g_log, "%s vb %ld: %ld steps, mean %.2f ms (logic %.2f, draw pipeline %.2f, IOP+sound %.2f, GS walk %.2f), max %.2f ms, over 20 ms %ld, over 40 ms %ld\n",
                     what, vb, w->n, w->sum / n / 1000, (w->sum - w->iop - w->gs - w->dg) / n / 1000, w->dg / n / 1000, w->iop / n / 1000, w->gs / n / 1000,
                     w->max / 1000.0, w->over20, w->over40);
#endif
}
static void finish(const char *why);
static int g_st_y;
#ifdef MGS2_PGO
static void pgo_vb(long vb);
#endif
#ifdef MGS2_GL
static long g_vb_now;                   /* the hook's vblank (host_vb_begin), for the picture's calls in host_vb_end */
void xl_psp_frame(long vb); void xl_psp_gl_vblank(long vb); void xl_psp_gl_end(void);   /* psp/xl_psp.c */
const char *host_dir(void) { return g_dir; }
#endif
static uintptr_t g_brk_max; static long g_brk_vb;   /* the program break's highest point and its vblank (host_vb_begin): the heap's peak */
void host_vb_begin(long vb)
{
  uint64_t t = now(), st = t - t_mark;
  { uintptr_t b = (uintptr_t)sbrk(0); if (b > g_brk_max) { g_brk_max = b; g_brk_vb = vb; } }
  if (reg_sp > 0 && reg_sp <= 16) { reg_t[reg_stk[reg_sp - 1]] += t - reg_since; reg_since = t; }   /* a vblank can fall inside a region */
  if (t_mark) {
    win_add(&w_cur, st);
    if (vb > g_deck_from) {
      win_add(&w_deck, st); win_add(&w_cfg[g_step_cfg], st);
      if (g_nsteps < g_cap_steps) { uint32_t *e = g_steps[g_nsteps++]; e[0] = (uint32_t)st; e[1] = (uint32_t)reg_t[HOST_IOP]; e[2] = (uint32_t)reg_t[HOST_GS];
        e[3] = (uint32_t)reg_t[HOST_DG]; e[4] = g_step_cfg; e[5] = *(uint32_t *)(g_mem + 0x18db74); e[6] = g_dg_calls[0]; e[7] = g_dg_calls[1];
        e[8] = g_disc_reads - g_dr0; e[9] = g_disc_sectors - g_ds0; e[10] = (uint32_t)(g_disc_us - g_du0);
#ifdef MGS2_XL
        e[11] = (uint32_t)reg_t[HOST_XL];
#endif
#ifdef MGS2_GL
        e[12] = (uint32_t)reg_t[HOST_GL]; e[13] = (uint32_t)reg_t[HOST_GW];
#endif
      }
    }
  }
#ifdef MGS2_GL
  g_vb_now = vb;
#endif
  w_dr += g_disc_reads - g_dr0; w_ds += g_disc_sectors - g_ds0; w_du += g_disc_us - g_du0;
  if (vb % g_win == 0) {
    win_print("window", vb, &w_cur);
    if (w_dr && g_log) fprintf(g_log, "disc vb %ld: %u pack reads, %u KB, %.2f ms\n", vb, w_dr, w_ds * 2, w_du / 1000.0);
    w_dr = w_ds = 0; w_du = 0;
    pspDebugScreenSetXY(0, g_st_y);
    pspDebugScreenPrintf("vb %6ld  step mean %6.2f ms  max %6.2f ms   \n", vb, w_cur.n ? w_cur.sum / (double)w_cur.n / 1000 : 0, w_cur.max / 1000.0);
    if (w_deck.n) pspDebugScreenPrintf("deck (vb %ld+): mean %6.2f ms over %ld steps   \n", g_deck_from, w_deck.sum / (double)w_deck.n / 1000, w_deck.n);
    memset(&w_cur, 0, sizeof w_cur);
    log_commit();
    scePowerTick(PSP_POWER_TICK_ALL);   /* research_review.md M18: no auto sleep or screen off in a long run (no input reaches the system) */
  }
  { int i; for (i = 0; i < g_nprof; i++)
      if (vb == g_prof_a[i]) { g_emuprof = 1; prof_mark(); fprintf(stderr, "emulator profile on at vb %ld\n", vb); }
      else if (vb == g_prof_b[i] && g_emuprof) { g_emuprof = 0; fprintf(stderr, "emulator profile off at vb %ld\n", vb); } }
#ifdef MGS2_PGO
  pgo_vb(vb);
#endif
  if (g_quit) finish("HOME");
}
void host_vb_end(void)
{
  if (g_hash_vb >= 0) {                 /* a hash was taken in this vblank's hook */
    pspDebugScreenSetXY(0, g_st_y + 2); pspDebugScreenPrintf("hash vb %6ld ee %016llx\n", g_hash_vb, (unsigned long long)g_hash_ee);
    g_hash_vb = -1; if (g_log) fflush(g_log);
  }
  g_step_cfg = cfg_index(g_opt);        /* boot.c opt_window chose the configuration of the step that starts now */
#ifdef MGS2_XL
  { void xl_psp_vblank(uint32_t full, uint32_t reduced); xl_psp_vblank(g_dg_calls[0], g_dg_calls[1]); }   /* psp/xl_psp.c: shown frames only */
#endif
#ifdef MGS2_GL
  xl_psp_gl_vblank(g_vb_now);           /* MGS2_GL_SHOT's saves, outside the steps' time */
#endif
  g_dg_calls[0] = g_dg_calls[1] = 0; g_dr0 = g_disc_reads; g_ds0 = g_disc_sectors; g_du0 = g_disc_us;
  memset(reg_t, 0, sizeof reg_t); t_mark = reg_since = now();   /* an open region counts on into the next step from here */
#ifdef MGS2_GL
  xl_psp_frame(g_vb_now);               /* the picture's work at this hook: in the step that starts now (HOST_GL, HOST_GW) */
#endif
}

/* ---- the end: summary, then wait for X (an automatic exit is the step after which other ports saw the console switch off) ---- */
/* the largest block malloc can still give (binary search, 16 KB steps): the heap's real headroom. The pointer goes through a
   volatile: GCC removes a malloc / free pair whose result is unused and takes it as a success (v7 printed the 64 MB limit). */
static void *volatile g_probe;
static unsigned heap_headroom(void) { unsigned lo = 0, hi = 64u << 20; while (hi - lo > 16384) { unsigned mid = lo + (hi - lo) / 2; g_probe = malloc(mid); if (g_probe) { free(g_probe); lo = mid; } else hi = mid; } g_probe = 0; return lo; }
/* v24 (audit M2): the largest block alone cannot tell free space from fragmentation or a leak.  newlib's own accounting (mallinfo: the
   arena taken from the heap block with sbrk, in use, free inside it in how many chunks, the releasable top), the heap block's start and the
   program break; with walk, every block malloc can still give, largest first (each kept until the next is found, then all freed) */
extern SceUID __psp_heap_blockid;        /* pspsdk libcglue: the heap's memory block */
static void mem_report(const char *when, int walk)
{
  struct mallinfo mi = mallinfo();
  out("memory %s: heap block at %08x, break %08x; malloc arena %u KB: in use %u KB, free %u KB in %d chunks, top %u KB\n", when,
      (unsigned)(uintptr_t)sceKernelGetBlockHeadAddr(__psp_heap_blockid), (unsigned)(uintptr_t)sbrk(0), (unsigned)mi.arena >> 10,
      (unsigned)mi.uordblks >> 10, (unsigned)mi.fordblks >> 10, mi.ordblks, (unsigned)mi.keepcost >> 10);
  if (walk) {
    static void *b[48]; unsigned n = 0, tot = 0, s; char l[400]; int k = snprintf(l, sizeof l, "memory %s: blocks malloc can still give (KB):", when);
    while (n < 48 && (s = heap_headroom()) >= 16384 && (b[n] = malloc(s))) { tot += s; n++; if (k < (int)sizeof l - 12) k += snprintf(l + k, sizeof l - k, " %u", s >> 10); }
    out("%s = %u KB in %u blocks\n", l, tot >> 10, n);
    while (n) free(b[--n]);
  }
}
static uint8_t *g_main_lo, *g_main_hi;   /* MGS2_STACK_MARK: the main thread's stack above the host stack pool (guest thread 1 runs there),
                                            filled with 0xa5 by pool_init up to 1 KB below its own frame */
static void summary(void)
{
  char p[256]; FILE *f; long i;
  pspDebugScreenSetXY(0, g_st_y + 4);
  if (w_deck.n) {
    double n = (double)w_deck.n;
#if defined(MGS2_GL)
    out("deck steps (vb %ld+): %ld, mean %.2f ms: logic %.2f, draw pipeline %.2f, IOP+sound %.2f, GS walk %.2f, translator %.2f, picture %.2f, GE wait %.2f; max %.2f ms; "
        "over 20 ms %ld, over 40 ms %ld\n", g_deck_from, w_deck.n, w_deck.sum / n / 1000,
        (w_deck.sum - w_deck.iop - w_deck.gs - w_deck.dg - w_deck.xl - w_deck.gl - w_deck.gw) / n / 1000, w_deck.dg / n / 1000, w_deck.iop / n / 1000,
        w_deck.gs / n / 1000, w_deck.xl / n / 1000, w_deck.gl / n / 1000, w_deck.gw / n / 1000, w_deck.max / 1000.0, w_deck.over20, w_deck.over40);
#elif defined(MGS2_XL)
    out("deck steps (vb %ld+): %ld, mean %.2f ms: logic %.2f, draw pipeline %.2f, IOP+sound %.2f, GS walk %.2f, translator %.2f; max %.2f ms; over 20 ms %ld, over 40 ms %ld\n",
        g_deck_from, w_deck.n, w_deck.sum / n / 1000, (w_deck.sum - w_deck.iop - w_deck.gs - w_deck.dg - w_deck.xl) / n / 1000, w_deck.dg / n / 1000,
        w_deck.iop / n / 1000, w_deck.gs / n / 1000, w_deck.xl / n / 1000, w_deck.max / 1000.0, w_deck.over20, w_deck.over40);
#else
    out("deck steps (vb %ld+): %ld, mean %.2f ms: logic %.2f, draw pipeline %.2f, IOP+sound %.2f, GS walk %.2f; max %.2f ms; over 20 ms %ld, over 40 ms %ld\n",
        g_deck_from, w_deck.n, w_deck.sum / n / 1000, (w_deck.sum - w_deck.iop - w_deck.gs - w_deck.dg) / n / 1000, w_deck.dg / n / 1000,
        w_deck.iop / n / 1000, w_deck.gs / n / 1000, w_deck.max / 1000.0, w_deck.over20, w_deck.over40);
#endif
  } else out("no step reached the deck range (vb %ld+)\n", g_deck_from);
  for (i = 0; i < NCFG; i++) if (w_cfg[i].n) {
    const Win *w = &w_cfg[i]; double n = (double)w->n;
#if defined(MGS2_GL)
    out("  %-19s %4ld steps, mean %.2f ms: logic %.2f, draw %.2f, IOP %.2f, GS %.2f, translator %.2f, picture %.2f, GE wait %.2f; max %.2f\n", cfg_name[i],
        w->n, w->sum / n / 1000, (w->sum - w->iop - w->gs - w->dg - w->xl - w->gl - w->gw) / n / 1000, w->dg / n / 1000, w->iop / n / 1000, w->gs / n / 1000,
        w->xl / n / 1000, w->gl / n / 1000, w->gw / n / 1000, w->max / 1000.0);
#elif defined(MGS2_XL)
    out("  %-19s %4ld steps, mean %.2f ms: logic %.2f, draw %.2f, IOP %.2f, GS %.2f, translator %.2f; max %.2f\n", cfg_name[i], w->n, w->sum / n / 1000,
        (w->sum - w->iop - w->gs - w->dg - w->xl) / n / 1000, w->dg / n / 1000, w->iop / n / 1000, w->gs / n / 1000, w->xl / n / 1000, w->max / 1000.0);
#else
    out("  %-19s %4ld steps, mean %.2f ms: logic %.2f, draw %.2f, IOP %.2f, GS %.2f; max %.2f\n", cfg_name[i], w->n, w->sum / n / 1000,
        (w->sum - w->iop - w->gs - w->dg) / n / 1000, w->dg / n / 1000, w->iop / n / 1000, w->gs / n / 1000, w->max / 1000.0);
#endif
  }
  snprintf(p, sizeof p, "%ssteps.csv", g_dir);
  if (g_nsteps && (f = fopen(p, "w"))) {
#if defined(MGS2_GL)
    fprintf(f, "vb,step_us,iop_us,gs_us,dg_us,cfg,wait,full,lite,disc_n,disc_kb,disc_us,xl_us,gl_us,gw_us\n");
    for (i = 0; i < g_nsteps; i++) fprintf(f, "%ld,%u,%u,%u,%u,%u,%x,%u,%u,%u,%u,%u,%u,%u,%u\n", g_deck_from + 1 + i, g_steps[i][0], g_steps[i][1], g_steps[i][2],
                                          g_steps[i][3], g_steps[i][4], g_steps[i][5], g_steps[i][6], g_steps[i][7], g_steps[i][8], g_steps[i][9] * 2,
                                          g_steps[i][10], g_steps[i][11], g_steps[i][12], g_steps[i][13]);
#elif defined(MGS2_XL)
    fprintf(f, "vb,step_us,iop_us,gs_us,dg_us,cfg,wait,full,lite,disc_n,disc_kb,disc_us,xl_us\n");
    for (i = 0; i < g_nsteps; i++) fprintf(f, "%ld,%u,%u,%u,%u,%u,%x,%u,%u,%u,%u,%u,%u\n", g_deck_from + 1 + i, g_steps[i][0], g_steps[i][1], g_steps[i][2], g_steps[i][3], g_steps[i][4],
                                          g_steps[i][5], g_steps[i][6], g_steps[i][7], g_steps[i][8], g_steps[i][9] * 2, g_steps[i][10], g_steps[i][11]);
#else
    fprintf(f, "vb,step_us,iop_us,gs_us,dg_us,cfg,wait,full,lite,disc_n,disc_kb,disc_us\n");
    for (i = 0; i < g_nsteps; i++) fprintf(f, "%ld,%u,%u,%u,%u,%u,%x,%u,%u,%u,%u,%u\n", g_deck_from + 1 + i, g_steps[i][0], g_steps[i][1], g_steps[i][2], g_steps[i][3], g_steps[i][4],
                                          g_steps[i][5], g_steps[i][6], g_steps[i][7], g_steps[i][8], g_steps[i][9] * 2, g_steps[i][10]);
#endif
    fclose(f); out("steps.csv written (%ld vblanks)\n", g_nsteps);
  }
#ifdef MGS2_XL
  { void xl_psp_report(FILE *f); xl_psp_report(g_log); }
#endif
  { unsigned h = heap_headroom(); uintptr_t b = (uintptr_t)sbrk(0), up = g_brk_max > b ? g_brk_max - b : 0;
    out("free memory now: heap headroom %u KB; outside the heap: largest block %u KB, total %u KB\n", h / 1024, (unsigned)(sceKernelMaxFreeMemSize() / 1024), (unsigned)(sceKernelTotalFreeMemSize() / 1024));
    out("memory: the break's highest point %08x at vb %ld, %u KB above the break now: the headroom was about %d KB at its lowest\n",
        (unsigned)g_brk_max, g_brk_vb, (unsigned)(up >> 10), (int)(h / 1024) - (int)(up >> 10)); }
  if (g_main_hi > g_main_lo) { const uint8_t *q = g_main_lo; while (q < g_main_hi && *q == 0xa5) q++;
    out("stack: the main thread above the pool (guest thread 1) uses %u of %u bytes below pool_init's frame\n", (unsigned)(g_main_hi - q), (unsigned)(g_main_hi - g_main_lo)); }
  mem_report("at the end", 1);
  out("disc pack: %u reads, %u KB, %.2f s of host time in the run\n", g_disc_reads, g_disc_sectors * 2, g_disc_us / 1e6);
  if (g_logfail) out("log.txt: %d writes to the memory stick failed, so lines may be missing\n", g_logfail);
}
static int g_vol_locked;
static void psp_exit(void) { log_commit(); g_log = 0; g_quit = 2; if (g_vol_locked) sceKernelVolatileMemUnlock(0); sceKernelExitGame(); for (;;) sceKernelSleepThread(); }
static void finish(const char *why)
{
  static int once; if (once++) psp_exit();
#ifdef MGS2_GL
  xl_psp_gl_end();                      /* the GE idle and the text on the screen again */
#endif
  out("\nend: %s\n", why); summary(); log_commit();
  if (g_quit) psp_exit();               /* HOME: the user already chose to quit */
  out("Press X to exit.\n");
  for (;;) { SceCtrlData c; sceCtrlReadBufferPositive(&c, 1); if ((c.Buttons & PSP_CTRL_CROSS) || g_quit) break; sceKernelDelayThread(50000); }
  psp_exit();
}
/* boot.c and the runtime are built with -Dexit=mgs2_exit -Dabort=mgs2_abort: the machine's stops land here */
void mgs2_exit(int code) { char b[48]; if (g_log) fflush(g_log); snprintf(b, sizeof b, code ? "the machine stopped, code %d (see log.txt)" : "end of the run", code); finish(b); for (;;) {} }
void mgs2_abort(void) { finish("abort (see log.txt)"); for (;;) {} }
void psp_uc_end(void) { if (g_log) fprintf(g_log, "a coroutine returned with no link\n"); finish("a coroutine returned with no link"); }

/* ---- HOME ---- */
static int exit_cb(int a, int b, void *c)
{
  int i; (void)a; (void)b; (void)c; g_quit = 1;
  for (i = 0; i < 80 && g_quit != 2; i++) sceKernelDelayThread(25000);   /* the run answers at its next vblank */
  if (g_quit != 2) sceKernelExitGame();
  return 0;
}
static int cb_thread(SceSize n, void *a) { int id = sceKernelCreateCallback("exit", exit_cb, 0); (void)n; (void)a; sceKernelRegisterExitCallback(id); sceKernelSleepThreadCB(); return 0; }

/* ---- host stacks from the main thread's own stack (runtime/kernel.h HOST_STACK_POOL): a coroutine stack outside the thread's block
   powered the console off in SandstoneDev's port (sandstone_review.md 2.8); never freed (the threads keep theirs for the whole run) ---- */
static uint8_t *g_pool, *g_pool_end;
void *host_stack_alloc(size_t n)
{
  uint8_t *p = g_pool; n = (n + 63) & ~(size_t)63;
  if (!p || p + n > g_pool_end) { out("host stack pool exhausted (%u bytes asked, %u left): raise POOL_KB\n", (unsigned)n, (unsigned)(g_pool_end - g_pool)); return 0; }
  g_pool += n; return p;
}
static void pool_init(void)
{
  SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti;
  if (sceKernelReferThreadStatus(sceKernelGetThreadId(), &ti) < 0 || !ti.stack) { out("thread status unavailable: no host stack pool\n"); return; }
  g_pool = (uint8_t *)(((uintptr_t)ti.stack + (16 << 10) + 63) & ~(uintptr_t)63); g_pool_end = g_pool + ((size_t)POOL_KB << 10);
  { volatile uint8_t here; if ((uint8_t *)&here < g_pool_end + (512 << 10)) { out("main stack too small for the pool\n"); g_pool = g_pool_end = 0; }
    else if (mgs2_getenv("MGS2_STACK_MARK")) {   /* the deepest use of the rest, for summary() */
      g_main_lo = g_pool_end; g_main_hi = (uint8_t *)(((uintptr_t)&here - 1024) & ~(uintptr_t)63); memset(g_main_lo, 0xa5, (size_t)(g_main_hi - g_main_lo)); } }
}

/* ---- big buffers (runtime/iop_host.h HOST_BIG_ALLOC: the SPU RAM) from the 4 MB volatile block when the PSP grants it, else the heap;
   zeroed, never freed; the block is unlocked at exit (sandstone_review.md 2.8: it worked on hardware there; lost across sleep) ---- */
static uint8_t *g_vol, *g_vol_end;
static void vol_init(void)
{
  void *p = 0; int n = 0, r = sceKernelVolatileMemTryLock(0, &p, &n);
  if (r == 0 && p && n > 0) { g_vol = p; g_vol_end = g_vol + n; g_vol_locked = 1; }
  out("volatile memory: %s (%d, %d KB at %08x)\n", g_vol_locked ? "locked" : "not granted", r, n / 1024, (unsigned)(uintptr_t)p);
}
void *host_big_alloc(size_t n)
{
  n = (n + 63) & ~(size_t)63;
  if (g_vol && g_vol + n <= g_vol_end) { uint8_t *p = g_vol; g_vol += n; memset(p, 0, n); return p; }
  return calloc(1, n);
}
#ifdef MGS2_GL
/* the live picture's frame memory (psp/xl_psp.c): n bytes from the end of the volatile block (the SPU RAM takes its start), 64-byte
   aligned; 0 when they do not fit */
void *host_vol_tail(size_t n)
{
  n = (n + 63) & ~(size_t)63;
  if (!g_vol || (size_t)(g_vol_end - g_vol) < n) return 0;
  g_vol_end -= n; return g_vol_end;
}
#endif

/* ---- stage overlays (boot/dispatch.h STAGE_OVL, psp/ovl.ld): one stage image's translated code in memory at a time, read from
   ovl_<stage>.bin next to the EBOOT the first time the game calls into that image ---- */
#ifndef OVL_SIZE
#define OVL_SIZE 0x450000
#endif
uint8_t ovl_arena[OVL_SIZE] __attribute__((aligned(8192)));   /* 8 KB: the overlay code keeps its I-cache sets whatever the main ELF's size (tools/layout_sim.c) */
void rt_poll(void); void P100010(void);
struct OvlHdr { uint32_t magic; char name[12]; void (*init)(void); int (*call)(uint32_t); uint8_t *bss, *end; const void *chk[3];
  void (*const *ctor_a)(void), (*const *ctor_b)(void); };
static char ovl_cur[12];
static const char *ovl_cur_p;           /* v26: the loaded overlay's name as dispatch.h passes it (stage_code[].name, one pointer per stage):
                                           with OPT_STAGE_TAB a call with that pointer skips the name compare */
#ifdef MGS2_PGO
/* PGO=gen builds (psp/Makefile, emulator only): the game code and the runtime count their branches (-fprofile-generate); "pgo A B" in
   mgs2.cfg zeroes the counts at vblank A and writes the .gcda files at B into ms0:/pgo (which must exist), named by object, for a
   PGO=use build.  Of the overlays only PGO_STAGE (if any) is instrumented: its constructors register its counts at vblank A, when it
   must be the overlay in memory (v26: registering at its load failed on the free route, which loads w00a, d01t, then w00a again; the
   counts an overlay gathers before A are zeroed at A anyway), and loading another overlay between A and B ends the profile (the
   registered counts would be gone). */
void __gcov_reset(void); void __gcov_dump(void);
static long g_pgo_a = -1, g_pgo_b = -1; static int g_pgo_reg, g_pgo_bad;
static void pgo_ovl(const char *name)
{
  if (*PGO_STAGE && g_pgo_reg) { g_pgo_bad = 1; out("pgo: overlay %s loaded after %s was registered: no profile will be written\n", name, PGO_STAGE); }
}
static void pgo_vb(long vb)
{
  if (vb == g_pgo_a) {
    if (*PGO_STAGE) {                   /* PGO_STAGE empty: the overlays are not instrumented */
      const struct OvlHdr *h = (const struct OvlHdr *)ovl_arena; void (*const *c)(void);
      if (strcmp(ovl_cur, PGO_STAGE)) { g_pgo_bad = 1; out("pgo: overlay %s not in memory at vb %ld: no profile will be written\n", PGO_STAGE, vb); }
      else { for (c = h->ctor_a; c < h->ctor_b; c++) (*c)();
        g_pgo_reg = 1; fprintf(stderr, "pgo: overlay %s registered at vb %ld (%d constructors)\n", PGO_STAGE, vb, (int)(h->ctor_b - h->ctor_a)); }
    }
    __gcov_reset(); fprintf(stderr, "pgo: counts zeroed at vb %ld\n", vb);
  }
  if (vb == g_pgo_b) {
    if (g_pgo_bad || (*PGO_STAGE && !g_pgo_reg)) out("pgo: no profile written at vb %ld (overlay %s %s)\n", vb, PGO_STAGE, g_pgo_bad ? "replaced or missing" : "not registered");
    else {                              /* libgcov cannot create the build directory's path on the memory stick: <ms0:/pgo>/<object>.gcda */
      uint64_t t0 = now(); setenv("GCOV_PREFIX", "ms0:/pgo", 1); setenv("GCOV_PREFIX_STRIP", "99", 1); __gcov_dump();
      out("pgo: profile written to ms0:/pgo at vb %ld in %u ms (see stderr for libgcov errors)\n", vb, (unsigned)((now() - t0) / 1000)); }
  }
}
#endif
int stage_ovl_call(const char *name, uint32_t a)
{
  struct OvlHdr *h = (struct OvlHdr *)ovl_arena;
  if ((name != ovl_cur_p || !(g_opt & OPT_STAGE_TAB)) && strcmp(ovl_cur, name)) {
    char p[256]; FILE *f; size_t n; uint64_t t0 = now();
    ovl_cur[0] = 0; ovl_cur_p = 0; snprintf(p, sizeof p, "%sovl_%s.bin", g_dir, name);
    if (!(f = fopen(p, "rb"))) { out("stage overlay %s missing\n", p); return 0; }
    n = fread(ovl_arena, 1, OVL_SIZE, f); fclose(f);
    if (n < sizeof *h || h->magic != 0x314c564fu || strcmp(h->name, name) || h->end > ovl_arena + OVL_SIZE || h->bss > h->end) {
      out("stage overlay %s: bad file (%u bytes)\n", p, (unsigned)n); return 0; }
    if (h->chk[0] != (const void *)g_mem || h->chk[1] != (const void *)rt_poll || h->chk[2] != (const void *)P100010) {
      out("stage overlay %s is from another build of the EBOOT: copy the EBOOT and all ovl_*.bin together\n", p); return 0; }
    memset(h->bss, 0, h->end - h->bss);
    sceKernelDcacheWritebackInvalidateAll(); sceKernelIcacheInvalidateAll();
    snprintf(ovl_cur, sizeof ovl_cur, "%s", name);
#ifdef MGS2_PGO
    pgo_ovl(name);
#endif
    fprintf(stderr, "stage overlay %s loaded: %u bytes in %u ms\n", name, (unsigned)n, (unsigned)((now() - t0) / 1000));
  }
  ovl_cur_p = name;                     /* (reached only with that overlay loaded) */
  return h->call(a);
}

/* v18: does `cache 0x18` (create dirty exclusive) claim a line without disturbing anything else, and does boot.c rt_qcopy_nov's copy through
   it leave the right bytes in memory, for destination lines out of the cache and in it?  32 KB of dirty lines from a known pattern fill the
   16 KB cache (the bystanders), 8 KB is copied with an unaligned head and tail, everything is written back and all three buffers are read
   through the uncached mirror.  Any wrong word: g_cde_ok stays 0 and the copies keep the plain form.  MGS2_CDE=0 in mgs2.cfg turns it off. */
extern uint32_t g_cde_ok; void rt_qcopy_nov(uint8_t *d, const uint8_t *s, uint32_t n);
uint32_t g_cde_pass;                         /* the self-test passed (hwprobe.c probe 12 times the claimed lines only then) */
#ifdef GUEST_SPRSTATE
/* v18: the machine state lives in the scratchpad (runtime/guest.h GSVS, psp/spr_state.ld) and the VFPU forms load / store the VU0 state
   there: a round trip of lv.q / sv.q and lv.s / sv.s on scratchpad words, before the game starts (wrong words) */
static uint32_t spr_vfpu_test(void)
{
  volatile uint32_t *s = (volatile uint32_t *)0x00010000; uint32_t bad = 0, i;
  for (i = 0; i < 64; i++) s[i] = 0x5a5a0000u + i * 0x01010101u;
  for (i = 0; i < 64; i += 4) __asm__ volatile ("lv.q C000, 0(%0)\n\tsv.q C000, 256(%0)" :: "r"(s + i) : "memory");
  for (i = 0; i < 64; i++) __asm__ volatile ("lv.s S000, 0(%0)\n\tsv.s S000, 512(%0)" :: "r"(s + i) : "memory");
  for (i = 0; i < 64; i++) bad += (s[64 + i] != s[i]) + (s[128 + i] != s[i]);
  return bad;
}
#endif
static uint32_t cde_selftest(int hot)
{
  enum { NA = 32 << 10, NB = 8 << 10 };
  uint8_t *raw = malloc(NA + 2 * NB + 64), *a, *b, *c; uint32_t *w, i, bad = 0; const volatile uint32_t *u;
  if (!raw) return 0xffffffffu;
  a = (uint8_t *)(((uintptr_t)raw + 63) & ~(uintptr_t)63); b = a + NA; c = b + NB;
  for (w = (uint32_t *)a, i = 0; i < NA / 4; i++) w[i] = 0x11110000u + i;
  for (w = (uint32_t *)b, i = 0; i < NB / 4; i++) w[i] = 0x33330000u + i;
  for (w = (uint32_t *)c, i = 0; i < NB / 4; i++) w[i] = 0x44440000u + i;
  sceKernelDcacheWritebackInvalidateAll();
  for (w = (uint32_t *)a, i = 0; i < NA / 4; i++) w[i] = 0x22220000u + i;          /* the cache: dirty lines of a only */
  if (hot) for (w = (uint32_t *)c, i = 0; i < NB / 4; i += 16) w[i] = 0x55550000u + i;   /* or c's lines in it, dirty, with other words */
  g_cde_ok = 1; rt_qcopy_nov(c + 16, b + 48, NB - 64); g_cde_ok = 0;              /* 3 qwords to c's next line, 126 claimed lines, 1 qword */
  sceKernelDcacheWritebackInvalidateAll();
  for (u = (const volatile uint32_t *)((uintptr_t)a | 0x40000000u), i = 0; i < NA / 4; i++) bad += u[i] != 0x22220000u + i;
  for (u = (const volatile uint32_t *)((uintptr_t)b | 0x40000000u), i = 0; i < NB / 4; i++) bad += u[i] != 0x33330000u + i;
  for (u = (const volatile uint32_t *)((uintptr_t)c | 0x40000000u), i = 0; i < NB / 4; i++) {
    uint32_t want = i >= 4 && i < NB / 4 - 12 ? 0x33330000u + i + 8 : (hot && !(i & 15) ? 0x55550000u + i : 0x44440000u + i);
    bad += u[i] != want; }
  free(raw);
  return bad;
}

int main(int argc, char **argv)
{
  static char elf[256], vbs[16], *margv[4]; long vblanks = 25200; int clock333 = 1, probe = 1; char *cfg, *pad;
  g_memb = g_mem;
  pspFpuSetFCR31(0x01000001u);          /* lessons A9: flush to zero + round toward zero (the R5900 rule), before any guest code */
  { SceUID th = sceKernelCreateThread("cb", cb_thread, 0x11, 0xfa0, 0, 0); if (th >= 0) sceKernelStartThread(th, 0, 0); }
#ifdef MGS2_GL
  pspDebugScreenInitEx((void *)(0x44000000u + 0x154000u), PSP_DISPLAY_PIXEL_FORMAT_8888, 1);   /* psp/xl_psp.c GL_TXT: VRAM after the frame
                                                                                                    buffers and the depth buffer */
#else
  pspDebugScreenInit();
#endif
  sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
  if (argc > 0 && argv[0] && strrchr(argv[0], '/')) { size_t l = strrchr(argv[0], '/') - argv[0] + 1; if (l < sizeof g_dir) { memcpy(g_dir, argv[0], l); g_dir[l] = 0; } }
  /* everything the machine prints on stderr goes to log.txt (a 128 KB buffer, appended to the file every g_win vblanks in the hook, outside
     the timed steps: log_commit) */
  snprintf(g_logpath, sizeof g_logpath, "%slog.txt", g_dir);
  { SceUID f = sceIoOpen(g_logpath, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (f >= 0) { sceIoClose(f); if ((g_log = funopen(0, 0, log_write, 0, 0))) { setvbuf(g_log, 0, _IONBF, 0); stderr = g_log; } } }
  out("%s\nfolder %s\n", HARNESS_TITLE, g_dir);
  if (!g_log) out("log.txt could not be opened: results on screen only\n");
  { char p[256];
    snprintf(p, sizeof p, "%smgs2disc.pak", g_dir); env_set("MGS2_DISC", p);
    snprintf(p, sizeof p, "%smemcard0", g_dir); env_set("MGS2_MC0", p);
    env_set("MGS2_SKIP_MOVIES", "1"); env_set("MGS2_HASH_EVERY", "500"); env_set("MGS2_STACK_MARK", "1"); env_set("MGS2_SIF_LOG", "0"); }
  if ((pad = read_text("pad.txt"))) { char *e = pad + strlen(pad); while (e > pad && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = 0; env_set("MGS2_PAD", pad); free(pad); }
  else out("no pad.txt: the game gets no input\n");
  if ((cfg = read_text("mgs2.cfg"))) {   /* lines: KEY=VALUE (environment), "clock 333", "vblanks N", "deck N", "prof A B [all]" (up to 8); # comments */
    char *l = cfg, *nl;
    if ((uint8_t)l[0] == 0xEF && (uint8_t)l[1] == 0xBB && (uint8_t)l[2] == 0xBF) l += 3;   /* UTF-8 byte-order mark (Notepad) */
    for (; *l; l = nl) {   /* "clock 333" (the default) / "clock default" */
      char *eq, *e; nl = l + strcspn(l, "\n"); if (*nl) *nl++ = 0;
      e = l + strlen(l); while (e > l && (e[-1] == '\r' || e[-1] == ' ')) *--e = 0;
      if (!*l || *l == '#') continue;
      out("config: %s\n", l);
      if (!strncmp(l, "clock 333", 9)) clock333 = 1;
      else if (!strncmp(l, "clock default", 13)) clock333 = 0;
      else if (!strncmp(l, "vblanks ", 8)) vblanks = atol(l + 8);
      else if (!strcmp(l, "probe off")) probe = 0;
      else if (!strncmp(l, "deck ", 5)) g_deck_from = atol(l + 5);
#ifdef MGS2_PGO
      else if (!strncmp(l, "pgo ", 4)) sscanf(l + 4, "%ld %ld", &g_pgo_a, &g_pgo_b);
#endif
      else if (!strncmp(l, "prof ", 5)) { if (strstr(l, " all")) g_prof_all = 1;
#ifdef MGS2_GL
        if (strstr(l, " xgl")) g_prof_xl = 4; else if (strstr(l, " gl")) g_prof_xl = 3; else
#endif
#ifdef MGS2_XL
        if (strstr(l, " xg")) g_prof_xl = 2; else if (strstr(l, " xl")) g_prof_xl = 1;
#endif
        if (g_nprof < 8 && sscanf(l + 5, "%ld %ld", &g_prof_a[g_nprof], &g_prof_b[g_nprof]) == 2) g_nprof++; }
      else if ((eq = strchr(l, '='))) { *eq = 0; env_set(l, eq + 1); }
    }
    free(cfg);
  }
  /* without MGS2_OPT the run uses boot.c's default g_opt (from v21 all on, the reduced renderer included); "MGS2_OPT_CYCLE=24000:100"
     in mgs2.cfg brings the rotation back */
  if (mgs2_getenv("MGS2_OPT")) out("speedups: MGS2_OPT=%s\n", mgs2_getenv("MGS2_OPT"));
  else out("speedups: default (g_opt %08x)\n", (unsigned)g_opt);
  if (mgs2_getenv("MGS2_OPT_CYCLE")) out("speedups: take turns from vb:steps %s\n", mgs2_getenv("MGS2_OPT_CYCLE"));
  if (clock333) { int r = scePowerSetClockFrequency(333, 333, 166); out("clock 333 MHz requested: %d\n", r); }
  out("CPU %d MHz, bus %d MHz; FCR31 %08x\n", scePowerGetCpuClockFrequency(), scePowerGetBusClockFrequency(), (unsigned)pspFpuGetFCR31());
  /* v21: the loaded modules, so a CFW plugin (a thread taking CPU time or cache) shows in the log (ARK-4 notes, ark4_options.md).  The
     buffer size is passed as an element count: safe whether the firmware reads it as elements or bytes.  From user mode a kernel module
     may give no info (its ID only). */
  { static SceUID mid[512]; int n = 0, i, r = sceKernelGetModuleIdList(mid, 512, &n);
    if (r < 0) out("modules: list error %08x\n", (unsigned)r);
    else { out("modules: %d loaded\n", n);
      for (i = 0; i < n && i < 128; i++) { SceKernelModuleInfo mi; memset(&mi, 0, sizeof mi); mi.size = sizeof mi;
        if ((r = sceKernelQueryModuleInfo(mid[i], &mi)) >= 0) out("module %08x %.28s text %08x %u bytes\n", (unsigned)mid[i], mi.name, mi.text_addr, mi.text_size);
        else out("module %08x (no info: %08x)\n", (unsigned)mid[i], (unsigned)r); } } }
  { uint32_t c0 = cde_selftest(0), c1 = cde_selftest(1); const char *e = mgs2_getenv("MGS2_CDE");
    g_cde_pass = !c0 && !c1; g_cde_ok = g_cde_pass && !(e && *e == '0');
    out("claimed-line copies (cache 0x18): self-test %u / %u wrong words (lines out of / in the cache), %s\n", (unsigned)c0, (unsigned)c1,
        g_cde_ok ? "on" : !g_cde_pass ? "off" : "off (MGS2_CDE=0)"); }
  if (probe) { void hw_probe(void (*o)(const char *, ...)); hw_probe(out); }
  vol_init(); pool_init(); out("host stack pool: %u KB at %08x\n", (unsigned)((g_pool_end - g_pool) >> 10), (unsigned)(uintptr_t)g_pool);
  out("free memory: heap headroom %u KB; outside the heap: largest block %u KB, total %u KB\n", heap_headroom() / 1024, (unsigned)(sceKernelMaxFreeMemSize() / 1024), (unsigned)(sceKernelTotalFreeMemSize() / 1024));
  mem_report("at the start", 0);
  g_disc_clock = now;
  out("run: %ld vblanks, deck timing from vb %ld, hash every %s\n", vblanks, g_deck_from, mgs2_getenv("MGS2_HASH_EVERY"));
  g_cap_steps = vblanks > g_deck_from ? vblanks - g_deck_from : 0;
  if (g_cap_steps && !(g_steps = malloc(g_cap_steps * sizeof *g_steps))) g_cap_steps = 0;
  g_st_y = pspDebugScreenGetY() + 1;
  log_commit();                         /* the boot report is on the stick before the game starts */
  snprintf(elf, sizeof elf, "%sSLES_503.83", g_dir); snprintf(vbs, sizeof vbs, "%ld", vblanks);
  margv[0] = "mgs2"; margv[1] = elf; margv[2] = vbs; margv[3] = 0;
#ifdef GUEST_SPRSTATE
  { int rt_spr_check(void); uint32_t v = spr_vfpu_test(); int l = rt_spr_check();
    memset((void *)0x00010000, 0, 0x4000);   /* the state starts at zero, as it would in .bss (after the probes, which use the scratchpad) */
    out("machine state in the scratchpad: layout %s (%d), VFPU loads / stores there %u wrong words\n", l ? "WRONG" : "ok", l, (unsigned)v); }
#endif
  mgs2_main(3, margv);
  finish("the game's entry returned");
  return 0;
}
