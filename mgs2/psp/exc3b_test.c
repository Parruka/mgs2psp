/* exc3b_test.c (x86 dev check, 2026-10-28, task #73): psp/exc3_test.c for catcher 3b (psp/exc3b.c): the system's sleep set (and one
   case without it, which must keep interrupts off), the writer's first green line, the stopped thread's status and the sleep's
   returns in crash.txt.  Run with C=exc3b sh psp/exc3_test.sh.  exc3_test.c's notes follow.
   exc3_test.c (x86 dev check, 2026-10-27, task #71): crash catcher 3's record (psp/exc3.c: x_report, mgs2_exc_screen, and the writer
   thread's x_save with its green lines) as the PSP's debug screen shows it, as psp/v26me4_test.c does for catcher 2.  The screen is
   libpspdebug's pspDebugScreenPrintData: 68 columns, 34 rows; a 68th character or '\n' moves to the next row and clears it, and row 35
   wraps to row 0, over the title.  Records: the self-test with interrupts back on (and the writer's lines), a crash with every field at
   its widest in kernel mode (catcher 2's tail under MGS2_EXC=3: no file), the same under MGS2_EXC=1, and the dry run's.  Each also
   prints what the writer puts in crash.txt.  Fails (exit 1) when a record wraps, leaves a row of 68 characters or more, takes more than
   32 rows, or turns interrupts on when it must not.  Run with sh psp/exc3_test.sh. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

typedef int SceUID;
typedef unsigned int SceSize;
typedef uint32_t u32;
#define PSP_O_WRONLY 2
#define PSP_O_CREAT 0x200
#define PSP_O_APPEND 0x100
#define PSP_O_TRUNC 0x400
#define OVL_SIZE 0x460000
#define SCE_KERNEL_TMID_Thread 1
#define PSP_POWER_TICK_ALL 0
typedef struct { uint32_t low, hi; } SceKernelSysClock;
typedef struct { SceSize size; char name[32]; unsigned attr; int status; void *entry; void *stack; int stackSize; void *gpReg;
  int initPriority, currentPriority, waitType; SceUID waitId; int wakeupCount, exitStatus; SceKernelSysClock runClocks;
  unsigned intrPreemptCount, threadPreemptCount, releaseCount; } SceKernelThreadInfo;

static char file[16384]; static int filen;            /* what the writer appends to crash.txt */
static SceUID sceIoOpen(const char *p, int f, int m) { (void)p; (void)f; (void)m; return 3; }
static int sceIoWrite(SceUID f, const void *b, int n) { (void)f; if (filen + n < (int)sizeof file) { memcpy(file + filen, b, n); filen += n; file[filen] = 0; } return n; }
static int sceIoClose(SceUID f) { (void)f; return 0; }
static uint64_t sceKernelGetSystemTimeWide(void) { return 123456789012ull; }
static int sceKernelDelayThread(unsigned us) { (void)us; return 0; }
static int scePowerTick(int t) { (void)t; return 0; }
static SceUID sceKernelGetThreadId(void) { return 0x0499a7c3; }
static int suspended; static SceUID sceKernelSuspendThread(SceUID t) { suspended = t; return 0; }
static uint32_t th_stack = 0x0bd9c000, th_size = 0x224000;   /* user_main's block; the record's k0 0x0bfbff00 lies in it */
static int sceKernelGetThreadmanIdList(int k, SceUID *ids, int n, int *cnt) { (void)k; (void)n; ids[0] = 0x0480a2b1; ids[1] = 0x04a0c0d3; *cnt = 2; return 0; }
static int sceKernelReferThreadStatus(SceUID id, SceKernelThreadInfo *ti)
{
  if (id == 0x0480a2b1) { strcpy(ti->name, "battery"); ti->stack = (void *)(uintptr_t)0x0b200000; ti->stackSize = 0x2000; ti->currentPriority = 17; }
  else { strcpy(ti->name, "user_main"); ti->stack = (void *)(uintptr_t)th_stack; ti->stackSize = (int)th_size; ti->currentPriority = 32;
         ti->status = 12; ti->waitType = 1; }   /* waiting + suspended, asleep (the PSP's own numbers come back in crash.txt) */
  return 0;
}
static int logged; void mgs2_log_crash(const char *b, int n, int pending) { (void)b; logged = n | (pending << 30); }
static int batn; int me_bat_line(const char *why) { (void)why; batn++; return 0; }

static char scr[34][69]; static int X, Y, wraps, tabs, rowsmax;
static void clr(int y) { memset(scr[y], ' ', 68); scr[y][68] = 0; }
static void down(void) { X = 0; if (++Y == 34) { Y = 0; wraps++; } clr(Y); if (Y + 1 > rowsmax) rowsmax = Y + 1; }
static void pspDebugScreenSetBackColor(uint32_t c) { (void)c; }
static void pspDebugScreenSetTextColor(uint32_t c) { (void)c; }
static void pspDebugScreenClear(void) { int y; for (y = 0; y < 34; y++) clr(y); X = Y = 0; rowsmax = 1; }
static void pspDebugScreenSetXY(int x, int y) { if (x >= 0 && x < 68) X = x; if (y >= 0 && y < 34) Y = y; }
static void pspDebugScreenSetBase(u32 *b) { (void)b; }
static int pspDebugScreenPrintData(const char *b, int n)
{
  int i;
  for (i = 0; i < n; i++) {
    char c = b[i];
    if (c == '\n') down(); else if (c == '\r') X = 0; else if (c == '\t') tabs++;
    else { scr[Y][X] = c; if (++X == 68) down(); }
  }
  return n;
}

#define __executable_start t_exec
#define _fdata t_fdata
#define ovl_arena t_ovl
#include "exc_part.c"                                 /* psp/exc3.c up to x_find, without its #include lines (exc3_test.sh) */

volatile long me_mix_last_vb;
void mgs2_exc_handler(void) {}
uint32_t xl_psp_gl_screen(void) { return 0x44000000u; }

static int show(const char *name, int dry, int want_ints)
{
  int y, bad = 0, longest = 0; const char *s, *e;
  filen = 0; file[0] = 0; logged = 0; suspended = 0; batn = 0; mgs2_exc_ints = 0;
  if (dry) me_exc_vb(x_dry);
  else mgs2_exc_screen();
  if (x_seq != x_done) { x_save(); x_done = x_seq; }  /* the writer thread's turn */
  for (s = x_txt; *s; s = e + 1) { e = strchr(s, '\n'); if (!e) break; if (e - s > longest) longest = (int)(e - s); }
  printf("== %s: %d rows of 34, %d wraps, longest record line %d characters%s; interrupts back on %u; log.txt %s; suspended %08x; "
         "battery lines %d\n", name, rowsmax, wraps, longest, tabs ? ", TABS" : "", mgs2_exc_ints,
         logged ? (logged >> 30 ? "pending lines + record" : "record") : "nothing", (unsigned)suspended, batn);
  for (y = 0; y < rowsmax; y++) printf("%2d|%s|\n", y + 1, scr[y]);
  printf("-- crash.txt gets %d bytes%s\n%s", filen, filen ? "; after the record:" : "", filen ? file + x_len : "");
  if (wraps || tabs || longest > 67 || rowsmax > 32 || (int)mgs2_exc_ints != want_ints) { printf("FAIL\n"); bad = 1; }
  wraps = tabs = 0;
  return bad;
}

int main(void)
{
  ExcRegs *r = &mgs2_exc_regs; uint32_t i, a; int bad = 0;
  if (mmap((void *)0x08800000, 0x03800000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == MAP_FAILED) {
    perror("mmap"); return 2;
  }
  for (a = 0x08804000; a < 0x08be4fd0; a += 4) *(uint32_t *)(uintptr_t)a = a * 2654435761u;   /* "code": any words */
  for (a = 0x0bdff000; a < 0x0bdff200; a += 4) *(uint32_t *)(uintptr_t)a = 0x08b00000 + (a & 0xfff);
  x_main = 0x04a0c0d3; strcpy(x_crash, "ms0:/PSP/GAME/MGS2T_gl4/crash.txt");
  x_main_lo = th_stack; x_main_hi = th_stack + th_size; mgs2_exc_sleep[0] = 0x8801e2a4u;   /* (any kernel address) */

  /* the self-test under MGS2_EXC=3: the main thread's misaligned load, user mode, interrupts on: they come back on */
  x_mode = 3;
  memset(r, 0, sizeof *r);
  for (i = 0; i < 32; i++) r->r[i] = 0x08b80000 + i * 0x111;
  r->r[0] = 0; r->r[29] = 0x0bdff000; r->r[26] = 0x0bfbff00;
  r->epc = r->k_epc = (uint32_t)(uintptr_t)mgs2_exc_test_pc; r->cause = r->k_cause = 4u << 2; r->status = 0x60008613; r->k_status = 0x60008613;
  r->badvaddr = 0x08be7a12; r->k15 = 0x882ff800; r->k16 = 0x88231c84; r->k13 = 0; r->ic = 1; me_mix_last_vb = -1;
  bad |= show("self-test, MGS2_EXC=3", 0, 1);

  /* a crash with every field at its widest, in kernel mode with interrupts off: no file under MGS2_EXC=3 */
  r->epc = 0x08b91494; r->k_epc = 0x08b914a4; r->cause = 0x80000000u | (4u << 2); r->k_cause = 0x80000000u | (5u << 2);
  r->status = 0x60000002; r->k_status = 0x60000002; r->badvaddr = 0xffffffff; r->k13 = 0xffffffff; r->ic = 0;
  me_mix_last_vb = -2147483647L - 1;
  bad |= show("crash, widest, kernel mode, MGS2_EXC=3", 0, 0);

  /* user mode with interrupts on but inside an interrupt handler (control register 13 set): no file */
  r->status = 0x60008613; r->k_status = 0x60008613; r->k13 = 1; r->ic = 1; r->cause = r->k_cause = 7u << 2;
  bad |= show("crash in an interrupt handler, MGS2_EXC=3", 0, 0);

  /* user mode, interrupts on, but the kernel's interrupt enable off: no file */
  r->k13 = 0; r->ic = 0;
  bad |= show("crash with interrupts suspended, MGS2_EXC=3", 0, 0);

  /* a user-mode crash in another thread (k0 outside user_main's block, in battery's): file, no pending log lines */
  r->ic = 1; r->r[26] = 0x0b201f00; me_mix_last_vb = 28650;
  bad |= show("crash in the battery thread, MGS2_EXC=3", 0, 1);

  /* the self-test under MGS2_EXC=3 when the system's sleep was not found at the start: catcher 2's screen, interrupts stay off */
  mgs2_exc_sleep[0] = 0; r->r[26] = 0x0bfbff00; r->epc = r->k_epc = (uint32_t)(uintptr_t)mgs2_exc_test_pc; r->cause = r->k_cause = 4u << 2;
  me_mix_last_vb = -1;
  bad |= show("self-test, MGS2_EXC=3, the system's sleep not found", 0, 0);
  mgs2_exc_sleep[0] = 0x8801e2a4u;

  /* the self-test under MGS2_EXC=3 after the sleep returned twice (a wakeup): the counts in crash.txt and the green line */
  mgs2_exc_sleep[1] = 2; mgs2_exc_sleep[2] = 0x800201a7u;
  bad |= show("self-test, MGS2_EXC=3, the sleep returned twice", 0, 1);
  mgs2_exc_sleep[1] = 0; mgs2_exc_sleep[2] = 0;

  /* the self-test under MGS2_EXC=1: catcher 2's screen, interrupts stay off */
  x_mode = 1; r->r[26] = 0x0bfbff00; r->epc = r->k_epc = (uint32_t)(uintptr_t)mgs2_exc_test_pc; r->cause = r->k_cause = 4u << 2;
  me_mix_last_vb = -1;
  bad |= show("self-test, MGS2_EXC=1", 0, 0);

  /* the dry run */
  x_mode = 3; x_dry = 30000; me_mix_last_vb = 30000;
  bad |= show("dry run", 1, 0);
  printf(bad ? "exc3_test: FAIL\n" : "exc3_test: all records fit\n");
  return bad;
}
