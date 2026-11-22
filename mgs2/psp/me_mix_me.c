/* me_mix_me.c - v26me: the Media Engine's side of the voice mix split (boot/spu_split.c, psp/me_mix.c).  me_mix_work is the loop
   psp/me_mix.c starts on the Media Engine with PS2RC's bring-up (third_party/ps2rc/hle/psp/hle_me.c): it takes the commands of the
   ring in order, applies them to engine B (boot/spu_split_b.h), invalidates its cached lines of SPU RAM the main CPU announced as
   written (RAMW), writes a checkpoint every 10 s of sound and counts its own time.  B's code is PS2RC's spu2_lite.c compiled again here
   under me_ names, for the Media Engine (psp/v26me.sh: -fno-jump-tables -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns;
   the link of this unit alone must leave no undefined symbol but me_memset): hle_me.h's rules - no syscall, no libc, no function
   pointer or jump table, shared words only through the uncached alias.
   Built a second time with -DME_SIM (psp/v26me.sh) for the emulator check, MGS2_SPU_ME=2 (PPSSPP does not emulate the Media Engine):
   the same drain of the ring, under sim_ names, run by the main CPU itself whenever it waits for B (psp/me_mix.c wait_rd); its time is
   the system clock's (us). */
#ifdef ME_SIM
#define spu2_adpcm_block sim_spu2_adpcm_block
#define spu2_advance sim_spu2_advance
#define spu2_bind_ops sim_spu2_bind_ops
#define spu2_env_tick sim_spu2_env_tick
#define spu2_get_envx sim_spu2_get_envx
#define spu2_get_nax sim_spu2_get_nax
#define spu2_init sim_spu2_init
#define spu2_irq_take sim_spu2_irq_take
#define spu2_key_off sim_spu2_key_off
#define spu2_key_on sim_spu2_key_on
#define spu2_render sim_spu2_render
#define spu2_render48 sim_spu2_render48
#define spu2_set_addr sim_spu2_set_addr
#define spu2_set_irq sim_spu2_set_irq
#define spu2_set_param sim_spu2_set_param
#define spu2_set_position sim_spu2_set_position
#define spu2_sounding sim_spu2_sounding
#define spu2_vol_fixed sim_spu2_vol_fixed
#define spu2_vol_tick sim_spu2_vol_tick
#else
#define spu2_adpcm_block me_spu2_adpcm_block
#define spu2_advance me_spu2_advance
#define spu2_bind_ops me_spu2_bind_ops
#define spu2_env_tick me_spu2_env_tick
#define spu2_get_envx me_spu2_get_envx
#define spu2_get_nax me_spu2_get_nax
#define spu2_init me_spu2_init
#define spu2_irq_take me_spu2_irq_take
#define spu2_key_off me_spu2_key_off
#define spu2_key_on me_spu2_key_on
#define spu2_render me_spu2_render
#define spu2_render48 me_spu2_render48
#define spu2_set_addr me_spu2_set_addr
#define spu2_set_irq me_spu2_set_irq
#define spu2_set_param me_spu2_set_param
#define spu2_set_position me_spu2_set_position
#define spu2_sounding me_spu2_sounding
#define spu2_vol_fixed me_spu2_vol_fixed
#define spu2_vol_tick me_spu2_vol_tick
#endif
#include "spu2_lite.c"
#include "spu_split_b.h"
#include "me_mix.h"

#ifdef ME_SIM
#include <pspkernel.h>
#define ME_COUNT() sceKernelGetSystemTimeLow()
#define ME_DINVAL(p, n) ((void)0)        /* one CPU: nothing to invalidate */
#else
#define ME_COUNT() hle_me_count()
#define ME_DINVAL(p, n) hle_me_dinval(p, n)
#endif

typedef struct MeLoc { uint32_t rd, nck, maxr, renders, busy; } MeLoc;   /* the loop's copies of its ctl words */

/* the commands [l->rd, wr) applied to B in order; the counters published */
static void me_drain(volatile MeMixCtl *c, SplitB *x, MeLoc *l, uint32_t wr)
{
  volatile uint32_t *ring = (volatile uint32_t *)c->ring, *ck = (volatile uint32_t *)c->ck;
  uint32_t mask = c->mask, ckmask = c->ckmask, rd = l->rd, t0 = ME_COUNT();
  while (rd != wr) {
    uint32_t w0 = ring[2 * (rd & mask)], w1 = ring[2 * (rd & mask) + 1], op = SPU2C_OP(w0);
    if (op == SPU2C_RAMW) {
      ME_DINVAL(x->b.ram + (w1 & (SPU2_RAM_SIZE - 1u)), w0 >> 8);   /* the main CPU wrote it back before the command */
    } else {
      SplitCk k; uint32_t r0 = ME_COUNT();
      if (split_b_apply(x, w0, w1, &k)) {
        volatile uint32_t *e = ck + 8 * (l->nck & ckmask);
        e[0] = k.n; e[1] = k.frames_lo; e[2] = k.frames_hi; e[3] = k.crc; e[4] = k.vhash; e[5] = k.cmds; e[6] = ME_COUNT(); e[7] = 0;
        hle_me_sync();                  /* the entry in memory before the count that publishes it */
        c->nck = ++l->nck;
      }
      if (op == SPU2C_RENDER) { uint32_t d = ME_COUNT() - r0; if (d > l->maxr) l->maxr = d; l->renders++; }
    }
#ifdef ME_DIAG3
    c->last_w0 = w0; c->last_w1 = w1;   /* v26me3 (psp/v26me3.sh): the last command applied, for battery.txt's lines */
#endif
    c->rd = ++rd;
  }
  l->rd = rd;
  l->busy += ME_COUNT() - t0;
  c->busy = l->busy; c->maxr = l->maxr; c->renders = l->renders;
}

#ifdef ME_SIM
void me_mix_sim_drain(volatile MeMixCtl *c)
{
  MeLoc l; uint32_t wr = c->wr;
  l.rd = c->rd; l.nck = c->nck; l.maxr = c->maxr; l.renders = c->renders; l.busy = c->busy;
  if (wr != l.rd) me_drain(c, (SplitB *)c->b, &l, wr);
}
#else
/* the compiler's own memset calls (struct and array clears), renamed by psp/v26me.sh (objcopy --redefine-sym memset=me_memset) */
void *me_memset(void *d, int c, unsigned n)
{
  volatile uint8_t *p = (volatile uint8_t *)d;
  while (n--) *p++ = (uint8_t)c;
  return d;
}

#define ME_IDLE_NOPS 4096               /* about 12 us between polls of an empty ring (few uncached reads on the shared bus) */

#ifdef ME_REST
/* v30me5 (psp/v30me5.sh; Ffm chose "Build it", decision card 2026-10-26 20:43): after ME_REST_AFTER polls of an empty ring the loop
   rests (psp/me_rest.S: HALT until the main CPU's interrupt) instead of polling on at full speed, once the main CPU allows it (c->rest,
   set after it measured the idle polls; MGS2_ME_REST=0: never).  The handshake with psp/me_mix.c's split_plat_push: the ME counts the
   rest (sleeps), writes asleep, then looks at the ring once more and halts only if it is still empty; the main CPU writes wr, then looks
   at asleep and interrupts the ME once per rest (by sleeps).  Each side syncs between its write and its read, so at least one of the
   two sees the other's write: a command is never left behind with the ME halted.  After a wake the words me_rest.S's handler left in the
   boot memory go into the control block, with the words of Sony's ME requests (uOFW me_wrapper: the system's ME driver writes them
   before it interrupts the ME, on the same line as the main CPU's kick), so a wake by anyone but this program shows. */
void me_rest_init(void);
void me_rest(void);
extern char me_rest_halt[];
#define ME_REST_AFTER 4                 /* empty polls before a rest (a poll took about 75 us on the PSP: v30 run 5's idle polls) */
#define MW(i) (((volatile uint32_t *)0xBFC00400u)[i])   /* me_rest.S's words ([12..15]) */
#define RPCW(o) (*(volatile uint32_t *)(0xBFC00000u + (o)))
static void rpc_copy(volatile MeMixCtl *c)
{
  uint32_t w[5], i, d = 0;
  w[0] = RPCW(0x600); w[1] = RPCW(0x608); w[2] = RPCW(0x700); w[3] = RPCW(0x710); w[4] = RPCW(0x714);
  for (i = 0; i < 5; i++) { d |= w[i] ^ c->rpc[i]; c->rpc[i] = w[i]; }
  if (d) c->rpc_changes = c->rpc_changes + 1;
}
static void me_rest_once(volatile MeMixCtl *c, uint32_t rd)
{
  uint32_t w0 = MW(13);
  c->sleeps = c->sleeps + 1;
  hle_me_sync();
  c->asleep = 1;
  hle_me_sync();
  if (c->wr == rd && !c->hdr.exit_req) {
    me_rest();
    if (MW(13) != w0) {                 /* the handler ran */
      uint32_t e = MW(15), hl = (uint32_t)(uintptr_t)me_rest_halt & 0x7FFFFFFFu;
      if (e == hl || e == hl + 4u) c->halted = c->halted + 1; else c->pending = c->pending + 1;
      c->flags_last = MW(12); c->flags_or = MW(14);
      rpc_copy(c);
    } else c->nowake = c->nowake + 1;   /* the HALT returned with no interrupt taken */
  } else c->late = c->late + 1;
  c->asleep = 0;
  hle_me_sync();
}
#endif

void me_mix_work(volatile HleMeHdr *h)
{
#if defined(ME_PROG_MARK) && !defined(ME_SIM)
  *(volatile uint32_t *)0xBFC00414u = 0x4C4F4F50u;   /* v26me2 (psp/v26me2.sh): "LOOP" into progress word [5] (psp/me/hle_me_mgs2.c) */
#endif
  volatile MeMixCtl *c = (volatile MeMixCtl *)h;
  SplitB *x = (SplitB *)c->b;
  MeLoc l; uint32_t idle = c->idle;
#ifdef ME_REST
  uint32_t spins = 0, rest_set = 0;
  rpc_copy(c); c->rpc_changes = 0;      /* the request words as the loop starts */
#endif
  l.rd = c->rd; l.nck = c->nck; l.maxr = c->maxr; l.renders = c->renders; l.busy = c->busy;
  c->hdr.boots = c->hdr.boots + 1;
  c->hdr.state = 1;
  for (;;) {
    uint32_t wr, i;
    c->hdr.heartbeat = c->hdr.heartbeat + 1;
    c->hdr.count = hle_me_count();
    if (c->hdr.exit_req) break;
    wr = c->wr;
    if (wr == l.rd) {
      c->idle = ++idle;
#ifdef ME_REST
      if (c->rest) {
        if (!rest_set) { me_rest_init(); rest_set = 1; }
        if (++spins >= ME_REST_AFTER) { spins = 0; me_rest_once(c, l.rd); continue; }
      }
#endif
      for (i = 0; i < ME_IDLE_NOPS; i++) __asm__ volatile("nop");
      continue;
    }
#ifdef ME_REST
    spins = 0;
#endif
    me_drain(c, x, &l, wr);
  }
  hle_me_dwbinv_all();                  /* B's state and stack to memory before the halt (hle_me.h) */
  c->hdr.state = 2;
  hle_me_halt();
}
#endif
