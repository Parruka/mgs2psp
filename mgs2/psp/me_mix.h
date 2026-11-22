/* me_mix.h - v26me: the control block that psp/me_mix.c (main CPU) and psp/me_mix_me.c (Media Engine) share, read and written by both
   through the uncached alias only (0x40000000 | address).  Commands go main -> ME through a ring of two-word commands (spu2_split.h);
   the ME reports what it consumed, its checkpoints (boot/spu_split_b.h) and its own time (CP0 Count ticks). */
#ifndef ME_MIX_H
#define ME_MIX_H
#include <stdint.h>
#include "hle_me.h"
typedef struct MeMixCtl {
  HleMeHdr hdr;                         /* hle_me.h: magic, heartbeat, state, exit_req, count, boots, fault (64 bytes) */
  /* set by the main CPU before the start */
  uint32_t ring, mask;                  /* the ring's uncached address, entries - 1 (a power of two) */
  uint32_t ck, ckmask;                  /* the checkpoint array's uncached address, entries - 1 */
  uint32_t b;                           /* engine B (SplitB), cached, in the Media Engine's own region */
  uint32_t pad0[3];
  /* main CPU -> ME */
  uint32_t wr;                          /* commands pushed (counts on, wraps at 2^32) */
  uint32_t pad1[7];
  /* ME -> main CPU */
  uint32_t rd;                          /* commands consumed */
  uint32_t nck;                         /* checkpoints written */
  uint32_t busy;                        /* Count ticks spent on commands (wraps at 2^32, about 9 s: read the difference often) */
  uint32_t renders, maxr;               /* RENDER commands; the longest one in ticks */
  uint32_t idle;                        /* polls that found nothing */
  uint32_t last_w0, last_w1;            /* v26me3 (-DME_DIAG3): the last command the ME applied (v26me / v26me2: unused padding) */
#ifdef ME_REST
  /* v30me5 (-DME_REST, psp/v30me5.sh): the resting Media Engine (psp/me_rest.S, psp/me_mix_me.c, psp/me_mix.c) */
  uint32_t rest;                        /* main CPU -> ME: 1 = it may rest (set after the idle polls are measured; MGS2_ME_REST=0: never) */
  uint32_t asleep;                      /* ME: 1 from just before its last look at the ring until it is awake again */
  uint32_t sleeps;                      /* ME: rests begun (written before asleep) */
  uint32_t halted, pending, nowake;     /* ME: rests ended by the interrupt in the halt / by one taken before the halt / with no interrupt */
  uint32_t late;                        /* ME: rests given up: a command (or the exit request) came after asleep was written */
  uint32_t flags_last, flags_or;        /* ME: its interrupt flags 0xBC300000 as its handler found them: at the last wake, every bit seen */
  uint32_t rpc[5];                      /* ME: the words of Sony's ME requests (uOFW me_wrapper) at its last wake: 0xBFC00600 (the request),
                                           0xBFC00608 (its first argument), 0xBFC00700 (the state word), 0xBFC00710, 0xBFC00714 */
  uint32_t rpc_changes;                 /* ME: wakes at which those words differed from the wake before (the first copy: the loop's start) */
  uint32_t pad2;
} MeMixCtl;                             /* 224 bytes */
#else
} MeMixCtl;                             /* 160 bytes (the header's 64, then 24 words) */
#endif
void me_mix_work(volatile HleMeHdr *h);  /* psp/me_mix_me.c: the loop (runs on the Media Engine) */
void me_mix_sim_drain(volatile MeMixCtl *c);   /* psp/me_mix_me.c built with -DME_SIM: the loop's work, run by the main CPU (MGS2_SPU_ME=2) */
#endif
