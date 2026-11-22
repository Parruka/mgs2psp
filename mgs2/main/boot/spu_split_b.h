/* spu_split_b.h - v26me: engine B of the voice mix split (third_party/ps2rc/audio/spu2_split.h), the part that both the PSP's Media
   Engine (psp/me_mix_me.c) and the PC reference (boot/spu_split.c without __psp__) run: one command applied to B, the rendered samples
   summed into a checksum, and a checkpoint every SPLIT_CK_FRAMES rendered 48 kHz frames (frames, checksum, a hash of B's IOP-visible
   voice state, commands so far).  boot/spu_split.c takes A's state hash at the same render, so equal hashes say B followed A.
   Media Engine code (third_party/ps2rc/hle/psp/hle_me.h's rules): no libc, no function pointers, no jump tables. */
#ifndef SPU_SPLIT_B_H
#define SPU_SPLIT_B_H
#include <stdint.h>
#include "spu2_split.h"

#define SPLIT_CK_FRAMES 480000u        /* a checkpoint every 10 s of 48 kHz sound */

typedef struct SplitCk {               /* 32 bytes */
  uint32_t n;                          /* checkpoint number (0, 1, ...) */
  uint32_t frames_lo, frames_hi;       /* 48 kHz frames rendered when it was taken */
  uint32_t crc;                        /* FNV-1a of every output sample so far (16-bit values, left then right) */
  uint32_t vhash;                      /* split_vhash of the engine */
  uint32_t cmds;                       /* commands applied so far */
  uint32_t pad[2];
} SplitCk;

typedef struct SplitB {
  Spu2 b;                              /* engine B: A's state at the start, the full engine (decode, interpolation, envelope, volumes) */
  int16_t out[2 * SPU2_BLOCK_FRAMES];  /* the block a RENDER command renders (stereo s16, 48 kHz) */
  uint64_t frames, next_ck;
  uint32_t crc, cmds, nck;
} SplitB;

static inline uint32_t split_mix(uint32_t h, uint32_t x) { return (h ^ x) * 16777619u; }

/* the voice state the IOP model can observe or that decides it (positions, loop flags, envelope, volumes, ENDX); not the decoder's
   history or the samples (A in state-only mode does not keep them), not the IRQ latch (only A's is taken), and the envelope counter
   only while the voice sounds: a stopped voice's counter is never read before key-on sets it to 0, and A (state only) ticks the
   envelope over a whole block before it moves the position, so when an end flag stops a voice mid-block A's dead counter is a few
   ticks ahead of B's (sound_notes.md, v26me) */
static inline uint32_t split_vhash(const Spu2 *s)
{
  uint32_t h = 2166136261u; unsigned i;
  for (i = 0; i < SPU2_VOICES; i++) {
    const Spu2Voice *v = &s->v[i];
    h = split_mix(h, v->phase); h = split_mix(h, (uint32_t)v->env); h = split_mix(h, v->phase < SPU2_ENV_STOPPED ? v->env_counter : 0);
    h = split_mix(h, v->counter); h = split_mix(h, v->blk); h = split_mix(h, v->idx); h = split_mix(h, v->flags);
    h = split_mix(h, v->lsa); h = split_mix(h, v->lsa_written); h = split_mix(h, v->ssa); h = split_mix(h, v->pitch);
    h = split_mix(h, v->adsr1); h = split_mix(h, v->adsr2);
    h = split_mix(h, (uint32_t)v->vl.value); h = split_mix(h, v->vl.counter); h = split_mix(h, v->vl.reg);
    h = split_mix(h, (uint32_t)v->vr.value); h = split_mix(h, v->vr.counter); h = split_mix(h, v->vr.reg);
  }
  h = split_mix(h, (uint32_t)s->endx); h = split_mix(h, (uint32_t)(s->endx >> 32));
  h = split_mix(h, (uint32_t)s->clock); h = split_mix(h, (uint32_t)(s->clock >> 32));
  return h;
}

static inline void split_b_init(SplitB *x)
{
  unsigned i;
  for (i = 0; i < 2 * SPU2_BLOCK_FRAMES; i++) x->out[i] = 0;
  x->frames = 0; x->next_ck = SPLIT_CK_FRAMES; x->crc = 2166136261u; x->cmds = 0; x->nck = 0;
}

/* one command (not RAMW: the platform's cache maintenance); 1 = a checkpoint was taken into *ck */
static inline int split_b_apply(SplitB *x, uint32_t w0, uint32_t w1, SplitCk *ck)
{
  uint32_t k = spu2_cmd_apply(&x->b, w0, w1, x->out), i, h;
  x->cmds++;
  if (!k) return 0;
  h = x->crc;
  for (i = 0; i < 2 * k; i++) h = split_mix(h, (uint16_t)x->out[i]);
  x->crc = h; x->frames += k;
  if (x->frames < x->next_ck) return 0;
  x->next_ck += SPLIT_CK_FRAMES;
  ck->n = x->nck++; ck->frames_lo = (uint32_t)x->frames; ck->frames_hi = (uint32_t)(x->frames >> 32);
  ck->crc = x->crc; ck->vhash = split_vhash(&x->b); ck->cmds = x->cmds; ck->pad[0] = ck->pad[1] = 0;
  return 1;
}
#endif
