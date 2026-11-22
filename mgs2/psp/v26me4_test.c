/* v26me4_test.c (x86 dev check, 2026-10-24): the crash catcher's record (psp/v26me4_exc.c: x_report, mgs2_exc_screen) as the PSP's
   debug screen shows it.  The screen is libpspdebug's pspDebugScreenPrintData as disassembled from the v26me4 build: 68 columns, 34
   rows; a 68th character or '\n' moves to the next row and clears it, and row 35 wraps to row 0, over the title.  Three records: the
   self-test as the PSP should show it, a crash with every field at its widest (both EPCs in code, a delay slot, kernel mode, 10-digit
   Media Engine numbers, the lowest vblank number), and the dry run's.  Fails (exit 1) when a record wraps, leaves a row of 68 characters
   or more, or takes more than 29 rows.  Run with sh psp/v26me4_test.sh. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

typedef int SceUID;
#define PSP_O_WRONLY 2
#define PSP_O_CREAT 0x200
#define PSP_O_APPEND 0x100
#define PSP_O_TRUNC 0x400
#define OVL_SIZE 0x460000
static SceUID sceIoOpen(const char *p, int f, int m) { (void)p; (void)f; (void)m; return -1; }
static int sceIoWrite(SceUID f, const void *b, int n) { (void)f; (void)b; return n; }
static int sceIoClose(SceUID f) { (void)f; return 0; }

static char scr[34][69]; static int X, Y, wraps, tabs, rowsmax;
static void clr(int y) { memset(scr[y], ' ', 68); scr[y][68] = 0; }
static void down(void) { X = 0; if (++Y == 34) { Y = 0; wraps++; } clr(Y); if (Y + 1 > rowsmax) rowsmax = Y + 1; }
static void pspDebugScreenSetBackColor(uint32_t c) { (void)c; }
static void pspDebugScreenSetTextColor(uint32_t c) { (void)c; }
static void pspDebugScreenClear(void) { int y; for (y = 0; y < 34; y++) clr(y); X = Y = 0; rowsmax = 1; }
static void pspDebugScreenSetXY(int x, int y) { if (x >= 0 && x < 68) X = x; if (y >= 0 && y < 34) Y = y; }
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
#include "exc_part.c"                                 /* psp/v26me4_exc.c up to x_find, without its #include lines (v26me4_test.sh) */

volatile long me_mix_last_vb;
static uint32_t st_v[6], st_d[7];
void me_mix_state(uint32_t v[6]) { memcpy(v, st_v, sizeof st_v); }
void me_mix_diag(uint32_t d[7]) { memcpy(d, st_d, sizeof st_d); }
int me_bat_line(const char *why) { (void)why; return 0; }
void mgs2_exc_handler(void) {}

static int show(const char *name, int dry)
{
  int y, bad = 0, n, longest = 0; const char *s, *e;
  if (dry) { n = x_report(&mgs2_exc_regs, "dry run: a made-up record, no exception; the run goes on"); x_screen(x_txt, n); }
  else mgs2_exc_screen();
  for (s = x_txt; *s; s = e + 1) { e = strchr(s, '\n'); if (!e) break; if (e - s > longest) longest = (int)(e - s); }
  printf("== %s: %d rows of 34, %d wraps, longest record line %d characters%s\n", name, rowsmax, wraps, longest, tabs ? ", TABS" : "");
  for (y = 0; y < rowsmax; y++) printf("%2d|%s|\n", y + 1, scr[y]);
  if (wraps || tabs || longest > 67 || rowsmax > 29) { printf("FAIL\n"); bad = 1; }
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

  /* the self-test: the main thread's misaligned load, both readings equal, user mode; the sound mix not started yet */
  memset(r, 0, sizeof *r);
  for (i = 0; i < 32; i++) r->r[i] = 0x08b80000 + i * 0x111;
  r->r[0] = 0; r->r[29] = 0x0bdff000;
  r->epc = r->k_epc = (uint32_t)(uintptr_t)mgs2_exc_test_pc; r->cause = r->k_cause = 4u << 2; r->status = 0x60000013; r->k_status = 0x60000013;
  r->badvaddr = 0x08be7a12; r->k15 = 0x88012340; r->k16 = 0x88abc000; me_mix_last_vb = -1;
  bad |= show("self-test", 0);

  /* a crash with every field at its widest */
  r->epc = 0x08b91494; r->k_epc = 0x08b914a4; r->cause = 0x80000000u | (4u << 2); r->k_cause = 0x80000000u | (5u << 2);
  r->status = 0x60000002; r->k_status = 0x60000002; r->badvaddr = 0xffffffff; me_mix_last_vb = -2147483647L - 1;
  for (i = 0; i < 6; i++) st_v[i] = 0xffffffffu;
  for (i = 0; i < 7; i++) st_d[i] = 0xffffffffu;
  bad |= show("crash, widest", 0);

  /* the dry run's title, with a plain sound-mix state */
  me_mix_last_vb = 30000; st_v[0] = 2; st_v[1] = 0; st_v[2] = 370470; st_v[3] = 367381; st_v[4] = 59; st_v[5] = 0;
  memset(st_d, 0, sizeof st_d); st_d[1] = 0x00020701; st_d[2] = 0x00000fc3;
  bad |= show("dry run", 1);
  printf(bad ? "v26me4_test: FAIL\n" : "v26me4_test: all records fit\n");
  return bad;
}
