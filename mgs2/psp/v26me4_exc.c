/* v26me4_exc.c - v26me4 (2026-10-24): the crash catcher, second form.  v26me3's (psp/v26me_exc.c) returned from the exception into a
   trap that made system calls (thread name, time, files); its self-test switched Ffm's PSP off before the game started (psp_harness.md
   "Build v26me3", run 1), so that route does not work on this PSP (6.60 / ARK-4).  Here the handler (psp/v26me4_exc.S) never returns:
   in the exception itself it stores the registers and calls mgs2_exc_screen, which writes the record on the screen through the debug
   screen's plain VRAM writes (no system call, no libc), and then spins.  The PSP stays on with the record until the power switch; no
   file is written at a crash (the memory stick needs interrupts and threads).  So a power-off with the catcher on and its self-test
   passed means no exception reached it (a power fault, or an exception the kernel kept for itself) [I].  Registered in kernel mode as
   v26me3's: sctrlHENFindFunction + KUBridge (PS2RC's ps2rc_kcall), the handler at its kernel address; given back before every exit
   (psp/me_mix.c's sceKernelExitGame wrap, tt_port lessons A10).
   mgs2.cfg MGS2_EXC: 1 = on; at the start the screen offers the self-test (X: a deliberate misaligned load in the main thread, which
   must end on the screen as a record titled SELF-TEST; O or 15 s without an answer: the run starts); 0 or absent = off.  The emulator
   does not run the kernel call (the catcher stays off there); MGS2_EXC_DRY=<vb> puts a made-up record on the screen and in crash.txt and
   log.txt at that vblank and goes on (the emulator check of the record's code).  Nothing here writes flash or a hardware register. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <systemctrl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ps2rc_kcall.h"

typedef struct { uint32_t r[32], status, lo, hi, badvaddr, cause, epc, k_epc, k_status, k_cause, k15, k16; } ExcRegs;   /* the .S's layout */
ExcRegs mgs2_exc_regs;
volatile uint32_t mgs2_exc_taken;
void mgs2_exc_handler(void);
void mgs2_exc_test_pc(void);                          /* the self-test's load (a label in x_selftest) */
void mgs2_exc_screen(void);
extern volatile long me_mix_last_vb;                  /* psp/me_mix.c */
void me_mix_state(uint32_t v[6]);
void me_mix_diag(uint32_t d[7]);
int me_bat_line(const char *why);                     /* psp/v26me_bat.c */
int me_bat_hold(void);                                /* psp/v26me_bat.c (-DME_EXC_HOLD): its file's semaphore */
void me_bat_unhold(void);
extern char ovl_arena[];                              /* main_psp.c: the stage overlays' code (OVL_SIZE) */
extern char __executable_start[], _fdata[];           /* the EBOOT's code and read-only data */
#ifdef MGS2_GL_EXC
/* MGS2T_gl1 (psp/gl1.sh, 2026-10-27): the game's picture may be on the display instead of the debug screen's text buffer; psp/xl_psp.c
   gives the frame buffer on the display (0: the text is), and the record is written there.  The Media Engine builds do not define it. */
uint32_t xl_psp_gl_screen(void);
#endif

static int x_on;                                      /* the handler is registered */
static uint32_t x_fn_rel;                             /* sceKernelReleaseDefaultExceptionHandler */
static long x_dry = -1;
static char x_crash[256];
static char x_txt[4096];
static volatile uint32_t x_word[2], x_sink;

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
   every row here stays under 68 characters (a full row of 68 would add an empty one) and the record with the tail takes 29 rows at most
   (psp/v26me4_test.c prints it as the screen shows it, at the widest values).  What, the sound mix's vblank, both readings of the
   cause / EPC / Status, the registers, the code around both EPCs, 42 stack words, the code ranges, the Media Engine's state.  No system
   call and no libc: it runs in the exception */
static int x_report(const ExcRegs *r, const char *what)
{
  static const char *const nm[34] = { "zr", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra", "lo", "hi" };
  char *p = x_txt; uint32_t sp = r->r[29] & ~3u, v[6], d[7]; int i;
  p = put(p, what); p = put(p, "\n sound mix vb "); p = num(p, me_mix_last_vb);
  p = put(p, ", c15 "); p = hex(p, r->k15); p = put(p, ", c16 "); p = hex(p, r->k16);
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
  me_mix_state(v); me_mix_diag(d);
  p = put(p, " ME mode "); p = unum(p, v[0]); p = put(p, " (1 on), heartbeat "); p = unum(p, v[1]); p = put(p, ", state "); p = unum(p, v[5]);
  p = put(p, "\n    pushed "); p = unum(p, v[2]); p = put(p, ", consumed "); p = unum(p, v[3]); p = put(p, ", checkpoints "); p = unum(p, v[4]);
  p = put(p, "\n    idle "); p = unum(p, d[0]); p = put(p, ", last cmd "); p = hex(p, d[1]); *p++ = ' '; p = hex(p, d[2]);
  p = put(p, ", ME EPC "); p = hex(p, d[3]); p = put(p, "\n    power switch "); p = unum(p, d[4]); p = put(p, ", parks "); p = unum(p, d[5]);
  p = put(p, ", flags "); p = hex(p, d[6]); *p++ = '\n';
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

/* from the handler (psp/v26me4_exc.S), in the exception, on the catcher's stack; it returns to the handler's spin */
void mgs2_exc_screen(void)
{
  static const char tail[] = "Please photograph this screen, then hold the power switch up until\nthe PSP switches off (if it stays on, take the battery out).";
  const ExcRegs *r = &mgs2_exc_regs; uint32_t t = (uint32_t)mgs2_exc_test_pc; int n;
  if (r->epc == t || r->k_epc == t) {
    char h[80], *p = h;
    p = put(p, "SELF-TEST: the crash catcher works (expected EPC "); p = hex(p, t); p = put(p, ")"); *p = 0;
    n = x_report(r, h);
  } else n = x_report(r, "CRASH: an exception reached the crash catcher");
#ifdef MGS2_GL_EXC
  { uint32_t b = xl_psp_gl_screen(); if (b) pspDebugScreenSetBase((u32 *)b); }   /* (a plain store: the debug screen's base) */
#endif
  x_screen(x_txt, n);
  pspDebugScreenPrintData(tail, sizeof tail - 1);
}

/* the dry run (MGS2_EXC_DRY=<vb>, from psp/me_mix.c's vblank wrap in the game thread): a made-up record on the screen and in crash.txt
   and log.txt, and the run goes on */
void me_exc_vb(long vb)
{
  ExcRegs f; int i, n;
  if (vb != x_dry) return;
  memset(&f, 0, sizeof f);
  for (i = 0; i < 32; i++) f.r[i] = 0x01010101u * (uint32_t)i;
  f.r[29] = (uint32_t)__builtin_frame_address(0); f.r[31] = (uint32_t)__builtin_return_address(0);
  f.status = 0x60000013u; f.cause = 4u << 2; f.epc = (uint32_t)me_exc_vb + 16; f.badvaddr = 2;
  f.k_status = 0x60000013u; f.k_cause = 4u << 2; f.k_epc = (uint32_t)me_exc_vb + 32; f.k15 = 0x15151515u; f.k16 = 0x16161616u;
  n = x_report(&f, "dry run: a made-up record, no exception; the run goes on");
  x_screen(x_txt, n); x_append(x_crash, x_txt, n); fwrite(x_txt, 1, n, stderr); me_bat_line("dry run");
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
  const char *e = getenv("MGS2_EXC"), *d = getenv("MGS2_EXC_DRY"); uint32_t fn, ret; int mode = e ? atoi(e) : 0, r, k; SceUID f;
  char l[240];
  snprintf(x_crash, sizeof x_crash, "%.*scrash.txt", n, dir);
  if ((f = sceIoOpen(x_crash, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777)) >= 0) sceIoClose(f);
  if (d) x_dry = atol(d);
  if (x_dry >= 0) fprintf(stderr, "crash catcher: dry run of the record at vb %ld (MGS2_EXC_DRY)\n", x_dry);
  if (mode <= 0) { x_note("crash catcher: off (no MGS2_EXC line in mgs2.cfg)\n"); return; }
  if (is_emulator()) { x_note("crash catcher: off (emulator: PPSSPP does not run the kernel call)\n"); return; }
  fn = x_find(0x0A998599u, 0x565C0B0Eu); x_fn_rel = x_find(0x409DBB53u, 0x291FF031u);   /* sceKernel(Register|Release)DefaultExceptionHandler */
  if (!fn || !x_fn_rel) {
    snprintf(l, sizeof l, "crash catcher: not installed (functions not found: %08x %08x)\n", (unsigned)fn, (unsigned)x_fn_rel); x_note(l); return;
  }
  if ((r = x_call(fn, &ret)) != 0) {
    snprintf(l, sizeof l, "crash catcher: not installed (kernel call %08x, result %08x)\n", (unsigned)r, (unsigned)ret); x_note(l); return;
  }
  x_on = 1;
  snprintf(l, sizeof l, "crash catcher: on (handler %08x; a crash leaves its record on the screen, no file)\n", (unsigned)mgs2_exc_handler); x_note(l);
  pspDebugScreenPrintf("\nCrash catcher test: press X to run it now. The PSP should then show a red record titled SELF-TEST and stay on: "
                       "photograph it, then hold the power switch up to switch off.\nPress O, or wait 15 s, to start the run without the test.\n");
  if ((k = x_ask()) == 'X') {
    int held;
    x_note("crash catcher: self-test started (a misaligned load in the main thread; the PSP should show its record and stay on)\n");
    held = me_bat_hold();                             /* no battery.txt write under way when the PSP stops in the test */
    x_selftest();
    if (held) me_bat_unhold();
    x_note("crash catcher: self-test load went through with no exception (not expected); the run goes on\n");   /* (an unaligned load allowed) */
    return;
  }
  x_note(k == 'O' ? "crash catcher: self-test skipped (O); the run starts\n" : "crash catcher: self-test skipped (no answer in 15 s); the run starts\n");
  pspDebugScreenPrintf("run starting\n");
}
