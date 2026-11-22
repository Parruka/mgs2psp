/* spu_split.c - v26me (2026-10-23; Ffm chose the Media Engine for the game's sound at 18:28): PS2RC's voice mix split
   (third_party/ps2rc/audio/spu2_split.h) on our IOP model, as a measurement.  runtime/iop_host.h's engine A keeps the IOP-visible voice
   state in state-only mode, as in every build; with MGS2_SPU_ME=1 every register write and render block of A is also recorded as a
   command for a second engine B that decodes and mixes the voices: the work v24s timed on the main CPU (sound_notes.md, +7.53 ms a deck
   step).  On the PSP B runs on the Media Engine (psp/me_mix.c, psp/me_mix_me.c); on the PC this file applies each command at once (the
   reference).  B's samples go nowhere yet: they are summed into a checksum (boot/spu_split_b.h).
   Linked after the program's own objects and reached through the link's --wrap (psp/v26me.sh; the PC build links it the same way), so
   iop_host.h and boot.c are not changed and the program's code keeps its addresses:
     --wrap=spu2_bind_ops  iop_host_init's binding of A: the recording wrappers (spu2_rec_bind_ops), B = a copy of A as the full engine
     --wrap=spu2_render48  spu_step's render of A: spu2_split.c r_advance, one RENDER command per engine block
     --wrap=iop_rt_create  the IOP runtime's SPU RAM writes: the barrier before (B has consumed every command) and RAMW after
   spu2_split.c is built with -Dspu2_bind_ops=__real_spu2_bind_ops -Dspu2_render48=__real_spu2_render48 (its own calls stay A's).
   Checkpoints (every 10 s of sound): A's state hash at the render that reaches one, B's state hash and output checksum at the same
   render; "mix: ck" lines in the log.  Equal state hashes: B followed A.  Equal checksums with the PC's run: B's samples are the PC's. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spu2_split.h"
#include "iop_rt.h"
#include "spu_split_b.h"
#include "spu_split.h"

void __real_spu2_bind_ops(Spu2 *s, Spu2Ops *ops, void (*timer_cb)(void *), void *timer_user, int lockstep);
void __real_spu2_render48(Spu2 *s, int16_t *out, uint32_t frames);
IopRt *__real_iop_rt_create(const IopRtConfig *cfg);

static Spu2Rec g_rec; static Spu2Ops g_ops;
static Spu2 *g_a;                       /* A while the split runs */
int g_split_on;                         /* 1: commands are recorded and B consumes them */
#define SPLIT_ACK 4096                  /* A's checkpoints kept for the comparison (11 hours of sound) */
static uint32_t g_avh[SPLIT_ACK]; static uint64_t g_afr, g_anext = SPLIT_CK_FRAMES; static uint32_t g_ank;
static uint32_t g_ck_eq, g_ck_ne, g_ck_first_ne = ~0u;

static void rec_push(void *u, uint32_t w0, uint32_t w1)
{
  (void)u;
  if (SPU2C_OP(w0) == SPU2C_RENDER) {   /* A has just rendered this block (r_advance): its state is the one B will have after it */
    g_afr += w1;
    if (g_afr >= g_anext) { g_anext += SPLIT_CK_FRAMES; if (g_ank < SPLIT_ACK) g_avh[g_ank] = split_vhash(g_a); g_ank++; }
  }
  split_plat_push(w0, w1);
}
static void rec_barrier(void *u) { (void)u; split_plat_barrier(); }
static void rec_ram_written(void *u, uint32_t a, uint32_t n) { (void)u; split_plat_ram_written(a, n); }

/* B's checkpoint (from the platform, in order): compared with A's of the same number, printed */
void split_ck_b(const SplitCk *ck, const char *extra)
{
  uint32_t av = ck->n < SPLIT_ACK && ck->n < g_ank ? g_avh[ck->n] : 0; int ok = ck->n < SPLIT_ACK && ck->n < g_ank && av == ck->vhash;
  if (ok) g_ck_eq++; else { g_ck_ne++; if (g_ck_first_ne == ~0u) g_ck_first_ne = ck->n; }
  fprintf(stderr, "mix: ck %u frames %llu cmds %u crc %08x vB %08x vA %08x %s%s\n", (unsigned)ck->n,
          (unsigned long long)ck->frames_lo | (unsigned long long)ck->frames_hi << 32, (unsigned)ck->cmds, (unsigned)ck->crc,
          (unsigned)ck->vhash, (unsigned)av, ok ? "eq" : "NE", extra ? extra : "");
}
void split_report(const char *when)
{
  fprintf(stderr, "mix %s: %llu commands (%llu writes dropped as no-ops), %llu renders, %llu frames, %llu SPU RAM writes (%llu KB), "
          "%llu barriers; checkpoints A %u, B equal %u, B different %u", when, (unsigned long long)g_rec.cmds,
          (unsigned long long)g_rec.dropped, (unsigned long long)g_rec.renders, (unsigned long long)g_rec.frames,
          (unsigned long long)g_rec.ramw, (unsigned long long)(g_rec.ramw_bytes >> 10), (unsigned long long)g_rec.barriers, (unsigned)g_ank,
          (unsigned)g_ck_eq, (unsigned)g_ck_ne);
  if (g_ck_ne) fprintf(stderr, " (first %u)", (unsigned)g_ck_first_ne);
  fprintf(stderr, "\n");
}
uint64_t split_cmds(void) { return g_rec.cmds; }
uint64_t split_frames(void) { return g_rec.frames; }

void __wrap_spu2_bind_ops(Spu2 *s, Spu2Ops *ops, void (*timer_cb)(void *), void *timer_user, int lockstep)
{
  const char *e = getenv("MGS2_SPU_ME"); char why[160]; SplitB *b0;
  if (!e || !atoi(e) || g_a) { __real_spu2_bind_ops(s, ops, timer_cb, timer_user, lockstep); return; }
  if (!s->x_state_only || timer_user || lockstep) {   /* the split needs iop_host.h's binding as it is: A state-only, no timer, no lockstep */
    fprintf(stderr, "mix: off (A state-only %d, timer %d, lockstep %d)\n", s->x_state_only, timer_user != 0, lockstep);
    __real_spu2_bind_ops(s, ops, timer_cb, timer_user, lockstep); return;
  }
  spu2_rec_bind_ops(&g_rec, s, &g_ops, timer_cb, timer_user);   /* A's own binding (spu2_bind_ops, lockstep) + the recording wrappers */
  g_rec.push = rec_push; g_rec.barrier = rec_barrier; g_rec.ram_written = rec_ram_written; g_rec.user = 0;
  b0 = split_plat_b();                  /* B's memory (on the PSP the Media Engine's own region) */
  if (!b0) { fprintf(stderr, "mix: off (no memory for engine B)\n"); __real_spu2_bind_ops(s, ops, timer_cb, timer_user, lockstep); return; }
  memcpy(&b0->b, s, sizeof *s);         /* B = A as it is now (nothing has been written yet), as the full engine */
  b0->b.x_state_only = 0; b0->b.timer_cb = 0; b0->b.timer_user = 0; b0->b.sink = 0; b0->b.sink_user = 0; b0->b.tap = 0; b0->b.tap_user = 0;
  split_b_init(b0);
  g_a = s;
  if (!split_plat_start(why, sizeof why)) {
    fprintf(stderr, "mix: off (%s)\n", why); g_a = 0;
    __real_spu2_bind_ops(s, ops, timer_cb, timer_user, lockstep); return;
  }
  spu2_rec_set_mode(&g_rec, 1);         /* recording; A stays state-only */
  *ops = g_ops; ops->advance = 0;       /* iop_host.h copies the operations into the runtime's table; it renders through spu2_render48 */
  g_split_on = 1;
  fprintf(stderr, "mix: on (%s)\n", why);
}

void __wrap_spu2_render48(Spu2 *s, int16_t *out, uint32_t frames)
{
  if (g_split_on && s == g_a && !out) g_ops.advance(&g_rec, frames);   /* r_advance: A's blocks + one RENDER each */
  else __real_spu2_render48(s, out, frames);
}

IopRt *__wrap_iop_rt_create(const IopRtConfig *cfg)
{
  IopRtConfig c;
  if (!g_split_on) return __real_iop_rt_create(cfg);
  c = *cfg; c.sd.dma_begin = g_ops.dma_begin; c.sd.dma_done = g_ops.dma_done;
  return __real_iop_rt_create(&c);
}

/* the split stops recording (the platform's consumer died): A goes on as before, state only, nothing recorded */
void split_stop(const char *why)
{
  if (!g_split_on) return;
  g_split_on = 0; g_rec.mode = 0; g_a->x_state_only = 1;
  fprintf(stderr, "mix: stopped (%s)\n", why);
}

#ifndef __psp__
/* ---- the PC reference: B applied at once, in this process (the SPU RAM image is shared, so RAMW needs nothing) ---- */
static SplitB g_b;
SplitB *split_plat_b(void) { return &g_b; }
static void pc_report(void) { split_report("at the end"); }
int split_plat_start(char *why, int n) { snprintf(why, n, "PC reference: B applies each command at once"); atexit(pc_report); return 1; }
static int g_chk = -1, g_chk_left = 400;   /* dev: MGS2_MIX_CHECK=1 compares A and B after every render, field by field, and prints
                                              each (voice, field) difference when it appears and when it goes */
static uint32_t g_chk_m[SPU2_VOICES + 1];
static void chk_voices(uint32_t w0, uint32_t w1)
{
  unsigned i, f; (void)w0; (void)w1;
  for (i = 0; i <= SPU2_VOICES && g_chk_left > 0; i++) {
    uint32_t m = 0; long av[20], bv[20]; static const char *const nm[20] = { "phase", "env", "env_counter", "counter", "blk", "idx", "flags",
      "lsa", "lsa_written", "ssa", "pitch", "adsr1", "adsr2", "vl.value", "vl.counter", "vl.reg", "vr.value", "vr.counter", "vr.reg", "endx" };
    if (i < SPU2_VOICES) {
      const Spu2Voice *a = &g_a->v[i], *b = &g_b.b.v[i]; int k = 0;
#define F(x) av[k] = (long)a->x; bv[k] = (long)b->x; if (av[k] != bv[k]) m |= 1u << k; k++;
      F(phase) F(env) F(env_counter) F(counter) F(blk) F(idx) F(flags) F(lsa) F(lsa_written) F(ssa) F(pitch) F(adsr1) F(adsr2)
      F(vl.value) F(vl.counter) F(vl.reg) F(vr.value) F(vr.counter) F(vr.reg)
#undef F
      if (a->phase >= SPU2_ENV_STOPPED && b->phase >= SPU2_ENV_STOPPED) m &= ~(1u << 2);   /* env_counter: dead while stopped */
    } else { av[19] = (long)(g_a->endx & 0xffffffffffffull); bv[19] = (long)(g_b.b.endx & 0xffffffffffffull); if (g_a->endx != g_b.b.endx) m = 1u << 19; }
    if (m == g_chk_m[i]) continue;
    for (f = 0; f < 20; f++) if ((m ^ g_chk_m[i]) >> f & 1) {
      if (m >> f & 1) fprintf(stderr, "mix check: frames %llu voice %u %s differs: A %lx B %lx (A phase %ld env %ld)\n", (unsigned long long)g_b.frames, i,
                              nm[f], av[f], bv[f], i < SPU2_VOICES ? (long)g_a->v[i].phase : -1, i < SPU2_VOICES ? (long)g_a->v[i].env : -1);
      else fprintf(stderr, "mix check: frames %llu voice %u %s equal again\n", (unsigned long long)g_b.frames, i, nm[f]);
      g_chk_left--;
    }
    g_chk_m[i] = m;
  }
}
void split_plat_push(uint32_t w0, uint32_t w1)
{
  SplitCk ck;
  if (SPU2C_OP(w0) == SPU2C_RAMW) return;
  if (split_b_apply(&g_b, w0, w1, &ck)) split_ck_b(&ck, 0);
  if (g_chk < 0) g_chk = getenv("MGS2_MIX_CHECK") && atoi(getenv("MGS2_MIX_CHECK"));
  if (g_chk && SPU2C_OP(w0) == SPU2C_RENDER) chk_voices(w0, w1);
}
void split_plat_barrier(void) {}
void split_plat_ram_written(uint32_t a, uint32_t n) { (void)a; (void)n; }
#endif
