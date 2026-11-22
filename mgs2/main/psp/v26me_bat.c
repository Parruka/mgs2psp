/* v26me_bat.c - v26me (2026-10-23): battery.txt, as v25f's (psp/xhook.c part 2), with the Media Engine's state in every line.  A thread
   writes a line next to the EBOOT at the start, every 60 s, when external power / charging / the low flag changes, at HOME and at the
   end; each line is opened, appended and closed, so the file survives a power-off and shows how far the run and the Media Engine got.
   Reads only (scePower), Ffm's rule.  Linked after v26's objects with the other new units (psp/v26me.sh); the start through the link's
   --wrap of mgs2_main (main_psp.c's call of the machine), the HOME and end lines from psp/me_mix.c (which wraps mgs2_exit and
   sceKernelExitGame), the latest vblank from me_mix.c's host_vb_begin wrap, the Media Engine's state from me_mix_state().  No libc in
   the lines (the thread's stack is 8 KB; v25f's helpers). */
#include <pspkernel.h>
#include <psppower.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef ME_DIAG4
#include <unistd.h>
#ifndef ME_DIAG3
#error ME_DIAG4 adds to ME_DIAG3's lines
#endif
#endif

extern volatile long me_mix_last_vb;   /* psp/me_mix.c: the latest vblank (-1 before the first) */
void me_mix_state(uint32_t v[6]);      /* psp/me_mix.c: the split's state and the Media Engine's counters */
#ifdef ME_REST
void me_mix_rest(uint32_t r[16]);      /* psp/me_mix.c (v30me5) */
int me_mix_me_live(void);
int hle_me_rpc_words(uint32_t w[5]);   /* psp/me/hle_me_mgs2.c (v30me5) */
#endif
#ifdef ME_SONY
int hle_me_sony_words(uint32_t w[3]);  /* psp/me/hle_me_mgs2.c (v30me6) */
#endif
#ifdef ME_DIAG5
int scePowerGetForceSuspendCapacity(void);   /* psppower.h has no prototypes for these two (libpsppower has the stubs) */
int scePowerGetLowBatteryCapacity(void);
#endif
#ifdef ME_DIAG3
/* v26me3 (psp/v26me3.sh, after v26me2's run ended in a power-off): a line every 5 s instead of every 60 s, each with psp/me_mix.c's
   me_mix_diag words too (the Media Engine's idle polls, its last command, the EPC of an exception it took, the power callback's
   counts and last flags), so the file shows the last seconds before a stop */
void me_mix_diag(uint32_t d[7]);
#define BAT_EVERY 1                     /* checks of 5 s per line */
#define BAT_WHY "every 5 s"
#if defined(SYS_DIAG)
#define BAT_LINE 1600                   /* SYS_DIAG's part too (about 200 more); the HOME callback thread has 4,000 bytes of stack */
#elif defined(ME_REST) || defined(ME_DIAG5)
#define BAT_LINE 1400                   /* v30me5's words too (about 1,160 at most; the HOME callback thread has 4,000 bytes of stack) */
#else
#define BAT_LINE 600
#endif
static char *hex(char *p, uint32_t u) { int i; for (i = 28; i >= 0; i -= 4) *p++ = "0123456789abcdef"[(u >> i) & 15u]; return p; }
#else
#ifdef SYS_DIAG
#error SYS_DIAG adds to the lines of ME_DIAG3
#endif
#define BAT_EVERY 12
#define BAT_WHY "every 60 s"
#define BAT_LINE 400
#endif

static char bat_path[256];
static uint64_t bat_t0;
static SceUID bat_sema = -1;            /* the thread, HOME and the end may write at once */
static char *put(char *p, const char *s) { while (*s) *p++ = *s++; return p; }
static char *unum(char *p, unsigned long u) { char t[16]; int n = 0; do t[n++] = '0' + u % 10; while (u /= 10); while (n) *p++ = t[--n]; return p; }
static char *num(char *p, long v) { if (v < 0) *p++ = '-'; return unum(p, v < 0 ? -(unsigned long)v : (unsigned long)v); }

/* gl28 (MGS2_BAT_REC=1; power_critique.md E2, reduced to the battery line: no new file, no new write pattern): every line also gives
   the battery thread's own lateness (its 5-s wait as asked and as it came: interrupts off or a busy kernel show here), and the picture
   loop's and the ME's state as psp/xl_psp.c xl_psp_rec reads it (plain and uncached reads, no lock: the queue's head and tail, the
   loop's and the ME's heartbeats, asleep and the rests, whose turn it is, the GE's lists and finishes, the lists held for the screen).
   MGS2_BAT_REC=0 (the default): the lines are gl27's and the thread takes no extra time. */
char *mgs2_getenv(const char *k) __attribute__((weak));   /* psp/main_psp.c: mgs2.cfg's settings (read before mgs2_main) */
void xl_psp_rec(uint32_t v[16]) __attribute__((weak));   /* psp/xl_psp.c (gl28) */
#define BR_ASK 5000000u                 /* the battery thread's wait, us */
static int bat_rec;                     /* MGS2_BAT_REC */
static volatile uint32_t br_wait, br_late, br_max, br_run, br_n;   /* the last wait as it came; its lateness; the most since the last 5-s
                                                                      line and in the run; waits timed */
static void br_timed(uint64_t dt)       /* one wait of BR_ASK, timed */
{
  uint32_t w = dt > 0xffffffffu ? 0xffffffffu : (uint32_t)dt, l = w > BR_ASK ? w - BR_ASK : 0;
  br_wait = w; br_late = l; br_n++;
  if (l > br_max) br_max = l;
  if (l > br_run) br_run = l;
}
static char *br_part(char *p, int reset)   /* the line's part (reset: the 5-s lines) */
{
  uint32_t v[16];
  p = put(p, "; gl28 rec: battery thread waited "); p = unum(p, br_wait); p = put(p, " us of "); p = unum(p, BR_ASK);
  p = put(p, " (late "); p = unum(p, br_late); p = put(p, " us, most since the last line "); p = unum(p, br_max);
  p = put(p, ", in the run "); p = unum(p, br_run); p = put(p, ", waits "); p = unum(p, br_n); p = put(p, ")");
  if (reset) br_max = 0;
  if (!xl_psp_rec) return put(p, "; no picture loop in this build");
  xl_psp_rec(v);
  if (!(v[0] & 1u)) return put(p, "; picture loop not set up");
  p = put(p, "; picture: "); p = put(p, v[0] & 16u ? "STOPPED" : v[0] & 8u ? "main CPU's turn" : "the ME's turn");
  p = put(p, v[0] & 2u ? " (loop on the ME" : v[0] & 4u ? " (the ME held after a take-back" : v[0] & 64u ? " (the ME stopped at the end"
           : " (loop not on the ME");
  p = put(p, v[0] & 32u ? ", may rest)" : ")"); p = put(p, ", queue head "); p = unum(p, v[1]); p = put(p, " tail "); p = unum(p, v[2]);
  p = put(p, ", loop heart "); p = unum(p, v[3]); p = put(p, " at "); p = unum(p, v[4]); p = put(p, ", ME heart "); p = unum(p, v[5]);
  p = put(p, " state "); p = unum(p, v[9]); p = put(p, ", asleep "); p = unum(p, v[6]); p = put(p, ", rests "); p = unum(p, v[7]);
  p = put(p, ", rest word "); p = unum(p, v[8]); p = put(p, ", kicks "); p = unum(p, v[15]); p = put(p, ", turns "); p = unum(p, v[14]);
  p = put(p, "; GE lists "); p = unum(p, v[10]); p = put(p, ", finishes "); p = unum(p, v[11]); p = put(p, ", loop lists ");
  p = unum(p, v[13]); p = put(p, ", held for the screen "); p = unum(p, v[12]);
  return p;
}

#ifdef SYS_DIAG
/* gl2d (2026-10-27, task #62; me_switchoff_plan.md "The codec call"): what the PSP's own threads get while the game runs, and the
   system's power events the moment they come.  Reads only, user mode; no game state, no timing of the game thread.
   - A thread at priority 0x6F, below the game thread's 0x20 (pspsdk crt0's default when PSP_MAIN_THREAD_PRIORITY is not given): it asks
     to wake every 10 ms; each 5-s line gives its longest lateness since the previous one, its wakes over 100 ms late and its wakes.
     The PSP runs the highest-priority ready thread, so its lateness is how long the game thread ran without blocking (a disc read, a
     log write, a wait for the GE): the time the system's threads below 0x20 wait too (the battery readings froze through v30's
     10-minute dense hash stretch, and 6 of the 7 stops came where the game thread blocks least).
   - The thread manager's counts since the previous 5-s line (sceKernelReferSystemStatus): thread switches, returns from idle, idle ms.
   - A power callback (scePowerRegisterCallback, its thread at 0x11 as the battery thread) writes a line at once for every power event
     with its flags: a stop the system announces (power switch, suspend, standby, low battery) is in the file before the power goes;
     none means the power went without the system's word.
   - The threads user mode can see (sceKernelGetThreadmanIdList / sceKernelReferThreadStatus: name, priority, status, run time) at the
     start and every 60 s. */
static volatile uint32_t sd_late_max, sd_late100, sd_wakes;
static SceKernelSystemStatus sd_prev;
static int sd_have;
static volatile int sd_pw_slot = -1, sd_pw_status = 1;   /* the power thread sets them; 1 = not yet */
static int sd_canary(SceSize n, void *a)
{
  (void)n; (void)a;
  for (;;) {
    uint64_t t0 = (uint64_t)sceKernelGetSystemTimeWide(), dt;
    sceKernelDelayThread(10000);
    dt = (uint64_t)sceKernelGetSystemTimeWide() - t0;
    dt = dt > 10000 ? dt - 10000 : 0;
    if (dt > 0xffffffffu) dt = 0xffffffffu;
    if (dt > sd_late_max) sd_late_max = (uint32_t)dt;
    if (dt > 100000) sd_late100++;
    sd_wakes++;
  }
  return 0;
}
static uint64_t sd_clk(const SceKernelSysClock *c) { return (uint64_t)c->hi << 32 | c->low; }
/* the line's part; reset (the 5-s lines of the battery thread): the counts start again from now */
static char *sd_part(char *p, int reset)
{
  SceKernelSystemStatus s; int ok;
  p = put(p, "; low-priority thread: longest wait "); p = unum(p, sd_late_max / 1000); p = put(p, " ms, waits over 100 ms ");
  p = unum(p, sd_late100); p = put(p, ", wakes "); p = unum(p, sd_wakes);
  memset(&s, 0, sizeof s); s.size = sizeof s;
  if (!(ok = sceKernelReferSystemStatus(&s) >= 0)) p = put(p, "; system status not given");
  else if (sd_have) {
    p = put(p, "; thread switches "); p = unum(p, s.threadSwitchCount - sd_prev.threadSwitchCount);
    p = put(p, ", out of idle "); p = unum(p, s.comesOutOfIdleCount - sd_prev.comesOutOfIdleCount);
    p = put(p, ", idle "); p = unum(p, (unsigned long)((sd_clk(&s.idleClocks) - sd_clk(&sd_prev.idleClocks)) / 1000)); p = put(p, " ms");
  }
  if (reset) { sd_late_max = 0; sd_late100 = 0; sd_wakes = 0; if (ok) { sd_prev = s; sd_have = 1; } }
  return p;
}
#endif

static int bat_line(const char *why, int reset)
{
  static const char *const st[4] = { "Media Engine off", "Media Engine on", "Media Engine simulated", "Media Engine stopped" };
  char l[BAT_LINE], *p = l; SceUID f; uint32_t v[6];
  int on = scePowerIsPowerOnline(), ch = scePowerIsBatteryCharging(), lo = scePowerIsLowBattery();
  uint64_t t = (uint64_t)sceKernelGetSystemTimeWide() - bat_t0;
  if (!bat_path[0]) return 0;
  p = put(p, why); p = put(p, " t "); p = num(p, (long)(t / 1000000)); p = put(p, " s, vb "); p = num(p, me_mix_last_vb);
  p = put(p, ": "); p = num(p, scePowerGetBatteryLifePercent()); p = put(p, "%, "); p = num(p, scePowerGetBatteryVolt());
  p = put(p, " mV, "); p = num(p, scePowerGetBatteryTemp()); p = put(p, " C, "); p = num(p, scePowerGetBatteryLifeTime());
  p = put(p, " min left (charging status "); p = num(p, scePowerGetBatteryChargingStatus()); p = put(p, "); external power "); p = num(p, on > 0);
  p = put(p, ", charging "); p = num(p, ch > 0); p = put(p, ", low "); p = num(p, lo > 0); p = put(p, "; ");
  me_mix_state(v); p = put(p, st[v[0] & 3]);
  if (v[0]) {
    p = put(p, ": heartbeat "); p = unum(p, v[1]); p = put(p, ", state "); p = unum(p, v[5]); p = put(p, ", commands ");
    p = unum(p, v[2]); p = put(p, " pushed, "); p = unum(p, v[3]); p = put(p, " consumed, checkpoints "); p = unum(p, v[4]);
#ifdef ME_DIAG3
    { uint32_t d[7]; me_mix_diag(d);
      p = put(p, ", idle polls "); p = unum(p, d[0]); p = put(p, ", last command "); p = hex(p, d[1]); *p++ = ' '; p = hex(p, d[2]);
      p = put(p, ", exception EPC "); p = hex(p, d[3]); p = put(p, "; power switch "); p = unum(p, d[4]); p = put(p, ", parks ");
      p = unum(p, d[5]); p = put(p, ", last power flags "); p = hex(p, d[6]); }
#endif
#ifdef ME_REST
    /* v30me5 (-DME_REST): the resting ME's counts (psp/me_mix.c me_mix_rest) and the words of Sony's ME requests, as the ME found them
       at its last wake and, while the ME runs, as the main CPU reads them now (a kernel read) */
    { uint32_t r[16], w[5]; int i;
      me_mix_rest(r);
      p = put(p, "; rests "); p = unum(p, r[2]); p = put(p, " (in the halt "); p = unum(p, r[3]); p = put(p, ", before it ");
      p = unum(p, r[4]); p = put(p, ", no interrupt "); p = unum(p, r[5]); p = put(p, ", given up "); p = unum(p, r[6]);
      p = put(p, r[0] ? "), may rest" : "), may not rest"); p = put(p, r[1] ? ", resting" : ", awake"); p = put(p, ", kicks ");
      p = unum(p, r[7]); p = put(p, " + "); p = unum(p, r[8]); p = put(p, " waiting, ME flags seen "); p = hex(p, r[9]);
      p = put(p, ", request words (ME)"); for (i = 0; i < 5; i++) { *p++ = ' '; p = hex(p, r[11 + i]); }
      p = put(p, ", changed at "); p = unum(p, r[10]); p = put(p, " wakes");
      if (me_mix_me_live() && hle_me_rpc_words(w) >= 0) { p = put(p, ", (main)"); for (i = 0; i < 5; i++) { *p++ = ' '; p = hex(p, w[i]); } } }
#endif
#ifdef ME_SONY
    /* v30me6 (-DME_SONY): the main CPU's 0xBC100000 (the word Sony's NMI handler reads), the reset register 0xBC10004C and how many
       of the kernel's 7 run words at 0xBFC00000 differ now (kernel reads; all ffffffff when the call fails) */
    { uint32_t s[3];
      hle_me_sony_words(s);
      p = put(p, "; NMI word (main) "); p = hex(p, s[0]); p = put(p, ", reset register "); p = hex(p, s[1]);
      p = put(p, ", start code words differing "); p = hex(p, s[2]); }
#endif
  }
#ifdef ME_DIAG4
  /* v26me4 (-DME_DIAG4, psp/v26me4.sh; after v26me2 and v26me3's run 2 both stopped 38.6 / 39.5 minutes in, at different points of the
     game): the time since the PSP was switched on, the clocks (a change by the system would show here), the program break (newlib's
     malloc takes its arena with sbrk: a growing break is a growing heap) and the free memory outside the heap.  Reads only.  Only
     functions the program links already (main_psp.c's clock and memory lines, the unsigned 64-bit division above), so the libraries'
     code and data keep their layout. */
  p = put(p, "; since power-on "); p = num(p, (long)((uint64_t)sceKernelGetSystemTimeWide() / 1000000)); p = put(p, " s, CPU ");
  p = num(p, scePowerGetCpuClockFrequency()); p = put(p, " MHz, bus "); p = num(p, scePowerGetBusClockFrequency());
  p = put(p, " MHz, break "); p = hex(p, (uint32_t)(uintptr_t)sbrk(0)); p = put(p, ", free memory ");
  p = unum(p, (unsigned long)(sceKernelTotalFreeMemSize() >> 10)); p = put(p, " KB (largest block ");
  p = unum(p, (unsigned long)(sceKernelMaxFreeMemSize() >> 10)); p = put(p, " KB)");
#endif
#ifdef ME_DIAG5
  /* v30me5 (-DME_DIAG5): the battery's remaining and full capacity and the system's "suspend required" flag in every line, its
     force-suspend and low-battery capacities in the first (scePower reads only; the 39-minute and 20-minute stops showed nothing in
     the percent, the voltage or the low flag) */
  p = put(p, "; capacity "); p = num(p, scePowerGetBatteryRemainCapacity()); p = put(p, " of "); p = num(p, scePowerGetBatteryFullCapacity());
  p = put(p, " mAh, suspend required "); p = num(p, scePowerIsSuspendRequired());
  { static int first = 1; if (first) { first = 0;   /* the first line ("start", the battery thread) */
    p = put(p, ", force-suspend capacity "); p = num(p, scePowerGetForceSuspendCapacity()); p = put(p, " mAh, low-battery capacity ");
    p = num(p, scePowerGetLowBatteryCapacity()); p = put(p, " mAh");
  } }
#endif
#ifdef SYS_DIAG
  p = sd_part(p, reset);
#else
  (void)reset;
#endif
  if (bat_rec) p = br_part(p, reset);   /* gl28 */
  *p++ = '\n';
  if (bat_sema >= 0) sceKernelWaitSema(bat_sema, 1, 0);
  if ((f = sceIoOpen(bat_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777)) >= 0) { sceIoWrite(f, l, p - l); sceIoClose(f); }
  if (bat_sema >= 0) sceKernelSignalSema(bat_sema, 1);
  return (on > 0) << 2 | (ch > 0) << 1 | (lo > 0);
}
int me_bat_line(const char *why) { return bat_line(why, 0); }

#ifdef SYS_DIAG
/* a text line as it is (the thread list), appended as bat_line's */
static void sd_write(const char *l, int n)
{
  SceUID f;
  if (!bat_path[0]) return;
  if (bat_sema >= 0) sceKernelWaitSema(bat_sema, 1, 0);
  if ((f = sceIoOpen(bat_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777)) >= 0) { sceIoWrite(f, l, n); sceIoClose(f); }
  if (bat_sema >= 0) sceKernelSignalSema(bat_sema, 1);
}
/* the threads user mode can see, 8 to a line: name, current / initial priority, status (1 running, 2 ready, 4 waiting, 8 suspended,
   16 dormant, 32 dead), run time in ms; on the battery thread (its stack is 8 KB) */
static void sd_threads(const char *why)
{
  static SceUID ids[128]; char l[1200], *p; int n = 0, r, i, k;
  uint64_t t = (uint64_t)sceKernelGetSystemTimeWide() - bat_t0;
  r = sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, ids, 128, &n);
  p = l; p = put(p, "threads "); p = put(p, why); p = put(p, " t "); p = num(p, (long)(t / 1000000)); p = put(p, " s, vb ");
  p = num(p, me_mix_last_vb); p = put(p, ": list "); p = hex(p, (uint32_t)r); p = put(p, ", "); p = num(p, n); p = put(p, " threads");
  if (r < 0) n = 0; else if (n > 128) n = 128;
  for (i = 0; i < n; i++) {
    SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti;
    if (i % 8 == 0) { *p++ = '\n'; sd_write(l, p - l); p = l; p = put(p, "  "); }
    else p = put(p, "; ");
    if ((k = sceKernelReferThreadStatus(ids[i], &ti)) < 0) { p = put(p, "id "); p = hex(p, (uint32_t)ids[i]); p = put(p, " refused "); p = hex(p, (uint32_t)k); continue; }
    ti.name[31] = 0; p = put(p, ti.name); *p++ = ' '; p = num(p, ti.currentPriority); *p++ = '/'; p = num(p, ti.initPriority);
    p = put(p, " st "); p = num(p, ti.status); p = put(p, " run "); p = unum(p, (unsigned long)(sd_clk(&ti.runClocks) / 1000)); p = put(p, " ms");
  }
  *p++ = '\n'; sd_write(l, p - l);
}
static int sd_power_cb(int count, int flags, void *arg)
{
  static const struct { uint32_t bit; const char *name; } fl[] = {
    { PSP_POWER_CB_POWER_SWITCH, " power-switch" }, { PSP_POWER_CB_HOLD_SWITCH, " hold-switch" }, { PSP_POWER_CB_STANDBY, " standby" },
    { PSP_POWER_CB_RESUME_COMPLETE, " resume-complete" }, { PSP_POWER_CB_RESUMING, " resuming" },
    { PSP_POWER_CB_SUSPENDING, " suspending" }, { PSP_POWER_CB_AC_POWER, " ac-power" }, { PSP_POWER_CB_BATTERY_LOW, " battery-low" },
    { PSP_POWER_CB_BATTERY_EXIST, " battery" } };
  char w[200], *p = w; unsigned i; (void)count; (void)arg;
  p = put(p, "power event "); p = hex(p, (uint32_t)flags); *p++ = ':';
  for (i = 0; i < sizeof fl / sizeof fl[0]; i++) if ((uint32_t)flags & fl[i].bit) p = put(p, fl[i].name);
  p = put(p, ", level "); p = unum(p, (uint32_t)flags & PSP_POWER_CB_BATTPOWER); *p = 0;
  bat_line(w, 0);
  return 0;
}
static int sd_power_thread(SceSize n, void *a)
{
  SceUID cb = sceKernelCreateCallback("diag_power", sd_power_cb, 0); int r; (void)n; (void)a;
  r = cb < 0 ? cb : scePowerRegisterCallback(-1, cb);              /* -1: a free slot, its number back (psp/me/hle_me_mgs2.c) */
  if (r >= 0) sd_pw_slot = r;
  else if (cb >= 0 && (r = scePowerRegisterCallback(0, cb)) >= 0) sd_pw_slot = 0;
  sd_pw_status = r;
  sceKernelSleepThreadCB();
  return 0;
}
#endif
/* the exits (psp/gl1_glue.c's sceKernelExitGame wrap, after the HOME line): SYS_DIAG's power callback out of the system's list */
void me_bat_exit(void)
{
#ifdef SYS_DIAG
  if (sd_pw_slot >= 0) { scePowerUnregisterCallback(sd_pw_slot); sd_pw_slot = -1; }
#endif
}

#ifdef ME_EXC_HOLD
/* v26me4 (-DME_EXC_HOLD from psp/v26me4.sh only): psp/v26me4_exc.c's self-test takes the file's semaphore before its deliberate
   exception and keeps it, so no battery.txt write is under way when the PSP stops there (a stop in the middle of a memory stick write
   is the one way the test could harm a file); 1 when held */
int me_bat_hold(void) { return bat_sema >= 0 && sceKernelWaitSema(bat_sema, 1, 0) >= 0; }
void me_bat_unhold(void) { if (bat_sema >= 0) sceKernelSignalSema(bat_sema, 1); }
#endif

static int bat_thread(SceSize n, void *arg)
{
  int last = bat_line("start", 1), i; (void)n; (void)arg;
#ifdef SYS_DIAG
  int lines = 0;
  sd_threads("at the start");
#endif
  for (;;) {                                      /* checks every 5 s (reads only), writes every 60 s (v26me3: 5 s) or on a change */
    for (i = 0; i < BAT_EVERY; i++) {
      int k; uint64_t t0 = bat_rec ? (uint64_t)sceKernelGetSystemTimeWide() : 0;   /* gl28 */
      sceKernelDelayThread(BR_ASK);
      if (bat_rec) br_timed((uint64_t)sceKernelGetSystemTimeWide() - t0);
      k = (scePowerIsPowerOnline() > 0) << 2 | (scePowerIsBatteryCharging() > 0) << 1 | (scePowerIsLowBattery() > 0);
      if (k != last) { last = me_bat_line("change"); i = -1; }
    }
    last = bat_line(BAT_WHY, 1);
#ifdef SYS_DIAG
    if (++lines % 12 == 0) sd_threads("every 60 s");
#endif
  }
  return 0;
}

/* the start (main_psp.c main's call of the machine, argv[1] = <EBOOT folder>/SLES_503.83): the battery thread, above the game thread's
   priority as the HOME thread (it sleeps all but a few ms a minute) */
int __real_mgs2_main(int argc, char **argv);
int __wrap_mgs2_main(int argc, char **argv)
{
  SceUID th, f; const char *e = argc > 1 && argv[1] ? strrchr(argv[1], '/') : 0; int n = e ? (int)(e - argv[1]) + 1 : 0;
  bat_t0 = (uint64_t)sceKernelGetSystemTimeWide();
  { const char *r = mgs2_getenv ? mgs2_getenv("MGS2_BAT_REC") : 0; bat_rec = r && r[0] == '1' && !r[1]; }   /* gl28 */
  fprintf(stderr, "gl28 battery recorder (MGS2_BAT_REC %d): %s\n", bat_rec, bat_rec ? "every battery.txt line adds the battery thread's "
          "lateness and the picture loop's and the ME's state (reads only)" : "off: battery.txt's lines as gl27's");
  bat_sema = sceKernelCreateSema("battery", 0, 1, 1, 0);
  snprintf(bat_path, sizeof bat_path, "%.*sbattery.txt", n, n ? argv[1] : "");
  if ((f = sceIoOpen(bat_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777)) >= 0) sceIoClose(f);
  fprintf(stderr, "v26me: battery log to %s " BAT_WHY ", with the Media Engine's state\n", bat_path);
#ifdef SYS_DIAG
  /* before the battery thread, so its first lines have the power callback and the low-priority thread */
  if ((th = sceKernelCreateThread("diag_power", sd_power_thread, 0x11, 0x2000, 0, 0)) >= 0) sceKernelStartThread(th, 0, 0);
  { int i; for (i = 0; i < 20 && sd_pw_status == 1; i++) sceKernelDelayThread(1000); }
  fprintf(stderr, "sysdiag: power callback %s (%08x, slot %d)\n", sd_pw_status >= 0 ? "registered" : "NOT registered", (unsigned)sd_pw_status, sd_pw_slot);
  th = sceKernelCreateThread("diag_low", sd_canary, 0x6F, 0x1000, 0, 0);
  if (th >= 0) sceKernelStartThread(th, 0, 0);
  fprintf(stderr, "sysdiag: low-priority thread (0x6F, a wake every 10 ms) %s (%08x)\n", th >= 0 ? "started" : "NOT started", (unsigned)th);
#endif
  if ((th = sceKernelCreateThread("battery", bat_thread, 0x11, 0x2000, 0, 0)) >= 0) sceKernelStartThread(th, 0, 0);
  else fprintf(stderr, "v26me: battery thread not started (%08x)\n", (unsigned)th);
#ifdef ME_EXC
  { void me_exc_install(const char *dir, int n); me_exc_install(n ? argv[1] : "", n); }   /* psp/v26me_exc.c or psp/v26me4_exc.c */
#endif
  return __real_mgs2_main(argc, argv);
}
