/* me_probe.c - gl14mp (2026-11-02, task #132; mgs2/me_probe_notes.md, me_picture_study.md section 7): the Media Engine probe, the main
   CPU's side.  gl14's game with nothing changed; this unit gives what psp/gl1_glue.c gives gl14 (its three wraps and the battery log's
   two calls: psp/gl14mp.sh renames those five in gl14's gl1_glue.o, which stays linked where it was, so every gl14 function keeps its
   address; gl1_glue.o's me_mix_last_vb stays the word of the latest vblank) and adds:
     __wrap_hw_probe  after the existing hardware probe, before the game: the ME started as v30me6c started it (psp/me/hle_me_mgs2.c
                      with -DME_SONY -DME_SONY_BOOT -DME_KADDR -DME_REST: Sony's start, kernel helpers at their kernel alias, the ME
                      resting between jobs), its clock calibrated, then the probes: copies by the main CPU, the ME (plain) and the
                      system's DMA copy (4), the kernels' speed on both cores (2), each core's cost to the other (3, at rest), the
                      generated code on the ME, and last the two in which an op the ME might not take could stop it: the float ops on
                      both cores (1) and the ME's copy with claimed lines (if the ME stopped in one, it is started again, cold, for
                      the turns); every "mep" line in log.txt.  Its big buffers lie in game RAM before the game is loaded (zero
                      before and after, checked: scr_open); the control block, the ME's region and the ring in user memory
                      outside the heap (kept_take), so the game's heap is gl14's.
     host_vb_begin    after main_psp.c's step timing (so outside the timed steps): the turns MGS2_MEP_CYCLE=<vb>:<n>:<m>,<m>...
                      (boot.c opt_window's slot rule): m 0 the ME at rest, 1 the paced stream, 2 the burst stream, 3 the DMA copy
                      (a helper thread, 3.4 MB per 150 ms in 64 KB copies, only when the boot probe found that the wait in
                      sceDmacMemcpy gives the CPU back); a "mep vb" line per 100-vblank window with the ME's and the DMA's counts.
     mgs2_exit, sceKernelExitGame   the helper and the ME stopped (the ME held in reset), then gl1_glue.c's battery line and handler.
   The game's state is not touched: the ME and the DMA read game RAM only; they write the ring (64 KB), the ME's region and the
   control block.  MGS2_MEP=0 in mgs2.cfg: nothing of this runs (gl14 with the ME off).  The emulator has no ME (hle_me_start refuses
   there): its parts are skipped and said so; the main CPU's parts and the DMA run as the emulator models them. */
#include <pspkernel.h>
#include <psppower.h>
#include <pspdmac.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "hle_me.h"
#include "hle_me_mgs2.h"
#include "ps2rc_exc.h"
#include "me_probe.h"

typedef void (*OutFn)(const char *fmt, ...);
volatile Ps2rcMeBlock ps2rc_me_block;   /* hle_me_mgs2.c's link to PS2RC's sleep PRX: none here */
extern volatile long me_mix_last_vb;    /* psp/gl1_glue.c: the latest vblank (-1 before the first), for battery.txt and the crash record */
extern uint8_t g_mem[];                 /* the game's RAM (runtime/guest.h): read only here */
extern uint32_t g_cde_ok;               /* boot.c: main_psp.c's claimed-line self-test passed */
void rt_qcopy_nov(uint8_t *d, const uint8_t *s, uint32_t n);
char *mgs2_getenv(const char *k);
int me_bat_line(const char *why);
void me_bat_exit(void);
void me_exc_vb(long vb);
void me_exc_release(void);

const char *const mep_fp_name[MEP_FP_OPS] = { "add.s", "sub.s", "mul.s", "div.s", "sqrt.s", "abs.s", "neg.s", "cvt.s.w", "cvt.w.s",
  "trunc.w.s", "round.w.s", "ceil.w.s", "floor.w.s", "c.eq.s", "c.lt.s", "c.le.s", "c.un.s" };

#define UNC(p) ((void *)(0x40000000u | (uint32_t)(uintptr_t)(p)))
#define CTL_BYTES ((sizeof(MepCtl) + 63u) & ~63u)
#define REGION_BYTES (16u << 10)        /* the ME's own memory: its stack at the top */
#define STACK_BYTES (8u << 10)
#define RING_BYTES (64u << 10)          /* the streams' and the DMA's destination */
#define SRC_OFF 0x00400000u             /* the streams' window in game RAM: 4 MB in, 3,456 KB long (a step with transfers reads about
                                           3.4 MB: me_picture_study.md section 3) */
#define SRC_BYTES 0x00360000u
#define DM_CHUNK (64u << 10)
#define SYNC() __asm__ volatile("sync" ::: "memory")

static OutFn out;
static MepCtl *g_ctl_c; static volatile MepCtl *g_c;
static uint8_t *g_region, *g_ring;
static int g_me;                        /* 1: the ME runs mep_work; 0: not started; -1: started, then stopped or parked */
static int g_on;                        /* the probe runs (MGS2_MEP is not 0) */
static double g_mhz;                    /* the ME's Count ticks per microsecond (0: not known) */
static uint32_t g_seq;
static uint32_t g_kicked = 0xFFFFFFFFu; static uint32_t g_kicks, g_rekicks; static uint64_t g_kick_us, g_t_post;

static uint64_t now_us(void) { return (uint64_t)sceKernelGetSystemTimeWide(); }
static uint32_t fcr_get(void) { uint32_t v; __asm__ volatile("cfc1 %0, $31" : "=r"(v)); return v; }
static void fcr_set(uint32_t v) { __asm__ volatile(".set push\n.set noreorder\nctc1 %0, $31\nnop\nnop\n.set pop" :: "r"(v)); }
static uint32_t g_rs = 2463534242u;
static uint32_t rnd(void) { g_rs ^= g_rs << 13; g_rs ^= g_rs >> 17; g_rs ^= g_rs << 5; return g_rs; }

/* ---- the ME: start, jobs, kicks, failure ---- */
static void kick_check(void)            /* wake a resting ME, once per rest (psp/me_mix.c's me_kick_check) */
{
  uint32_t s; uint64_t t0;
  if (g_me != 1 || !g_c->asleep) return;
  s = g_c->sleeps;
  if (s == g_kicked) return;
  g_kicked = s; t0 = now_us();
  hle_me_kick(); g_kicks++;
  g_kick_us += now_us() - t0;
}
static void me_dead(const char *what)
{
  uint32_t ca, ep, bv;
  if (hle_me_exception(&ca, &ep, &bv))
    fprintf(stderr, "mep: %s; the ME took an exception: Cause %08x EPC %08x BadVAddr %08x; it is held in reset, the probe stops\n",
            what, (unsigned)ca, (unsigned)ep, (unsigned)bv);
  else
    fprintf(stderr, "mep: %s; ME heartbeat %u, state %u, job %u of %u, mode %u; it is held in reset, the probe stops\n", what,
            (unsigned)g_c->hdr.heartbeat, (unsigned)g_c->hdr.state, (unsigned)g_c->done_seq, (unsigned)g_seq, (unsigned)g_c->s_mode);
  hle_me_park(); g_me = -1;
}
static void post(uint32_t job, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
  g_c->a[0] = a0; g_c->a[1] = a1; g_c->a[2] = a2; g_c->a[3] = a3; g_c->job = job;
  SYNC(); g_t_post = now_us(); g_c->job_seq = ++g_seq; SYNC();
  kick_check();
}
/* wait for the posted job (polls every 50 us, the main CPU asleep between them); 0: an exception or no answer in ms */
static int job_wait(uint32_t ms, uint64_t *wall)
{
  uint64_t t, lk = now_us(); uint32_t ca, ep, bv;
  while (g_c->done_seq != g_seq) {
    if (hle_me_exception(&ca, &ep, &bv)) { me_dead("during a job"); return 0; }
    t = now_us();
    if (t - g_t_post > (uint64_t)ms * 1000u) { me_dead("a job got no answer"); return 0; }
    if (g_c->asleep && t - lk > 500u) { hle_me_kick(); g_rekicks++; lk = t; }
    sceKernelDelayThread(50);
  }
  if (wall) *wall = now_us() - g_t_post;
  return 1;
}
static int job(uint32_t j, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t ms)
{
  if (g_me != 1) return 0;
  post(j, a0, a1, a2, a3);
  return job_wait(ms, 0);
}
static double me_ns(uint32_t ticks, double n) { return g_mhz > 0 ? (double)ticks * 1000.0 / g_mhz / n : 0; }
static double ma_ns(uint64_t us, double n) { return (double)us * 1000.0 / n; }

/* ---- memory.  The boot probe's big buffers: game RAM before the game is loaded.  main_psp.c calls hw_probe before mgs2_main loads
   the game, so g_mem is still all zero (.bss); the probe takes g_mem + 16 MB .. + 24 MB, checks first that it is zero, and zeroes it
   again at the end, out of both caches (checked), so the game starts on the same RAM.  In the first gl14mp build these buffers were
   malloc'd: a small block kept above them held the heap's top up over 2 MB of free chunks, 287 KB of them still free
   at the run's end, and the game's highest heap point was about 420 KB above gl14's.  The control block, the ME's region and the
   ring (81 KB, kept for the turns) come from the user memory outside the heap (764 KB free there at the start of gl14's emulator run,
   736 KB at the end of gl13's PSP run), so the game's heap is gl14's but for the probe's code. ---- */
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
static void *scr_take(uint32_t n)       /* 64-byte aligned; given back with scr_give(the mark read before) */
{
  void *p;
  n = (n + 63u) & ~63u;
  if (!g_scr_ok || n > SCR_BYTES - g_scr_used) return 0;
  p = g_mem + SCR_OFF + g_scr_used; g_scr_used += n;
  if (g_scr_used > g_scr_max) g_scr_max = g_scr_used;
  return p;
}
static void scr_give(uint32_t mark) { g_scr_used = mark; }
static void scr_close(void)             /* after the ME's last job on it (every job ends with the ME's D-cache written back and empty) */
{
  uint32_t nz;
  if (!g_scr_ok) return;
  memset(g_mem + SCR_OFF, 0, SCR_BYTES);
  sceKernelDcacheWritebackInvalidateAll(); sceKernelIcacheInvalidateAll();
  nz = scr_nonzero(); g_scr_ok = 0;
  out("mep scratch: %u KB used at most, zeroed again: %u words not zero%s\n", (unsigned)(g_scr_max >> 10), (unsigned)nz,
      nz ? " (THE GAME'S RAM DIFFERS FROM GL14'S: the hashes will show it)" : "");
}
static uint8_t *kept_take(uint32_t n, const char **where)
{
  SceUID b = sceKernelAllocPartitionMemory(2, "mep_kept", PSP_SMEM_Low, n + 64, 0);
  if (b >= 0) { *where = "user memory outside the heap"; return (uint8_t *)(((uintptr_t)sceKernelGetBlockHeadAddr(b) + 63) & ~(uintptr_t)63); }
  *where = "the heap (no block outside it)";
  return memalign(64, n);
}

static int me_start(void)
{
  char why[200], pg[400]; int r, pw; const char *where = ""; uint8_t *k = kept_take(CTL_BYTES + REGION_BYTES + RING_BYTES, &where);
  if (!k) { out("mep: no memory for the control block, the ME's region and the ring\n"); return 0; }
  g_ctl_c = (MepCtl *)k; g_region = k + CTL_BYTES; g_ring = g_region + REGION_BYTES;
  memset(k, 0, CTL_BYTES + REGION_BYTES + RING_BYTES);
  out("mep: the control block, the ME's region and the ring (%u KB) at %08x, in %s\n", (unsigned)((CTL_BYTES + REGION_BYTES + RING_BYTES) >> 10),
      (unsigned)(uintptr_t)k, where);
  sceKernelDcacheWritebackInvalidateAll();   /* from now on the control block only through the uncached alias */
  g_c = UNC(g_ctl_c);
  g_c->fcr31 = 0x01000001u; g_c->rest = 0; g_c->mode = MEP_M_REST;
  g_c->src = (uint32_t)(uintptr_t)(g_mem + SRC_OFF); g_c->src_bytes = SRC_BYTES;
  g_c->ring = (uint32_t)(uintptr_t)g_ring; g_c->ring_bytes = RING_BYTES;
  pw = hle_me_power_init();
  r = hle_me_start(mep_work, &g_c->hdr, g_region, REGION_BYTES, STACK_BYTES, why, sizeof why);
  hle_me_progress(pg, sizeof pg); out("mep me: %s\n", pg);
  { char sl[600]; hle_me_sony_line(sl, sizeof sl); out("mep me start: %s\n", sl); }
  if (!r) { out("mep: the ME was not started: %s; power callback %d; its parts are skipped\n", why, pw); return 0; }
  g_me = 1;
  { uint32_t r40, r50; hle_me_regs_found(&r40, &r50);
    out("mep: the ME started: heartbeat %u, scePowerLock %d, power callback %d, ME registers found %08x / %08x, reset register %08x, "
        "its FCR31 at the start %08x\n", (unsigned)g_c->hdr.heartbeat, hle_me_power_lock_rc(), pw, (unsigned)r40, (unsigned)r50,
        (unsigned)hle_me_reset_reg_seen(), (unsigned)g_c->fcr31_found); }
  return 1;
}

/* Count against the microsecond clock over 200 ms, through the word the ME rewrites every ~3 us */
static void me_cal(void)
{
  uint64_t t1, t2; uint32_t c1, c2;
  g_c->cal_run = 1; SYNC();
  post(MEP_J_CAL, 0, 0, 0, 0);
  sceKernelDelayThread(10000);
  t1 = now_us(); c1 = g_c->cal; sceKernelDelayThread(200000); t2 = now_us(); c2 = g_c->cal;
  g_c->cal_run = 0; SYNC();
  if (!job_wait(2000, 0)) return;
  g_mhz = t2 > t1 ? (double)(uint32_t)(c2 - c1) / (double)(t2 - t1) : 0;
  out("mep clock: the ME's Count runs at %.3f MHz (%u ticks in %u us)%s\n", g_mhz, (unsigned)(c2 - c1), (unsigned)(t2 - t1),
      c2 == c1 ? ": it does not count, so the ME's times below are 0 (only the main CPU's wall time of each job)" : "");
  g_c->rest = 1; SYNC();                /* from now on the ME rests between jobs: no polls on the bus during the main CPU's own tests */
}

/* ---- probe 1: the float ops ---- */
static const uint32_t k_special[40] = {
  0x00000000u, 0x80000000u, 0x00000001u, 0x80000001u, 0x007fffffu, 0x807fffffu, 0x00400000u, 0x00800000u, 0x80800000u, 0x00800001u,
  0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffc00000u, 0x7f800001u, 0xffffffffu, 0xfffc56e0u, 0x3f800000u,
  0xbf800000u, 0x3f000000u, 0x3effffffu, 0x3f7fffffu, 0x3fc00000u, 0x40000000u, 0x4b000000u, 0x4b7fffffu, 0x4effffffu, 0x4f000000u,
  0xcf000000u, 0xcf000001u, 0x5f000000u, 0x2f800000u, 0x0b800000u, 0x1f800000u, 0x3eaaaaabu, 0x3f000001u, 0x3fbfffffu, 0x34000000u };
static void probe_fp(void)
{
  const uint32_t NS = 40, NP = 40 * 40 + 65536; uint32_t mk = g_scr_used, *in = scr_take(NP * 8), *om = scr_take(NP * 4), *oe = scr_take(NP * 4);
  uint32_t i, j, k, op, all_eq = 1;
  if (!in || !om || !oe) { out("mep fp: no memory\n"); goto end; }
  for (k = 0, i = 0; i < NS; i++) for (j = 0; j < NS; j++, k++) { in[2 * k] = k_special[i]; in[2 * k + 1] = k_special[j]; }
  g_rs = 0x1234567u; for (; k < NP; k++) { in[2 * k] = rnd(); in[2 * k + 1] = rnd(); }
  out("mep fp: %u input pairs (the 40 special values in all pairs, then random words), each op on both cores under FCR31 01000001; "
      "one-operand ops use the first of each pair\n", (unsigned)NP);
  for (op = 0; op < MEP_FP_OPS; op++) {
    uint32_t fs = fcr_get(), fm, fe = 0, te = 0, eq = 0, shown = 0; uint64_t t0, t1;
    char ex[3][96];
    fcr_set(0x01000001u); t0 = now_us(); fm = mep_fp_run(op, in, NP, om); t1 = now_us(); fcr_set(fs);
    if (g_me != 1) { out("mep fp %-9s: main CPU only (%.2f ms; FCR31 after %08x)\n", mep_fp_name[op], (t1 - t0) / 1000.0, (unsigned)fm); continue; }
    sceKernelDcacheWritebackInvalidateAll();
    if (!job(MEP_J_FP, op, (uint32_t)(uintptr_t)in, NP, (uint32_t)(uintptr_t)oe, 5000)) {
      out("mep fp %-9s: the ME stopped in this op (main %.2f ms, FCR31 after %08x)\n", mep_fp_name[op], (t1 - t0) / 1000.0, (unsigned)fm);
      break; }
    fe = g_c->r[0]; te = g_c->r[1];
    sceKernelDcacheInvalidateRange(oe, NP * 4);
    for (k = 0; k < NP; k++) {
      if (om[k] == oe[k]) { eq++; continue; }
      if (shown < 3) snprintf(ex[shown++], sizeof ex[0], " [%08x %08x: main %08x, ME %08x]", (unsigned)in[2 * k], (unsigned)in[2 * k + 1], (unsigned)om[k], (unsigned)oe[k]);
    }
    if (eq != NP) all_eq = 0;
    out("mep fp %-9s: equal %u of %u; FCR31 after: main %08x, ME %08x; main %.2f ms, ME %.2f ms%s%s%s%s\n", mep_fp_name[op],
        (unsigned)eq, (unsigned)NP, (unsigned)fm, (unsigned)fe, (t1 - t0) / 1000.0, me_ns(te, 1e6), shown ? "; first differing:" : "",
        shown > 0 ? ex[0] : "", shown > 1 ? ex[1] : "", shown > 2 ? ex[2] : "");
  }
  if (g_me == 1) out("mep fp: %s\n", all_eq ? "every result equal bit for bit on the two cores" : "some results differ (lines above)");
end:
  scr_give(mk);
}

/* ---- probe 2: the kernels' speed ---- */
static void make_chain(uint32_t *p, uint32_t n, uint32_t stride, uint32_t *ord)   /* hwprobe.c's: a random cycle over n slots */
{
  uint32_t i;
  for (i = 0; i < n; i++) ord[i] = i;
  for (i = n - 1; i > 0; i--) { uint32_t j = rnd() % (i + 1), t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
  for (i = 0; i < n; i++) p[ord[i] * stride] = ord[(i + 1) % n] * stride;
}
static void line_jump(uint32_t *code, uint32_t i, int32_t to)   /* hwprobe.c's */
{
  uint32_t *w = code + i * 16;
  w[0] = to < 0 ? 0x03e00008u : 0x08000000u | (((uint32_t)(uintptr_t)(code + (uint32_t)to * 16) >> 2) & 0x03ffffffu);
  w[1] = 0;
}
typedef struct { double alu, falu, lines, chase; } KTimes;
static KTimes g_ka_m, g_ka_e;           /* alone */
#define NALU 100000u
#define NFALU 50000u
#define NL (65536u)                     /* 4 MB of 64-byte lines */
static void k_main(uint32_t *big, KTimes *t)
{
  uint64_t t0, t1;
  t0 = now_us(); mep_k_alu(NALU); t1 = now_us(); t->alu = ma_ns(t1 - t0, NALU);
  t0 = now_us(); mep_k_falu(NFALU); t1 = now_us(); t->falu = ma_ns(t1 - t0, NFALU);
  sceKernelDcacheWritebackInvalidateAll();
  t0 = now_us(); mep_k_lines(big, NL); t1 = now_us(); t->lines = ma_ns(t1 - t0, NL);
  sceKernelDcacheWritebackInvalidateAll();
  t0 = now_us(); mep_k_chase(big, 0, NL); t1 = now_us(); t->chase = ma_ns(t1 - t0, NL);
}
static int k_me(uint32_t *big, KTimes *t)
{
  if (!job(MEP_J_KERN, MEP_K_ALU, NALU, 0, 0, 5000)) return 0; t->alu = me_ns(g_c->r[0], NALU);
  if (!job(MEP_J_KERN, MEP_K_FALU, NFALU, 0, 0, 5000)) return 0; t->falu = me_ns(g_c->r[0], NFALU);
  if (!job(MEP_J_KERN, MEP_K_LINES, (uint32_t)(uintptr_t)big, NL, 0, 5000)) return 0; t->lines = me_ns(g_c->r[0], NL);
  if (!job(MEP_J_KERN, MEP_K_CHASE, (uint32_t)(uintptr_t)big, 0, NL, 5000)) return 0; t->chase = me_ns(g_c->r[0], NL);
  return 1;
}
static void k_line(const char *what, const KTimes *a, const KTimes *b, const KTimes *ra, const KTimes *rb)
{
  if (ra) out("mep %s: alu %.2f ns a round (x%.3f of alone), falu %.2f (x%.3f), one load a line %.2f ns (x%.3f), chase %.2f ns (x%.3f)\n",
              what, a->alu, a->alu / ra->alu, a->falu, a->falu / ra->falu, a->lines, a->lines / ra->lines, a->chase, a->chase / ra->chase);
  else if (b) out("mep %s: alu %.2f ns a round of 64 + 2 (ME %.2f, ME/main %.3f), falu %.2f ns a round of 32 + 2 (ME %.2f, %.3f), one "
                  "load a line over 4 MB %.2f ns (ME %.2f, %.3f), chase over 4 MB %.2f ns a step (ME %.2f, %.3f)\n", what,
                  a->alu, b->alu, b->alu / a->alu, a->falu, b->falu, b->falu / a->falu, a->lines, b->lines, b->lines / a->lines,
                  a->chase, b->chase, b->chase / a->chase);
  else out("mep %s: alu %.2f ns a round of 64 + 2, falu %.2f ns a round of 32 + 3, one load a line over 4 MB %.2f ns, chase over 4 MB "
           "%.2f ns a step\n", what, a->alu, a->falu, a->lines, a->chase);
  (void)rb;
}
static void probe_code(void)
{
  const uint32_t nl = (1u << 20) / 64, small = 32;
  uint32_t mk = g_scr_used, *code = scr_take(1u << 20), *ord = scr_take(nl * sizeof(uint32_t)), i; uint64_t t0, t1; void (*fn)(void);
  double m1 = 0, e1 = 0, m2 = 0, e2 = 0, m3 = 0, e3 = 0;
  if (!code || !ord) { out("mep icache: no memory\n"); scr_give(mk); return; }
  memset(code, 0, 1u << 20);
  for (i = 0; i < nl; i++) ord[i] = i;
  g_rs = 999u;
  for (i = nl - 1; i > 0; i--) { uint32_t j = rnd() % (i + 1), t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
  for (i = 0; i + 1 < nl; i++) line_jump(code, ord[i], (int32_t)ord[i + 1]);
  line_jump(code, ord[nl - 1], -1);
  sceKernelDcacheWritebackAll(); sceKernelIcacheInvalidateAll();
  fn = (void (*)(void))(void *)(code + ord[0] * 16);
  t0 = now_us(); fn(); t1 = now_us(); m1 = ma_ns(t1 - t0, nl);
  if (job(MEP_J_KERN, MEP_K_CODE, (uint32_t)(uintptr_t)fn, 0, 0, 5000)) e1 = me_ns(g_c->r[0], nl);
  memset(code, 0, small * 64);
  for (i = 0; i + 1 < small; i++) line_jump(code, i, (int32_t)(i + 1));
  line_jump(code, small - 1, -1);
  /* the in-cache chain: the ME runs it once (misses) inside its job, so its number is per pass of 32 lines; the main CPU's after a
     warm pass, 512 passes */
  sceKernelDcacheWritebackAll(); sceKernelIcacheInvalidateAll();
  fn = (void (*)(void))(void *)code; fn();
  t0 = now_us(); for (i = 0; i < nl / small; i++) fn(); t1 = now_us(); m2 = ma_ns(t1 - t0, nl);
  memset(code, 0, 1u << 20);
  code[(1u << 18) - 2] = 0x03e00008u; code[(1u << 18) - 1] = 0;
  sceKernelDcacheWritebackAll(); sceKernelIcacheInvalidateAll();
  fn = (void (*)(void))(void *)code;
  t0 = now_us(); fn(); t1 = now_us(); m3 = ma_ns(t1 - t0, nl);
  if (job(MEP_J_KERN, MEP_K_CODE, (uint32_t)(uintptr_t)fn, 0, 0, 5000)) e3 = me_ns(g_c->r[0], nl);
  sceKernelIcacheInvalidateAll();
  if (g_me == 1)
    out("mep icache: a jump to a new line (1 MB, random order) main %.1f ns, ME %.1f ns (%.3f); straight-line code main %.1f ns a "
        "64-byte line, ME %.1f ns (%.3f); main in cache %.1f ns a jump\n", m1, e1, m1 > 0 ? e1 / m1 : 0, m3, e3, m3 > 0 ? e3 / m3 : 0, m2);
  else out("mep icache, main CPU only: a jump to a new line (1 MB, random order) %.1f ns; straight-line code %.1f ns a 64-byte line; in "
           "cache %.1f ns a jump\n", m1, m3, m2);
  (void)e2;
  scr_give(mk);
}

/* ---- probe 4: the copies, and the DMA's wait ---- */
static uint32_t verify(const uint32_t *d, const uint32_t *s, uint32_t n)
{
  uint32_t i, bad = 0;
  for (i = 0; i < n / 4; i++) bad += d[i] != s[i];
  return bad;
}
static volatile uint32_t g_spin_n; static volatile int g_spin_stop;
static int spin_thread(SceSize n, void *a) { (void)n; (void)a; while (!g_spin_stop) g_spin_n++; return 0; }
static int g_dm_yields;                 /* the wait in sceDmacMemcpy gives the CPU back (the DMA turns run only then) */
static double g_cp_me_plain, g_cp_me_cde, g_cp_dmac;   /* MB/s */
#define CPN (2u << 20)
static void probe_copy(void)
{
  uint32_t mk = g_scr_used, *src = scr_take(CPN), *dst = scr_take(CPN), i, bad; uint64_t t0, t1; int rc;
  if (!src || !dst) { out("mep copy: no memory\n"); goto end; }
  for (i = 0; i < CPN / 4; i++) src[i] = (i * 0x9e3779b1u) ^ 0x5a5a5a5au;
#define MAIN_COPY(name, stmt) do { memset(dst, 0xEE, CPN); sceKernelDcacheWritebackInvalidateAll(); t0 = now_us(); stmt; t1 = now_us(); \
    bad = verify(dst, src, CPN); out("mep copy 2 MB, main %s: %.2f ms, %.1f MB/s, %u wrong words\n", name, (t1 - t0) / 1000.0, \
    (double)CPN / (double)(t1 - t0), (unsigned)bad); } while (0)
  MAIN_COPY("memcpy", memcpy(dst, src, CPN));
  if (g_cde_ok) MAIN_COPY("rt_qcopy_nov (claimed lines)", rt_qcopy_nov((uint8_t *)dst, (const uint8_t *)src, CPN));
  MAIN_COPY("mep_k_copy", mep_k_copy(dst, src, CPN));
  if (g_cde_ok) MAIN_COPY("mep_k_copy_cde", mep_k_copy_cde(dst, src, CPN));
  if (g_me == 1) {                      /* the ME's plain copy (its claimed-line form comes last of all: probe_me_cde) */
    double ms, wb;
    memset(dst, 0xEE, CPN); sceKernelDcacheWritebackInvalidateAll();
    if (!job(MEP_J_COPY, MEP_C_PLAIN, (uint32_t)(uintptr_t)dst, (uint32_t)(uintptr_t)src, CPN, 5000)) goto end;
    bad = verify(dst, src, CPN); ms = me_ns(g_c->r[0], 1e6); wb = me_ns(g_c->r[1], 1e6);
    g_cp_me_plain = ms > 0 ? CPN / ms / 1000.0 : 0;
    out("mep copy 2 MB, ME plain: %.2f ms, %.1f MB/s, then its write-back %.3f ms; %u wrong words\n", ms, g_cp_me_plain, wb, (unsigned)bad);
  }
  { static const uint32_t sz[5] = { 256, 4096, 65536, 262144, CPN }, reps[5] = { 64, 64, 32, 8, 2 };
    for (i = 0; i < 5; i++) {
      uint32_t r, nbad; memset(dst, 0xEE, CPN); sceKernelDcacheWritebackInvalidateAll(); rc = 0;
      t0 = now_us(); for (r = 0; r < reps[i] && rc >= 0; r++) rc = sceDmacMemcpy(dst, src, sz[i]); t1 = now_us();
      sceKernelDcacheInvalidateRange(dst, CPN);
      nbad = verify(dst, src, sz[i]);
      if (sz[i] == CPN && rc >= 0) g_cp_dmac = (double)CPN * reps[i] / (double)(t1 - t0);
      out("mep copy, DMA (sceDmacMemcpy) %u bytes x %u: %.2f us a call, %.1f MB/s, result %08x, %u wrong words\n", (unsigned)sz[i],
          (unsigned)reps[i], (double)(t1 - t0) / reps[i], (double)sz[i] * reps[i] / (double)(t1 - t0), (unsigned)rc, (unsigned)nbad);
    } }
  {   /* does sceDmacTryMemcpy return before the copy is done? (the last word read through the uncached alias) */
    volatile uint32_t *u = (volatile uint32_t *)UNC(dst); uint64_t t2; uint32_t last = CPN / 4 - 1, at_ret;
    memset(dst, 0xEE, CPN); sceKernelDcacheWritebackInvalidateAll();
    t0 = now_us(); rc = sceDmacTryMemcpy(dst, src, CPN); t1 = now_us(); at_ret = u[last] == src[last];
    t2 = t1; while (u[last] != src[last] && now_us() - t1 < 100000u) {} t2 = now_us();
    sceKernelDcacheInvalidateRange(dst, CPN);
    out("mep copy, DMA (sceDmacTryMemcpy) 2 MB: result %08x, the call %.1f us, the last word %s at its return, there after %.1f us; "
        "%u wrong words\n", (unsigned)rc, (double)(t1 - t0), at_ret ? "copied" : "not copied", (double)(t2 - t0),
        (unsigned)verify(dst, src, CPN));
  }
  {   /* the wait in sceDmacMemcpy: a thread of lower priority counts while the main thread is in the call */
    SceUID th = sceKernelCreateThread("mep_spin", spin_thread, 0x70, 0x1000, 0, 0); uint32_t n0, n1; double rate, given;
    if (th < 0) { out("mep copy, DMA wait: no thread (%08x)\n", (unsigned)th); goto end; }
    g_spin_stop = 0; g_spin_n = 0; sceKernelStartThread(th, 0, 0);
    n0 = g_spin_n; t0 = now_us(); sceKernelDelayThread(20000); t1 = now_us(); n1 = g_spin_n;
    rate = (double)(uint32_t)(n1 - n0) / (double)(t1 - t0);
    sceKernelDcacheWritebackInvalidateAll();
    n0 = g_spin_n; t0 = now_us(); rc = sceDmacMemcpy(dst, src, CPN); t1 = now_us(); n1 = g_spin_n;
    given = rate > 0 ? (double)(uint32_t)(n1 - n0) / rate / (double)(t1 - t0) : 0;
    g_spin_stop = 1; { SceUInt to = 200000; sceKernelWaitThreadEnd(th, &to); } sceKernelDeleteThread(th);
    g_dm_yields = rc >= 0 && given > 0.5;
    out("mep copy, DMA wait: in sceDmacMemcpy of 2 MB (%.1f us, result %08x) the main CPU ran another thread %.0f%% of the time "
        "(its count %.2f a us); the DMA turns %s\n", (double)(t1 - t0), (unsigned)rc, 100 * given, rate,
        g_dm_yields ? "run" : "do not run (the wait holds the CPU)");
  }
end:
  scr_give(mk);
}

/* the ME's copy with claimed lines (cache 0x18 on the ME): checked on 64 KB, then timed on 2 MB.  The burst stream does not use it
   (plain stores, as the boot probe's share test).  Last of the boot probe with the floats: an op the ME does not take stops it, and
   it is then started again for the turns (me_restart) */
static void probe_me_cde(void)
{
  uint32_t mk = g_scr_used, *src, *dst, i, bad; double ms, wb;
  if (g_me != 1) return;
  src = scr_take(CPN); dst = scr_take(CPN);
  if (!src || !dst) { out("mep copy, ME with claimed lines: no memory\n"); goto end; }
  for (i = 0; i < CPN / 4; i++) src[i] = (i * 0x9e3779b1u) ^ 0x5a5a5a5au;
  memset(dst, 0xEE, CPN); sceKernelDcacheWritebackInvalidateAll();
  if (!job(MEP_J_COPY, MEP_C_CDE, (uint32_t)(uintptr_t)dst, (uint32_t)(uintptr_t)src, 64u << 10, 5000)) {
    out("mep copy 64 KB, ME with claimed lines (cache 0x18): the ME stopped in it\n"); goto end; }
  bad = verify(dst, src, 64u << 10);
  out("mep copy 64 KB, ME with claimed lines (cache 0x18), the check: %u wrong words\n", (unsigned)bad);
  if (bad) goto end;
  memset(dst, 0xEE, CPN); sceKernelDcacheWritebackInvalidateAll();
  if (!job(MEP_J_COPY, MEP_C_CDE, (uint32_t)(uintptr_t)dst, (uint32_t)(uintptr_t)src, CPN, 5000)) goto end;
  bad = verify(dst, src, CPN); ms = me_ns(g_c->r[0], 1e6); wb = me_ns(g_c->r[1], 1e6);
  g_cp_me_cde = ms > 0 ? CPN / ms / 1000.0 : 0;
  out("mep copy 2 MB, ME with claimed lines: %.2f ms, %.1f MB/s, then its write-back %.3f ms; %u wrong words\n", ms, g_cp_me_cde, wb,
      (unsigned)bad);
end:
  scr_give(mk);
}

/* the ME stopped in the floats or the claimed lines: started again for the turns, cold (its caches set up with Index Store Tag, so no
   line the stopped ME held is written back over memory the main CPU has freed and used since).  It polls until the start has seen
   its heartbeat, then may rest, as at the first start */
static void me_restart(void)
{
  char why[200]; int r;
  memset((void *)g_c, 0, CTL_BYTES);
  g_c->fcr31 = 0x01000001u; g_c->mode = MEP_M_REST; g_c->rest = 0;
  g_c->src = (uint32_t)(uintptr_t)(g_mem + SRC_OFF); g_c->src_bytes = SRC_BYTES;
  g_c->ring = (uint32_t)(uintptr_t)g_ring; g_c->ring_bytes = RING_BYTES;
  SYNC(); g_seq = 0; g_kicked = 0xFFFFFFFFu;
  hle_me_set_cold(1);
  r = hle_me_start(mep_work, &g_c->hdr, g_region, REGION_BYTES, STACK_BYTES, why, sizeof why);
  hle_me_set_cold(0);
  if (r) { g_me = 1; g_c->rest = 1; SYNC(); }
  out("mep: the ME started again, cold, for the turns: %s\n", r ? "running" : why);
}

/* ---- the DMA helper thread: 64 KB copies from game RAM to the ring, one every g_dm_period us (0 = without a pause).  (No double
   sums or conversions to integers here or anywhere in this unit: gl14's objects use no libgcc helper for those, and a new one would
   be taken from its archive earlier than in gl14's link and move the libraries' code; psp/gl14mp.sh checks it.) ---- */
#define DM_PERIOD_US 2757u              /* 64 KB every 2,757 us: 3.4 MB (3,565,158 bytes) per 150 ms */
static SceUID g_dm_th = -1, g_dm_ev = -1;
static volatile int g_dm_on, g_dm_quit; static volatile uint32_t g_dm_period;
static volatile uint32_t g_dm_calls, g_dm_err, g_dm_rc, g_dm_skip; static volatile uint64_t g_dm_bytes, g_dm_us, g_dm_wb_us;
static int dm_thread(SceSize n, void *a)
{
  uint32_t off = 0, n_on = 0; uint64_t t_on = 0;
  (void)n; (void)a;
  while (!g_dm_quit) {
    uint64_t t0, t1, t2; int rc;
    if (!g_dm_on) { sceKernelWaitEventFlag(g_dm_ev, 1, PSP_EVENT_WAITOR | PSP_EVENT_WAITCLEAR, 0, 0); t_on = 0; continue; }
    if (!t_on) { t_on = now_us(); n_on = 0; }
    if (g_dm_period) {
      uint64_t due = t_on + (uint64_t)n_on * g_dm_period, t = now_us();
      if (t < due) { sceKernelDelayThread((SceUInt)(due - t)); continue; }
      if (t - due >= g_dm_period) {     /* late by a whole period or more (the thread did not get the CPU): those copies are dropped,
                                           not made in a burst, so the traffic keeps its rate; counted for the window line */
        uint32_t k = (uint32_t)((t - due) / g_dm_period); n_on += k; g_dm_skip += k; }
    }
    t0 = now_us();
    sceKernelDcacheWritebackRange(g_mem + SRC_OFF + off, DM_CHUNK);   /* as a hand-off would: the source's lines to memory first */
    t1 = now_us();
    rc = sceDmacMemcpy(g_ring, g_mem + SRC_OFF + off, DM_CHUNK);
    t2 = now_us();
    g_dm_calls++; g_dm_us += t2 - t1; g_dm_wb_us += t1 - t0;
    n_on++;                             /* (failed calls paced too) */
    if (rc < 0) { g_dm_err++; g_dm_rc = (uint32_t)rc; } else g_dm_bytes += DM_CHUNK;
    off += DM_CHUNK; if (off + DM_CHUNK > SRC_BYTES) off = 0;
  }
  return 0;
}
static void dm_set(int on, uint32_t period_us)
{
  if (g_dm_th < 0) return;
  g_dm_period = period_us; g_dm_on = on;
  if (on) sceKernelSetEventFlag(g_dm_ev, 1);
}
static void dm_init(void)
{
  if (!g_dm_yields) return;
  g_dm_ev = sceKernelCreateEventFlag("mep_dmac", 0, 0, 0);
  if (g_dm_ev < 0) { out("mep dma thread: no event flag (%08x)\n", (unsigned)g_dm_ev); return; }
  g_dm_th = sceKernelCreateThread("mep_dmac", dm_thread, 0x18, 0x2000, 0, 0);
  if (g_dm_th < 0 || sceKernelStartThread(g_dm_th, 0, 0) < 0) { out("mep dma thread: not started (%08x)\n", (unsigned)g_dm_th); g_dm_th = -1; }
}

/* ---- probe 3 at rest: each core's cost to the other ---- */
static void stream_set(uint32_t m) { if (g_me == 1) { g_c->mode = m; SYNC(); if (m != MEP_M_REST) kick_check(); } }
static void probe_share(uint32_t *big)
{
  uint32_t m, b0, r0, w0, u0; uint64_t t0, t1; KTimes kt;
  if (g_me == 1) for (m = MEP_M_PACED; m <= MEP_M_BURST; m++) {
    stream_set(m); sceKernelDelayThread(20000);
    b0 = g_c->s_blocks; r0 = g_c->s_rd; w0 = g_c->s_wr; u0 = g_c->s_busy; t0 = now_us();
    k_main(big, &kt);
    t1 = now_us();
    { uint32_t db = g_c->s_blocks - b0, dr = g_c->s_rd - r0, dw = g_c->s_wr - w0; double us = (double)(t1 - t0);
      stream_set(MEP_M_REST);
      k_line(m == MEP_M_PACED ? "share, the ME's paced stream on, the main CPU's kernels" : "share, the ME's burst stream on, the main CPU's kernels",
             &kt, 0, &g_ka_m, 0);
      out("mep share: the ME's %s stream meanwhile %u blocks, read %.1f MB/s, wrote %.1f MB/s, busy %.0f%% (Count)%s\n",
          m == MEP_M_PACED ? "paced" : "burst", (unsigned)db, dr * 64.0 / us, dw * 64.0 / us, g_mhz > 0 ? 100.0 * (g_c->s_busy - u0) / g_mhz / us : 0,
          m == MEP_M_BURST ? (g_c->cde ? ", ring lines claimed" : ", ring lines read first (plain stores)") : ""); }
    sceKernelDelayThread(5000);
  }
  if (g_me == 1) {                      /* the ME's kernels while the main CPU copies 1 MB over and over */
    uint32_t mk = g_scr_used, *cs = scr_take(1u << 20), *cd = scr_take(1u << 20), nc, k; KTimes ke; double *f[4] = { &ke.alu, &ke.falu, &ke.lines, &ke.chase };
    static const uint32_t kk[4] = { MEP_K_ALU, MEP_K_FALU, MEP_K_LINES, MEP_K_CHASE };
    if (cs && cd) {
      memset(cs, 1, 1u << 20); sceKernelDcacheWritebackInvalidateAll(); nc = 0; t0 = now_us();
      for (k = 0; k < 4 && g_me == 1; k++) {
        uint32_t a1 = kk[k] == MEP_K_ALU ? NALU : kk[k] == MEP_K_FALU ? NFALU : (uint32_t)(uintptr_t)big;
        uint32_t a2 = kk[k] == MEP_K_LINES ? NL : 0, a3 = kk[k] == MEP_K_CHASE ? NL : 0, ca, ep, bv;
        post(MEP_J_KERN, kk[k], a1, a2, a3);
        while (g_c->done_seq != g_seq) {
          mep_k_copy(cd, cs, 1u << 20); nc++;
          if (hle_me_exception(&ca, &ep, &bv) || now_us() - g_t_post > 5000000u) { me_dead("a job got no answer (main CPU copying)"); break; }
        }
        if (g_me == 1) *f[k] = me_ns(g_c->r[0], kk[k] == MEP_K_ALU ? NALU : kk[k] == MEP_K_FALU ? NFALU : NL);
      }
      t1 = now_us();
      if (g_me == 1) {
        k_line("share, the main CPU copying, the ME's kernels", &ke, 0, &g_ka_e, 0);
        out("mep share: the main CPU meanwhile copied %.1f MB/s (mep_k_copy, 1 MB a pass)\n", (double)nc * (1u << 20) / (double)(t1 - t0));
      }
    } else out("mep share: no memory for the main CPU's copy\n");
    scr_give(mk);
  }
  if (g_dm_th >= 0) {                   /* both cores' kernels while the DMA copies without a pause */
    uint64_t b0d = g_dm_bytes, u0d = g_dm_us; KTimes ke;
    dm_set(1, 0); sceKernelDelayThread(5000); t0 = now_us();
    k_main(big, &kt);
    t1 = now_us();
    k_line("share, the DMA copying, the main CPU's kernels", &kt, 0, &g_ka_m, 0);
    if (g_me == 1 && k_me(big, &ke)) k_line("share, the DMA copying, the ME's kernels", &ke, 0, &g_ka_e, 0);
    dm_set(0, 0);
    out("mep share: the DMA meanwhile copied %.1f MB/s over the main CPU's kernels (%.1f ms in its calls)\n",
        (double)(g_dm_bytes - b0d) / (double)(t1 - t0), (g_dm_us - u0d) / 1000.0);
    sceKernelDelayThread(5000);
  }
}

/* ---- the boot probe: after hwprobe.c's ---- */
static int g_from = -1, g_n = 100, g_ncyc; static int g_cyc[16]; static int g_m;
static void window_mark(void);
void __real_hw_probe(OutFn o);
void __wrap_hw_probe(OutFn o)
{
  const char *e = mgs2_getenv("MGS2_MEP"); uint32_t *big, *ord, mk; uint64_t t_all = now_us();
  __real_hw_probe(o);
  out = o;
  g_on = !(e && *e == '0');
  if (!g_on) { out("mep: MGS2_MEP=0: no ME probe, the ME stays off\n"); return; }
  scr_open();
  me_start();
  if (g_me == 1) me_cal();
  probe_copy();
  dm_init();
  mk = g_scr_used; big = scr_take(4u << 20); ord = scr_take(NL * sizeof(uint32_t));
  if (big && ord) {
    g_rs = 4242u; make_chain(big, NL, 16, ord);
    sceKernelDcacheWritebackInvalidateAll();
    k_main(big, &g_ka_m);
    if (g_me == 1 && k_me(big, &g_ka_e)) k_line("speed, alone", &g_ka_m, &g_ka_e, 0, 0);
    else k_line("speed, alone, main CPU only", &g_ka_m, 0, 0, 0);
    probe_share(big);
    {   /* stores last (they overwrite the chain): one a line over 4 MB, then the cache written back */
      uint64_t t0, t1, t2; double em = 0, ew = 0;
      sceKernelDcacheWritebackInvalidateAll();
      t0 = now_us(); mep_k_stores(big, NL, 1); t1 = now_us(); sceKernelDcacheWritebackInvalidateAll(); t2 = now_us();
      if (job(MEP_J_KERN, MEP_K_STORES, (uint32_t)(uintptr_t)big, NL, 2, 5000)) { em = me_ns(g_c->r[0], NL); ew = me_ns(g_c->r[2], 1e6); }
      if (g_me == 1)
        out("mep speed, alone: one store a line over 4 MB main %.2f ns (then its whole-cache write-back %.3f ms), ME %.2f ns (%.3f; then "
            "its write-back of the range %.3f ms)\n", ma_ns(t1 - t0, NL), (t2 - t1) / 1000.0, em, em / ma_ns(t1 - t0, NL), ew);
      else out("mep speed, alone, main CPU only: one store a line over 4 MB %.2f ns (then its whole-cache write-back %.3f ms)\n",
               ma_ns(t1 - t0, NL), (t2 - t1) / 1000.0);
    }
  } else out("mep speed: no memory for the 4 MB buffer\n");
  if (g_me == 1) { job(MEP_J_FLUSH, 0, 0, 0, 0, 2000); }   /* nothing of the boot buffers left in the ME's cache before they are reused */
  scr_give(mk);
  probe_code();
  if (g_me == 1) { job(MEP_J_FLUSH, 0, 0, 0, 0, 2000); }
  {   /* the two probes an op the ME might not take could stop it: last, and the ME started again if one did (cold: no line it held
         is written back after this) */
    int alive = g_me == 1;
    probe_fp();
    probe_me_cde();
    if (alive && g_me != 1) me_restart();
  }
  scr_close();                          /* the game's RAM zero again before the game is loaded */
  if ((e = mgs2_getenv("MGS2_MEP_CYCLE"))) {
    int k = 0; char *p;
    if (sscanf(e, "%d:%d%n", &g_from, &g_n, &k) != 2 || g_n <= 0 || e[k] != ':') g_from = -1;
    else for (p = (char *)e + k, g_ncyc = 0; (*p == ':' || *p == ',') && g_ncyc < 16; ) g_cyc[g_ncyc++] = (int)strtol(p + 1, &p, 10);
    if (!g_ncyc) g_from = -1;
  }
  window_mark();                        /* the turns' counts start here (not with the boot probe's streams) */
  out("mep: the boot probe took %.2f s; turns %s (%d windows of %d vblanks from vb %d); the ME %s; kicks %u (+%u while waiting, "
      "%.1f us each); the DMA turns %s\n", (now_us() - t_all) / 1e6, g_from >= 0 ? e : "none", g_ncyc, g_n, g_from,
      g_me == 1 ? "rests until its turns" : "is not running", (unsigned)g_kicks, (unsigned)g_rekicks, g_kicks ? (double)g_kick_us / g_kicks : 0,
      g_dm_th >= 0 ? "run" : "do not run");
}

/* ---- the turns ---- */
static uint32_t w_b, w_r, w_w, w_u, w_sl, w_ha, w_pe, w_nw, w_la, w_k; static uint64_t w_db, w_dus, w_dwb; static uint32_t w_dc, w_de, w_ds;
static void window_line(long vb)
{
  /* the counts since the last line are those of steps vb-100 .. vb-1 (step v runs after the hook of vblank v); main_psp.c's window
     line for vb covers steps vb-99 .. vb: the mode of all but one step of either is that of step vb-1 */
  int m = vb - 1 > g_from ? g_cyc[((vb - 1 - g_from - 1) / g_n) % g_ncyc] : 0;
  uint32_t b = 0, r = 0, w = 0, u = 0, sl = 0, ha = 0, pe = 0, nw = 0, la = 0, dc = g_dm_calls, de = g_dm_err, ds = g_dm_skip;
  uint64_t db = g_dm_bytes, dus = g_dm_us, dwb = g_dm_wb_us;
  if (g_me == 1) { b = g_c->s_blocks; r = g_c->s_rd; w = g_c->s_wr; u = g_c->s_busy; sl = g_c->sleeps; ha = g_c->halted; pe = g_c->pending; nw = g_c->nowake; la = g_c->late; }
  fprintf(stderr, "mep vb %ld: mode %d; ME %u blocks, read %.2f MB, wrote %.2f MB, busy %.1f ms, rests %u (in the halt %u, before it %u, "
          "no interrupt %u, given up %u), kicks %u; DMA %u calls, %.2f MB, %.1f ms in the calls, %.1f ms writing back, %u errors (last %08x), "
          "%u periods dropped\n",
          vb, m, (unsigned)(b - w_b), (r - w_r) * 64.0 / 1048576.0, (w - w_w) * 64.0 / 1048576.0, g_mhz > 0 ? (u - w_u) / g_mhz / 1000.0 : 0,
          (unsigned)(sl - w_sl), (unsigned)(ha - w_ha), (unsigned)(pe - w_pe), (unsigned)(nw - w_nw), (unsigned)(la - w_la),
          (unsigned)(g_kicks - w_k), (unsigned)(dc - w_dc), (double)(db - w_db) / 1048576.0, (dus - w_dus) / 1000.0, (dwb - w_dwb) / 1000.0,
          (unsigned)(de - w_de), (unsigned)g_dm_rc, (unsigned)(ds - w_ds));
  if (g_me == 1 && (m == 1 || m == 2) && b == w_b) me_dead("no stream block in a whole window");
  w_b = b; w_r = r; w_w = w; w_u = u; w_sl = sl; w_ha = ha; w_pe = pe; w_nw = nw; w_la = la; w_k = g_kicks; w_dc = dc; w_de = de;
  w_db = db; w_dus = dus; w_dwb = dwb; w_ds = ds;
}
static void window_mark(void)
{
  if (g_me == 1) { w_b = g_c->s_blocks; w_r = g_c->s_rd; w_w = g_c->s_wr; w_u = g_c->s_busy; w_sl = g_c->sleeps; w_ha = g_c->halted;
                   w_pe = g_c->pending; w_nw = g_c->nowake; w_la = g_c->late; }
  w_k = g_kicks; w_dc = g_dm_calls; w_de = g_dm_err; w_db = g_dm_bytes; w_dus = g_dm_us; w_dwb = g_dm_wb_us; w_ds = g_dm_skip;
}
static void mep_vb(long vb)
{
  int m;
  if (!g_on || g_from < 0) return;
  { uint32_t ca, ep, bv; if (g_me == 1 && hle_me_exception(&ca, &ep, &bv)) me_dead("checked at a vblank"); }
  m = vb > g_from ? g_cyc[((vb - g_from - 1) / g_n) % g_ncyc] : 0;   /* boot.c opt_window's slot: the step that starts now */
  if (m != g_m) {
    stream_set(m == 1 ? MEP_M_PACED : m == 2 ? MEP_M_BURST : MEP_M_REST);
    dm_set(m == 3, DM_PERIOD_US);
    g_m = m;
  }
  if (vb % 100 == 0 && vb > g_from) window_line(vb);
}
static void mep_end(const char *when)
{
  if (g_dm_th >= 0) { SceUInt to = 200000; g_dm_quit = 1; g_dm_on = 0; sceKernelSetEventFlag(g_dm_ev, 1); sceKernelWaitThreadEnd(g_dm_th, &to); g_dm_th = -1; }
  if (g_me == 1) {
    int acked; g_c->mode = MEP_M_REST; SYNC();
    acked = hle_me_stop(100); g_me = -1;
    fprintf(stderr, "mep: the ME stopped (%s), %s, held in reset; kicks %u (+%u while waiting); DMA %u calls, %.1f MB, %u errors\n", when,
            acked ? "it acknowledged" : "no acknowledgement", (unsigned)g_kicks, (unsigned)g_rekicks, (unsigned)g_dm_calls,
            (double)g_dm_bytes / 1048576.0, (unsigned)g_dm_err);
  }
}

/* ---- gl1_glue.c's words and wraps, with the probe's ---- */
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
  mep_vb(vb);
}
void __real_mgs2_exit(int code);
void __wrap_mgs2_exit(int code) { mep_end("at the end"); me_bat_line("end"); __real_mgs2_exit(code); }
int __real_sceKernelExitGame(void);
int __wrap_sceKernelExitGame(void)
{
  me_exc_release();
  mep_end("HOME");
  me_bat_line("HOME");
  me_bat_exit();
  return __real_sceKernelExitGame();
}
