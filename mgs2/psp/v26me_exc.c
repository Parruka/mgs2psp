/* v26me_exc.c - v26me3 (2026-10-24): the crash catcher, after v26me2's PSP run ended in a power-off with nothing in its log (Ffm chose
   to add it, decision card 2026-10-24 14:41).  On this PSP a crash (an exception nobody handles) switched the console off (tt_port
   lessons A10: the counter read).  With a default exception handler of our own (psp/v26me_exc.S, registered in kernel mode through
   ARK-4's KUBridge as tt_port's psp_bench / psp_game did) the faulting thread goes on in mgs2_exc_trap instead: the record on the
   screen first (plain VRAM writes and no system call, safe in any context); then, when the thread had interrupts on (Status.IE), the
   full record (thread name, its stack) on the screen and in crash.txt next to the EBOOT, log.txt with the lines the harness still held
   (main_psp.c's static g_logbuf / g_logn / g_logpath, at fixed offsets from g_emuprof in main_psp.o's .bss: LOG_D* from
   psp/v26me3.sh, which checks them in the linked ELF), a battery.txt line, the Media Engine back in reset; then it waits for X (never
   an exit on its own).  Every exit gives the handler back to the kernel first (psp/me_mix.c's sceKernelExitGame wrap; tt_port lessons
   A10's order); HOME still works while the trap waits.  A later exception parks its thread and keeps the first record.
   mgs2.cfg MGS2_EXC: 1 = on, with a self-test at the start (a thread of its own makes a misaligned load, which must come back here: its
   record goes to crash.txt marked as the self-test and the run goes on; not caught: the handler is given back and the run goes on
   without it), 2 = on without the self-test, 0 or absent = off.  The emulator does not run the kernel call (the catcher stays off
   there); MGS2_EXC_DRY=<vb> writes a made-up record at that vblank from the game thread and goes on (the emulator check of the
   report's code).  Nothing here writes flash or a hardware register; the handler lives in RAM only.  No libc in the trap's report: it
   may run where libc's locks are held. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspsuspend.h>
#include <systemctrl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ps2rc_kcall.h"

typedef struct { uint32_t r[32], status, lo, hi, badvaddr, cause, epc; } ExcRegs;   /* psp/v26me_exc.S's layout */
ExcRegs mgs2_exc_regs;
volatile uint32_t mgs2_exc_taken;
void mgs2_exc_handler(void);
void mgs2_exc_trap(void);

extern volatile long me_mix_last_vb;                  /* psp/me_mix.c */
void me_mix_state(uint32_t v[6]);
void me_mix_diag(uint32_t d[7]);
void me_mix_halt(void);
int me_bat_line(const char *why);                     /* psp/v26me_bat.c */
extern volatile uint32_t g_emuprof;                   /* main_psp.c: a global in the same .bss as its log buffer */
extern char ovl_arena[];                              /* main_psp.c: the stage overlays' code (OVL_SIZE) */
extern char __executable_start[], _fdata[];           /* the EBOOT's code and read-only data */
/* g_emuprof's address made opaque to the compiler (the log's offsets lie outside g_emuprof itself) */
static char *log_base(void) { uintptr_t a = (uintptr_t)&g_emuprof; __asm__("" : "+r"(a)); return (char *)a; }
#define LOG_BUF (log_base() + LOG_DBUF)
#define LOG_N (*(volatile int *)(log_base() + LOG_DN))
#define LOG_PATH ((const char *)log_base() + LOG_DPATH)
#define LOG_SIZE (1 << 17)

static int x_on;                                      /* the handler is registered */
static uint32_t x_fn_rel;                             /* sceKernelReleaseDefaultExceptionHandler */
static volatile SceUID x_test_th = -1; static volatile int x_test_done;
static long x_dry = -1;
static uint64_t x_t0;
static char x_crash[256], x_tname[32];
static char x_txt[4096];

static char *put(char *p, const char *s) { while (*s) *p++ = *s++; return p; }
static char *hex(char *p, uint32_t u) { int i; for (i = 28; i >= 0; i -= 4) *p++ = "0123456789abcdef"[(u >> i) & 15u]; return p; }
static char *unum(char *p, unsigned long u) { char t[16]; int n = 0; do t[n++] = '0' + u % 10; while (u /= 10); while (n) *p++ = t[--n]; return p; }
static char *num(char *p, long v) { if (v < 0) *p++ = '-'; return unum(p, v < 0 ? -(unsigned long)v : (unsigned long)v); }
static int in_code(uint32_t a)
{
  return (a >= (uint32_t)__executable_start && a + 32 <= (uint32_t)_fdata) ||
         (a >= (uint32_t)ovl_arena && a + 32 <= (uint32_t)ovl_arena + OVL_SIZE);
}

/* the record into x_txt: what, when, the cause, the thread, the registers, the code at EPC, the stack, the Media Engine's state.
   io = 0: no system call at all (no time, no thread name or stack bounds) */
static int x_report(const ExcRegs *r, const char *what, int io)
{
  static const char *const nm[32] = { "zr", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra" };
  static const char *const cd[16] = { "interrupt", "TLB modification", "TLB load or fetch", "TLB store", "address error on load or fetch",
    "address error on store", "bus error on fetch", "bus error on data", "syscall", "breakpoint", "reserved instruction",
    "coprocessor unusable", "overflow", "trap", "code 14", "FPU" };
  char *p = x_txt; uint32_t c = (r->cause >> 2) & 31, e = r->epc & ~3u, sp = r->r[29] & ~3u, v[6], d[7], lo = 0, hi = 0; int i;
  SceUID id = io ? sceKernelGetThreadId() : -1;
  x_tname[0] = 0;
  if (io && id >= 0) {
    SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti;
    if (sceKernelReferThreadStatus(id, &ti) >= 0) { memcpy(x_tname, ti.name, 31); x_tname[31] = 0; lo = (uint32_t)ti.stack; hi = lo + (uint32_t)ti.stackSize; }
  }
  p = put(p, what);
  if (io) { p = put(p, " at t "); p = num(p, (long)(((uint64_t)sceKernelGetSystemTimeWide() - x_t0) / 1000000)); p = put(p, " s,"); }
  p = put(p, " vb "); p = num(p, me_mix_last_vb); p = put(p, ": "); p = put(p, c < 16 ? cd[c] : "code ?"); p = put(p, " (Cause ");
  p = hex(p, r->cause); p = put(p, ") in thread \""); p = put(p, io ? x_tname : "?"); p = put(p, "\" (id "); p = hex(p, (uint32_t)id);
  p = put(p, "), EPC "); p = hex(p, r->epc); p = put(p, ", BadVAddr "); p = hex(p, r->badvaddr); p = put(p, ", Status ");
  p = hex(p, r->status); p = put(p, (r->status & 1) ? " (interrupts on)\n" : " (interrupts off)\n");
  for (i = 0; i < 32; i++) { p = put(p, i & 3 ? "  " : " "); p = put(p, nm[i]); *p++ = ' '; p = hex(p, r->r[i]); if ((i & 3) == 3) *p++ = '\n'; }
  p = put(p, " lo "); p = hex(p, r->lo); p = put(p, "  hi "); p = hex(p, r->hi); *p++ = '\n';
  if (e >= 16 && in_code(e - 16)) { p = put(p, " code at EPC-16:"); for (i = -4; i < 4; i++) { *p++ = ' '; p = hex(p, *(volatile uint32_t *)(e + 4 * i)); } *p++ = '\n'; }
  if (lo && sp >= lo && sp + 96 <= hi) {
    p = put(p, " stack at SP (the thread's "); p = hex(p, lo); *p++ = '-'; p = hex(p, hi); p = put(p, "):");
    for (i = 0; i < 24; i++) { p = put(p, i & 7 ? " " : "\n  "); p = hex(p, *(volatile uint32_t *)(sp + 4 * i)); }
    *p++ = '\n';
  }
  p = put(p, " EBOOT code "); p = hex(p, (uint32_t)__executable_start); *p++ = '-'; p = hex(p, (uint32_t)_fdata); p = put(p, ", overlay arena ");
  p = hex(p, (uint32_t)ovl_arena); *p++ = '-'; p = hex(p, (uint32_t)ovl_arena + OVL_SIZE); *p++ = '\n';
  me_mix_state(v); me_mix_diag(d);
  p = put(p, " Media Engine: mode "); p = unum(p, v[0]); p = put(p, " (1 on), heartbeat "); p = unum(p, v[1]); p = put(p, ", state "); p = unum(p, v[5]);
  p = put(p, ", commands "); p = unum(p, v[2]); p = put(p, " pushed, "); p = unum(p, v[3]); p = put(p, " consumed, checkpoints "); p = unum(p, v[4]);
  p = put(p, ", idle polls "); p = unum(p, d[0]); p = put(p, ", last command "); p = hex(p, d[1]); *p++ = ' '; p = hex(p, d[2]);
  p = put(p, ", its exception EPC "); p = hex(p, d[3]); p = put(p, "; power switch "); p = unum(p, d[4]); p = put(p, ", parks "); p = unum(p, d[5]);
  p = put(p, ", last power flags "); p = hex(p, d[6]); p = put(p, "\n");
  return (int)(p - x_txt);
}

static void x_append(const char *path, const char *b, int n)
{
  SceUID f;
  if (!path[0]) return;
  if ((f = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777)) >= 0) { sceIoWrite(f, b, n); sceIoClose(f); }
}
static void x_note(const char *s) { x_append(x_crash, s, (int)strlen(s)); }
/* log.txt: first what the harness still held (its lines since the last window), then the record */
static void x_log(const char *b, int n)
{
  int k = LOG_N;
  if (k > 0 && k <= LOG_SIZE) { x_append(LOG_PATH, LOG_BUF, k); LOG_N = 0; }
  x_append(LOG_PATH, b, n);
}
static void x_screen(const char *b, int n)
{
  pspDebugScreenSetBackColor(0xff000000); pspDebugScreenClear(); pspDebugScreenSetTextColor(0xff4040ff);
  pspDebugScreenSetXY(0, 0); pspDebugScreenPrintData(b, n); pspDebugScreenSetTextColor(0xffffffff);
}

void mgs2_exc_trap(void)
{
  static const char tail[] = "\nPlease photograph this screen if you can; crash.txt and log.txt next to the EBOOT have it too. X: exit\n";
  static const char tail0[] = "\nInterrupts were off: nothing more can run. Please photograph this screen, then hold the power switch up to switch off.\n";
  const ExcRegs *r = &mgs2_exc_regs; int io = (r->status & 1) != 0, n;
  SceCtrlData pad;
  if (io && x_test_th >= 0 && sceKernelGetThreadId() == x_test_th) {   /* the self-test's load: write it down, arm again, end that thread */
    n = x_report(r, "self-test (a deliberate misaligned load in a thread of its own at the start; the run goes on)", 1);
    x_append(x_crash, x_txt, n);
    x_test_done = 1; mgs2_exc_taken = 0;
    sceKernelExitDeleteThread(0);
  }
  n = x_report(r, "CRASH", 0);                       /* the screen first, with no system call */
  x_screen(x_txt, n);
  if (!io) { pspDebugScreenPrintData(tail0, sizeof tail0 - 1); for (;;) { } }   /* nothing else is safe; the screen keeps it */
  n = x_report(r, "CRASH", 1);
  x_screen(x_txt, n);
  x_append(x_crash, x_txt, n);
  x_log(x_txt, n);
  if (strcmp(x_tname, "battery")) me_bat_line("CRASH");     /* (not from the battery thread itself: it may hold the file's lock) */
  me_mix_halt();
  pspDebugScreenPrintData(tail, sizeof tail - 1);
  for (;;) { sceCtrlReadBufferPositive(&pad, 1); if (pad.Buttons & PSP_CTRL_CROSS) break; sceKernelDelayThread(50000); }
  sceKernelVolatileMemUnlock(0);                            /* as main_psp.c's exit (an error if it was not held) */
  sceKernelExitGame();                                      /* psp/me_mix.c's wrap: the handler back first */
  for (;;) sceKernelSleepThread();
}

/* the dry run (MGS2_EXC_DRY=<vb>, from psp/me_mix.c's vblank wrap in the game thread): a made-up record through the same report,
   screen and files, and the run goes on */
void me_exc_vb(long vb)
{
  ExcRegs f; int i, n;
  if (vb != x_dry) return;
  memset(&f, 0, sizeof f);
  for (i = 0; i < 32; i++) f.r[i] = 0x01010101u * (uint32_t)i;
  f.r[29] = (uint32_t)__builtin_frame_address(0); f.r[31] = (uint32_t)__builtin_return_address(0);
  f.status = 0x60000013u; f.cause = 4u << 2; f.epc = (uint32_t)me_exc_vb + 16; f.badvaddr = 2;
  n = x_report(&f, "dry run (MGS2_EXC_DRY: a made-up record, no exception)", 0);
  x_screen(x_txt, n);
  n = x_report(&f, "dry run (MGS2_EXC_DRY: a made-up record, no exception; the run goes on)", 1);
  x_screen(x_txt, n); x_append(x_crash, x_txt, n); x_log(x_txt, n); me_bat_line("dry run");
}

static uint32_t x_find(uint32_t nid660, uint32_t nid150)   /* 6.60 firmware NIDs, then 1.50's (tt_port's psp_bench / psp_game) */
{
  uint32_t f = sctrlHENFindFunction("sceExceptionManager", "ExceptionManagerForKernel", nid660);
  return f ? f : sctrlHENFindFunction("sceExceptionManager", "ExceptionManagerForKernel", nid150);
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

static volatile uint32_t x_word[2], x_sink;
static int x_test(SceSize n, void *a)
{
  uint32_t v; (void)n; (void)a;
  __asm__ volatile("lw %0, 2(%1)" : "=r"(v) : "r"(x_word) : "memory");   /* misaligned: an address error on load (Cause code 4) */
  x_sink = v;
  return 0;                                                               /* not reached while the catcher works */
}

static int is_emulator(void) { uint32_t out = 0; return sceIoDevctl("kemulator:", 0x00000003, NULL, 0, &out, sizeof out) >= 0; }

void me_exc_install(const char *dir, int n)
{
  const char *e = getenv("MGS2_EXC"), *d = getenv("MGS2_EXC_DRY"); uint32_t fn, ret; int mode = e ? atoi(e) : 0, r; SceUID f;
  char l[200];
  x_t0 = (uint64_t)sceKernelGetSystemTimeWide();
  snprintf(x_crash, sizeof x_crash, "%.*scrash.txt", n, dir);
  if ((f = sceIoOpen(x_crash, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777)) >= 0) sceIoClose(f);
  if (d) x_dry = atol(d);
  if (x_dry >= 0) fprintf(stderr, "crash catcher: dry run of the report at vb %ld (MGS2_EXC_DRY)\n", x_dry);
  if (mode <= 0) { fprintf(stderr, "crash catcher: off (no MGS2_EXC line in mgs2.cfg)\n"); x_note("crash catcher: off\n"); return; }
  if (is_emulator()) { fprintf(stderr, "crash catcher: off (emulator: PPSSPP does not run the kernel call)\n"); x_note("crash catcher: off (emulator)\n"); return; }
  fn = x_find(0x0A998599u, 0x565C0B0Eu); x_fn_rel = x_find(0x409DBB53u, 0x291FF031u);   /* sceKernel(Register|Release)DefaultExceptionHandler */
  if (!fn || !x_fn_rel) {
    snprintf(l, sizeof l, "crash catcher: not installed (functions not found: %08x %08x)\n", (unsigned)fn, (unsigned)x_fn_rel);
    fputs(l, stderr); x_note(l); return;
  }
  if ((r = x_call(fn, &ret)) != 0) {
    snprintf(l, sizeof l, "crash catcher: not installed (kernel call %08x, result %08x)\n", (unsigned)r, (unsigned)ret);
    fputs(l, stderr); x_note(l); return;
  }
  x_on = 1;
  if (mode != 1) {
    snprintf(l, sizeof l, "crash catcher: on, no self-test (handler %08x)\n", (unsigned)mgs2_exc_handler); fputs(l, stderr); x_note(l); return;
  }
  x_test_done = 0;
  x_note("crash catcher: on; self-test starting (if the PSP switches off now, set MGS2_EXC=0 in mgs2.cfg)\n");
  if ((x_test_th = sceKernelCreateThread("exc_test", x_test, 0x30, 0x1000, THREAD_ATTR_USER, 0)) < 0) {
    snprintf(l, sizeof l, "crash catcher: on, self-test thread not created (%08x)\n", (unsigned)x_test_th); fputs(l, stderr); x_note(l);
    x_test_th = -1; return;
  }
  { SceUInt to = 2000000; sceKernelStartThread(x_test_th, 0, 0); r = sceKernelWaitThreadEnd(x_test_th, &to); }
  if (x_test_done) {
    const ExcRegs *g = &mgs2_exc_regs;
    snprintf(l, sizeof l, "crash catcher: on; self-test caught (Cause %08x, EPC %08x, BadVAddr %08x%s; crash.txt) and the run goes on\n",
             (unsigned)g->cause, (unsigned)g->epc, (unsigned)g->badvaddr,
             ((g->cause >> 2) & 31) == 4 && g->badvaddr == (uint32_t)x_word + 2 ? ": as expected" : ": NOT the expected load");
  } else {
    snprintf(l, sizeof l, "crash catcher: self-test NOT caught (wait %08x): the handler is given back, the run goes on without it\n", (unsigned)r);
    sceKernelTerminateDeleteThread(x_test_th); me_exc_release();
  }
  fputs(l, stderr); x_note(l);
  x_test_th = -1;
}
