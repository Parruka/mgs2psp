/* me_probe.h - gl14mp (2026-11-02, task #132; mgs2/me_probe_notes.md): the Media Engine probe.  The control block psp/me_probe.c
   (main CPU) and psp/me_probe_me.c (Media Engine) share, read and written by both through the uncached alias only (0x40000000 |
   address), and the kernels both cores run (psp/me_probe_kern.c: compiled under the ME's rules, called by the main CPU at their user
   address and by the ME at the kernel alias of the same code). */
#ifndef ME_PROBE_H
#define ME_PROBE_H
#include <stdint.h>
#include "hle_me.h"

enum { MEP_J_NONE, MEP_J_CAL, MEP_J_FP, MEP_J_KERN, MEP_J_COPY, MEP_J_FLUSH };            /* jobs (MepCtl.job) */
enum { MEP_K_ALU, MEP_K_FALU, MEP_K_LINES, MEP_K_CHASE, MEP_K_STORES, MEP_K_CODE };        /* kernels (MEP_J_KERN, a[0]) */
enum { MEP_C_PLAIN, MEP_C_CDE };                                                          /* copies (MEP_J_COPY, a[0]) */
enum { MEP_M_REST, MEP_M_PACED, MEP_M_BURST };                                            /* the ME's streams (MepCtl.mode) */
#define MEP_FP_OPS 17                   /* mep_fp_run's ops: me_probe_kern.c */
#define MEP_PACE_REPT 97                /* the paced stream's work per line read: 97 x 3 dependent integer instructions */
#define MEP_LINES_PER_BLOCK 1024u       /* a stream block: 64 KB read; the ME looks at its words between blocks */

typedef struct MepCtl {
  HleMeHdr hdr;                         /* hle_me.h: magic, heartbeat, state, exit_req, count, boots, fault (64 bytes) */
  /* main CPU -> ME */
  uint32_t job, job_seq;                /* the job and its number: a new number posts it (arguments written first) */
  uint32_t a[6];                        /* its arguments */
  uint32_t mode;                        /* the stream: MEP_M_* (read between blocks) */
  uint32_t fcr31;                       /* the FPU rule the ME sets for MEP_J_FP (the main CPU's: 0x01000001) */
  uint32_t rest;                        /* 1: the ME may rest (halt) when it has nothing to do */
  uint32_t cal_run;                     /* MEP_J_CAL runs while 1 */
  uint32_t src, src_bytes;              /* the streams' source window in game RAM (cached address; read only) and its size */
  uint32_t ring, ring_bytes;            /* the streams' ring (cached address, the ME's writes) */
  uint32_t cde;                         /* 1: the burst stream claims its ring lines (cache 0x18; after the ME's own copy check) */
  uint32_t pad0[11];
  /* ME -> main CPU */
  uint32_t done_seq;                    /* = job_seq when the job is done */
  uint32_t r[7];                        /* its results (Count ticks, values) */
  uint32_t cal;                         /* Count, rewritten while cal_run */
  uint32_t fcr31_found;                 /* the ME's FCR31 at its start, before it set any */
  uint32_t asleep, sleeps, halted, pending, nowake, late;   /* the rests (psp/me_mix_me.c's handshake, psp/me_rest.S) */
  uint32_t flags_last, flags_or;        /* the ME's interrupt flags at its last wake, every bit found */
  uint32_t s_rd, s_wr;                  /* the streams: 64-byte lines read and written */
  uint32_t s_busy;                      /* Count ticks in the streams (wraps: read the difference often) */
  uint32_t s_blocks;                    /* blocks done */
  uint32_t s_mode;                      /* the stream the ME is in */
  uint32_t pad1[9];
} MepCtl;

/* the kernels (psp/me_probe_kern.c), both cores */
uint32_t mep_k_alu(uint32_t n);                                      /* n x (64 dependent addu / xor + the loop's 2) */
uint32_t mep_k_falu(uint32_t n);                                     /* n x (16 dependent add.s / mul.s pairs + the loop's 2) */
uint32_t mep_k_lines(const uint32_t *p, uint32_t nlines);            /* one load per 64-byte line, 4 lines per round */
uint32_t mep_k_chase(const uint32_t *p, uint32_t start, uint32_t steps);   /* dependent loads: k = p[k] */
void mep_k_stores(uint32_t *p, uint32_t nlines, uint32_t v);         /* one store per 64-byte line */
void mep_k_copy(uint32_t *d, const uint32_t *s, uint32_t nbytes);    /* 64 bytes a round; d, s, nbytes 64-aligned */
void mep_k_copy_cde(uint32_t *d, const uint32_t *s, uint32_t nbytes);   /* the same, each line of d claimed first (cache 0x18) */
uint32_t mep_fp_run(uint32_t op, const uint32_t *in, uint32_t n, uint32_t *out);   /* in: n pairs; out: n results; returns FCR31 */
extern const char *const mep_fp_name[MEP_FP_OPS];                  /* (main CPU only: a table of pointers) */

void mep_work(volatile HleMeHdr *h);   /* psp/me_probe_me.c: the ME's loop */
#endif
