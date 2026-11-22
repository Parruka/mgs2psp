/* me_ep.c - the ME's eDRAM probe, the main CPU's side (2026-11-03, task #150; mgs2/me_edram_probe_notes.md, me_handoff_design.md 4.9
   and section 6 step 3).  gl16's game with nothing changed; like gl14mp's psp/me_probe.c, this unit gives what psp/gl1_glue.c gives
   gl16 (its three wraps and the battery log's two calls: the build renames those five in gl16's gl1_glue.o, which stays linked where
   it was, so every gl16 function keeps its address) and adds:
     __wrap_hw_probe  after the existing hardware probe, before the game: the ME started as gl14mp and v30me6c started it (Sony's start,
                      kernel helpers at their kernel alias, the ME resting between jobs), its clock calibrated, then the eDRAM's turns,
                      one change each: A with the writes OKed for v30me6c only; B (a build made with -DEP_REG_B, after Ffm's yes) the
                      ME's 0xBC100050 = 0x0f; C (-DEP_REG_C, after Ffm's yes) B and the three VME writes.  Each turn: the first
                      2 MB filled with a pattern and checked; 2-4 MB filled as a job of its own and both halves checked (the size);
                      everything filled again and checked after 2 s; copies of 1 MB from main RAM into the eDRAM (plain stores,
                      uncached stores, claimed lines; each checked by the ME, the main CPU cannot read the eDRAM) and main RAM to main
                      RAM; the ME's reads over 1 MB of eDRAM and of main RAM; the main CPU's kernels while the ME copies into the eDRAM
                      and into main RAM.  A stop of the ME holds it in reset; it is started again, cold, and the turn's writes made
                      again.  Then the game's turns get the first turn whose checks passed (back to A's writes by a cold start first
                      when B or C ran).  Every "mep" line in log.txt.  The big buffers lie in game RAM before the game is loaded (zero
                      before and after, checked), the control block, the ME's region and the ring outside the heap (gl14mp's way).
     host_vb_begin    after main_psp.c's step timing: the turns MGS2_MEP_CYCLE=<vb>:<n>:<m>,<m>... (boot.c opt_window's slot rule):
                      m 0 the ME at rest, 1 the ME copies the 3,456 KB window of game RAM into the eDRAM's first MB without a pause,
                      2 the same into the main RAM ring (gl14mp's burst, the same run's reference); a "mep vb" line per 100-vblank
                      window with the ME's counts and the sentinel's (the rest of the eDRAM holds a pattern all run; the ME checks
                      4 KB of it after each 64 KB block it copies).
     mgs2_exit, sceKernelExitGame   the sentinel checked once more, the ME stopped (held in reset), then gl1_glue.c's battery line and
                      handler.
   The game's state is not touched: the ME reads game RAM only and writes the eDRAM, the ring, its region and the control block.
   The start's lines reach the stick one by one (main_psp.c's mgs2_log_crash appends the log's unwritten lines to log.txt), so if
   the whole PSP stops in a step, log.txt ends with the last step that finished; the game's part writes through stderr, which
   main_psp.c puts on the stick every window.
   MGS2_MEP=0 in mgs2.cfg: nothing of this runs (gl16 with the ME off).  The emulator has no ME (hle_me_start refuses there): its parts
   are skipped and said so; the main CPU's kernels run.
   Probe 2 (a build made with -DEP_P2, task #163; mgs2/me_edram_probe_notes.md "Probe 2"): the eDRAM through the kernel segments
   (me_ep.h); before the turns the ME reads its 0xBC000000-0xBC000044 (reads only); each turn starts with four one-word tests; after
   the turns one cached read through the user segment (probe 1's way) confirms the cause of probe 1's stops; a stop's line gives
   the address the ME was at (EpCtl.prog).  No register write beyond the turns'. */
#include <pspkernel.h>
#include <psppower.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "hle_me.h"
#include "hle_me_mgs2.h"
#include "ps2rc_exc.h"
#include "me_ep.h"

typedef void (*OutFn)(const char *fmt, ...);
void mgs2_log_crash(const char *b, int n, int pending);   /* psp/main_psp.c: the log's unwritten lines appended to log.txt at once */
volatile Ps2rcMeBlock ps2rc_me_block;   /* hle_me_mgs2.c's link to PS2RC's sleep PRX: none here */
extern volatile long me_mix_last_vb;    /* psp/gl1_glue.c: the latest vblank (-1 before the first), for battery.txt and the crash record */
extern uint8_t g_mem[];                 /* the game's RAM (runtime/guest.h): read only here */
char *mgs2_getenv(const char *k);
int me_bat_line(const char *why);
void me_bat_exit(void);
void me_exc_vb(long vb);
void me_exc_release(void);

#define UNC(p) ((void *)(0x40000000u | (uint32_t)(uintptr_t)(p)))
#define CTL_BYTES ((sizeof(EpCtl) + 63u) & ~63u)
#define REGION_BYTES (16u << 10)        /* the ME's own memory in main RAM: its stack at the top */
#define STACK_BYTES (8u << 10)
#define RING_BYTES (64u << 10)          /* the main RAM copy's destination (gl14mp's ring) */
#define SRC_OFF 0x00400000u             /* the stream's window in game RAM: 4 MB in, 3,456 KB long (gl14mp's) */
#define SRC_BYTES 0x00360000u
#define T1M (1u << 20)
#define SYNC() __asm__ volatile("sync" ::: "memory")
#define NONE 0xFFFFFFFFu

static OutFn out, g_out;                /* out: the start's lines (out_now while the start runs); g_out: main_psp.c's (screen and log) */
static EpCtl *g_ctl_c; static volatile EpCtl *g_c;
static uint8_t *g_region, *g_ring;
static int g_me;                        /* 1: the ME runs ep_work; 0: not started; -1: started, then stopped or parked */
static int g_on;                        /* the probe runs (MGS2_MEP is not 0) */
static double g_mhz;                    /* the ME's Count ticks per microsecond (0: not known) */
static uint32_t g_seq;
static uint32_t g_kicked = NONE; static uint32_t g_kicks, g_rekicks; static uint64_t g_kick_us, g_t_post;
static int g_cur = -1;                  /* the turn whose writes are made now (0 A, 1 B, 2 C; -1 none yet) */

static uint64_t now_us(void) { return (uint64_t)sceKernelGetSystemTimeWide(); }
/* a line of the start, on the screen and in the log, and on the stick at once with every line before it (main thread only, the log's
   only writer while the start runs): a stop of the whole PSP in a step leaves log.txt ending with the last step that finished */
static void out_now(const char *fmt, ...)
{
  char b[512]; va_list ap;
  va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
  g_out("%s", b);
  mgs2_log_crash("", 0, 1);
}
static uint32_t g_rs = 2463534242u;
static uint32_t rnd(void) { g_rs ^= g_rs << 13; g_rs ^= g_rs >> 17; g_rs ^= g_rs << 5; return g_rs; }

/* ---- the ME: start, jobs, kicks, failure (gl14mp's) ---- */
static void kick_check(void)            /* wake a resting ME, once per rest */
{
  uint32_t s; uint64_t t0;
  if (g_me != 1 || !g_c->asleep) return;
  s = g_c->sleeps;
  if (s == g_kicked) return;
  g_kicked = s; t0 = now_us();
  hle_me_kick(); g_kicks++;
  g_kick_us += now_us() - t0;
}
static void me_dead(const char *what)   /* (the log only, as gl14mp: it may come while the game runs, when nothing goes on the screen) */
{
  uint32_t ca, ep, bv;
  if (hle_me_exception(&ca, &ep, &bv))
    fprintf(stderr, "mep: %s; the ME took an exception: Cause %08x EPC %08x BadVAddr %08x; it is held in reset\n", what, (unsigned)ca,
            (unsigned)ep, (unsigned)bv);
  else
    fprintf(stderr, "mep: %s; ME heartbeat %u, state %u, job %u of %u, mode %u; it is held in reset\n", what, (unsigned)g_c->hdr.heartbeat,
            (unsigned)g_c->hdr.state, (unsigned)g_c->done_seq, (unsigned)g_seq, (unsigned)g_c->s_mode);
#ifdef EP_P2
  fprintf(stderr, "mep: the ME was at %08x (job %u, its first argument %08x)\n", (unsigned)g_c->prog, (unsigned)g_c->job,
          (unsigned)g_c->a[0]);
#endif
  hle_me_park(); g_me = -1;
}
static void post(uint32_t job, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
  g_c->a[0] = a0; g_c->a[1] = a1; g_c->a[2] = a2; g_c->a[3] = a3; g_c->job = job;
  SYNC(); g_t_post = now_us(); g_c->job_seq = ++g_seq; SYNC();
  kick_check();
}
/* wait for the posted job (polls every 50 us, the main CPU asleep between them); 0: an exception or no answer in ms */
static int job_wait(uint32_t ms)
{
  uint64_t t, lk = now_us(); uint32_t ca, ep, bv;
  while (g_c->done_seq != g_seq) {
    if (hle_me_exception(&ca, &ep, &bv)) { me_dead("during a job"); return 0; }
    t = now_us();
    if (t - g_t_post > (uint64_t)ms * 1000u) { me_dead("a job got no answer"); return 0; }
    if (g_c->asleep && t - lk > 500u) { hle_me_kick(); g_rekicks++; lk = t; }
    sceKernelDelayThread(50);
  }
  return 1;
}
static int job(uint32_t j, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t ms)
{
  if (g_me != 1) return 0;
  post(j, a0, a1, a2, a3);
  return job_wait(ms);
}
static double me_ns(uint32_t ticks, double n) { return g_mhz > 0 ? (double)ticks * 1000.0 / g_mhz / n : 0; }
static double me_ms(uint32_t ticks) { return g_mhz > 0 ? (double)ticks / g_mhz / 1000.0 : 0; }
static double ma_ns(uint64_t us, double n) { return (double)us * 1000.0 / n; }

/* ---- memory (gl14mp's): the boot probe's big buffers in game RAM before the game is loaded, zero before and after; the control
   block, the ME's region and the ring outside the heap ---- */
#define SCR_OFF 0x01000000u
#define SCR_BYTES 0x00800000u
static uint32_t g_scr_used, g_scr_max; static int g_scr_ok;
static uint32_t scr_nonzero(void)
{
  const uint32_t *p = (const uint32_t *)(g_mem + SCR_OFF); uint32_t i, n = 0;
  for (i = 0; i < SCR_BYTES / 4; i++) n += p[i] != 0;
  return n;
}
static void scr_open(void)
{
  uint32_t nz;
  sceKernelDcacheWritebackInvalidateAll();
  nz = scr_nonzero(); g_scr_ok = nz == 0; g_scr_used = g_scr_max = 0;
  out("mep scratch: game RAM %08x-%08x before the game is loaded: %u words not zero%s\n", (unsigned)SCR_OFF, (unsigned)(SCR_OFF + SCR_BYTES),
      (unsigned)nz, nz ? ": not used, the tests that need it are skipped" : "; the boot probe's buffers go there");
}
static void *scr_take(uint32_t n)       /* 64-byte aligned */
{
  void *p;
  n = (n + 63u) & ~63u;
  if (!g_scr_ok || n > SCR_BYTES - g_scr_used) return 0;
  p = g_mem + SCR_OFF + g_scr_used; g_scr_used += n;
  if (g_scr_used > g_scr_max) g_scr_max = g_scr_used;
  return p;
}
static void scr_close(void)             /* after the ME's last job on it (every job ends with the ME's D-cache written back and empty) */
{
  uint32_t nz;
  if (!g_scr_ok) return;
  memset(g_mem + SCR_OFF, 0, SCR_BYTES);
  sceKernelDcacheWritebackInvalidateAll(); sceKernelIcacheInvalidateAll();
  nz = scr_nonzero(); g_scr_ok = 0;
  out("mep scratch: %u KB used at most, zeroed again: %u words not zero%s\n", (unsigned)(g_scr_max >> 10), (unsigned)nz,
      nz ? " (THE GAME'S RAM DIFFERS FROM GL16'S: the hashes will show it)" : "");
}
static uint8_t *kept_take(uint32_t n, const char **where)
{
  SceUID b = sceKernelAllocPartitionMemory(2, "mep_kept", PSP_SMEM_Low, n + 64, 0);
  if (b >= 0) { *where = "user memory outside the heap"; return (uint8_t *)(((uintptr_t)sceKernelGetBlockHeadAddr(b) + 63) & ~(uintptr_t)63); }
  *where = "the heap (no block outside it)";
  return memalign(64, n);
}

static void ctl_init(void)              /* the main CPU's words of the control block (through the uncached alias) */
{
  g_c->rest = 0; g_c->mode = EP_M_REST; g_c->kind = EP_C_MAIN;
  g_c->src = (uint32_t)(uintptr_t)(g_mem + SRC_OFF); g_c->src_bytes = SRC_BYTES;
  g_c->ring = (uint32_t)(uintptr_t)g_ring; g_c->ring_bytes = RING_BYTES;
  g_c->sent_off = 0; g_c->sent_bytes = 0; g_c->sent_seed = 0;
  g_c->sent_first_off = NONE; g_c->sent_first_val = 0; g_c->sent_blk[0] = g_c->sent_blk[1] = 0; g_c->sent_kb = 0; g_c->sent_bad = 0;
}
static int me_start(void)
{
  char why[200], pg[400]; int r, pw; const char *where = ""; uint8_t *k = kept_take(CTL_BYTES + REGION_BYTES + RING_BYTES, &where);
  if (!k) { out("mep: no memory for the control block, the ME's region and the ring\n"); return 0; }
  g_ctl_c = (EpCtl *)k; g_region = k + CTL_BYTES; g_ring = g_region + REGION_BYTES;
  memset(k, 0, CTL_BYTES + REGION_BYTES + RING_BYTES);
  out("mep: the control block, the ME's region and the ring (%u KB) at %08x, in %s\n", (unsigned)((CTL_BYTES + REGION_BYTES + RING_BYTES) >> 10),
      (unsigned)(uintptr_t)k, where);
  sceKernelDcacheWritebackInvalidateAll();   /* from now on the control block only through the uncached alias */
  g_c = UNC(g_ctl_c);
  ctl_init();
  pw = hle_me_power_init();
  r = hle_me_start(ep_work, &g_c->hdr, g_region, REGION_BYTES, STACK_BYTES, why, sizeof why);
  hle_me_progress(pg, sizeof pg); out("mep me: %s\n", pg);
  { char sl[600]; hle_me_sony_line(sl, sizeof sl); out("mep me start: %s\n", sl); }
  if (!r) { out("mep: the ME was not started: %s; power callback %d; its parts are skipped\n", why, pw); return 0; }
  g_me = 1; g_cur = 0;
  { uint32_t r40, r50; hle_me_regs_found(&r40, &r50);
    out("mep: the ME started: heartbeat %u, scePowerLock %d, power callback %d, ME registers found %08x / %08x, reset register %08x\n",
        (unsigned)g_c->hdr.heartbeat, hle_me_power_lock_rc(), pw, (unsigned)r40, (unsigned)r50, (unsigned)hle_me_reset_reg_seen()); }
  return 1;
}

/* Count against the microsecond clock over 200 ms, through the word the ME rewrites every ~3 us; then the ME rests between jobs */
static void me_cal(void)
{
  uint64_t t1, t2; uint32_t c1, c2;
  g_c->cal_run = 1; SYNC();
  post(EP_J_CAL, 0, 0, 0, 0);
  sceKernelDelayThread(10000);
  t1 = now_us(); c1 = g_c->cal; sceKernelDelayThread(200000); t2 = now_us(); c2 = g_c->cal;
  g_c->cal_run = 0; SYNC();
  if (!job_wait(2000)) return;
  g_mhz = t2 > t1 ? (double)(uint32_t)(c2 - c1) / (double)(t2 - t1) : 0;
  out("mep clock: the ME's Count runs at %.3f MHz (%u ticks in %u us)%s\n", g_mhz, (unsigned)(c2 - c1), (unsigned)(t2 - t1),
      c2 == c1 ? ": it does not count, so the ME's times below are 0" : "");
  g_c->rest = 1; SYNC();
}

/* the turn t's writes made (after a start, which makes A's): B the ME's 0xBC100050 = 0x0f; C B's and the three VME writes; each value
   read before and after.  0: the ME stopped */
static int ep_apply(int t)
{
  char tc = "ABC"[t];
  if (g_me != 1) return 0;
  if (!job(EP_J_REG, 0, 0, 0, 0, 2000)) return 0;
  out("mep %c: the ME's 0xBC100050 reads %08x\n", tc, (unsigned)g_c->r[0]);
#ifdef EP_REG_B
  if (t >= 1) {
    if (!job(EP_J_REG, EP_R_B, 0, 0, 0, 2000)) { out("mep %c: the ME stopped in turn B's write\n", tc); return 0; }
    out("mep %c: turn B's write: the ME's 0xBC100050 %08x -> 0x0000000f, reads %08x after\n", tc, (unsigned)g_c->r[0], (unsigned)g_c->r[1]);
  }
#endif
#ifdef EP_REG_C
  if (t >= 2) {
    if (!job(EP_J_REG, EP_R_C, 0, 0, 0, 2000)) { out("mep %c: the ME stopped in turn C's writes\n", tc); return 0; }
    out("mep %c: turn C's writes: 0xBCC00000 %08x -> ffffffff (reads %08x), 0xBCC00030 %08x -> 1 (%08x), 0xBCC00040 %08x -> 1 (%08x)\n",
        tc, (unsigned)g_c->r[1], (unsigned)g_c->r[4], (unsigned)g_c->r[2], (unsigned)g_c->r[5], (unsigned)g_c->r[3], (unsigned)g_c->r[6]);
  }
#endif
  g_cur = t;
  return 1;
}

/* the ME stopped: started again, cold (its caches set up with Index Store Tag, so no line the stopped ME held is written back over
   memory the main CPU uses since; gl14mp's me_restart), with A's writes; then the turn t's made again (t < 0: none) */
static void me_restart(const char *why_now, int t)
{
  char why[200]; int r;
  memset((void *)g_c, 0, CTL_BYTES);
  ctl_init();
  SYNC(); g_seq = 0; g_kicked = NONE;
  hle_me_set_cold(1);
  r = hle_me_start(ep_work, &g_c->hdr, g_region, REGION_BYTES, STACK_BYTES, why, sizeof why);
  hle_me_set_cold(0);
  if (r) { g_me = 1; g_cur = 0; g_c->rest = 1; SYNC(); }
  out("mep: the ME started again, cold, %s: %s\n", why_now, r ? "running" : why);
  if (r && t > 0) ep_apply(t);
}

/* ---- the main CPU's kernels (gl14mp's k_main), alone and while the ME copies ---- */
typedef struct { double alu, lines, chase; } KTimes;
static KTimes g_ka_m;
#define NALU 100000u
#define NL (65536u)                     /* 4 MB of 64-byte lines */
static void make_chain(uint32_t *p, uint32_t n, uint32_t stride, uint32_t *ord)   /* hwprobe.c's: a random cycle over n slots */
{
  uint32_t i;
  for (i = 0; i < n; i++) ord[i] = i;
  for (i = n - 1; i > 0; i--) { uint32_t j = rnd() % (i + 1), t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
  for (i = 0; i < n; i++) p[ord[i] * stride] = ord[(i + 1) % n] * stride;
}
static void k_main(uint32_t *big, KTimes *t)
{
  uint64_t t0, t1;
  t0 = now_us(); mep_k_alu(NALU); t1 = now_us(); t->alu = ma_ns(t1 - t0, NALU);
  sceKernelDcacheWritebackInvalidateAll();
  t0 = now_us(); mep_k_lines(big, NL); t1 = now_us(); t->lines = ma_ns(t1 - t0, NL);
  sceKernelDcacheWritebackInvalidateAll();
  t0 = now_us(); mep_k_chase(big, 0, NL); t1 = now_us(); t->chase = ma_ns(t1 - t0, NL);
}

/* ---- one turn of the eDRAM's tests ---- */
static int g_ran[3], g_pass[3], g_kind[3] = { -1, -1, -1 };
static uint32_t g_size[3];
static const char *const k_kind[4] = { "main RAM to main RAM", "into the eDRAM, plain stores", "into the eDRAM, claimed lines (cache 0x18)",
                                       "into the eDRAM, uncached stores" };
static int stopped(int t, const char *what)
{
  char w[96];
  snprintf(w, sizeof w, "after the stop in turn %c (%s)", "ABC"[t], what);
  me_restart(w, t);
  return 0;
}
/* fill and check lines; 0: the ME stopped (started again) */
static int ep_fill(int t, const char *what, uint32_t off, uint32_t n, uint32_t seed)
{
  if (!job(EP_J_FILL, off, n, seed, 0, 10000)) return stopped(t, what);
  out("mep %c: %s: eDRAM %07x-%07x filled, %.2f ms (%.1f MB/s)\n", "ABC"[t], what, (unsigned)off, (unsigned)(off + n), me_ms(g_c->r[6]),
      me_ms(g_c->r[6]) > 0 ? (double)n / me_ms(g_c->r[6]) / 1000.0 : 0);
  return 1;
}
static uint32_t ep_check(int t, const char *what, uint32_t off, uint32_t n, uint32_t seed)
{
  uint32_t bad;
  if (!job(EP_J_CHECK, off, n, seed, 0, 10000)) { stopped(t, what); return NONE; }
  bad = g_c->r[0];
  if (bad) out("mep %c: %s: eDRAM %07x-%07x checked: %u words changed (the first at %07x: %08x, the pattern's %08x; 64 KB blocks "
               "with a change %08x %08x), %.2f ms\n", "ABC"[t], what, (unsigned)off, (unsigned)(off + n), (unsigned)bad, (unsigned)g_c->r[1],
               (unsigned)g_c->r[2], (unsigned)g_c->r[3], (unsigned)g_c->r[5], (unsigned)g_c->r[4], me_ms(g_c->r[6]));
  else out("mep %c: %s: eDRAM %07x-%07x checked: every word as written, %.2f ms\n", "ABC"[t], what, (unsigned)off, (unsigned)(off + n),
           me_ms(g_c->r[6]));
  return bad;
}
static uint32_t verify(const uint32_t *d, const uint32_t *s, uint32_t n)
{
  uint32_t i, bad = 0;
  for (i = 0; i < n / 4; i++) bad += d[i] != s[i];
  return bad;
}
/* the copies and the reads; the fastest copy into the eDRAM that checked equal is the turn's kind */
static void ep_speeds(int t, uint32_t *src1, uint32_t *dst1, uint32_t *chain1)
{
  static const uint32_t kinds[4] = { EP_C_MAIN, EP_C_PLAIN, EP_C_UNC, EP_C_CDE };
  char tc = "ABC"[t]; uint32_t best = NONE, k; int unc_ok = 0;
  for (k = 0; k < 4 && g_me == 1; k++) {
    uint32_t kind = kinds[k], bad; double ms, wb, rate;
    /* the destination filled with a pattern first, so a copy that wrote nothing there (the data of the copy before it is the same)
       shows in its check */
    if (kind != EP_C_MAIN && !job(EP_J_FILL, 0, T1M, 0xC0DE0000u + kind, 0, 10000)) { stopped(t, "the fill before a copy"); continue; }
    if (kind == EP_C_CDE) {             /* claimed lines on the eDRAM: 64 KB checked first (gl14mp checked them on main RAM only) */
      if (!job(EP_J_COPY, EP_C_CDE, 0, (uint32_t)(uintptr_t)src1, 64u << 10, 5000)) { stopped(t, "the claimed-line copy, 64 KB"); continue; }
      if (!job(EP_J_CMP, 0, (uint32_t)(uintptr_t)src1, 64u << 10, 0, 5000)) { stopped(t, "the claimed-line copy's check"); continue; }
      if (g_c->r[0]) { out("mep %c: 4. copy 64 KB, ME, into the eDRAM, claimed lines: %u wrong words; not timed on 1 MB\n", tc, (unsigned)g_c->r[0]); continue; }
    }
    if (kind == EP_C_MAIN) { memset(dst1, 0xEE, T1M); sceKernelDcacheWritebackInvalidateAll(); }
    if (!job(EP_J_COPY, kind, kind == EP_C_MAIN ? (uint32_t)(uintptr_t)dst1 : 0, (uint32_t)(uintptr_t)src1, T1M, 5000)) {
      stopped(t, k_kind[kind]); continue; }
    ms = me_ms(g_c->r[0]); wb = me_ms(g_c->r[1]); rate = ms > 0 ? (double)T1M / ms / 1000.0 : 0;
    if (kind == EP_C_MAIN) { sceKernelDcacheInvalidateRange(dst1, T1M); bad = verify(dst1, src1, T1M); }
    else {
      if (!job(EP_J_CMP, 0, (uint32_t)(uintptr_t)src1, T1M, 0, 5000)) { stopped(t, "a copy's check"); continue; }
      bad = g_c->r[0];
    }
    out("mep %c: 4. copy 1 MB, ME, %s: %.3f ms, %.1f MB/s, then its write-back %.3f ms; %u wrong words\n", tc, k_kind[kind], ms, rate,
        wb, (unsigned)bad);
    if (kind == EP_C_UNC && !bad) unc_ok = 1;
    if (kind != EP_C_MAIN && !bad && (best == NONE || g_c->r[0] < best)) { best = g_c->r[0]; g_kind[t] = (int)kind; }
  }
  /* the reads: the 1 MB chain copied into the eDRAM's second MB and checked, then the ME's kernels over it and over the chain in main
     RAM (every job starts with the ME's cache empty) */
  if (g_me == 1 && job(EP_J_COPY, EP_C_PLAIN, T1M, (uint32_t)(uintptr_t)chain1, T1M, 5000) &&
      job(EP_J_CMP, T1M, (uint32_t)(uintptr_t)chain1, T1M, 0, 5000)) {
    double le = 0, lm = 0, ce = 0, cu = 0, cm = 0; const uint32_t n = T1M / 64u, nu = 4096u;
    if (g_c->r[0]) { out("mep %c: 4. reads: the chain's copy in the eDRAM has %u wrong words; not timed\n", tc, (unsigned)g_c->r[0]); return; }
    if (job(EP_J_KERN, MEP_K_LINES, EP_EDRAM + T1M, n, 0, 5000)) le = me_ns(g_c->r[0], n);
    if (job(EP_J_KERN, MEP_K_LINES, (uint32_t)(uintptr_t)chain1, n, 0, 5000)) lm = me_ns(g_c->r[0], n);
    if (job(EP_J_KERN, MEP_K_CHASE, EP_EDRAM + T1M, 0, n, 5000)) ce = me_ns(g_c->r[0], n);
    if (unc_ok && job(EP_J_KERN, MEP_K_CHASE, EP_EDRAM_UNC + T1M, 0, nu, 5000)) cu = me_ns(g_c->r[0], nu);
    if (job(EP_J_KERN, MEP_K_CHASE, (uint32_t)(uintptr_t)chain1, 0, n, 5000)) cm = me_ns(g_c->r[0], n);
    if (g_me == 1)
      out("mep %c: 4. reads over 1 MB, ME: one load a line eDRAM %.2f ns, main RAM %.2f ns; a chase eDRAM %.2f ns a step, eDRAM "
          "uncached %.2f%s, main RAM %.2f\n", tc, le, lm, ce, cu, unc_ok ? "" : " (not run: the uncached copy did not check equal)", cm);
    else stopped(t, "the reads");
  } else if (g_me != 1) stopped(t, "the chain's copy");
}
/* the main CPU's kernels while the ME copies the (zero) game RAM window without a pause into the eDRAM (the turn's kind), then into
   the main RAM ring */
static void stream_set(uint32_t m) { if (g_me == 1) { g_c->mode = m; SYNC(); if (m != EP_M_REST) kick_check(); } }
static void ep_share(int t, uint32_t *big)
{
  uint32_t i, b0, r0, u0, db; uint64_t t0, t1; KTimes kt; char tc = "ABC"[t];
  for (i = 0; i < 2 && g_me == 1; i++) {
    uint32_t kind = i == 0 ? (uint32_t)g_kind[t] : EP_C_MAIN;
    if (i == 0 && g_kind[t] < 0) continue;
    g_c->kind = kind; g_c->sent_bytes = 0; SYNC();
    stream_set(EP_M_COPY); sceKernelDelayThread(20000);
    b0 = g_c->s_blocks; r0 = g_c->s_rd; u0 = g_c->s_busy; t0 = now_us();
    k_main(big, &kt);
    t1 = now_us();
    { uint32_t dr = g_c->s_rd - r0; double us = (double)(t1 - t0);
      db = g_c->s_blocks - b0;
      stream_set(EP_M_REST);
      out("mep %c: 5. the main CPU's kernels while the ME copies %s: alu %.2f ns a round (x%.3f of alone), one load a line %.2f ns "
          "(x%.3f), chase %.2f ns (x%.3f); the ME meanwhile %u blocks, %.1f MB/s each way, busy %.0f%%\n", tc, k_kind[kind], kt.alu,
          kt.alu / g_ka_m.alu, kt.lines, kt.lines / g_ka_m.lines, kt.chase, kt.chase / g_ka_m.chase, (unsigned)db, dr * 64.0 / us,
          g_mhz > 0 ? 100.0 * (g_c->s_busy - u0) / g_mhz / us : 0); }
    sceKernelDelayThread(5000);
    /* a stop in a stream (an exception, or no block copied while the kernels ran): the ME started again; a turn whose stream into the
       eDRAM stopped is not used for the game's turns */
    if (g_me == 1 && (hle_me_exception(&b0, &r0, &u0) || !db)) {
      me_dead(db ? "after a stream at the start" : "no stream block while the main CPU's kernels ran");
      if (i == 0) g_pass[t] = 0;
      stopped(t, i == 0 ? "the stream into the eDRAM" : "the stream into main RAM");
    }
  }
}
#ifdef EP_P2
/* ---- probe 2 (task #163): the ME's 0xBC000000-0xBC000044 read before the turns (reads only: the custom ME core writes all ones
   there, "unlock memory" and "unlock user registers"), four one-word tests at the start of each turn, and after the turns one
   cached read through the user segment, the way probe 1's fill reached the eDRAM ---- */
#define W_OFF 0x100u                    /* the one-word tests' eDRAM offset */
static const char *const k_word[6] = { "", "an uncached read", "an uncached write read back",
                                       "a cached write, written back, read uncached", "an uncached write read cached",
                                       "a cached read through the user segment" };
static void ep_snap(void)
{
  static const uint32_t at[3] = { 0xBC000000u, 0xBC000020u, 0xBC000040u }, nw[3] = { 8u, 8u, 2u };
  uint32_t k, i; char b[100]; int m;
  for (k = 0; k < 3u && g_me == 1; k++) {
    if (!job(EP_J_SNAP, at[k], nw[k], 0, 0, 2000)) { me_restart("after the stop in the register reads", -1); return; }
    for (i = 0, m = 0; i < nw[k]; i++) m += snprintf(b + m, sizeof b - (size_t)m, " %08x", (unsigned)g_c->r[i]);
    out("mep: the ME's %08x-%08x read:%s\n", (unsigned)at[k], (unsigned)(at[k] + 4u * nw[k] - 4u), b);
  }
}
/* the four tests at eDRAM offset W_OFF, each a job of its own; 1: all four ran (each one's result in the log), 0: the ME stopped in
   one (it is started again with the turn's writes; the turn stops) */
static int ep_words(int t)
{
  char tc = "ABC"[t]; uint32_t k, v, a;
  for (k = 1; k <= 4u; k++) {
    v = 0xED000000u | ((uint32_t)t << 16) | (k << 8) | 0x5Au;
    a = (k == 3u ? EP_EDRAM : EP_EDRAM_UNC) + W_OFF;   /* the address the test writes (test 1: reads) */
    if (!job(EP_J_WORD, k, W_OFF, v, 0, 2000)) {
      out("mep %c: 0. test %u, %s at %08x: the ME stopped; the turn stops here\n", tc, (unsigned)k, k_word[k], (unsigned)a);
      stopped(t, k_word[k]); return 0;
    }
    if (k == 1u) out("mep %c: 0. test 1, %s at %08x: %08x, no stop\n", tc, k_word[1], (unsigned)a, (unsigned)g_c->r[0]);
    else out("mep %c: 0. test %u, %s at %08x: wrote %08x, read %08x (%s)\n", tc, (unsigned)k, k_word[k], (unsigned)a, (unsigned)v,
             (unsigned)g_c->r[0], g_c->r[0] == v ? "as written" : "DIFFERENT");
  }
  return 1;
}
static void ep_user_seg(void)
{
  if (g_me != 1) { out("mep: the user segment's read: not made (the ME is not running)\n"); return; }
  if (job(EP_J_WORD, 5, 0x00000100u, 0, 0, 2000))
    out("mep: the user segment: %s at 00000100: %08x, no stop (the user segment reaches the eDRAM for this read)\n", k_word[5],
        (unsigned)g_c->r[0]);
  else {
    out("mep: the user segment: %s at 00000100: the ME stopped, as at probe 1's first store\n", k_word[5]);
    me_restart("after the user segment's read", -1);
  }
}
#endif
static void ep_turn(int t, uint32_t *src1, uint32_t *dst1, uint32_t *chain1, uint32_t *big)
{
  char tc = "ABC"[t]; uint32_t s0 = 0x5EED0000u + 16u * (uint32_t)t, lo, hi, size = 0, bad;
  g_ran[t] = 1;
  if (g_me != 1) { out("mep %c: the ME is not running: the turn is skipped\n", tc); return; }
  if (!ep_apply(t)) { stopped(t, "its writes"); return; }   /* A: 0xBC100050 read; B, C: their writes made (again) and read */
#ifdef EP_P2
  if (!ep_words(t)) return;             /* 0. probe 2's one-word tests (a stop ends the turn) */
#endif
  /* 1. the first 2 MB */
  if (!ep_fill(t, "1. the first 2 MB", 0, 2 * T1M, s0)) return;
  lo = ep_check(t, "1. the first 2 MB", 0, 2 * T1M, s0);
  if (lo == NONE) return;
  if (lo) { out("mep %c: 1. the eDRAM did not keep what the ME wrote in its first 2 MB: the turn stops here\n", tc); return; }
  /* 2. 2-4 MB as a job of its own (a write past the eDRAM's end may stop the ME), then both halves */
  if (!job(EP_J_FILL, 2 * T1M, 2 * T1M, s0 + 1, 0, 10000)) {
    out("mep %c: 2. the ME stopped at its writes above 2 MB: the eDRAM is taken as 2 MB\n", tc);
    stopped(t, "the writes above 2 MB"); size = 2 * T1M;
    if (g_me != 1) return;
  } else {
    out("mep %c: 2. 2-4 MB: eDRAM 0200000-0400000 filled, %.2f ms\n", tc, me_ms(g_c->r[6]));
    lo = ep_check(t, "2. the first 2 MB again", 0, 2 * T1M, s0);
    if (lo == NONE) return;
    hi = ep_check(t, "2. 2-4 MB", 2 * T1M, 2 * T1M, s0 + 1);
    if (hi == NONE) return;
    if (!lo && !hi) size = 4 * T1M;
    else if (lo && !hi) { size = 2 * T1M; out("mep %c: 2. the writes above 2 MB changed the first 2 MB: the same 2 MB seen twice; 2 MB\n", tc); }
    else if (!lo && hi) { size = 2 * T1M; out("mep %c: 2. the words above 2 MB were not kept: 2 MB\n", tc); }
    else { out("mep %c: 2. both halves changed: the size is not known; the turn stops here\n", tc); return; }
  }
  out("mep %c: the eDRAM's size: %u MB\n", tc, (unsigned)(size >> 20));
  g_size[t] = size;
  /* 3. all of it filled again and checked after 2 s (the ME at rest meanwhile) */
  if (!ep_fill(t, "3. all", 0, size, s0 + 2)) return;
  sceKernelDelayThread(2000000);
  bad = ep_check(t, "3. all, after 2 s", 0, size, s0 + 2);
  if (bad == NONE) return;
  g_pass[t] = bad == 0;
  out("mep %c: 3. %s\n", tc, bad ? "words changed by themselves within 2 s: the eDRAM is not usable with this turn's writes"
                                 : "every word kept over 2 s");
  /* 4, 5 */
  ep_speeds(t, src1, dst1, chain1);
  if (g_me == 1) ep_share(t, big);
}

/* ---- the boot probe: after hwprobe.c's ---- */
static int g_from = -1, g_n = 100, g_ncyc; static int g_cyc[16]; static int g_m;
static int g_turn = -1, g_ekind = -1;   /* the game's turns: the eDRAM turn they use (-1: none) and its copy */
static uint32_t g_esize;
static void window_mark(void);
void __real_hw_probe(OutFn o);
void __wrap_hw_probe(OutFn o)
{
  const char *e = mgs2_getenv("MGS2_MEP"); uint32_t *big, *ord, *src1, *dst1, *chain1, *ord1, i; uint64_t t_all = now_us();
  int nturns = 1, t;
  __real_hw_probe(o);
  g_out = o; out = out_now;             /* the start's lines on the stick one by one */
  g_on = !(e && *e == '0');
  if (!g_on) { out("mep: MGS2_MEP=0: no ME probe, the ME stays off\n"); return; }
#ifdef EP_REG_B
  nturns = 2;
#endif
#ifdef EP_REG_C
  nturns = 3;
#endif
  out("mep: the eDRAM probe (step 3); this build's turns: A (the writes OKed for v30me6c only)%s%s\n",
      nturns > 1 ? ", B (the ME's 0xBC100050 = 0x0f)" : "", nturns > 2 ? ", C (B and 0xBCC00000 = ffffffff, 0xBCC00030 = 1, 0xBCC00040 = 1)" : "");
#ifdef EP_P2
  out("mep: probe 2: the eDRAM through the kernel segments, 0x80000000 cached and 0xA0000000 uncached (probe 1: 0x00000000 and "
      "0x40000000, where the ME stopped at its first store)\n");
#endif
  scr_open();
  me_start();
  if (g_me == 1) me_cal();
#ifdef EP_P2
  if (g_me == 1) ep_snap();
#endif
  big = scr_take(4u << 20); ord = scr_take(NL * sizeof(uint32_t));
  src1 = scr_take(T1M); dst1 = scr_take(T1M); chain1 = scr_take(T1M); ord1 = scr_take((T1M / 64u) * sizeof(uint32_t));
  if (!big || !ord || !src1 || !dst1 || !chain1 || !ord1) { out("mep: no memory for the boot probe's buffers: the turns are skipped\n"); goto turns; }
  g_rs = 4242u; make_chain(big, NL, 16, ord);
  g_rs = 777u; memset(chain1, 0, T1M); make_chain(chain1, T1M / 64u, 16, ord1);
  for (i = 0; i < T1M / 4; i++) src1[i] = (i * 0x9e3779b1u) ^ 0x5a5a5a5au;
  sceKernelDcacheWritebackInvalidateAll();
  k_main(big, &g_ka_m);
  out("mep: the main CPU's kernels alone: alu %.2f ns a round of 64 + 2, one load a line over 4 MB %.2f ns, chase over 4 MB %.2f ns a step\n",
      g_ka_m.alu, g_ka_m.lines, g_ka_m.chase);
  for (t = 0; t < nturns; t++) ep_turn(t, src1, dst1, chain1, big);
#ifdef EP_P2
  ep_user_seg();                        /* last: it may stop the ME (started again cold, with A's writes) */
#endif
turns:
  /* the game's turns: the first turn whose checks passed, with A's writes again by a cold start if a later turn's were made */
  for (t = 0; t < nturns; t++) if (g_pass[t] && g_kind[t] >= 0) { g_turn = t; break; }
#ifdef EP_P2
  if (g_me == 1 && g_turn != g_cur && (g_cur > 0 || g_turn > 0)) me_restart("for the game's turns", g_turn);
#else
  if (g_me == 1 && g_cur > 0 && g_turn != g_cur) me_restart("for the game's turns", g_turn);
#endif
  if (g_me == 1 && g_turn >= 0) {
    uint32_t seed = 0x5E7E0000u;
    g_esize = g_size[g_turn]; g_ekind = g_kind[g_turn];
    if (job(EP_J_FILL, EP_ARENA, g_esize - EP_ARENA, seed, 0, 10000)) {
      g_c->sent_off = EP_ARENA; g_c->sent_bytes = g_esize - EP_ARENA; g_c->sent_seed = seed; g_c->sent_first_off = NONE; SYNC();
    } else { me_restart("after the stop in the sentinel's fill", -1); g_turn = -1; }
  }
  if (g_me == 1) job(EP_J_FLUSH, 0, 0, 0, 0, 2000);   /* nothing of the boot buffers left in the ME's cache before they are zeroed */
  scr_close();                          /* the game's RAM zero again before the game is loaded */
  if ((e = mgs2_getenv("MGS2_MEP_CYCLE"))) {
    int k = 0; char *p;
    if (sscanf(e, "%d:%d%n", &g_from, &g_n, &k) != 2 || g_n <= 0 || e[k] != ':') g_from = -1;
    else for (p = (char *)e + k, g_ncyc = 0; (*p == ':' || *p == ',') && g_ncyc < 16; ) g_cyc[g_ncyc++] = (int)strtol(p + 1, &p, 10);
    if (!g_ncyc) g_from = -1;
  }
  window_mark();
  out("mep: the boot probe took %.2f s; turns %s (%d windows of %d vblanks from vb %d); the game's eDRAM windows %s%s%s%s; the ME %s; "
      "kicks %u (+%u while waiting, %.1f us each)\n", (now_us() - t_all) / 1e6, g_from >= 0 ? e : "none", g_ncyc, g_n, g_from,
      g_turn >= 0 ? "use turn " : "run as rest windows (no turn passed)", g_turn >= 0 ? (g_turn == 0 ? "A" : g_turn == 1 ? "B" : "C") : "",
      g_turn >= 0 ? ", copy " : "", g_turn >= 0 ? k_kind[g_ekind] : "", g_me == 1 ? "rests until its turns" : "is not running",
      (unsigned)g_kicks, (unsigned)g_rekicks, g_kicks ? (double)g_kick_us / g_kicks : 0);
  if (g_turn >= 0) out("mep: the sentinel: eDRAM %07x-%07x, 4 KB checked after each 64 KB block the ME copies\n", (unsigned)EP_ARENA,
                       (unsigned)g_esize);
  out = o;                              /* (the game's part writes through stderr only) */
}

/* ---- the turns ---- */
static uint32_t w_b, w_r, w_u, w_sl, w_ha, w_pe, w_nw, w_la, w_k, w_skb;
static void window_line(long vb)
{
  /* the counts since the last line are those of steps vb-100 .. vb-1 (gl14mp's rule) */
  int m = vb - 1 > g_from ? g_cyc[((vb - 1 - g_from - 1) / g_n) % g_ncyc] : 0;
  uint32_t b = 0, r = 0, u = 0, sl = 0, ha = 0, pe = 0, nw = 0, la = 0, skb = 0, sbad = 0, sfo = NONE, sfv = 0, sb0 = 0, sb1 = 0;
  if (g_me == 1) { b = g_c->s_blocks; r = g_c->s_rd; u = g_c->s_busy; sl = g_c->sleeps; ha = g_c->halted; pe = g_c->pending; nw = g_c->nowake;
                   la = g_c->late; skb = g_c->sent_kb; sbad = g_c->sent_bad; sfo = g_c->sent_first_off; sfv = g_c->sent_first_val;
                   sb0 = g_c->sent_blk[0]; sb1 = g_c->sent_blk[1]; }
  fprintf(stderr, "mep vb %ld: mode %d; ME %u blocks, copied %.2f MB, busy %.1f ms, rests %u (in the halt %u, before it %u, no interrupt %u, "
          "given up %u), kicks %u; sentinel %u KB checked, %u words changed in the run so far (first at %07x: %08x; blocks %08x %08x)\n",
          vb, m, (unsigned)(b - w_b), (r - w_r) * 64.0 / 1048576.0, g_mhz > 0 ? (u - w_u) / g_mhz / 1000.0 : 0, (unsigned)(sl - w_sl),
          (unsigned)(ha - w_ha), (unsigned)(pe - w_pe), (unsigned)(nw - w_nw), (unsigned)(la - w_la), (unsigned)(g_kicks - w_k),
          (unsigned)(skb - w_skb), (unsigned)sbad, (unsigned)(sfo == NONE ? 0 : sfo), (unsigned)sfv, (unsigned)sb1, (unsigned)sb0);
  if (g_me == 1 && ((m == 1 && g_turn >= 0) || m == 2) && b == w_b) me_dead("no stream block in a whole window");
  w_b = b; w_r = r; w_u = u; w_sl = sl; w_ha = ha; w_pe = pe; w_nw = nw; w_la = la; w_k = g_kicks; w_skb = skb;
}
static void window_mark(void)
{
  if (g_me == 1) { w_b = g_c->s_blocks; w_r = g_c->s_rd; w_u = g_c->s_busy; w_sl = g_c->sleeps; w_ha = g_c->halted; w_pe = g_c->pending;
                   w_nw = g_c->nowake; w_la = g_c->late; w_skb = g_c->sent_kb; }
  w_k = g_kicks;
}
static void ep_vb(long vb)
{
  int m;
  if (!g_on || g_from < 0) return;
  { uint32_t ca, epc, bv; if (g_me == 1 && hle_me_exception(&ca, &epc, &bv)) me_dead("checked at a vblank"); }
  m = vb > g_from ? g_cyc[((vb - g_from - 1) / g_n) % g_ncyc] : 0;   /* boot.c opt_window's slot: the step that starts now */
  if (m != g_m) {
    if (g_me == 1 && m == 1 && g_turn >= 0) { g_c->kind = (uint32_t)g_ekind; SYNC(); stream_set(EP_M_COPY); }
    else if (g_me == 1 && m == 2) { g_c->kind = EP_C_MAIN; SYNC(); stream_set(EP_M_COPY); }
    else stream_set(EP_M_REST);         /* (no control block is touched when the ME does not run: it may not exist) */
    g_m = m;
  }
  if (vb % 100 == 0 && vb > g_from) window_line(vb);
}
static void ep_end(const char *when)
{
  if (g_me != 1) return;
  g_c->mode = EP_M_REST; SYNC();
  if (g_turn >= 0 && g_c->sent_bytes) {
    uint32_t off = g_c->sent_off, n = g_c->sent_bytes, seed = g_c->sent_seed;
    if (job(EP_J_CHECK, off, n, seed, 0, 2000))
      fprintf(stderr, "mep: the sentinel checked whole at the end (%s): eDRAM %07x-%07x, %u words changed (first at %07x: %08x, the "
              "pattern's %08x; blocks %08x %08x); in the run %u KB checked, %u words changed\n", when, (unsigned)off, (unsigned)(off + n),
              (unsigned)g_c->r[0], (unsigned)(g_c->r[1] == NONE ? 0 : g_c->r[1]), (unsigned)g_c->r[2], (unsigned)g_c->r[3],
              (unsigned)g_c->r[5], (unsigned)g_c->r[4], (unsigned)g_c->sent_kb, (unsigned)g_c->sent_bad);
  }
  if (g_me == 1) {
    int acked = hle_me_stop(100); g_me = -1;
    fprintf(stderr, "mep: the ME stopped (%s), %s, held in reset; kicks %u (+%u while waiting)\n", when,
            acked ? "it acknowledged" : "no acknowledgement", (unsigned)g_kicks, (unsigned)g_rekicks);
  }
}

/* ---- gl1_glue.c's words and wraps, with the probe's (gl14mp's) ---- */
/* for psp/v26me_bat.c's battery lines: v[0] 0 off / 1 on / 3 stopped; v[1] heartbeat, v[2] ("pushed") stream blocks, v[3] ("consumed")
   rests, v[4] ("checkpoints") jobs, v[5] the loop's state */
void me_mix_state(uint32_t v[6])
{
  memset(v, 0, 6 * sizeof *v);
  if (!g_c) return;
  v[0] = g_me == 1 ? 1 : g_me < 0 ? 3 : 0; v[1] = g_c->hdr.heartbeat; v[2] = g_c->s_blocks; v[3] = g_c->sleeps; v[4] = g_c->done_seq;
  v[5] = g_c->hdr.state;
}
void me_mix_diag(uint32_t d[7])
{
  uint32_t c, e, b; HleMePower pw;
  memset(d, 0, 7 * sizeof *d);
  if (!g_c) return;
  d[1] = g_c->s_mode; d[2] = g_kicks;
  d[3] = hle_me_exception(&c, &e, &b) ? e : 0;
  hle_me_power_events(&pw); d[4] = pw.switches; d[5] = pw.parks; d[6] = pw.last_flags;
}
void __real_host_vb_begin(long vb);
void __wrap_host_vb_begin(long vb)
{
  __real_host_vb_begin(vb);             /* main_psp.c's step timing first: what follows falls between two steps */
  me_mix_last_vb = vb;
  me_exc_vb(vb);
  ep_vb(vb);
}
void __real_mgs2_exit(int code);
void __wrap_mgs2_exit(int code) { ep_end("at the end"); me_bat_line("end"); __real_mgs2_exit(code); }
int __real_sceKernelExitGame(void);
int __wrap_sceKernelExitGame(void)
{
  me_exc_release();
  ep_end("HOME");
  me_bat_line("HOME");
  me_bat_exit();
  return __real_sceKernelExitGame();
}
