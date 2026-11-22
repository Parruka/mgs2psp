/* me_ep.h - the ME's eDRAM probe (2026-11-03, task #150; mgs2/me_edram_probe_notes.md, me_handoff_design.md 4.9 and section 6 step
   3).  The control block psp/me_ep.c (main CPU) and psp/me_ep_me.c (Media Engine) share, read and written by both through the
   uncached alias only (0x40000000 | address).  The kernels both cores run are gl14mp's (psp/me_probe_kern.c, prototypes in
   psp/me_probe.h).  The eDRAM is the ME's alone: the main CPU never reads or writes it, so every check of what is in it runs on the
   ME. */
#ifndef ME_EP_H
#define ME_EP_H
#include <stdint.h>
#include "hle_me.h"
#include "me_probe.h"

/* the ME's eDRAM, as the ME sees it: 2 MB on the PSP-1000, 4 MB on later models by psp-media-engine-reload's README (memory map), at
   0x00000000; its uncached alias by the PSP's 0x40000000 rule [I].  Probe 1 (gl16ep's run, 2026-11-03) took a bus error on the ME
   at its first store there, in both turns.  Probe 2 (a build made with -DEP_P2, task #163) reaches it through the kernel segments,
   as the custom ME core's stack does (0x80200000 / 0x80400000: the top of a 2 or 4 MB eDRAM through the cached kernel segment):
   0x80000000 cached, 0xA0000000 uncached */
#ifdef EP_P2
#define EP_EDRAM      0x80000000u
#define EP_EDRAM_UNC  0xA0000000u
#else
#define EP_EDRAM      0x00000000u
#define EP_EDRAM_UNC  0x40000000u
#endif
#define EP_MB         0x00100000u
#define EP_ARENA      EP_MB             /* the game's turns copy into the eDRAM's first MB; the rest holds the sentinel's pattern */

enum { EP_J_NONE, EP_J_CAL, EP_J_KERN, EP_J_FILL, EP_J_CHECK, EP_J_COPY, EP_J_CMP, EP_J_FLUSH, EP_J_REG,
       EP_J_WORD, EP_J_SNAP };          /* jobs (EpCtl.job); the last two only in probe 2's build (-DEP_P2) */
enum { EP_C_MAIN, EP_C_PLAIN, EP_C_CDE, EP_C_UNC };     /* copies (EP_J_COPY a[0]): main RAM -> main RAM, and into the eDRAM with
                                                           plain stores, claimed lines (cache 0x18) or uncached stores */
enum { EP_M_REST, EP_M_COPY };                          /* the ME's stream (EpCtl.mode) */
enum { EP_R_B = 1, EP_R_C = 2 };                        /* EP_J_REG a[0]: turn B's write, turn C's three (only in a build made with
                                                           -DEP_REG_B / -DEP_REG_C, each after Ffm's yes) */
#define EP_LINES_PER_BLOCK 1024u        /* a stream block: 64 KB copied; the ME looks at its words between blocks */
#define EP_SENT_PER_BLOCK  (4u << 10)   /* the sentinel checked after each block: 4 KB (about 5% of a block's time) */

typedef struct EpCtl {
  HleMeHdr hdr;                         /* hle_me.h: magic, heartbeat, state, exit_req, count, boots, fault (64 bytes) */
  /* main CPU -> ME */
  uint32_t job, job_seq;                /* the job and its number: a new number posts it (arguments written first) */
  uint32_t a[6];                        /* its arguments */
  uint32_t mode;                        /* the stream: EP_M_* (read between blocks) */
  uint32_t rest;                        /* 1: the ME may rest (halt) when it has nothing to do */
  uint32_t cal_run;                     /* EP_J_CAL runs while 1 */
  uint32_t src, src_bytes;              /* the stream's source window in game RAM (cached address; read only) and its size */
  uint32_t kind;                        /* the stream's copy: into the arena (EP_C_PLAIN, EP_C_CDE, EP_C_UNC) or the ring (EP_C_MAIN) */
  uint32_t sent_off, sent_bytes, sent_seed;   /* the sentinel: eDRAM offset, size, the pattern's seed (0 bytes: none) */
  uint32_t ring, ring_bytes;            /* the main RAM ring (cached address, the ME's writes) for EP_C_MAIN, and its size */
  uint32_t pad0[9];
  /* ME -> main CPU */
  uint32_t done_seq;                    /* = job_seq when the job is done */
  uint32_t r[8];                        /* its results (Count ticks, values) */
  uint32_t cal;                         /* Count, rewritten while cal_run */
  uint32_t asleep, sleeps, halted, pending, nowake, late;   /* the rests (psp/me_rest.S's handshake, as gl14mp) */
  uint32_t flags_last, flags_or;        /* the ME's interrupt flags at its last wake, every bit found */
  uint32_t s_rd, s_wr;                  /* the stream: 64-byte lines read (game RAM) and written (eDRAM) */
  uint32_t s_busy;                      /* Count ticks in the stream (wraps: read the difference often) */
  uint32_t s_blocks;                    /* blocks done */
  uint32_t s_mode;                      /* the stream the ME is in */
  uint32_t sent_kb;                     /* the sentinel: KB checked (all run) */
  uint32_t sent_bad;                    /* words found changed (all run) */
  uint32_t sent_first_off, sent_first_val;   /* the first changed word: its eDRAM offset (0xFFFFFFFF: none yet) and value */
  uint32_t sent_blk[2];                 /* the 64 KB blocks of the eDRAM in which a changed word was found (bit n: offset n x 64 KB) */
  uint32_t prog;                        /* probe 2: the address the ME is at, written before each 64 KB it fills or checks, each copy,
                                           comparison or stream block, each one-word test and each register read; an odd value: the
                                           write-back of the range that starts one below it; 0xFFFFFFFC: the whole D-cache's write-back
                                           at a job's end (read by the main CPU when the ME stops) */
  uint32_t pad1[6];
} EpCtl;

/* the pattern: a word for each eDRAM offset, different for each seed (a filled range checked later shows any word that changed) */
static inline uint32_t ep_pat(uint32_t off, uint32_t seed)
{
  uint32_t x = (off * 0x9e3779b1u) ^ seed;
  x ^= x >> 15; x *= 0x2c1b3c6du; x ^= x >> 12;
  return x;
}

void ep_work(volatile HleMeHdr *h);     /* psp/me_ep_me.c: the ME's loop */
#endif
