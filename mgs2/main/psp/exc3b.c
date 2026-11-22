/* exc3b.c - crash catcher 3b (2026-10-28, task #73; psp/exc3b.S): psp/exc3.c (gl4's catcher 3) where the stopped thread sleeps inside
   the system instead of spinning.  gl4's self-test on Ffm's PSP drew the record and turned interrupts on, and then nothing ran (no
   writer line, no battery.txt line; the PSP off about 10 s later): psp/exc3b.S now hands the thread back to the system as PSPLink does
   (its kernel stack, a system-call context, k1 = 0) and calls the system's sceKernelSleepThread, which me_exc_install finds at the start
   (sctrlHENFindFunction, ThreadManForUser's NID 0x9ACE131E, the same on every firmware; a kernel address or nothing).  Not found:
   catcher 2's screen only.  The writer thread's first act is now a green line drawn before any system call ("the system runs again"),
   so a photo shows how far it got; crash.txt also gets the stopped thread's status and wait type and the sleep's returns.  Everything
   else is exc3.c's, whose notes follow.
   exc3.c - crash catcher 3 (2026-10-27, task #71; psp/exc3.S): psp/v26me4_exc.c (catcher 2) with the record also put in crash.txt.
   Why: after catcher 2 had drawn the record of gl2d's crash, Ffm's PSP switched itself off (Ffm 2026-10-27 21:10, "it went off by
   itself after taking the picture"), so a crash nobody watches looks like a switch-off without a record; the codec-call stops of v24,
   gl1 and gl3 (psp_harness.md "Build gl3") may be such crashes.  Catcher 2 kept interrupts off after the record, so no thread could write
   a file, and the PSP's own system stopped too (what then switches it off is not known [I]).
   mgs2.cfg MGS2_EXC=3: the handler (psp/exc3.S) draws the record as catcher 2 does, with interrupts off, and then, when the thread that
   took the exception was in user mode with interrupts on and not in an interrupt handler, turns interrupts back on and spins in kernel
   mode at its kernel address.  The other threads then run again: the writer thread (priority 0x10, above the harness's threads; it looks
   every 0.2 s) puts the record in crash.txt, finds the stopped thread (the one whose stack holds the record's k0: a user thread's k0
   points into its own stack block), puts the run's unwritten log lines (when it is the main thread) and the record in log.txt,
   suspends that thread (so it no longer spins: the other threads, HOME's dialog and the battery log run as when the game waits), writes
   a battery.txt line, keeps the system from auto sleep for 10 minutes (scePowerTick every 10 s, as the run does every window) and adds
   two green lines under the record.  So battery.txt then shows whether, and when, the PSP still switches off.  A crash in kernel mode, or
   with interrupts off, keeps catcher 2's behaviour (the screen only).  The writer uses system calls only (no libc: the stopped thread may
   hold its locks).  MGS2_EXC=1: catcher 2 as in gl3 (no writer thread).  The self-test (X at the start) is the same misaligned load;
   with 3 it must end with the record and the green lines, the PSP on, and HOME back to the system.
   The emulator does not run the kernel call (the catcher stays off there); MGS2_EXC_DRY=<vb> puts a made-up record on the screen and in
   log.txt at that vblank and hands it to the writer thread, which puts it in crash.txt (the emulator check of the record's code and of
   the writer); the run goes on.  Registered in kernel mode as catcher 2: sctrlHENFindFunction + KUBridge (PS2RC's ps2rc_kcall), the
   handler at its kernel address; given back before every exit (psp/gl1_glue.c's sceKernelExitGame wrap, tt_port lessons A10).  Nothing
   here writes flash or a hardware register.  The record (psp/exc3_test.sh previews it on the PC): the Media Engine rows of catcher 2 are
   left out (the gl builds have no Media Engine); the second row has the vblank, control registers 13, 15 and 16 and the interrupt
   enable. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <psppower.h>
#include <systemctrl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ps2rc_kcall.h"

typedef struct { uint32_t r[32], status, lo, hi, badvaddr, cause, epc, k_epc, k_status, k_cause, k15, k16, k13, ic; } ExcRegs;   /* the .S's layout */
ExcRegs mgs2_exc_regs;
volatile uint32_t mgs2_exc_taken, mgs2_exc_ints;
volatile uint32_t mgs2_exc_sleep[3];                 /* psp/exc3b.S: the system's sceKernelSleepThread; returns from it; last result */
void mgs2_exc_handler(void);
void mgs2_exc_test_pc(void);                          /* the self-test's load (a label in x_selftest) */
void mgs2_exc_screen(void);
extern volatile long me_mix_last_vb;                  /* psp/gl1_glue.c: the latest vblank */
int me_bat_line(const char *why);                     /* psp/v26me_bat.c */
int me_bat_hold(void);                                /* psp/v26me_bat.c (-DME_EXC_HOLD): its file's semaphore */
void me_bat_unhold(void);
void mgs2_log_crash(const char *b, int n, int pending);   /* psp/main_psp.c */
extern char ovl_arena[];                              /* main_psp.c: the stage overlays' code (OVL_SIZE) */
extern char __executable_start[], _fdata[];           /* the EBOOT's code and read-only data */
#ifdef MGS2_GL_EXC
/* the game's picture may be on the display instead of the debug screen's text buffer; psp/xl_psp.c gives the frame buffer on the
   display (0: the text is), and the record is written there */
uint32_t xl_psp_gl_screen(void);
#endif

#define X_POLL_US 200000                              /* the writer thread looks every 0.2 s (SYS_DIAG's diag_low wakes every 10 ms) */
#define X_TICK_POLLS 50                               /* after a crash: scePowerTick every 10 s ... */
#define X_TICKS 60                                    /* ... for 10 minutes */

static int x_on, x_mode;                              /* the handler is registered; MGS2_EXC */
static uint32_t x_fn_rel;                             /* sceKernelReleaseDefaultExceptionHandler */
static long x_dry = -1;
static char x_crash[256];
static char x_txt[4096];
static volatile uint32_t x_word[2], x_sink;
static volatile uint32_t x_seq, x_done;               /* records handed to the writer thread / written by it */
static volatile int x_len, x_real;                    /* the record's length; 1: a crash (0: the dry run) */
static SceUID x_main = -1;                            /* the main thread (the install runs in it) */
static uint32_t x_main_lo, x_main_hi;                 /* its stack block */

static char *put(char *p, const char *s) { while (*s) *p++ = *s++; return p; }
static char *hex(char *p, uint32_t u) { int i; for (i = 28; i >= 0; i -= 4) *p++ = "0123456789abcdef"[(u >> i) & 15u]; return p; }
static char *unum(char *p, unsigned long u) { char t[16]; int n = 0; do t[n++] = '0' + u % 10; while (u /= 10); while (n) *p++ = t[--n]; return p; }
static char *num(char *p, long v) { if (v < 0) *p++ = '-'; return unum(p, v < 0 ? -(unsigned long)v : (unsigned long)v); }
static int in_code(uint32_t a)
{
  return (a >= (uint32_t)__executable_start && a + 32 <= (uint32_t)_fdata) ||
         (a >= (uint32_t)ovl_arena && a + 32 <= (uint32_t)ovl_arena + OVL_SIZE);
}
static int in_ram(uint32_t a, uint32_t n) { return a >= 0x08800000u && a + n <= 0x0c000000u; }   /* user memory (52 MB: MEMSIZE=1) */
static const char *code_name(uint32_t cause)
{
  static const char *const cd[16] = { "interrupt", "TLB modification", "TLB load or fetch", "TLB store", "address error on load or fetch",
    "address error on store", "bus error on fetch", "bus error on data", "syscall", "breakpoint", "reserved instruction",
    "coprocessor unusable", "overflow", "trap", "code 14", "FPU" };
  uint32_t c = (cause >> 2) & 31;
  return c < 16 ? cd[c] : "code 16-31";
}
static char *code_at(char *p, uint32_t a, const char *label)   /* one row: the 6 words from a-12 to a+8 */
{
  int i; a &= ~3u;
  if (a < 12 || !in_code(a - 12)) return p;
  p = put(p, label);
  for (i = -3; i < 3; i++) { *p++ = ' '; p = hex(p, *(volatile uint32_t *)(a + 4 * i)); }
  return put(p, "\n");
}
static char *decoded(char *p, uint32_t cause, uint32_t st)   /* one row: the cause's name, the delay slot bit, the mode before it */
{
  p = put(p, "    "); p = put(p, code_name(cause)); if (cause & 0x80000000u) p = put(p, ", delay slot");
  p = put(p, (st & 0x18) ? ", user" : ", kernel"); return put(p, (st & 1) ? ", ints on\n" : ", ints off\n");
}

/* the record into x_txt, one screen: the debug screen has 68 columns and 34 rows and wraps its 35th row to the top, over the title, so
   every row here stays under 68 characters and the record with the tail and the writer's two lines takes 30 rows at most
   (psp/exc3_test.sh prints it as the screen shows it, at the widest values).  What, the vblank, control registers 13 / 15 / 16 and the
   interrupt enable, both readings of the cause / EPC / Status, the registers, the code around both EPCs, 42 stack words, the code
   ranges.  No system call and no libc: it runs in the exception */
static int x_report(const ExcRegs *r, const char *what)
{
  static const char *const nm[34] = { "zr", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra", "lo", "hi" };
  char *p = x_txt; uint32_t sp = r->r[29] & ~3u; int i;
  p = put(p, what); p = put(p, "\n vb "); p = num(p, me_mix_last_vb);
  p = put(p, ", c13 "); p = hex(p, r->k13); p = put(p, ", c15 "); p = hex(p, r->k15); p = put(p, ", c16 "); p = hex(p, r->k16);
  p = put(p, ", ic "); p = unum(p, r->ic);
  p = put(p, "\n COP0   EPC "); p = hex(p, r->epc); p = put(p, " Cause "); p = hex(p, r->cause); p = put(p, " Status "); p = hex(p, r->status);
  p = put(p, " Bad "); p = hex(p, r->badvaddr); *p++ = '\n'; p = decoded(p, r->cause, r->status);
  p = put(p, " kernel EPC "); p = hex(p, r->k_epc); p = put(p, " Cause "); p = hex(p, r->k_cause); p = put(p, " Status "); p = hex(p, r->k_status);
  *p++ = '\n'; p = decoded(p, r->k_cause, r->k_status);
  for (i = 0; i < 34; i++) {
    p = put(p, i % 5 ? "  " : " "); p = put(p, nm[i]); *p++ = ' '; p = hex(p, i < 32 ? r->r[i] : i == 32 ? r->lo : r->hi);
    if (i % 5 == 4 || i == 33) *p++ = '\n';
  }
  p = code_at(p, r->epc, " EPC-12:");
  if ((r->k_epc & ~3u) != (r->epc & ~3u)) p = code_at(p, r->k_epc, " kEPC-12:");
  if (in_ram(sp, 42 * 4)) for (i = 0; i < 42; i++) {
    if (i % 6 == 0) { p = put(p, " sp+"); *p++ = "0123456789abcdef"[(i * 4) >> 4]; *p++ = "0123456789abcdef"[(i * 4) & 15]; }
    *p++ = ' '; p = hex(p, *(volatile uint32_t *)(sp + 4 * i)); if (i % 6 == 5) *p++ = '\n';
  }
  p = put(p, " EBOOT code "); p = hex(p, (uint32_t)__executable_start); *p++ = '-'; p = hex(p, (uint32_t)_fdata); p = put(p, ", overlays ");
  p = hex(p, (uint32_t)ovl_arena); *p++ = '-'; p = hex(p, (uint32_t)ovl_arena + OVL_SIZE); *p++ = '\n';
  return (int)(p - x_txt);
}
static void x_screen(const char *b, int n)   /* the debug screen's own VRAM writes: no system call */
{
  pspDebugScreenSetBackColor(0xff000000); pspDebugScreenClear(); pspDebugScreenSetTextColor(0xff4040ff);
  pspDebugScreenSetXY(0, 0); pspDebugScreenPrintData(b, n); pspDebugScreenSetTextColor(0xffffffff);
}
static void x_append(const char *path, const char *b, int n)
{
  SceUID f;
  if (!path[0]) return;
  if ((f = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777)) >= 0) { sceIoWrite(f, b, n); sceIoClose(f); }
}
static void x_note(const char *s) { x_append(x_crash, s, (int)strlen(s)); fputs(s, stderr); }

/* from the handler (psp/exc3b.S), in the exception, on the catcher's stack, interrupts off; its answer in mgs2_exc_ints: 1 = the thread
   goes back to the system and sleeps there, interrupts back on (MGS2_EXC=3, the system's sleep found at the start, the thread was in user
   mode with interrupts on: Status KSU and IE, the interrupt enable, and not in an interrupt handler: control register 13) */
void mgs2_exc_screen(void)
{
  static const char tail1[] = "Please photograph this screen, then hold the power switch up until\nthe PSP switches off (if it stays on, take the battery out).";
  static const char tail3[] = "Interrupts are back on: the record also goes to crash.txt (green\nlines below say when). Please photograph this screen, then press\n"
                              "HOME to quit; if nothing happens in 10 s, hold the power switch up.";
  static const char off3[] = "Interrupts stay off at this crash, so no file is written.\n";
  const ExcRegs *r = &mgs2_exc_regs; uint32_t t = (uint32_t)mgs2_exc_test_pc; int n, ints;
  ints = x_mode == 3 && mgs2_exc_sleep[0] && (r->status & 0x18) && (r->status & 1) && r->ic && !r->k13;
  if (r->epc == t || r->k_epc == t) {
    char h[80], *p = h;
    p = put(p, "SELF-TEST: the crash catcher works (expected EPC "); p = hex(p, t); p = put(p, ")"); *p = 0;
    n = x_report(r, h);
  } else n = x_report(r, "CRASH: an exception reached the crash catcher");
#ifdef MGS2_GL_EXC
  { uint32_t b = xl_psp_gl_screen(); if (b) pspDebugScreenSetBase((u32 *)b); }   /* (a plain store: the debug screen's base) */
#endif
  x_screen(x_txt, n);
  if (ints) pspDebugScreenPrintData(tail3, sizeof tail3 - 1);
  else { if (x_mode == 3) pspDebugScreenPrintData(off3, sizeof off3 - 1); pspDebugScreenPrintData(tail1, sizeof tail1 - 1); }
  if (ints) { x_len = n; x_real = 1; x_seq++; }       /* for the writer thread, which runs once interrupts are on */
  mgs2_exc_ints = ints;
}

/* the writer thread's part: the thread whose stack holds the record's k0 (-1: none); its name and priority */
static SceUID x_stopped(char *nm, int *prio, int *st, int *wt)
{
  static SceUID ids[64]; int n = 0, i, j; uint32_t k0 = mgs2_exc_regs.r[26];
  if (sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, ids, 64, &n) < 0) return -1;
  if (n > 64) n = 64;
  for (i = 0; i < n; i++) {
    SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti;
    if (sceKernelReferThreadStatus(ids[i], &ti) < 0) continue;
    if (k0 >= (uint32_t)ti.stack && k0 < (uint32_t)ti.stack + (uint32_t)ti.stackSize) {
      for (j = 0; j < 31 && ti.name[j]; j++) nm[j] = ti.name[j];
      nm[j] = 0; *prio = ti.currentPriority; *st = ti.status; *wt = ti.waitType; return ids[i];
    }
  }
  return -1;
}
static void x_green(const char *b, int n) { pspDebugScreenSetTextColor(0xff40ff40); pspDebugScreenPrintData(b, n); pspDebugScreenSetTextColor(0xffffffff); }
/* the record (handed over by mgs2_exc_screen or the dry run) to crash.txt; after a crash also the stopped thread, log.txt, its
   suspension, battery.txt and the screen's green lines.  System calls only */
static void x_save(void)
{
  static const char run1[] = "\nThe system runs again: the writer thread is saving the record.";
  char l[400], *p = l, nm[32]; SceUID f, th = -1; int pr = 0, st = 0, wt = 0, k = 1, real = x_real, n = x_len;
  uint32_t k0 = mgs2_exc_regs.r[26]; uint64_t t;
  if (real && k0 >= x_main_lo && k0 < x_main_hi) x_green(run1, sizeof run1 - 1);   /* first, before any system call (the photo) */
  t = (uint64_t)sceKernelGetSystemTimeWide() / 1000000;
  nm[0] = 0;
  if (real) th = x_stopped(nm, &pr, &st, &wt);
  p = put(p, real ? "crash catcher 3b: the record above reached this file " : "crash catcher 3b: the dry run's record above reached this file ");
  p = unum(p, (unsigned long)t); p = put(p, " s after power-on (vb "); p = num(p, me_mix_last_vb); *p++ = ')';
  if (real) {
    if (th >= 0) {
      p = put(p, "; the stopped thread "); p = put(p, nm); p = put(p, " (priority "); p = num(p, pr); p = put(p, ", status "); p = num(p, st);
      p = put(p, ", wait type "); p = num(p, wt); *p++ = ')';
    } else p = put(p, "; no thread's stack holds its k0");
    p = put(p, "; returns from the system's sleep "); p = unum(p, mgs2_exc_sleep[1]);
    if (mgs2_exc_sleep[1]) { p = put(p, " (the last "); p = hex(p, mgs2_exc_sleep[2]); *p++ = ')'; }
  }
  *p++ = '\n';
  if ((f = sceIoOpen(x_crash, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777)) >= 0) { sceIoWrite(f, x_txt, n); sceIoWrite(f, l, p - l); sceIoClose(f); }
  if (!real) return;
  mgs2_log_crash(x_txt, n, th >= 0 && th == x_main);  /* the run's unwritten lines only when the main thread (their writer) stopped */
  if (th >= 0 && th != sceKernelGetThreadId()) k = sceKernelSuspendThread(th);
  p = l; p = put(p, "crash catcher 3b: log.txt has the record"); if (th == x_main) p = put(p, " after the run's unwritten lines");
  p = put(p, "; the stopped thread "); p = put(p, th < 0 ? "not suspended (not found)" : k < 0 ? "not suspended, error " : "suspended");
  if (th >= 0 && k < 0) p = hex(p, (uint32_t)k);
  p = put(p, "; no auto sleep for 10 minutes; battery.txt goes on\n");
  x_append(x_crash, l, p - l);
  if (th == x_main) {                                 /* the screen still shows the record: the main thread drew the frames */
    p = l; p = put(p, "\nSaved to crash.txt and log.txt "); p = unum(p, (unsigned long)t); p = put(p, " s after power-on.\n");
    p = put(p, mgs2_exc_sleep[1] ? "The stopped thread woke from sleep" : "The stopped thread sleeps");
    p = put(p, k >= 0 ? " and is suspended; HOME quits." : "; HOME quits.");
    x_green(l, p - l);
  }
  me_bat_line("crash record saved");                  /* last: it waits for battery.txt's semaphore, which a stopped thread may hold */
}
static int x_writer(SceSize a, void *b)
{
  int polls = 0, ticks = 0; (void)a; (void)b;
  for (;;) {
    sceKernelDelayThread(X_POLL_US);
    if (x_seq != x_done) { uint32_t s = x_seq; x_save(); x_done = s; if (x_real) { ticks = X_TICKS; polls = 0; } }
    if (ticks && ++polls >= X_TICK_POLLS) { polls = 0; ticks--; scePowerTick(PSP_POWER_TICK_ALL); }
  }
  return 0;
}

/* the dry run (MGS2_EXC_DRY=<vb>, from psp/gl1_glue.c's vblank wrap in the game thread): a made-up record on the screen and in log.txt,
   handed to the writer thread for crash.txt; the run goes on */
void me_exc_vb(long vb)
{
  ExcRegs f; int i, n;
  if (vb != x_dry) return;
  memset(&f, 0, sizeof f);
  for (i = 0; i < 32; i++) f.r[i] = 0x01010101u * (uint32_t)i;
  f.r[29] = (uint32_t)__builtin_frame_address(0); f.r[31] = (uint32_t)__builtin_return_address(0);
  f.status = 0x60000013u; f.cause = 4u << 2; f.epc = (uint32_t)me_exc_vb + 16; f.badvaddr = 2;
  f.k_status = 0x60000013u; f.k_cause = 4u << 2; f.k_epc = (uint32_t)me_exc_vb + 32; f.k15 = 0x15151515u; f.k16 = 0x16161616u; f.ic = 1;
  n = x_report(&f, "dry run: a made-up record, no exception; the run goes on");
  x_screen(x_txt, n); fwrite(x_txt, 1, n, stderr); me_bat_line("dry run");
  x_len = n; x_real = 0; x_seq++;
}

static uint32_t x_find(uint32_t nid660, uint32_t nid150)   /* 6.60 firmware NIDs, then 1.50's (tt_port's psp_bench / psp_game) */
{
  uint32_t f = sctrlHENFindFunction("sceExceptionManager", "ExceptionManagerForKernel", nid660);
  return f ? f : sctrlHENFindFunction("sceExceptionManager", "ExceptionManagerForKernel", nid150);
}
static uint32_t x_find_sleep(void)   /* the system's sceKernelSleepThread: ThreadManForUser's NID is the same on every firmware */
{
  uint32_t f = sctrlHENFindFunction("sceThreadManager", "ThreadManForUser", 0x9ACE131Eu);
  return (f & 0xF0000000u) == 0x80000000u ? f : 0;   /* a kernel address (psp/exc3b.S calls it in kernel mode), or none */
}
static int x_call(uint32_t fn, uint32_t *ret)
{
  Ps2rcKCall a; int r;
  memset(&a, 0, sizeof a); a.arg[0] = (uint32_t)mgs2_exc_handler | 0x80000000u;   /* the handler at its kernel address */
  r = ps2rc_kcall((void *)fn, &a); *ret = a.ret[0];
  return r < 0 ? r : (int)a.ret[0];
}
void me_exc_release(void)
{
  uint32_t ret;
  if (x_on) { x_on = 0; x_call(x_fn_rel, &ret); }
}
static __attribute__((noinline, noclone)) void x_selftest(void)
{
  uint32_t v;
  __asm__ volatile(".global mgs2_exc_test_pc\nmgs2_exc_test_pc:\n\tlw %0, 2(%1)" : "=r"(v) : "r"(x_word) : "memory");   /* misaligned: Cause code 4 */
  x_sink = v;
}
/* X or O, after any button held from the start is let go; 0 after 15 s */
static int x_ask(void)
{
  SceCtrlData pad; uint64_t t0 = (uint64_t)sceKernelGetSystemTimeWide(); int up = 0;
  while ((uint64_t)sceKernelGetSystemTimeWide() - t0 < 15000000u) {
    sceCtrlReadBufferPositive(&pad, 1);
    if (!(pad.Buttons & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE))) up = 1;
    else if (up) return (pad.Buttons & PSP_CTRL_CROSS) ? 'X' : 'O';
    sceKernelDelayThread(20000);
  }
  return 0;
}
static int is_emulator(void) { uint32_t out = 0; return sceIoDevctl("kemulator:", 0x00000003, NULL, 0, &out, sizeof out) >= 0; }
void me_exc_install(const char *dir, int n)
{
  const char *e = getenv("MGS2_EXC"), *d = getenv("MGS2_EXC_DRY"); uint32_t fn, ret; int r, k; SceUID f, th;
  char l[240];
  x_mode = e ? atoi(e) : 0; x_main = sceKernelGetThreadId();
  {
    SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti;   /* the main thread's stack block (x_save's first line) */
    if (sceKernelReferThreadStatus(x_main, &ti) >= 0) { x_main_lo = (uint32_t)ti.stack; x_main_hi = x_main_lo + (uint32_t)ti.stackSize; }
  }
  snprintf(x_crash, sizeof x_crash, "%.*scrash.txt", n, dir);
  if ((f = sceIoOpen(x_crash, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777)) >= 0) sceIoClose(f);
  if (d) x_dry = atol(d);
  if (x_dry >= 0) fprintf(stderr, "crash catcher: dry run of the record at vb %ld (MGS2_EXC_DRY)\n", x_dry);
  if (x_mode == 3 || x_dry >= 0) {                    /* the writer thread: before the emulator's return below (the dry run uses it) */
    th = sceKernelCreateThread("exc_writer", x_writer, 0x10, 0x2000, 0, 0);
    if (th >= 0) sceKernelStartThread(th, 0, 0);
    snprintf(l, sizeof l, "crash catcher 3b: writer thread %s (%08x, priority 0x10, a look every %d ms)\n", th >= 0 ? "started" : "NOT started",
             (unsigned)th, X_POLL_US / 1000); x_note(l);
  }
  if (x_mode <= 0) { x_note("crash catcher: off (no MGS2_EXC line in mgs2.cfg)\n"); return; }
  if (is_emulator()) { x_note("crash catcher: off (emulator: PPSSPP does not run the kernel call)\n"); return; }
  fn = x_find(0x0A998599u, 0x565C0B0Eu); x_fn_rel = x_find(0x409DBB53u, 0x291FF031u);   /* sceKernel(Register|Release)DefaultExceptionHandler */
  if (!fn || !x_fn_rel) {
    snprintf(l, sizeof l, "crash catcher: not installed (functions not found: %08x %08x)\n", (unsigned)fn, (unsigned)x_fn_rel); x_note(l); return;
  }
  if (x_mode == 3) mgs2_exc_sleep[0] = x_find_sleep();   /* before the handler is on: a crash then always sees it */
  if ((r = x_call(fn, &ret)) != 0) {
    snprintf(l, sizeof l, "crash catcher: not installed (kernel call %08x, result %08x)\n", (unsigned)r, (unsigned)ret); x_note(l); return;
  }
  x_on = 1;
  if (x_mode == 3 && mgs2_exc_sleep[0])
    snprintf(l, sizeof l, "crash catcher 3b: on (handler %08x, the system's sleep %08x; a crash leaves its record on the screen, the thread "
             "sleeps inside the system and the writer thread puts the record in crash.txt)\n", (unsigned)mgs2_exc_handler, (unsigned)mgs2_exc_sleep[0]);
  else if (x_mode == 3)
    snprintf(l, sizeof l, "crash catcher 3b: on (handler %08x), but the system's sleep was not found: a crash leaves its record on the screen "
             "only, as catcher 2\n", (unsigned)mgs2_exc_handler);
  else snprintf(l, sizeof l, "crash catcher: on (handler %08x; a crash leaves its record on the screen, no file)\n", (unsigned)mgs2_exc_handler);
  x_note(l);
  if (x_mode == 3)
    pspDebugScreenPrintf("\nCrash catcher test: press X to run it now. The PSP should then show a red record titled SELF-TEST and, "
                         "under it, green lines saying it was saved. Leave it on for 2 minutes, then press HOME to quit (if nothing "
                         "happens in 10 s, hold the power switch up).\nPress O, or wait 15 s, to start the run without the test.\n");
  else
    pspDebugScreenPrintf("\nCrash catcher test: press X to run it now. The PSP should then show a red record titled SELF-TEST and stay on: "
                         "photograph it, then hold the power switch up to switch off.\nPress O, or wait 15 s, to start the run without the test.\n");
  if ((k = x_ask()) == 'X') {
    int held = 0;
    x_note(x_mode == 3 ? "crash catcher 3b: self-test started (a misaligned load in the main thread; the record, then the writer thread's lines)\n"
                       : "crash catcher: self-test started (a misaligned load in the main thread; the PSP should show its record and stay on)\n");
    if (x_mode != 3) held = me_bat_hold();            /* catcher 2: no battery.txt write under way when the PSP stops in the test */
    x_selftest();                                     /* (catcher 3: the battery thread must go on, so the file is not held) */
    if (held) me_bat_unhold();
    x_note("crash catcher: self-test load went through with no exception (not expected); the run goes on\n");   /* (an unaligned load allowed) */
    return;
  }
  x_note(k == 'O' ? "crash catcher: self-test skipped (O); the run starts\n" : "crash catcher: self-test skipped (no answer in 15 s); the run starts\n");
  pspDebugScreenPrintf("run starting\n");
}
