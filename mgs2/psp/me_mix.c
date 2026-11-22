/* me_mix.c - v26me (2026-10-23): the PSP side of the voice mix split (boot/spu_split.c), engine B on the Media Engine.  Ffm chose the
   Media Engine for the game's sound (decision card, 18:28); this build measures it: what the full voice mix costs the Media Engine,
   what the split costs the main CPU (state-only engine A, the commands, the waits) and whether the main CPU's game work slows down while
   the Media Engine works (the shared bus).  Nothing is played: B's samples go into a checksum (boot/spu_split_b.h).
   Bring-up: PS2RC's hle_me.c (third_party/ps2rc/hle/psp, copied unchanged; its register sequence comes from mcidclan's
   me-feed-graphics-engine, MIT, credited in third_party/ps2rc/NOTICE.md): ARK-4's KUBridge (kuKernelCall) runs one function of this
   program in kernel mode that copies a reset stub to the Media Engine's reset vector and releases the ME's reset bit; the stub sets the
   ME's memory range and bus clock enables (not a clock speed), its exception handler (it records and halts) and jumps to me_mix_work
   (psp/me_mix_me.c).  scePowerLock is taken at the start and kept (hle_me.h: a suspend with the ME taken over would hang); the ME is
   held in reset again at the end of the run and before any exit.  The emulator does not emulate the ME: there the split stays off.
   Linked after v26's objects with the link's --wrap (psp/v26me.sh), so v26's code keeps its addresses:
     host_vb_begin      after main_psp.c's own: the ME's counters per step (me_steps.csv) and per window ("mix vb" lines), its
                        checkpoints ("mix: ck" lines), an exception or a stall (the split stops; the game goes on as v26)
     mgs2_exit          the end of the run: the last commands consumed, the totals, the ME stopped; then main_psp.c's end
     sceKernelExitGame  HOME: the ME stopped first
   The latest vblank and the split's state also go to psp/v26me_bat.c's battery lines (battery.txt), with a line at HOME and at the end.
   mgs2.cfg: MGS2_SPU_ME=1 turns the split on (without it this program is v26); MGS2_ME_STEPS_FROM=<vb> (default 24000) the first step
   of me_steps.csv.  MGS2_SPU_ME=2 is the emulator check (PPSSPP does not emulate the Media Engine): no Media Engine, the loop's work
   (me_mix_me.c built with -DME_SIM) is done by the main CPU whenever it would wait for the Media Engine (a full ring, a barrier, the
   end), so the ring, the barriers, the checkpoints and the reports run as on the console; its times are not the Media Engine's. */
#include <pspkernel.h>
#include <psppower.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "hle_me.h"
#if defined(HLE_ME_PROGRESS) || defined(ME_REST)
#include "hle_me_mgs2.h"                /* v26me2 (psp/v26me2.sh): the port's copy of the bring-up, with its progress words */
#endif
#include "ps2rc_exc.h"
#include "me_mix.h"
#include "spu_split.h"

volatile Ps2rcMeBlock ps2rc_me_block;   /* hle_me.c's link to PS2RC's sleep PRX: none here (psp/me/ps2rc_exc.h) */

#define RING_N 16384u                   /* commands (two words each): 128 KB */
#define CK_N 1024u                      /* checkpoints the ME can hold before the main CPU reads them */
#define REGION_BYTES (64u << 10)        /* the ME's own memory: engine B at the bottom, its stack at the top */
#define STACK_BYTES (32u << 10)
#define UNC(p) ((void *)(0x40000000u | (uint32_t)(uintptr_t)(p)))
#ifdef ME_REST
#define CTL_BYTES ((sizeof(MeMixCtl) + 63u) & ~63u)   /* v30me5: whole cache lines, so no other block shares the control block's last
                                                          line (a cached write there would write stale words of it back over the ME's) */
#else
#define CTL_BYTES sizeof(MeMixCtl)
#endif

static MeMixCtl *g_ctl_c; static volatile MeMixCtl *g_c;   /* the control block: allocated (cached address) / used (uncached) */
static uint32_t *g_ring_c; static volatile uint32_t *g_ring;
static uint32_t *g_ck_c; static volatile uint32_t *g_ck;
static uint8_t *g_region;
static int g_me;                        /* 1: the ME runs the loop; -1: it ran and was stopped */
static int g_sim;                       /* MGS2_SPU_ME=2: the main CPU drains the ring itself (me_mix_sim_drain) */
static uint32_t g_wr, g_free;           /* commands pushed; ring entries known free */
static uint32_t g_nck_seen;
static double g_mhz;                    /* the ME's Count rate, measured at the start */
/* the main CPU's waits: ring full / barrier (before an SPU RAM write) */
static uint64_t g_full_n, g_full_us, g_bar_n, g_bar_us, g_bar_wait_n, g_bar_max_us;
/* per window and per step */
static uint32_t g_busy0, g_rd0, g_ren0; static uint64_t g_busy_all;
static uint32_t g_w_busy, g_w_steps, g_w_maxstep; static uint64_t g_w_cmds0, g_w_fr0;
static long g_steps_from = 24000; static uint32_t (*g_st)[3]; static long g_nst, g_cap;   /* per step: ME busy (ticks), commands, renders */
volatile long me_mix_last_vb = -1;      /* for psp/v26me_bat.c's battery lines: the latest vblank */
int me_bat_line(const char *why) __attribute__((weak));   /* psp/v26me_bat.c: a battery line (HOME, the end) */

static uint64_t now_us(void) { return (uint64_t)sceKernelGetSystemTimeWide(); }

#ifdef ME_DIAG3
/* v26me3 (psp/v26me3.sh, after v26me2's run ended in a power-off): battery.txt's lines every 5 s carry these words too, so the file shows
   the last seconds before a stop: the Media Engine's idle polls, the last command it applied, the EPC of an exception it took (0 if none),
   and the power callback's counts (power switch, parks) and last flags (hle_me_mgs2.c).  And the Media Engine's busy time from its idle
   polls (its Count register reads 0 on the PSP): polls per second with an empty ring, measured in the 100 ms after its start (before
   the split records anything), against the polls of each window: busy = the share of the window it did not poll. */
static double g_idle_hz;
static uint32_t g_w_idle0; static uint64_t g_w_t0;
void me_mix_diag(uint32_t d[7])
{
  uint32_t c, e = 0, b; HleMePower pw;
  d[0] = g_c ? g_c->idle : 0; d[1] = g_c ? g_c->last_w0 : 0; d[2] = g_c ? g_c->last_w1 : 0;
  d[3] = g_c && hle_me_exception(&c, &e, &b) ? e : 0;
  hle_me_power_events(&pw); d[4] = pw.switches; d[5] = pw.parks; d[6] = pw.last_flags;
}
#endif
#ifdef ME_EXC
/* the crash catcher (-DME_EXC): v26me3's psp/v26me_exc.c halts the Media Engine after its record; v26me4's psp/v26me4_exc.c just spins */
void me_mix_halt(void) { if (g_me == 1 && !g_sim) { hle_me_stop(50); g_me = -1; } }
void me_exc_vb(long vb);
void me_exc_release(void);
#endif

#ifdef ME_REST
/* v30me5 (-DME_REST, psp/v30me5.sh; Ffm chose "Build it", decision card 2026-10-26 20:43): the resting Media Engine (psp/me_mix_me.c,
   psp/me_rest.S).  Once its idle polls are measured (split_plat_start) the ME may rest when it has nothing to do (MGS2_ME_REST=0: never,
   it polls as in v30).  After each push the main CPU looks at asleep and, once per rest (by sleeps), interrupts the ME: hle_me_kick,
   one write of 1 to 0xBC100044 in kernel mode (psp/me/hle_me_mgs2.c).  While it waits for the ME (a full ring, a barrier, the end) it
   interrupts it again every 200 us that the ME rests with commands queued and no progress (a lost wake: not expected; counted).  The
   2-second stall check stays: the mix stops, the ME is parked, the game goes on, exact (me_dead).  hle_me_stop interrupts the ME after
   its exit request (hle_me_mgs2.c), so a resting ME leaves its loop as before. */
static int g_rest;                      /* the ME may rest */
static uint32_t g_kicked = 0xFFFFFFFFu; /* the ME's sleeps at the last kick after a push */
static uint64_t g_kicks, g_rekicks, g_kick_us;   /* kicks after a push, kicks while waiting, the time of the first */
static uint32_t g_w_sl, g_w_ha, g_w_pe, g_w_nw, g_w_la; static uint64_t g_w_k, g_w_ku, g_w_rk;   /* window starts ("mix vb" lines) */
#ifndef ME_DIAG3
#error ME_REST needs ME_DIAG3 (the lines it adds to)
#endif
static void me_kick_check(void)
{
  uint32_t s; uint64_t t0;
  if (!g_c->asleep) return;
  s = g_c->sleeps;
  if (s == g_kicked) return;
  g_kicked = s; t0 = now_us();
  hle_me_kick(); g_kicks++;
  g_kick_us += now_us() - t0;
}
/* for psp/v26me_bat.c's battery lines: r[0] may rest, [1] asleep, [2] rests, [3] ended in the halt, [4] before it, [5] with no
   interrupt, [6] given up, [7] kicks, [8] kicks while waiting, [9] every flag bit the ME found, [10] request word changes, [11..15]
   the request words at the ME's last wake (me_mix.h) */
void me_mix_rest(uint32_t r[16])
{
  int i;
  for (i = 0; i < 16; i++) r[i] = 0;
  if (!g_c || g_sim) return;
  r[0] = g_rest; r[1] = g_c->asleep; r[2] = g_c->sleeps; r[3] = g_c->halted; r[4] = g_c->pending; r[5] = g_c->nowake; r[6] = g_c->late;
  r[7] = (uint32_t)g_kicks; r[8] = (uint32_t)g_rekicks; r[9] = g_c->flags_or; r[10] = g_c->rpc_changes;
  for (i = 0; i < 5; i++) r[11 + i] = g_c->rpc[i];
}
int me_mix_me_live(void) { return g_me == 1 && !g_sim; }   /* the ME was started and not stopped (the battery log's kernel reads) */
#endif

/* for psp/v26me_bat.c's battery lines (its own thread): 0 off / 1 on / 2 simulated / 3 stopped, then the Media Engine's heartbeat,
   the commands pushed and consumed, the checkpoints and the loop's state */
void me_mix_state(uint32_t v[6])
{
  v[0] = g_me == 1 ? (g_sim ? 2 : 1) : g_me < 0 ? 3 : 0;
  v[1] = g_c ? g_c->hdr.heartbeat : 0; v[2] = g_wr; v[3] = g_c ? g_c->rd : 0; v[4] = g_c ? g_c->nck : 0; v[5] = g_c ? g_c->hdr.state : 0;
}

SplitB *split_plat_b(void)
{
  if (!g_region) {
    g_region = memalign(64, REGION_BYTES); g_ctl_c = memalign(64, CTL_BYTES);
    g_ring_c = memalign(64, RING_N * 8); g_ck_c = memalign(64, CK_N * 32);
    if (!g_region || !g_ctl_c || !g_ring_c || !g_ck_c) return 0;
    memset(g_region, 0, REGION_BYTES); memset(g_ctl_c, 0, CTL_BYTES); memset(g_ring_c, 0, RING_N * 8); memset(g_ck_c, 0, CK_N * 32);
    sceKernelDcacheWritebackInvalidateRange(g_ctl_c, CTL_BYTES);   /* from now on only through the uncached alias */
    sceKernelDcacheWritebackInvalidateRange(g_ring_c, RING_N * 8);
    sceKernelDcacheWritebackInvalidateRange(g_ck_c, CK_N * 32);
    g_c = UNC(g_ctl_c); g_ring = UNC(g_ring_c); g_ck = UNC(g_ck_c);
  }
  return (SplitB *)g_region;            /* spu_split.c writes B's start state here (cached; hle_me_start writes the caches back) */
}

int split_plat_start(char *why, int n)
{
  char w[160]; int r, pw; uint32_t c0, c1; uint64_t t0, t1;
  if (!g_region) { snprintf(why, n, "no memory"); return 0; }
  if (getenv("MGS2_ME_STEPS_FROM")) g_steps_from = atol(getenv("MGS2_ME_STEPS_FROM"));
  g_c->ring = (uint32_t)(uintptr_t)g_ring; g_c->mask = RING_N - 1; g_c->ck = (uint32_t)(uintptr_t)g_ck; g_c->ckmask = CK_N - 1;
  g_c->b = (uint32_t)(uintptr_t)g_region; g_c->wr = 0; g_c->rd = 0; g_c->nck = 0; g_c->busy = 0; g_c->renders = 0; g_c->maxr = 0; g_c->idle = 0;
  if (getenv("MGS2_SPU_ME") && atoi(getenv("MGS2_SPU_ME")) == 2) {
    g_sim = 1; g_me = 1; g_free = RING_N; g_mhz = 1.0;   /* times in us of the system clock */
    snprintf(why, n, "simulated: no Media Engine, the main CPU drains the ring whenever it would wait (an emulator check, not a timing)");
    return 1;
  }
  pw = hle_me_power_init();
  r = hle_me_start(me_mix_work, &g_c->hdr, g_region, REGION_BYTES, STACK_BYTES, w, sizeof w);
#ifdef HLE_ME_PROGRESS
  { char pg[400]; hle_me_progress(pg, sizeof pg); fprintf(stderr, "me: %s\n", pg); }
#endif
#ifdef ME_SONY
  { char sl[600]; hle_me_sony_line(sl, sizeof sl); fprintf(stderr, "me start: %s\n", sl); }   /* v30me6 */
#endif
  if (!r) { snprintf(why, n, "Media Engine not started: %s; power callback %d", w, pw); return 0; }
  g_me = 1; g_free = RING_N;
  /* the ME's Count rate against the system clock, over 100 ms (the loop writes its Count at every poll) */
#ifdef ME_DIAG3
  { uint32_t i0, i1;
    t0 = now_us(); c0 = g_c->hdr.count; i0 = g_c->idle; sceKernelDelayThread(100000); t1 = now_us(); c1 = g_c->hdr.count; i1 = g_c->idle;
    g_idle_hz = t1 > t0 ? (double)(uint32_t)(i1 - i0) * 1e6 / (double)(t1 - t0) : 0;
    g_w_idle0 = i1; g_w_t0 = t1; }
#else
  t0 = now_us(); c0 = g_c->hdr.count; sceKernelDelayThread(100000); t1 = now_us(); c1 = g_c->hdr.count;
#endif
  g_mhz = t1 > t0 ? (double)(uint32_t)(c1 - c0) / (double)(t1 - t0) : 0;
  {
    uint32_t r40, r50; hle_me_regs_found(&r40, &r50);
    snprintf(why, n, "Media Engine started: heartbeat %u, Count %.1f MHz, scePowerLock %d, power callback %d, ME registers found "
             "%08x / %08x, reset register %08x", (unsigned)g_c->hdr.heartbeat, g_mhz, hle_me_power_lock_rc(), pw, (unsigned)r40,
             (unsigned)r50, (unsigned)hle_me_reset_reg_seen());
#ifdef ME_DIAG3
    { size_t k = strlen(why); if ((int)k < n) snprintf(why + k, n - k, ", idle polls %.0f per second", g_idle_hz); }
#endif
  }
#ifdef ME_REST
  {   /* after the idle polls are measured: the ME may rest (MGS2_ME_REST=0: never); its own line (why is cut at 159 characters) with
         the idle polls and the request words as the main CPU reads them and as the ME found them at its start */
    uint32_t w[5]; int rk;
    g_rest = !getenv("MGS2_ME_REST") || atoi(getenv("MGS2_ME_REST")) != 0;
    g_c->rest = (uint32_t)g_rest;
    rk = hle_me_rpc_words(w);
    fprintf(stderr, "me rest: %s; idle polls %.0f per second; Sony's ME request words 0xBFC00600 %08x, 608 %08x, 700 %08x, 710 %08x, "
            "714 %08x (kernel read %d), as the ME found them at its start %08x %08x %08x %08x %08x\n",
            g_rest ? "the ME rests when idle (v30me5)" : "the ME polls when idle (MGS2_ME_REST=0)", g_idle_hz,
            (unsigned)w[0], (unsigned)w[1], (unsigned)w[2], (unsigned)w[3], (unsigned)w[4], rk, (unsigned)g_c->rpc[0],
            (unsigned)g_c->rpc[1], (unsigned)g_c->rpc[2], (unsigned)g_c->rpc[3], (unsigned)g_c->rpc[4]);
  }
#endif
  g_busy0 = g_c->busy; g_rd0 = 0; g_ren0 = 0;
  return 1;
}

static void me_dead(const char *what)
{
  uint32_t cause, epc, bad; char b[200];
  if (hle_me_exception(&cause, &epc, &bad))
    snprintf(b, sizeof b, "%s; the Media Engine took an exception: Cause %08x EPC %08x BadVAddr %08x", what, (unsigned)cause, (unsigned)epc, (unsigned)bad);
  else
    snprintf(b, sizeof b, "%s; Media Engine heartbeat %u, state %u, consumed %u of %u", what, (unsigned)g_c->hdr.heartbeat,
             (unsigned)g_c->hdr.state, (unsigned)g_c->rd, (unsigned)g_wr);
  split_stop(b);
  hle_me_park(); g_me = -1;
}

/* wait (spin on the uncached word) until cond; 0 = the ME made no progress for 2 s */
static int wait_rd(uint32_t want_free, uint64_t *us)
{
  uint64_t t0 = now_us(), t = t0; uint32_t last = g_c->rd, prog = (uint32_t)t0;
#ifdef ME_REST
  uint32_t lk = (uint32_t)t0;           /* the last kick while waiting */
#endif
  if (g_sim) { me_mix_sim_drain(g_c); *us += now_us() - t0; return 1; }
  for (;;) {
    uint32_t rd = g_c->rd, i;
    if (want_free ? g_wr - rd <= RING_N - want_free : rd == g_wr) { *us += now_us() - t0; return 1; }
    for (i = 0; i < 64; i++) __asm__ volatile("nop");
    t = now_us();
    if (rd != last) { last = rd; prog = (uint32_t)t; }
    else if ((uint32_t)t - prog > 2000000u) { *us += t - t0; return 0; }
#ifdef ME_REST
    else if (g_rest && (uint32_t)t - prog > 200u && (uint32_t)t - lk > 200u && g_c->asleep) { lk = (uint32_t)t; hle_me_kick(); g_rekicks++; }
#endif
  }
}

void split_plat_push(uint32_t w0, uint32_t w1)
{
  if (g_me != 1) return;
  if (!g_free) {
    g_free = RING_N - (g_wr - g_c->rd);
    if (!g_free) {
      g_full_n++;
      if (!wait_rd(RING_N / 4, &g_full_us)) { me_dead("the command ring stayed full for 2 s"); return; }
      g_free = RING_N - (g_wr - g_c->rd);
    }
  }
  g_ring[2 * (g_wr & (RING_N - 1))] = w0; g_ring[2 * (g_wr & (RING_N - 1)) + 1] = w1;
  __asm__ volatile("sync" ::: "memory");
  g_c->wr = ++g_wr; g_free--;
#ifdef ME_REST
  if (g_rest) { __asm__ volatile("sync" ::: "memory"); me_kick_check(); }   /* wr in memory before asleep is read (me_mix_me.c) */
#endif
}

void split_plat_barrier(void)
{
  uint64_t us = 0;
  if (g_me != 1) return;
  g_bar_n++;
  if (g_c->rd == g_wr) return;
  g_bar_wait_n++;
  if (!wait_rd(0, &us)) { g_bar_us += us; me_dead("a barrier waited 2 s"); return; }
  g_bar_us += us; if (us > g_bar_max_us) g_bar_max_us = us;
}

void split_plat_ram_written(uint32_t addr, uint32_t size)
{
  if (g_me != 1) return;
  sceKernelDcacheWritebackRange((uint8_t *)((SplitB *)g_region)->b.ram + addr, size);   /* the ME reads SPU RAM from memory after its RAMW */
}

static void collect_cks(void)
{
  uint32_t nck = g_c->nck;
  while (g_nck_seen != nck) {
    volatile uint32_t *e = g_ck + 8 * (g_nck_seen & (CK_N - 1)); SplitCk k; char x[64];
    k.n = e[0]; k.frames_lo = e[1]; k.frames_hi = e[2]; k.crc = e[3]; k.vhash = e[4]; k.cmds = e[5]; k.pad[0] = k.pad[1] = 0;
    snprintf(x, sizeof x, " (ME Count %08x)", (unsigned)e[6]);
    split_ck_b(&k, x); g_nck_seen++;
  }
}

static double ms(uint64_t ticks) { return g_mhz > 0 ? ticks / g_mhz / 1000.0 : 0; }

void __real_host_vb_begin(long vb);
void __wrap_host_vb_begin(long vb)
{
  uint32_t busy, rd, ren, db;
  __real_host_vb_begin(vb);             /* main_psp.c's step timing first: what follows falls between two steps */
  me_mix_last_vb = vb;
#ifdef ME_EXC
  me_exc_vb(vb);                        /* v26me3: psp/v26me_exc.c's MGS2_EXC_DRY (an emulator check of the crash report) */
#endif
  if (g_me != 1) return;
  { uint32_t c, e, b; if (!g_sim && hle_me_exception(&c, &e, &b)) { me_dead("checked at a vblank"); return; } }
  busy = g_c->busy; rd = g_c->rd; ren = g_c->renders;
  db = busy - g_busy0; g_busy_all += db;
  if (vb > g_steps_from) {
    if (!g_st) { g_cap = 8000; g_st = malloc(g_cap * sizeof *g_st); }   /* 96 KB: the timed steps of either route (1,200 / 6,000) */
    if (g_st && g_nst < g_cap) { g_st[g_nst][0] = db; g_st[g_nst][1] = rd - g_rd0; g_st[g_nst][2] = ren - g_ren0; g_nst++; }
  }
  g_w_busy += db; g_w_steps++; if (db > g_w_maxstep) g_w_maxstep = db;
  g_busy0 = busy; g_rd0 = rd; g_ren0 = ren;
  collect_cks();
  if (vb % 100 == 0) {
    uint64_t cm = split_cmds(), fr = split_frames();
    fprintf(stderr, "mix vb %ld: ME busy %.2f ms per step (max %.2f), %.1f commands and %.0f frames per step, ring full %llu (%.1f ms), "
            "barriers %llu waited %llu (%.1f ms, max %.2f ms), longest render %.3f ms\n", vb,
            g_w_steps ? ms(g_w_busy) / g_w_steps : 0, ms(g_w_maxstep), g_w_steps ? (double)(cm - g_w_cmds0) / g_w_steps : 0,
            g_w_steps ? (double)(fr - g_w_fr0) / g_w_steps : 0, (unsigned long long)g_full_n, g_full_us / 1000.0,
            (unsigned long long)g_bar_n, (unsigned long long)g_bar_wait_n, g_bar_us / 1000.0, g_bar_max_us / 1000.0, ms(g_c->maxr));
#ifdef ME_DIAG3
    { uint32_t id = g_c->idle; uint64_t t = now_us(); double sec = (double)(t - g_w_t0) / 1e6;
      double f = g_idle_hz > 0 && sec > 0 ? (double)(uint32_t)(id - g_w_idle0) / (g_idle_hz * sec) : 0;
#ifdef ME_REST
      if (g_rest) {                     /* v30me5: a resting ME polls only before its rests, so its polls no longer give its busy time */
        uint32_t sl = g_c->sleeps, ha = g_c->halted, pe = g_c->pending, nw = g_c->nowake, la = g_c->late;
        uint64_t k = g_kicks - g_w_k;
        fprintf(stderr, "mix vb %ld: ME rests %u (woken in the halt %u, before it %u, with no interrupt %u; given up %u), idle polls %u in "
                "%.2f s; kicks %llu (%.1f us each) and %llu while waiting; now %s; its flags %08x (all bits seen %08x); request words "
                "%08x %08x %08x %08x %08x (changed at %u wakes)\n", vb, (unsigned)(sl - g_w_sl), (unsigned)(ha - g_w_ha),
                (unsigned)(pe - g_w_pe), (unsigned)(nw - g_w_nw), (unsigned)(la - g_w_la), (unsigned)(id - g_w_idle0), sec,
                (unsigned long long)k, k ? (double)(g_kick_us - g_w_ku) / (double)k : 0.0, (unsigned long long)(g_rekicks - g_w_rk),
                g_c->asleep ? "resting" : "awake", (unsigned)g_c->flags_last, (unsigned)g_c->flags_or, (unsigned)g_c->rpc[0],
                (unsigned)g_c->rpc[1], (unsigned)g_c->rpc[2], (unsigned)g_c->rpc[3], (unsigned)g_c->rpc[4], (unsigned)g_c->rpc_changes);
        g_w_sl = sl; g_w_ha = ha; g_w_pe = pe; g_w_nw = nw; g_w_la = la; g_w_k = g_kicks; g_w_ku = g_kick_us; g_w_rk = g_rekicks;
      } else
#endif
      if (g_idle_hz > 0)                /* a Media Engine (not the emulator's simulation) */
        fprintf(stderr, "mix vb %ld: ME idle polls %u in %.2f s (%.1f%% of an idle ME's), so busy %.2f ms per step\n", vb,
                (unsigned)(id - g_w_idle0), sec, 100 * f, g_w_steps && f < 1 ? (1 - f) * sec * 1000 / g_w_steps : 0);
      g_w_idle0 = id; g_w_t0 = t; }
#endif
    g_w_busy = g_w_steps = g_w_maxstep = 0; g_w_cmds0 = cm; g_w_fr0 = fr;
  }
}

static void me_end(const char *when)
{
  static int done;
  if (done++ || !g_me) return;
  if (g_me == 1) {
    uint64_t us = 0; int drained = wait_rd(0, &us), acked;
    collect_cks();
    fprintf(stderr, "mix %s: ME busy %.1f ms in all (Count %.1f MHz), %u renders, idle polls %u; ring full %llu times (%.1f ms), "
            "barriers %llu, waited %llu (%.1f ms, max %.2f ms); last commands %s\n", when, ms(g_busy_all), g_mhz,
            (unsigned)g_c->renders, (unsigned)g_c->idle, (unsigned long long)g_full_n, g_full_us / 1000.0, (unsigned long long)g_bar_n,
            (unsigned long long)g_bar_wait_n, g_bar_us / 1000.0, g_bar_max_us / 1000.0, drained ? "consumed" : "NOT consumed");
#ifdef ME_REST
    if (!g_sim)
      fprintf(stderr, "mix %s: ME rests %u (woken in the halt %u, before it %u, with no interrupt %u; given up %u); kicks %llu (%.1f ms "
              "in all) and %llu while waiting; its flags %08x (all bits seen %08x); request words %08x %08x %08x %08x %08x (changed at "
              "%u wakes)\n", when, (unsigned)g_c->sleeps, (unsigned)g_c->halted, (unsigned)g_c->pending, (unsigned)g_c->nowake,
              (unsigned)g_c->late, (unsigned long long)g_kicks, g_kick_us / 1000.0, (unsigned long long)g_rekicks,
              (unsigned)g_c->flags_last, (unsigned)g_c->flags_or, (unsigned)g_c->rpc[0], (unsigned)g_c->rpc[1], (unsigned)g_c->rpc[2],
              (unsigned)g_c->rpc[3], (unsigned)g_c->rpc[4], (unsigned)g_c->rpc_changes);
#endif
    if (g_sim) { g_me = -1; fprintf(stderr, "mix: simulation ended\n"); }
    else { acked = hle_me_stop(100); g_me = -1; fprintf(stderr, "mix: Media Engine stopped (%s), held in reset\n", acked ? "it acknowledged" : "no acknowledgement"); }
  }
  split_report(when);
  if (g_nst) {
    char p[256]; FILE *f; long i; const char *d = getenv("MGS2_DISC"), *sl = d ? strrchr(d, '/') : 0;   /* main_psp.c: <folder>mgs2disc.pak */
    snprintf(p, sizeof p, "%.*sme_steps.csv", sl ? (int)(sl - d + 1) : 0, sl ? d : "");
    if ((f = fopen(p, "w"))) {
      fprintf(f, "vb,me_busy_us,cmds,renders\n");
      for (i = 0; i < g_nst; i++) fprintf(f, "%ld,%.1f,%u,%u\n", g_steps_from + 1 + i, ms(g_st[i][0]) * 1000.0, (unsigned)g_st[i][1], (unsigned)g_st[i][2]);
      fclose(f); fprintf(stderr, "me_steps.csv written (%ld steps)\n", g_nst);
    }
  }
}

void __real_mgs2_exit(int code);
void __wrap_mgs2_exit(int code) { me_end("at the end"); if (me_bat_line) me_bat_line("end"); __real_mgs2_exit(code); }

int __real_sceKernelExitGame(void);
int __wrap_sceKernelExitGame(void)
{
  int lock;
#ifdef ME_EXC
  me_exc_release();                     /* v26me3: the crash catcher's handler given back before anything else (tt_port lessons A10) */
#endif
  if (g_me == 1 && !g_sim) { hle_me_stop(50); g_me = -1; }
  if (me_bat_line) me_bat_line("HOME");
  hle_me_power_shutdown(&lock);
  return __real_sceKernelExitGame();
}
